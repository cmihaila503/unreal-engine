#include "Director/Dialogue/DialogueSubsystem.h"

#include "Director/Dialogue/DialogueDefinition.h"
#include "Director/Dialogue/DialogueSettings.h"
#include "Director/Interaction/InteractionSubsystem.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Character/MurdarHUD.h"
#include "AI/MurdarPoliceAIController.h" // ADAPT: path of AMurdarPoliceAIController

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	std::string ToStd(const FGameplayTag& T) { return T.IsValid() ? std::string(TCHAR_TO_UTF8(*T.GetTagName().ToString())) : std::string(); }
	std::string ToStd(const FName& N) { return N.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*N.ToString())); }
	std::string ToStd(const FText& T) { return std::string(TCHAR_TO_UTF8(*T.ToString())); }
	FString FromStd(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }

	MurdarDialogue::FCond ToCond(const FDialogueCondition& C)
	{
		MurdarDialogue::FCond Out;
		for (const FGameplayTag& T : C.RequiredFacts) { Out.Required.push_back(ToStd(T)); }
		for (const FGameplayTag& T : C.ForbiddenFacts) { Out.Forbidden.push_back(ToStd(T)); }
		for (const FDialogueValueCheck& V : C.Values) { Out.Values.push_back({ ToStd(V.Value), V.Min, V.Max }); }
		return Out;
	}

	std::vector<MurdarDialogue::FEffect> ToEffects(const TArray<FDialogueEffect>& Es)
	{
		std::vector<MurdarDialogue::FEffect> Out;
		for (const FDialogueEffect& E : Es)
		{
			MurdarDialogue::FEffect X;
			switch (E.Kind)
			{
			case EDialogueEffect::SetFact:   X.Kind = MurdarDialogue::EEffect::SetFact; break;
			case EDialogueEffect::ClearFact: X.Kind = MurdarDialogue::EEffect::ClearFact; break;
			case EDialogueEffect::AddValue:  X.Kind = MurdarDialogue::EEffect::AddValue; break;
			case EDialogueEffect::Publish:   X.Kind = MurdarDialogue::EEffect::Publish; break;
			case EDialogueEffect::PayPolice: X.Kind = MurdarDialogue::EEffect::PayPolice; break;
			}
			X.Tag = ToStd(E.Tag);
			X.Amount = E.Amount;
			Out.push_back(X);
		}
		return Out;
	}
}

UDialogueSubsystem* UDialogueSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UDialogueSubsystem>() : nullptr;
}

bool UDialogueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

float UDialogueSubsystem::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f; }

void UDialogueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadDialogues();
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UDialogueSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
}

void UDialogueSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	Runtime.Reset();
	Spec.Reset();
	Super::Deinitialize();
}

void UDialogueSubsystem::LoadDialogues()
{
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("Dialogue")), Ids);
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (UDialogueDefinition* D = Cast<UDialogueDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad()))
		{
			Dialogues.Add(D);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Dialogue: %d loaded"), Dialogues.Num());
}

void UDialogueSubsystem::OnBusEvent(const FGameEvent& E)
{
	// Barks first: they never start a conversation and may come while one runs.
	if (E.Tag == MurdarTags::Event_Police_Line || E.Tag == Tag(TEXT("Event.Dialogue.Bark")))
	{
		Bark(E);
	}
	if (bInHandler) { return; } // our own Event.Dialogue.* / Publish effects don't start other conversations
	for (UDialogueDefinition* D : Dialogues)
	{
		if (D && D->StartOnEvent.IsValid() && E.Tag.MatchesTag(D->StartOnEvent)
			&& (!D->StartOnPayload.IsValid() || E.Payload.MatchesTag(D->StartOnPayload)))
		{
			if (StartDialogue(D, const_cast<AActor*>(E.Source.Get()))) { return; }
		}
	}
}

MurdarDialogue::FQuery UDialogueSubsystem::MakeQuery() const
{
	TWeakObjectPtr<const UNarrativeStateSubsystem> State = UNarrativeStateSubsystem::Get(this);
	MurdarDialogue::FQuery Q;
	Q.HasFact = [State](const std::string& T)
	{
		const FGameplayTag G = FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(T.c_str())), false);
		return State.IsValid() && G.IsValid() && State->HasFact(G);
	};
	Q.Value = [State](const std::string& T)
	{
		const FGameplayTag G = FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(T.c_str())), false);
		return State.IsValid() && G.IsValid() ? State->GetValue(G) : 0.f;
	};
	return Q;
}

void UDialogueSubsystem::BuildSpec(const UDialogueDefinition* D, MurdarDialogue::FDialogueSpec& Out) const
{
	const UDialogueSettings* S = GetDefault<UDialogueSettings>();
	for (const FDialogueEntry& E : D->Entries) { Out.Entries.push_back({ ToCond(E.Condition), ToStd(E.Node) }); }
	for (const FDialogueNode& N : D->Nodes)
	{
		MurdarDialogue::FNode X;
		X.Id = ToStd(N.Id);
		X.Speaker = ToStd(N.Speaker);
		X.Text = ToStd(N.Text);
		X.Seconds = N.Seconds;
		if (X.Seconds <= 0.f && !N.Voice.IsNull())
		{
			// Small files, loaded when the conversation starts; the voice length is the line's time.
			if (const USoundBase* Sound = N.Voice.LoadSynchronous()) { X.Seconds = Sound->GetDuration() + S->VoiceTailSeconds; }
		}
		X.Cond = ToCond(N.Condition);
		X.Effects = ToEffects(N.Effects);
		for (const FDialogueChoice& C : N.Choices)
		{
			MurdarDialogue::FChoice Y;
			Y.Text = ToStd(C.Text);
			Y.Cond = ToCond(C.Condition);
			Y.Effects = ToEffects(C.Effects);
			Y.Next = ToStd(C.Next);
			X.Choices.push_back(Y);
		}
		X.ChoiceTimeout = N.ChoiceTimeout;
		X.DefaultChoice = N.DefaultChoice;
		X.Next = ToStd(N.Next);
		Out.Nodes.push_back(X);
	}
}

bool UDialogueSubsystem::StartDialogue(UDialogueDefinition* Dialogue, AActor* InSpeaker)
{
	if (!Dialogue) { return false; }
	if (IsTalking())
	{
		if (!Dialogue->bInterruptsOthers) { return false; }
		AbortDialogue();
	}

	TUniquePtr<MurdarDialogue::FDialogueSpec> NewSpec = MakeUnique<MurdarDialogue::FDialogueSpec>();
	BuildSpec(Dialogue, *NewSpec);
	TUniquePtr<MurdarDialogue::FRuntime> NewRuntime = MakeUnique<MurdarDialogue::FRuntime>();
	std::vector<MurdarDialogue::FStep> Steps;
	if (!NewRuntime->Start(*NewSpec, MakeQuery(), Now(), Steps))
	{
		return false; // no entry for the way things are now: not an error, the man has nothing to say
	}

	Spec = MoveTemp(NewSpec);
	Runtime = MoveTemp(NewRuntime);
	Active = Dialogue;
	Speaker = InSpeaker;
	bHadSpeaker = InSpeaker != nullptr;
	NodeVoices.Reset();
	for (const FDialogueNode& N : Dialogue->Nodes) { NodeVoices.Add(N.Id.ToString(), N.Voice); }

	if (UInteractionSubsystem* Interaction = UInteractionSubsystem::Get(this))
	{
		Interaction->PushBlock();
		bBlockingInteraction = true;
	}
	PublishDialogue(TEXT("Event.Dialogue.Started"));
	Apply(Steps);
	return true;
}

void UDialogueSubsystem::AbortDialogue()
{
	if (!Runtime.IsValid()) { return; }
	std::vector<MurdarDialogue::FStep> Steps;
	Runtime->Abort(Steps);
	Apply(Steps);
}

bool UDialogueSubsystem::IsChoosing() const
{
	return Runtime.IsValid() && Runtime->GetState() == MurdarDialogue::FRuntime::EState::Choosing;
}

bool UDialogueSubsystem::Choose(int32 VisibleIndex)
{
	if (!Runtime.IsValid()) { return false; }
	std::vector<MurdarDialogue::FStep> Steps;
	const bool bOk = Runtime->Choose(VisibleIndex, Now(), MakeQuery(), Steps);
	Apply(Steps);
	return bOk;
}

void UDialogueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsTalking()) { return; }

	// The speaker is gone (killed, despawned) or he walked / drove away: the conversation is over.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	const UDialogueDefinition* D = Active.Get();
	if (!D || (bHadSpeaker && !Speaker.IsValid())
		|| (D->MaxDistanceCm > 0.f && Speaker.IsValid() && Player
			&& FVector::DistSquared(Player->GetActorLocation(), Speaker->GetActorLocation()) > FMath::Square(D->MaxDistanceCm)))
	{
		AbortDialogue();
		return;
	}

	if (IsChoosing() && PC)
	{
		const UDialogueSettings* S = GetDefault<UDialogueSettings>();
		for (int32 i = 0; i < NumChoices; ++i)
		{
			const bool bKey = S->ChoiceKeys.IsValidIndex(i) && PC->WasInputKeyJustPressed(S->ChoiceKeys[i]);
			const bool bPad = S->GamepadChoiceKeys.IsValidIndex(i) && PC->WasInputKeyJustPressed(S->GamepadChoiceKeys[i]);
			if (bKey || bPad)
			{
				Choose(i);
				return;
			}
		}
	}

	std::vector<MurdarDialogue::FStep> Steps;
	Runtime->Tick(Now(), MakeQuery(), Steps);
	Apply(Steps);
}

void UDialogueSubsystem::Apply(const std::vector<MurdarDialogue::FStep>& Steps)
{
	TGuardValue<bool> Guard(bInHandler, true);
	for (const MurdarDialogue::FStep& Step : Steps)
	{
		switch (Step.Kind)
		{
		case MurdarDialogue::EStep::Line:
		{
			const FString Text = FromStd(Step.Text);
			const FString Line = Step.Speaker.empty() ? Text : FString::Printf(TEXT("%s: %s"), *FromStd(Step.Speaker), *Text);
			if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->ShowSubtitle(FText::FromString(Line), Step.Seconds); }
			if (const TSoftObjectPtr<USoundBase>* Voice = NodeVoices.Find(FromStd(Runtime->CurrentNodeId())); Voice && !Voice->IsNull())
			{
				PlayVoice(Voice->LoadSynchronous(), Speaker.Get(), FVector::ZeroVector);
			}
			break;
		}
		case MurdarDialogue::EStep::Choices:
		{
			FString Prompt;
			for (int32 i = 0; i < int32(Step.ChoiceTexts.size()); ++i)
			{
				Prompt += FString::Printf(TEXT("%s%d  %s"), i ? TEXT("     ") : TEXT(""), i + 1, *FromStd(Step.ChoiceTexts[i]));
			}
			ChoicesPrompt = FText::FromString(Prompt);
			NumChoices = int32(Step.ChoiceTexts.size());
			break;
		}
		case MurdarDialogue::EStep::ChoicesClosed:
			ChoicesPrompt = FText::GetEmpty();
			NumChoices = 0;
			break;
		case MurdarDialogue::EStep::Effect:
			ApplyEffect(Step.Effect);
			break;
		case MurdarDialogue::EStep::End:
			if (Step.Reason != "done" && Step.Reason != "timeout" && Step.Reason != "aborted")
			{
				UE_LOG(LogTemp, Error, TEXT("Dialogue %s ended: %s"), *GetNameSafe(Active.Get()), *FromStd(Step.Reason));
			}
			PublishDialogue(TEXT("Event.Dialogue.Ended"));
			if (bBlockingInteraction)
			{
				if (UInteractionSubsystem* Interaction = UInteractionSubsystem::Get(this)) { Interaction->PopBlock(); }
				bBlockingInteraction = false;
			}
			ChoicesPrompt = FText::GetEmpty();
			NumChoices = 0;
			Active.Reset();
			Speaker.Reset();
			// Runtime/Spec stay until the next start: we may be inside Runtime's own call right now.
			break;
		}
	}
}

void UDialogueSubsystem::ApplyEffect(const MurdarDialogue::FEffect& Effect)
{
	const FGameplayTag T = FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(Effect.Tag.c_str())), false);
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	switch (Effect.Kind)
	{
	case MurdarDialogue::EEffect::SetFact:   if (State && T.IsValid()) { State->SetFact(T); } break;
	case MurdarDialogue::EEffect::ClearFact: if (State && T.IsValid()) { State->ClearFact(T); } break;
	case MurdarDialogue::EEffect::AddValue:
		// Stat.Honor is written only through FHonorToken (chapter director): refused here as in missions.
		if (State && T.IsValid() && T != MurdarTags::Stat_Honor) { State->AddValue(T, Effect.Amount); }
		break;
	case MurdarDialogue::EEffect::Publish:
		if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this); Bus && T.IsValid())
		{
			const AActor* S = Speaker.Get();
			Bus->Publish(FGameEvent(T, S, S ? S->GetActorLocation() : FVector::ZeroVector, Effect.Amount, Active.IsValid() ? Active->DialogueTag : FGameplayTag()));
		}
		break;
	case MurdarDialogue::EEffect::PayPolice:
	{
		const APawn* Pawn = Cast<APawn>(Speaker.Get());
		if (AMurdarPoliceAIController* Police = Pawn ? Cast<AMurdarPoliceAIController>(Pawn->GetController()) : nullptr)
		{
			Police->ReceiveBribe(Effect.Amount); // says TakesMoney or RefusesMoney itself; money moves on Event.Police.Bribed
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Dialogue %s: PayPolice but the speaker %s is not a police unit"), *GetNameSafe(Active.Get()), *GetNameSafe(Speaker.Get()));
		}
		break;
	}
	}
}

void UDialogueSubsystem::Bark(const FGameEvent& E)
{
	const FDialogueVoiceSet* Set = GetDefault<UDialogueSettings>()->BarkVoices.Find(E.Payload);
	if (!Set || Set->Variants.Num() == 0) { return; }
	const int I = Barks.Pick(ToStd(E.Payload), Set->Variants.Num(), Now(), GetDefault<UDialogueSettings>()->BarkCooldownSeconds, FMath::FRand());
	if (I >= 0)
	{
		PlayVoice(Set->Variants[I].LoadSynchronous(), E.Source.Get(), E.Location);
	}
}

void UDialogueSubsystem::PlayVoice(USoundBase* Sound, const AActor* At, const FVector& Fallback) const
{
	if (!Sound) { return; }
	USoundAttenuation* Attenuation = GetDefault<UDialogueSettings>()->VoiceAttenuation.LoadSynchronous();
	if (At)
	{
		// Attached, so a unit talking from a moving car is heard from the car.
		UGameplayStatics::SpawnSoundAttached(Sound, At->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, Attenuation);
	}
	else if (!Fallback.IsZero())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Fallback, 1.f, 1.f, 0.f, Attenuation);
	}
	else
	{
		UGameplayStatics::PlaySound2D(this, Sound); // a phone, a thought
	}
}

void UDialogueSubsystem::PublishDialogue(const TCHAR* TagName) const
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		const AActor* S = Speaker.Get();
		Bus->Publish(FGameEvent(Tag(TagName), S, S ? S->GetActorLocation() : FVector::ZeroVector, 0.f, Active.IsValid() ? Active->DialogueTag : FGameplayTag()));
	}
}

UDialogueDefinition* UDialogueSubsystem::FindByName(const FString& Name) const
{
	for (UDialogueDefinition* D : Dialogues)
	{
		if (D && D->GetName().Contains(Name)) { return D; }
	}
	return nullptr;
}

FString UDialogueSubsystem::Describe() const
{
	FString Out = FString::Printf(TEXT("Dialogue: %d loaded; active: %s node %s%s\n"), Dialogues.Num(),
		Active.IsValid() ? *Active->GetName() : TEXT("-"), Runtime.IsValid() ? *FromStd(Runtime->CurrentNodeId()) : TEXT("-"),
		IsChoosing() ? *FString::Printf(TEXT(" choosing [%s]"), *ChoicesPrompt.ToString()) : TEXT(""));
	for (const UDialogueDefinition* D : Dialogues)
	{
		Out += FString::Printf(TEXT("  %-32s on %s %s\n"), *D->GetName(), *D->StartOnEvent.ToString(), *D->StartOnPayload.ToString());
	}
	return Out;
}
