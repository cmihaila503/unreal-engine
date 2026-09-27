// Marks a pawn as one of a gang's (added by UGangSubsystem to the members it spawns; placed gang NPCs can have it too).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GangMemberComponent.generated.h"

UCLASS(ClassGroup = (Murdar), meta = (BlueprintSpawnableComponent))
class MURDAR_GAMEDEV_API UGangMemberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gang", meta = (Categories = "Gang")) FGameplayTag Gang;
};
