// Project Settings > Game > Murdar Civilians. Owner of every population tunable (spec §0.8, police spec §1.11).
// Numbers here are provisional until Phase 7 measures what a civilian costs; each says why it is what it is.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "CivilianPopulationSettings.generated.h"

class UCivilianProfile;

USTRUCT(BlueprintType)
struct FCivilianArchetypeWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Civilian")
	TSoftObjectPtr<UCivilianProfile> Profile;

	/** Relative share of this archetype among spawns. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Civilian", meta = (ClampMin = "0"))
	float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct FCivilianZoneDensity
{
	GENERATED_BODY()

	/** The AZoneVolume tag this applies to (the zone the player is in decides the density). */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Civilian", meta = (Categories = "Zone"))
	FGameplayTag Zone;

	/** People per hectare (100 m × 100 m) of the spawn disc. A busy market ~60, a residential street ~10. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Civilian", meta = (ClampMin = "0", ClampMax = "200"))
	float PeoplePerHectare = 10.f;

	/** Archetype mix in this zone. Empty = DefaultArchetypes. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Civilian")
	TArray<FCivilianArchetypeWeight> Archetypes;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Civilians"))
class MURDAR_GAMEDEV_API UCivilianPopulationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Master switch. Off = no civilians are spawned (existing ones are left alone). */
	UPROPERTY(EditAnywhere, config, Category = "Population")
	bool bEnabled = true;

	/** Hard cap on active civilians. Provisional: Phase 7 replaces it with the measured budget. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (ClampMin = "0", ClampMax = "300"))
	int32 MaxCivilians = 30;

	/** Population update period. 2 Hz: people walk ~0.7 m per update, far below the ring sizes. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (Units = "s", ClampMin = "0.1", ClampMax = "2"))
	float UpdateIntervalSeconds = 0.5f;

	/** Spawns per update. Spreads the spawn cost so a fill never hitches (each spawn is a full character). */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxSpawnsPerUpdate = 2;

	/** Candidate points tried per update before giving up until the next one. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (ClampMin = "1", ClampMax = "64"))
	int32 SpawnAttemptsPerUpdate = 8;

	/** Inner spawn radius. Close enough that people walk into view soon, far enough that a spawn behind a corner
	 *  isn't heard (footsteps) or caught by a quick camera turn. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (Units = "cm", ClampMin = "1000", ClampMax = "30000"))
	float SpawnRingMinCm = 4000.f;

	/** Outer spawn radius. Also the disc whose area the density is applied to. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (Units = "cm", ClampMin = "2000", ClampMax = "40000"))
	float SpawnRingMaxCm = 9000.f;

	/** Civilians beyond this, unseen, go back to the pool. Kept above SpawnRingMaxCm so a spawn is never
	 *  immediately a despawn candidate (clamped at use). */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (Units = "cm", ClampMin = "3000", ClampMax = "50000"))
	float DespawnDistanceCm = 12000.f;

	/** How long a far civilian must have been out of view before it may be despawned. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (Units = "s", ClampMin = "0", ClampMax = "60"))
	float DespawnUnseenSeconds = 5.f;

	/** No two civilians spawn closer than this (no clumps, no overlapping capsules). */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (Units = "cm", ClampMin = "100", ClampMax = "2000"))
	float MinSeparationCm = 300.f;

	/** Added to the camera's half FOV when testing "in view": the camera can swing while the spawn happens. */
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "deg", ClampMin = "0", ClampMax = "45"))
	float ViewConeMarginDeg = 10.f;

	/** Used only if the player has no camera manager. */
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "deg", ClampMin = "30", ClampMax = "170"))
	float FallbackFovDeg = 90.f;

	/** Visibility is traced to the head and to the chest; if either is unobstructed the person is visible. */
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "100", ClampMax = "220"))
	float HeadHeightCm = 165.f;

	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "50", ClampMax = "180"))
	float ChestHeightCm = 120.f;

	/** Must match the Human agent in DefaultEngine.ini (SupportedAgents). ADAPT: if MurdarNav already exposes the
	 *  Human agent properties, read them from there and delete these two (one source of truth). */
	UPROPERTY(EditAnywhere, config, Category = "Navigation", meta = (Units = "cm", ClampMin = "10", ClampMax = "200"))
	float NavAgentRadiusCm = 40.f;

	UPROPERTY(EditAnywhere, config, Category = "Navigation", meta = (Units = "cm", ClampMin = "50", ClampMax = "300"))
	float NavAgentHeightCm = 180.f;

	/** Search box half-size when projecting a candidate point onto the navmesh. Z is large for slopes and kerbs. */
	UPROPERTY(EditAnywhere, config, Category = "Navigation", meta = (Units = "cm", ClampMin = "10", ClampMax = "1000"))
	float ProjectExtentXYCm = 200.f;

	UPROPERTY(EditAnywhere, config, Category = "Navigation", meta = (Units = "cm", ClampMin = "10", ClampMax = "2000"))
	float ProjectExtentZCm = 500.f;

	/** Density outside any listed zone. */
	UPROPERTY(EditAnywhere, config, Category = "Density", meta = (ClampMin = "0", ClampMax = "200"))
	float DefaultPeoplePerHectare = 10.f;

	UPROPERTY(EditAnywhere, config, Category = "Density")
	TArray<FCivilianZoneDensity> ZoneDensities;

	UPROPERTY(EditAnywhere, config, Category = "Density")
	TArray<FCivilianArchetypeWeight> DefaultArchetypes;

	/** Body when neither the archetype nor this is set: ADAPT → UMurdarAISettings::NPCPawnClass via SpawnNPC. */
	UPROPERTY(EditAnywhere, config, Category = "Population")
	TSoftClassPtr<APawn> DefaultPawnClass;

	/** Pooled (inactive) bodies kept for reuse. Beyond this they are destroyed. */
	UPROPERTY(EditAnywhere, config, Category = "Population", meta = (ClampMin = "0", ClampMax = "300"))
	int32 MaxPooled = 30;

	/** 0 = a different street every run. Non-zero = repeatable spawns for tests (gap analysis C10). */
	UPROPERTY(EditAnywhere, config, Category = "Testing")
	int32 RandomSeed = 0;
};
