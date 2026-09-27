// The AI's stuck register. Every agent runs its own MurdarAIQ::FWatchdog (traffic in Think, foot NPCs in their think)
// and reports its level here each think. This subsystem:
//  - recycles what is stuck for good (level 3) or stranded, when nobody sees it (the population refills elsewhere);
//  - spots gridlocks (3+ cars at level 2 on one junction) and logs them with the junction's zone;
//  - runs the soak test: `Murdar.AI.Soak 600 [tour]` records every escalation for 10 minutes (optionally teleporting
//    the player round the navmesh every 60 s so the population keeps moving), writes Saved/Logs/AISoak_<time>.csv and
//    prints PASS / FAIL. That is the number that says whether the AI got better - not an impression;
//  - `Murdar.AI.Stuck` lists who is stuck now, `Murdar.AI.StuckDraw 1` marks them in the world.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Quality/AIQualityRules.h"
#include "AIWatchdogSubsystem.generated.h"

UCLASS()
class MURDAR_GAMEDEV_API UAIWatchdogSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAIWatchdogSubsystem* Get(const UObject* WorldContext);

	/**
	 * An agent's think: its watchdog level (0..3), whether it just went up, whether it has given up on its own
	 * (stranded), a short reason for the log, and the junction zone it waits at (INDEX_NONE when none).
	 */
	void Report(AActor* Agent, MurdarAIQ::EAgent Kind, int32 Level, bool bEscalated, bool bStranded, const FString& Why, int32 JunctionZone = INDEX_NONE);
	/** Gone (destroyed, pooled): forget it. */
	void Forget(AActor* Agent);

	/** Its current level (0 when unknown) - traffic reads the leader's to decide whether a queue is really stuck. */
	int32 LevelOf(const AActor* Agent) const;

	/** Soak control (also the console commands). */
	void StartSoak(float Seconds, bool bTour);
	void StopSoak();
	bool IsSoaking() const { return bSoaking; }
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FEntry
	{
		MurdarAIQ::EAgent Kind = MurdarAIQ::EAgent::Traffic;
		int32 Level = 0;
		bool bStranded = false;
		float Since = 0.f;          // when this level began
		int32 JunctionZone = INDEX_NONE;
		FString Why;
	};
	struct FEvent
	{
		float Time = 0.f;
		MurdarAIQ::EAgent Kind = MurdarAIQ::EAgent::Traffic;
		int32 Level = 0;            // 1..3, 4 = recycled, 5 = gridlock
		FString Who;
		FVector Where = FVector::ZeroVector;
		FString Why;
	};

	void Tick1Hz();
	void RecyclePass();
	void GridlockPass();
	void TourStep();
	void FinishSoak();
	bool IsVisibleToPlayer(const AActor* A) const;
	bool IsInChase(const AActor* A) const;
	bool IsPlayerOwned(const AActor* A) const;
	void Draw() const;

	TMap<TWeakObjectPtr<AActor>, FEntry> Agents;
	/** Gridlocked zones already logged (so one gridlock is one event, not one per second). */
	TSet<int32> KnownGridlocks;
	FTimerHandle TickTimer;

	bool bSoaking = false;
	bool bTour = false;
	float SoakStart = 0.f;
	float SoakEnd = 0.f;
	float NextTour = 0.f;
	MurdarAIQ::FSoakStats Stats;
	TArray<FEvent> Events;
};
