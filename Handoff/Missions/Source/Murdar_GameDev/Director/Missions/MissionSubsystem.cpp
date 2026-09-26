#include "Director/Missions/MissionSubsystem.h"

#include "Director/Missions/MissionDefinition.h"
#include "Director/ChapterDirector.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Character/MurdarHUD.h"

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr float TickSeconds = 0.25f;
	// Synthetic fail events the subsystem feeds the runtime (the bus events need a filter first: whose death?).
	const char* const FailPlayerDied = "Mission.Fail.PlayerDied";
	const char* const FailArrested = "Mission.Fail.Arrested";

	std::string ToStd(const FGameplayTag& T) { return T.IsValid() ? std::string(TCHAR_TO_UTF8(*T.GetTagName().ToString())) : std::string(); }
	FGameplayTag FromStd(const std::string& S) { return FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(S.c_str())), false); }
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
}

UMissionSubsystem* UMissionSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UMissionSubsystem>() : nullptr;
}

bool UMissionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMissionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadMissions();
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		// One subscription on the root: missions start and progress on any event the authors name.
		TWeakObjectPtr<UMissionSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UMissionSubsystem::Tick4Hz, TickSeconds, true);
}

void UMissionSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Runtime.Reset();
	Super::Deinitialize();
}

void UMissionSubsystem::LoadMissions()
{
	// Every DA_Mission_* the AssetManager scans (README §Patches 2). A handful of small assets: synchronous is fine.
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("Mission")), Ids);
	for (const FPrimaryAssetId& Id : Ids)
	{
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
		if (UMissionDefinition* M = Cast<UMissionDefinition>(Path.TryLoad()))
		{
			Missions.Add(M);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Missions: %d loaded"), Missions.Num());
}

bool UMissionSubsystem::IsDone(const UMissionDefinition* M) const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return M && State && M->MissionTag.IsValid() && State->HasFact(M->MissionTag);
}

bool UMissionSubsystem::CanStart(const UMissionDefinition* M) const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return M && State && M->Objectives.Num() > 0 && (M->bRepeatable || !IsDone(M)) && State->Check(M->RequiredFacts, M->ForbiddenFacts);
}

MurdarMission::FMissionSpec UMissionSubsystem::BuildSpec(const UMissionDefinition* M) const
{
	MurdarMission::FMissionSpec S;
	S.Id = ToStd(M->MissionTag);
	S.TimeLimitSeconds = M->TimeLimitSeconds;
	for (const FGameplayTag& T : M->FailOnEvents) { S.FailOnEvents.push_back(ToStd(T)); }
	for (const FGameplayTag& T : M->FailIfFacts) { S.FailIfFacts.push_back(ToStd(T)); }
	if (M->bFailOnPlayerDeath) { S.FailOnEvents.push_back(FailPlayerDied); }
	if (M->bFailOnArrest) { S.FailOnEvents.push_back(FailArrested); }
	for (const FMissionObjective& O : M->Objectives)
	{
		MurdarMission::FObjectiveSpec OS;
		OS.Id = TCHAR_TO_UTF8(*O.Id.ToString());
		OS.CompleteOnEvent = ToStd(O.CompleteOnEvent);
		OS.CompleteOnPayload = ToStd(O.CompleteOnPayload);
		for (const FGameplayTag& T : O.RequiredFacts) { OS.RequiredFacts.push_back(ToStd(T)); }
		OS.TimeLimitSeconds = O.TimeLimitSeconds;
		if (!O.ReachActorTag.IsNone())
		{
			// The place is an actor the level designer tags (a garage door, a phone booth). Missing = the objective can't
			// complete by place — logged loudly, it's an authoring bug.
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (It->ActorHasTag(O.ReachActorTag))
				{
					const FVector L = It->GetActorLocation();
					OS.bReach = true;
					OS.ReachLocation = {float(L.X), float(L.Y), float(L.Z)};
					OS.ReachRadiusCm = O.ReachRadiusCm;
					break;
				}
			}
			if (!OS.bReach)
			{
				UE_LOG(LogTemp, Error, TEXT("Mission %s objective %s: no actor tagged %s in this level"),
					*M->GetName(), *O.Id.ToString(), *O.ReachActorTag.ToString());
				OS.bReach = true; // unreachable on purpose: better stuck and logged than silently skipped
				OS.ReachRadiusCm = 0.f;
				OS.ReachLocation = {1.e9f, 1.e9f, 1.e9f};
			}
		}
		S.Objectives.push_back(OS);
	}
	return S;
}

bool UMissionSubsystem::StartMission(UMissionDefinition* M)
{
	if (!M || Active.IsValid() || M->Objectives.Num() == 0)
	{
		return false;
	}
	Active = M;
	Runtime = MakeUnique<MurdarMission::FMissionRuntime>(BuildSpec(M));
	PublishMission(TEXT("Event.Mission.Started"));
	UE_LOG(LogTemp, Log, TEXT("Mission started: %s"), *M->GetName());
	Apply(Runtime->Start(GetWorld()->GetTimeSeconds()));
	return true;
}

bool UMissionSubsystem::RetryLastFailed()
{
	UMissionDefinition* M = LastFailed.Get();
	if (!M || Active.IsValid())
	{
		return false;
	}
	if (!M->RetryCheckpoint.IsNone())
	{
		if (UChapterDirector* Director = UChapterDirector::Get(this)) { Director->GoToCheckpoint(M->RetryCheckpoint); }
	}
	return StartMission(M);
}

void UMissionSubsystem::AbortActive(const FString& Reason)
{
	if (Runtime && Active.IsValid())
	{
		Apply(Runtime->Abort(TCHAR_TO_UTF8(*Reason)));
	}
}

void UMissionSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (bInHandler)
	{
		return; // our own Event.Mission.* published while applying steps: not an input
	}
	TGuardValue<bool> Guard(bInHandler, true);

	// A load ends whatever was running (active missions are not saved; RetryCheckpoint is how the player gets back).
	if (E.Tag.MatchesTagExact(MurdarTags::Event_State_Loaded))
	{
		Runtime.Reset();
		Active.Reset();
		return;
	}

	if (Runtime && Active.IsValid())
	{
		std::string TagName = ToStd(E.Tag);
		// Whose death? Only the player's fails a mission.
		if (E.Tag.MatchesTag(MurdarTags::Event_Actor_Died))
		{
			if (E.Source != UGameplayStatics::GetPlayerPawn(this, 0)) { return; }
			TagName = FailPlayerDied;
		}
		else if (E.Tag.MatchesTag(MurdarTags::Event_Police_Arrested))
		{
			TagName = FailArrested;
		}
		Apply(Runtime->OnEvent(TagName, ToStd(E.Payload)));
		return;
	}

	// Nothing running: does this event start a mission?
	for (UMissionDefinition* M : Missions)
	{
		if (M && M->StartOnEvent.IsValid() && E.Tag.MatchesTag(M->StartOnEvent)
			&& (!M->StartOnPayload.IsValid() || E.Payload.MatchesTag(M->StartOnPayload)) && CanStart(M))
		{
			StartMission(M);
			return;
		}
	}
}

void UMissionSubsystem::Tick4Hz()
{
	if (!Runtime || !Active.IsValid())
	{
		return;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector L = Player ? Player->GetActorLocation() : FVector(1.e9f); // no pawn (respawning): nowhere
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	auto HasFact = [State](const std::string& F) { const FGameplayTag T = FromStd(F); return State && T.IsValid() && State->HasFact(T); };
	TGuardValue<bool> Guard(bInHandler, true);
	Apply(Runtime->Tick(GetWorld()->GetTimeSeconds(), {float(L.X), float(L.Y), float(L.Z)}, HasFact));
}

void UMissionSubsystem::Apply(const std::vector<MurdarMission::FStep>& Steps)
{
	UMissionDefinition* M = Active.Get();
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	if (!M)
	{
		return;
	}
	for (const MurdarMission::FStep& S : Steps)
	{
		switch (S.Kind)
		{
		case MurdarMission::EStepKind::ObjectiveStarted:
			if (M->Objectives.IsValidIndex(S.Objective)) { Subtitle(M->Objectives[S.Objective].Text); }
			PublishMission(TEXT("Event.Mission.Objective"), float(S.Objective));
			break;
		case MurdarMission::EStepKind::ObjectiveCompleted:
			if (State && M->Objectives.IsValidIndex(S.Objective))
			{
				for (const FGameplayTag& F : M->Objectives[S.Objective].FactsOnComplete) { State->SetFact(F); }
			}
			break;
		case MurdarMission::EStepKind::Succeeded:
			if (State)
			{
				for (const FGameplayTag& F : M->FactsOnSuccess) { State->SetFact(F); }
				if (M->MissionTag.IsValid()) { State->SetFact(M->MissionTag); } // "done", and saved with the facts
				for (const TPair<FGameplayTag, float>& R : M->ValueRewards) { State->AddValue(R.Key, R.Value); }
			}
			Subtitle(M->SuccessText);
			PublishMission(TEXT("Event.Mission.Succeeded"));
			UE_LOG(LogTemp, Log, TEXT("Mission succeeded: %s"), *M->GetName());
			Active.Reset();
			Runtime.Reset();
			if (!M->SuccessCheckpoint.IsNone())
			{
				if (UChapterDirector* Director = UChapterDirector::Get(this)) { Director->ReachCheckpoint(M->SuccessCheckpoint); } // autosaves
			}
			return;
		case MurdarMission::EStepKind::Failed:
			if (State) { for (const FGameplayTag& F : M->FactsOnFail) { State->SetFact(F); } }
			Subtitle(M->FailText);
			PublishMission(TEXT("Event.Mission.Failed"));
			UE_LOG(LogTemp, Log, TEXT("Mission failed: %s (%s)"), *M->GetName(), UTF8_TO_TCHAR(S.Reason.c_str()));
			LastFailed = M;
			Active.Reset();
			Runtime.Reset();
			return;
		}
	}
}

void UMissionSubsystem::Subtitle(const FText& Text) const
{
	if (!Text.IsEmpty())
	{
		if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->ShowSubtitle(Text); }
	}
}

void UMissionSubsystem::PublishMission(const TCHAR* TagName, float Magnitude) const
{
	UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this);
	const UMissionDefinition* M = Active.Get();
	if (Bus && M)
	{
		FGameEvent E;
		E.Tag = Tag(TagName);
		E.Payload = M->MissionTag;
		E.Magnitude = Magnitude;
		Bus->Publish(E);
	}
}

UMissionDefinition* UMissionSubsystem::FindByName(const FString& Name) const
{
	for (UMissionDefinition* M : Missions)
	{
		if (M && (M->GetName().Contains(Name) || M->MissionTag.ToString().Contains(Name))) { return M; }
	}
	return nullptr;
}

FString UMissionSubsystem::Describe() const
{
	FString Out = FString::Printf(TEXT("missions: %d known"), Missions.Num());
	if (const UMissionDefinition* M = Active.Get())
	{
		const int32 O = Runtime ? Runtime->GetObjective() : -1;
		Out += FString::Printf(TEXT(" | ACTIVE %s objective %d/%d (%s)"), *M->GetName(), O + 1, M->Objectives.Num(),
			M->Objectives.IsValidIndex(O) ? *M->Objectives[O].Id.ToString() : TEXT("-"));
	}
	for (const UMissionDefinition* M : Missions)
	{
		Out += FString::Printf(TEXT("\n  %s %s%s"), *M->GetName(), IsDone(M) ? TEXT("[done]") : TEXT(""),
			CanStart(M) ? TEXT(" [available]") : TEXT(""));
	}
	if (const UMissionDefinition* F = LastFailed.Get()) { Out += FString::Printf(TEXT("\n  last failed: %s (MurdarMissionRetry)"), *F->GetName()); }
	return Out;
}
