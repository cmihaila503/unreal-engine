// Project Settings > Game > Murdar Heat. Owner of the heat *model* tunables (decay, repeats, hysteresis) and of the
// witness reporting delays. NOT the owner of: the crime → heat table and the wanted thresholds (12/40/75), which stay
// in UFactionMemorySubsystem where they are "kept on one screen" (PROJECT_OVERVIEW §4.8) — ToConfig() copies them in.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MurdarHeatModel.h"
#include "MurdarHeatSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Heat"))
class MURDAR_GAMEDEV_API UMurdarHeatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---- Decay ----

	/** Out-of-sight decay at severity 0. The value the faction memory used before (0.6/s). ADAPT: if the memory
	 *  holds it as a member, delete that one (one source of truth). */
	UPROPERTY(EditAnywhere, config, Category = "Decay", meta = (ClampMin = "0.01", ClampMax = "10"))
	float DecayPerSecond = 0.6f;

	/** No decay for this long after police last saw him or a crime was reported: they are still actively looking. */
	UPROPERTY(EditAnywhere, config, Category = "Decay", meta = (Units = "s", ClampMin = "0", ClampMax = "120"))
	float DecayDelaySeconds = 10.f;

	/** Decay multiplier after the worst crime in the table (murder). 0.25: murder heat lasts ~4× longer than
	 *  speeding heat — a killing isn't forgotten in two minutes. */
	UPROPERTY(EditAnywhere, config, Category = "Decay", meta = (ClampMin = "0.05", ClampMax = "1"))
	float SevereCrimeDecayScale = 0.25f;

	/** How long the worst recent crime keeps slowing the decay. */
	UPROPERTY(EditAnywhere, config, Category = "Decay", meta = (Units = "s", ClampMin = "0", ClampMax = "3600"))
	float SeverityMemorySeconds = 300.f;

	/** Decay multiplier while any unit is searching for him. */
	UPROPERTY(EditAnywhere, config, Category = "Decay", meta = (ClampMin = "0", ClampMax = "1"))
	float SearchDecayScale = 0.5f;

	// ---- Repeats ----

	/** Repeats of the same crime type inside this window give diminishing heat (continuous speeding). */
	UPROPERTY(EditAnywhere, config, Category = "Repeats", meta = (Units = "s", ClampMin = "0", ClampMax = "600"))
	float RepeatWindowSeconds = 60.f;

	/** Each repeat multiplies the gain by this. */
	UPROPERTY(EditAnywhere, config, Category = "Repeats", meta = (ClampMin = "0", ClampMax = "1"))
	float RepeatFactor = 0.5f;

	/** A repeat never gives less than this fraction of the base heat. */
	UPROPERTY(EditAnywhere, config, Category = "Repeats", meta = (ClampMin = "0", ClampMax = "1"))
	float RepeatFloor = 0.1f;

	// ---- Wanted level ----

	/** A wanted level is kept until heat falls this far below its threshold (no Stop↔Pursuit flapping). */
	UPROPERTY(EditAnywhere, config, Category = "Wanted", meta = (ClampMin = "0", ClampMax = "20"))
	float WantedHysteresisHeat = 5.f;

	// ---- Witnesses ----

	/** A civilian witness with no police around reports after a random delay in this range — the time to reach a
	 *  phone booth or a station in 1990s Romania. The foot AI may bring it forward (reached a phone) later. */
	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "s", ClampMin = "0", ClampMax = "600"))
	float ReportDelayMinSeconds = 30.f;

	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "s", ClampMin = "0", ClampMax = "900"))
	float ReportDelayMaxSeconds = 90.f;

	/** A witness with a police unit this close tells them almost at once. */
	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "cm", ClampMin = "0", ClampMax = "20000"))
	float NearbyPoliceReportDistanceCm = 3000.f;

	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "s", ClampMin = "0", ClampMax = "30"))
	float NearbyPoliceReportSeconds = 3.f;

	/** A second, third ... witness of the same incident adds this fraction of its heat. */
	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (ClampMin = "0", ClampMax = "1"))
	float CorroborationFraction = 0.25f;

	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxCorroborations = 2;

	/** Two witnessed crimes of the same type this close in space and time are one incident (several people saw
	 *  the same shooting). */
	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "cm", ClampMin = "0", ClampMax = "10000"))
	float IncidentMergeDistanceCm = 1500.f;

	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "s", ClampMin = "0", ClampMax = "60"))
	float IncidentMergeSeconds = 5.f;

	/** Report queue update period. */
	UPROPERTY(EditAnywhere, config, Category = "Witnesses", meta = (Units = "s", ClampMin = "0.1", ClampMax = "2"))
	float ReportUpdateIntervalSeconds = 0.5f;

	/** Heat changes kept for `MurdarHeatLog` / `Murdar.Heat.Debug`. */
	UPROPERTY(EditAnywhere, config, Category = "Debug", meta = (ClampMin = "1", ClampMax = "64"))
	int32 HistorySize = 16;

	/** Builds the pure model's config. The thresholds and the table maximum come from the faction memory. */
	MurdarHeat::FConfig ToConfig(float StopHeat, float PursuitHeat, float LethalHeat, float MaxCrimeHeat) const
	{
		MurdarHeat::FConfig C;
		C.StopHeat = StopHeat;
		C.PursuitHeat = PursuitHeat;
		C.LethalHeat = LethalHeat;
		C.MaxCrimeHeat = MaxCrimeHeat;
		C.WantedHysteresisHeat = WantedHysteresisHeat;
		C.DecayPerSecond = DecayPerSecond;
		C.DecayDelaySeconds = DecayDelaySeconds;
		C.SevereCrimeDecayScale = SevereCrimeDecayScale;
		C.SeverityMemorySeconds = SeverityMemorySeconds;
		C.SearchDecayScale = SearchDecayScale;
		C.RepeatWindowSeconds = RepeatWindowSeconds;
		C.RepeatFactor = RepeatFactor;
		C.RepeatFloor = RepeatFloor;
		C.CorroborationFraction = CorroborationFraction;
		C.MaxCorroborations = MaxCorroborations;
		C.IncidentMergeDistanceCm = IncidentMergeDistanceCm;
		C.IncidentMergeSeconds = IncidentMergeSeconds;
		C.HistorySize = HistorySize;
		return C;
	}
};
