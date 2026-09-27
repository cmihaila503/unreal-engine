// Project Settings > Game > Murdar Gangs.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GangSettings.generated.h"

class UDialogueDefinition;
class UWeaponDefinition;

USTRUCT(BlueprintType)
struct FGangDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Gang", meta = (Categories = "Gang")) FGameplayTag Gang;
	UPROPERTY(EditAnywhere, config, Category = "Gang") FText Name;
	/** Their streets: zone facts (Zone.*) set by UZoneTriggerSubsystem while he is inside. */
	UPROPERTY(EditAnywhere, config, Category = "Gang", meta = (Categories = "Zone")) FGameplayTagContainer Territory;
	/** Where their respect for him and the day the taxa runs out are kept (saved values). */
	UPROPERTY(EditAnywhere, config, Category = "Gang", meta = (Categories = "Stat")) FGameplayTag RespectStat;
	UPROPERTY(EditAnywhere, config, Category = "Gang", meta = (Categories = "Stat")) FGameplayTag PaidUntilStat;
	UPROPERTY(EditAnywhere, config, Category = "Gang") TSoftObjectPtr<UWeaponDefinition> Weapon;
	/** The taxa conversation (Handoff/Dialogue): its choices publish Event.Gang.TaxPaid / Event.Gang.TaxRefused. */
	UPROPERTY(EditAnywhere, config, Category = "Gang") TSoftObjectPtr<UDialogueDefinition> TaxDialogue;
	UPROPERTY(EditAnywhere, config, Category = "Gang", meta = (ClampMin = "-100", ClampMax = "100")) float StartRespect = 0.f;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Gangs"))
class MURDAR_GAMEDEV_API UGangSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(EditAnywhere, config, Category = "Gangs") TArray<FGangDefinition> Gangs;
	/** Members appear this far from him (navmesh), out of sight when possible. */
	UPROPERTY(EditAnywhere, config, Category = "Spawning", meta = (Units = "cm")) FVector2D SpawnRadius = FVector2D(2500.f, 5000.f);
	UPROPERTY(EditAnywhere, config, Category = "Spawning", meta = (Units = "cm")) float DespawnDistance = 15000.f;
};
