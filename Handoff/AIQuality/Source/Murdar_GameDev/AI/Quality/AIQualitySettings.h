// Project Settings > Game > Murdar AI Quality. Every fix in Handoff/AIQuality can be switched off here on its own, so
// a soak run can compare "with" and "without" one fix at a time (Docs/AI_SOAK_REPORT.md).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AI/Quality/AIQualityRules.h"
#include "AIQualitySettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar AI Quality"))
class MURDAR_GAMEDEV_API UAIQualitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	static const UAIQualitySettings* Get() { return GetDefault<UAIQualitySettings>(); }

	// --- Switches ---
	/** Traffic, pedestrians and gunmen watch their own progress and escalate (nudge, break the rule, give up). */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bWatchdog = true;
	/** Junction deadlock fixes: a stopped committed car no longer reserves the box; a parked siren no longer closes it. */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bJunctionFixes = true;
	/** Traffic drives round people, bodies and the player standing in the road (after a honk). */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bDriveRoundPeople = true;
	/** Traffic / pedestrians use the spatial index instead of scanning the world. */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bSpatialIndex = true;
	/** Gunmen take cover and peek instead of strafing in the open. */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bCover = true;
	/** Stuck / stranded actors nobody sees are recycled even inside the despawn distance. */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bRecycleStuck = true;
	/** Pedestrians keep right and step round each other and the player. */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bFootPassing = true;

	// --- Watchdog ---
	UPROPERTY(EditAnywhere, config, Category = "Watchdog", meta = (Units = "cm", ClampMin = "50", ClampMax = "1000")) float ProgressCm = 150.f;
	UPROPERTY(EditAnywhere, config, Category = "Watchdog", meta = (Units = "s", ClampMin = "1", ClampMax = "20")) float Level1Seconds = 4.f;
	UPROPERTY(EditAnywhere, config, Category = "Watchdog", meta = (Units = "s", ClampMin = "3", ClampMax = "60")) float Level2Seconds = 12.f;
	UPROPERTY(EditAnywhere, config, Category = "Watchdog", meta = (Units = "s", ClampMin = "10", ClampMax = "180")) float Level3Seconds = 30.f;
	/** A red light / a queue longer than this is a deadlock that looks legitimate. */
	UPROPERTY(EditAnywhere, config, Category = "Watchdog", meta = (Units = "s", ClampMin = "30", ClampMax = "600")) float LegitWaitMaxSeconds = 90.f;
	/** Never recycle closer than this to the player, even unseen (behind him in a narrow street). */
	UPROPERTY(EditAnywhere, config, Category = "Watchdog", meta = (Units = "cm", ClampMin = "500", ClampMax = "10000")) float RecycleMinDistanceCm = 1500.f;

	// --- Traffic ---
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "0.5", ClampMax = "10")) float CommittedStillSeconds = 3.f;
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "2", ClampMax = "30")) float CarRoundNearJunctionSeconds = 10.f;
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "0.5", ClampMax = "10")) float PersonHonkSeconds = 2.f;
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "1", ClampMax = "30")) float PersonRoundSeconds = 5.f;
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "0.5", ClampMax = "10")) float PlayerHonkSeconds = 1.5f;
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "1", ClampMax = "60")) float PlayerRoundSeconds = 8.f;
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "0", ClampMax = "10")) float BodyRoundSeconds = 1.5f;
	/** A car on its side or roof this long is stranded. */
	UPROPERTY(EditAnywhere, config, Category = "Traffic", meta = (Units = "s", ClampMin = "1", ClampMax = "20")) float FlippedSeconds = 4.f;

	// --- Spatial index ---
	UPROPERTY(EditAnywhere, config, Category = "Spatial index", meta = (Units = "cm", ClampMin = "500", ClampMax = "10000")) float CellCm = 2000.f;
	UPROPERTY(EditAnywhere, config, Category = "Spatial index", meta = (Units = "s", ClampMin = "0.02", ClampMax = "0.5")) float RebuildSeconds = 0.1f;

	// --- Cover ---
	/** Samples per ring, and the ring radii (cm) searched round the NPC. */
	UPROPERTY(EditAnywhere, config, Category = "Cover", meta = (ClampMin = "6", ClampMax = "24")) int32 CoverSamplesPerRing = 12;
	UPROPERTY(EditAnywhere, config, Category = "Cover") TArray<float> CoverRingsCm = { 350.f, 700.f, 1100.f };
	UPROPERTY(EditAnywhere, config, Category = "Cover", meta = (Units = "cm", ClampMin = "100", ClampMax = "800")) float AllySpacingCm = 300.f;
	UPROPERTY(EditAnywhere, config, Category = "Cover", meta = (Units = "cm", ClampMin = "300", ClampMax = "4000")) float MaxCoverPathCm = 1500.f;
	/** Seconds in one cover spot before moving on (the fight moves). */
	UPROPERTY(EditAnywhere, config, Category = "Cover", meta = (Units = "s", ClampMin = "3", ClampMax = "60")) float MaxStaySeconds = 12.f;
	/** Cover searches per second for the whole world (each is ~40 traces + a few paths). */
	UPROPERTY(EditAnywhere, config, Category = "Cover", meta = (ClampMin = "1", ClampMax = "30")) int32 CoverSearchesPerSecond = 6;

	MurdarAIQ::FWatchTuning Watch() const
	{
		MurdarAIQ::FWatchTuning T;
		T.ProgressCm = ProgressCm; T.Level1Seconds = Level1Seconds; T.Level2Seconds = Level2Seconds; T.Level3Seconds = Level3Seconds;
		T.LegitWaitMaxSeconds = LegitWaitMaxSeconds;
		return T;
	}

	MurdarAIQ::FObstacleTuning Obstacles(float CarRoundSeconds) const
	{
		MurdarAIQ::FObstacleTuning T;
		T.CarRoundSeconds = CarRoundSeconds; T.CarRoundNearJunctionSeconds = CarRoundNearJunctionSeconds;
		T.PersonHonkSeconds = PersonHonkSeconds; T.PersonRoundSeconds = PersonRoundSeconds;
		T.PlayerHonkSeconds = PlayerHonkSeconds; T.PlayerRoundSeconds = PlayerRoundSeconds; T.BodyRoundSeconds = BodyRoundSeconds;
		return T;
	}
};
