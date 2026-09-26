// Put on a PLACED actor whose state should survive a save: a door left open, a crate pushed, a lamp shot out.
// Spawned actors are ignored (their names are not stable across sessions). Blueprint: write Value when the state
// changes (door angle), and apply it in OnRestored.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldStateRestored, float, Value);

UCLASS(ClassGroup = (Murdar), meta = (BlueprintSpawnableComponent))
class MURDAR_GAMEDEV_API UWorldStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldStateComponent();

	/** Destroyed during play = gone after a load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World State") bool bSaveDestroyed = true;
	/** Moved (physics, pushed) = where it was left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World State") bool bSaveTransform = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World State") bool bSaveValue = false;

	/** The Blueprint keeps this current (e.g. 1 = door open); it is saved and handed back in OnRestored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World State", meta = (EditCondition = "bSaveValue")) float Value = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "World State") FOnWorldStateRestored OnRestored;

	/** Stable id: the actor's name in its level. None for spawned actors. */
	FName GetStableId() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
