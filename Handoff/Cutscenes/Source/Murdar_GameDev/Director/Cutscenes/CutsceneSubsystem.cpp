#include "Director/Cutscenes/CutsceneSubsystem.h"

#include "Director/Cutscenes/CutsceneDefinition.h"
#include "Director/Dialogue/DialogueSubsystem.h"          // Handoff/Dialogue
#include "Director/Interaction/InteractionSubsystem.h"    // Handoff/Interaction
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "AI/FactionMemorySubsystem.h"
#include "Character/MurdarHUD.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/AssetManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"                  // ADAPT: "LevelSequence", "MovieScene" in Build.cs
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

namespace { FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); } }

UCutsceneSubsystem* UCutsceneSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UCutsceneSubsystem>() : nullptr;
}

bool UCutsceneSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCutsceneSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("Cutscene")), Ids);
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (UCutsceneDefinition* C = Cast<UCutsceneDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad())) { All.Add(C); }
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UCutsceneSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
}

void UCutsceneSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (IsPlaying()) { End(); }
	Super::Deinitialize();
}

MurdarCutscene::FGate UCutsceneSubsystem::GateFor(const UCutsceneDefinition* C) const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	MurdarCutscene::FGate G;
	G.bFactsOk = !State || State->Check(C->RequiredFacts, C->ForbiddenFacts);
	G.bOnce = C->bOnce;
	G.bAlreadyPlayed = State && C->CutsceneTag.IsValid() && State->HasFact(C->CutsceneTag);
	G.Wanted = Mem ? int(Mem->GetWantedLevel()) : 0;
	G.bInDialogue = false; // a dialogue is aborted by a scene, not waited for (README): keep the story moving
	return G;
}

void UCutsceneSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (bInHandler) { return; }
	for (UCutsceneDefinition* C : All)
	{
		if (C && C->StartOnEvent.IsValid() && E.Tag.MatchesTag(C->StartOnEvent) && (!C->StartOnPayload.IsValid() || E.Payload.MatchesTag(C->StartOnPayload)))
		{
			Play(C);
		}
	}
}

bool UCutsceneSubsystem::Play(UCutsceneDefinition* C)
{
	if (!C) { return false; }
	switch (MurdarCutscene::Decide(GateFor(C)))
	{
	case MurdarCutscene::EDecision::Drop: return false;
	case MurdarCutscene::EDecision::Wait: return Queue.Push(TCHAR_TO_UTF8(*C->GetName()), GetWorld()->GetTimeSeconds());
	case MurdarCutscene::EDecision::Play: break;
	}
	if (IsPlaying()) { return Queue.Push(TCHAR_TO_UTF8(*C->GetName()), GetWorld()->GetTimeSeconds()); }
	ULevelSequence* Seq = C->Sequence.LoadSynchronous();
	if (!Seq) { UE_LOG(LogTemp, Error, TEXT("Cutscene %s has no sequence"), *C->GetName()); return false; }
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableMovementInput = true;
	Settings.bDisableLookAtInput = true;
	Settings.bHidePlayer = false;
	Player = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Seq, Settings, SequenceActor);
	if (!Player) { return false; }
	Active = C;
	Begin();
	Player->OnFinished.AddUniqueDynamic(this, &UCutsceneSubsystem::OnFinished);
	Player->Play();
	return true;
}

void UCutsceneSubsystem::Begin()
{
	TGuardValue<bool> Guard(bInHandler, true);
	if (UDialogueSubsystem* D = UDialogueSubsystem::Get(this)) { D->AbortDialogue(); }
	if (UInteractionSubsystem* I = UInteractionSubsystem::Get(this)) { I->PushBlock(); }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->GetPawn()) { PC->GetPawn()->DisableInput(PC); }
	if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->SetActorHiddenInGame(true); } // ADAPT: README §Patches 2 (Slate panels too)
	if (Active->bLetterbox && GEngine && GEngine->GameViewport)
	{
		static FSlateColorBrush Black(FLinearColor::Black);
		Letterbox = SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(0.12f)[SNew(SBorder).BorderImage(&Black)]
			+ SVerticalBox::Slot().FillHeight(0.76f)[SNew(SBox)]
			+ SVerticalBox::Slot().FillHeight(0.12f)[SNew(SBorder).BorderImage(&Black)];
		GEngine->GameViewport->AddViewportWidgetContent(Letterbox.ToSharedRef(), 40);
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Publish(FGameEvent(Tag(TEXT("Event.Cutscene.Started")), nullptr, FVector::ZeroVector, 0.f, Active->CutsceneTag)); }
}

void UCutsceneSubsystem::OnFinished() { End(); }

void UCutsceneSubsystem::Skip()
{
	if (IsPlaying() && Active->bSkippable && Player) { Player->GoToEndAndStop(); } // fires OnFinished
}

void UCutsceneSubsystem::End()
{
	TGuardValue<bool> Guard(bInHandler, true);
	UCutsceneDefinition* C = Active.Get();
	Active.Reset();
	if (Letterbox.IsValid() && GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(Letterbox.ToSharedRef()); }
	Letterbox.Reset();
	if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->SetActorHiddenInGame(false); }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->GetPawn()) { PC->GetPawn()->EnableInput(PC); }
	if (UInteractionSubsystem* I = UInteractionSubsystem::Get(this)) { I->PopBlock(); }
	if (C)
	{
		if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this))
		{
			if (C->CutsceneTag.IsValid()) { State->SetFact(C->CutsceneTag); }
			for (const FGameplayTag& F : C->FactsOnEnd) { State->SetFact(F); }
		}
		if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Publish(FGameEvent(Tag(TEXT("Event.Cutscene.Ended")), nullptr, FVector::ZeroVector, 0.f, C->CutsceneTag)); }
	}
	if (SequenceActor) { SequenceActor->Destroy(); SequenceActor = nullptr; }
	Player = nullptr;
}

void UCutsceneSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsPlaying())
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const bool bHeld = PC && (PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Enter) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom));
		if (SkipHold.Update(bHeld, DeltaTime)) { Skip(); }
		return;
	}
	// Something waited (a chase ended): play the next still worth playing.
	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	const bool bStillHot = Mem && int(Mem->GetWantedLevel()) >= 2; // pop only when it could play, or the wait never goes stale
	if (Queue.Size() > 0 && !bStillHot)
	{
		const std::string Next = Queue.Pop(GetWorld()->GetTimeSeconds());
		if (UCutsceneDefinition* C = FindByName(UTF8_TO_TCHAR(Next.c_str()))) { Play(C); }
	}
}

UCutsceneDefinition* UCutsceneSubsystem::FindByName(const FString& Name) const
{
	for (UCutsceneDefinition* C : All) { if (C && C->GetName().Contains(Name)) { return C; } }
	return nullptr;
}
