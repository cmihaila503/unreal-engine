#include "Director/Hints/HintSubsystem.h"

#include "AI/FactionMemorySubsystem.h"
#include "Character/MurdarHUD.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "UI/Menus/MurdarPlayerSettings.h" // Handoff/Menus (+ README patch: bHints)

#include "Engine/World.h"
#include "TimerManager.h"

namespace { FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); } }

UHintSubsystem* UHintSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UHintSubsystem>() : nullptr;
}

bool UHintSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHintSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UHintSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UHintSubsystem::Tick1Hz, 1.f, true);
}

void UHintSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Super::Deinitialize();
}

bool UHintSubsystem::Shown(const FGameplayTag& Hint) const
{
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	return S && Hint.IsValid() && S->HasFact(Hint);
}

void UHintSubsystem::Offer(const FGameplayTag& Hint)
{
	Scheduler.Offer(TCHAR_TO_UTF8(*Hint.ToString()), Shown(Hint), GetWorld()->GetTimeSeconds());
}

void UHintSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (E.Tag == Tag(TEXT("Event.Dialogue.Started"))) { ++Dialogues; }
	else if (E.Tag == Tag(TEXT("Event.Dialogue.Ended"))) { Dialogues = FMath::Max(0, Dialogues - 1); }
	else if (E.Tag == Tag(TEXT("Event.Cutscene.Started"))) { bCutscene = true; }
	else if (E.Tag == Tag(TEXT("Event.Cutscene.Ended"))) { bCutscene = false; }

	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	for (const FHintDefinition& H : GetDefault<UHintSettings>()->Hints)
	{
		if (H.OnEvent.IsValid() && E.Tag.MatchesTag(H.OnEvent) && (!H.OnPayload.IsValid() || E.Payload.MatchesTag(H.OnPayload))
			&& (!S || S->Check(H.RequiredFacts, FGameplayTagContainer())))
		{
			Offer(H.Hint);
		}
	}
}

bool UHintSubsystem::IsBusy() const
{
	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	return Dialogues > 0 || bCutscene || (Mem && int(Mem->GetWantedLevel()) >= 2);
}

void UHintSubsystem::Tick1Hz()
{
	if (!UMurdarPlayerSettings::Get()->bHints) { return; }
	const UHintSettings* Settings = GetDefault<UHintSettings>();
	MurdarHints::FTuning T; T.MinGapSeconds = Settings->MinGapSeconds; T.WaitForCalmSeconds = Settings->WaitForCalmSeconds;
	const std::string Id = Scheduler.Poll(GetWorld()->GetTimeSeconds(), IsBusy(), T);
	if (Id.empty()) { return; }
	const FGameplayTag Hint = Tag(UTF8_TO_TCHAR(Id.c_str()));
	for (const FHintDefinition& H : Settings->Hints)
	{
		if (H.Hint != Hint) { continue; }
		if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->ShowSubtitle(FText::Format(NSLOCTEXT("MurdarHints", "Fmt", "» {0}"), H.Text), Settings->ShowSeconds); }
		if (UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this)) { S->SetFact(Hint); }
	}
}

FString UHintSubsystem::Describe() const
{
	return FString::Printf(TEXT("Hints: %d waiting, busy %d, enabled %d"), Scheduler.Waiting(), IsBusy() ? 1 : 0, UMurdarPlayerSettings::Get()->bHints ? 1 : 0);
}
