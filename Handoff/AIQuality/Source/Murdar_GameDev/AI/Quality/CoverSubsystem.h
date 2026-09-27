// Where to take cover, where to hide, where to run - picked on the navmesh, not on a ring in the air.
//
// Before: a gunman strafed in the open 11 m from the player; out of ammo he picked the "hidden" point of a 12-point
// ring 18 m out that could be inside a building or across the road; a civilian ran 15 m straight away from the shots,
// into whatever was there. Now candidates are sampled on rings, projected onto the Human navmesh, checked for a sane
// path (not round the block), for the road, and - for cover - hidden at chest height from the threat but able to see
// him with a step to the side. Two gunmen do not pick the same wall (claims). Searches are budgeted per second for the
// whole world; an NPC that is refused this frame keeps what it was doing and asks again next think.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Quality/AIQualityRules.h"
#include "CoverSubsystem.generated.h"

UENUM()
enum class ESpotPurpose : uint8
{
	Cover,  // a fight: hidden, can peek, near our range
	Hide,   // out of the fight: hidden, far
	Flee    // a civilian: away, not the road
};

UCLASS()
class MURDAR_GAMEDEV_API UCoverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCoverSubsystem* Get(const UObject* WorldContext);

	/**
	 * Best spot for Me against Threat. OutPeek: for Cover, where to step to shoot from (else = OutSpot). False when the
	 * budget is spent this second (ask again later) or nothing acceptable was found (bOutNone = true: stop asking for a
	 * while and fall back to the old behaviour).
	 */
	bool FindSpot(ESpotPurpose Purpose, const APawn* Me, const AActor* Threat, float PreferredRangeCm, FVector& OutSpot, FVector& OutPeek, bool& bOutNone);

	/** Hold a cover spot (others keep AllySpacingCm off it) until released or the claimer is gone. */
	void Claim(const AActor* Claimer, const FVector& Spot);
	void Release(const AActor* Claimer);

	/** Can Threat see Me's chest right now (flanked in cover)? One trace. */
	bool IsExposed(const APawn* Me, const AActor* Threat) const;

private:
	bool TakeBudget();
	float NearestClaim(const FVector& P, const AActor* Except) const;
	bool OnRoad(const FVector& P) const;

	TMap<TWeakObjectPtr<const AActor>, FVector> Claims;
	int32 BudgetSecond = -1;
	int32 BudgetUsed = 0;
};
