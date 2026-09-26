// The saved world, inside FNarrativeState (one file, one version). Soft paths and plain values only, like the rest
// of the save: files survive code changes. Included by Director/NarrativeState.h (README §Patches 1).

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "WorldStateTypes.generated.h"

/** A placed actor that opted in (UWorldStateComponent). Keyed by map + the actor's level name. */
USTRUCT(BlueprintType)
struct MURDAR_GAMEDEV_API FWorldActorRecord
{
	GENERATED_BODY()

	UPROPERTY() FName Map;
	UPROPERTY() FName Id;
	/** Sticky: once destroyed, a later capture can't bring it back (MurdarWorld::FRecords). */
	UPROPERTY() bool bDestroyed = false;
	UPROPERTY() bool bHasTransform = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() bool bHasValue = false;
	/** Whatever the actor wants back: a door's open angle, a switch, a counter. */
	UPROPERTY() float Value = 0.f;
};

/** The car he drove last. */
USTRUCT(BlueprintType)
struct MURDAR_GAMEDEV_API FPlayerCarRecord
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;
	UPROPERTY() FSoftClassPath VehicleClass;
	UPROPERTY() FSoftObjectPath Definition;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float Damage01 = 0.f;
};

USTRUCT(BlueprintType)
struct MURDAR_GAMEDEV_API FMurdarWorldState
{
	GENERATED_BODY()

	/** Short package name of the map the save was made on. Empty = an old save: nothing is applied. */
	UPROPERTY() FName Map;

	UPROPERTY() bool bHasPlayer = false;
	UPROPERTY() FTransform Player;
	UPROPERTY() bool bPlayerInCar = false;

	UPROPERTY() FPlayerCarRecord Car;

	UPROPERTY() TArray<FWorldActorRecord> Actors;

	/** Not saved: set right after a load from file, cleared once applied (lives in the game instance across the map
	 *  change that ContinueFromSave may do). */
	bool bPendingApply = false;

	FWorldActorRecord* Find(FName InMap, FName InId)
	{
		return Actors.FindByPredicate([&](const FWorldActorRecord& R) { return R.Map == InMap && R.Id == InId; });
	}

	FWorldActorRecord& Upsert(FName InMap, FName InId)
	{
		if (FWorldActorRecord* R = Find(InMap, InId)) { return *R; }
		FWorldActorRecord& N = Actors.AddDefaulted_GetRef();
		N.Map = InMap;
		N.Id = InId;
		return N;
	}
};
