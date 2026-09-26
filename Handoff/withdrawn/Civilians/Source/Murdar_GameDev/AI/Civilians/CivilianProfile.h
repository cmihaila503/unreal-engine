// Civilian personality. Personality changes preferences, not the state machine (same rule as the police profiles).
// Phase 1 only reads PawnClassOverride; each later phase adds the fields it consumes (spec §10), not before.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CivilianProfile.generated.h"

UCLASS(BlueprintType)
class MURDAR_GAMEDEV_API UCivilianProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Body for this archetype (outfit/mesh variety). Empty = the population default, then UMurdarAISettings::NPCPawnClass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian")
	TSoftClassPtr<APawn> PawnClassOverride;

	/** Walking pace. Consumed from Phase 2 (Wander). People walk 1.2–1.5 m/s; old people ~0.9. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian", meta = (Units = "cm/s", ClampMin = "60", ClampMax = "220"))
	float WalkSpeedCms = 135.f;
};
