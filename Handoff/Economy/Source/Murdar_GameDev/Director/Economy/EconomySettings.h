// Project Settings > Game > Murdar Economy. Starting cash, inflation, what can be bought, cash on bodies.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Weapons/WeaponDefinition.h" // EAmmoType — ADAPT path
#include "EconomySettings.generated.h"

class AMoneyPickup;

USTRUCT(BlueprintType)
struct FEconomyItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Item") FText Name;

	/** Lei on day 0. Inflation (if on) raises it per game day. */
	UPROPERTY(EditAnywhere, config, Category = "Item", meta = (ClampMin = "0")) float BasePrice = 50.f;

	UPROPERTY(EditAnywhere, config, Category = "Item") bool bGivesAmmo = false;
	UPROPERTY(EditAnywhere, config, Category = "Item", meta = (EditCondition = "bGivesAmmo")) EAmmoType AmmoType = EAmmoType::Pistol;
	UPROPERTY(EditAnywhere, config, Category = "Item", meta = (EditCondition = "bGivesAmmo", ClampMin = "1")) int32 AmmoAmount = 12;

	/** A weapon handed over (a man in a garage). Kit ammo comes with it. */
	UPROPERTY(EditAnywhere, config, Category = "Item") TSoftObjectPtr<UWeaponDefinition> Weapon;

	/** Everything else (a key, a fake ID, information) is a fact. */
	UPROPERTY(EditAnywhere, config, Category = "Item", meta = (Categories = "Fact")) FGameplayTagContainer FactsOnBuy;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Economy"))
class MURDAR_GAMEDEV_API UEconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Cash in the pocket at the start of a new game (set once; saved with the narrative state afterwards). */
	UPROPERTY(EditAnywhere, config, Category = "Wallet", meta = (ClampMin = "0")) float StartingCash = 300.f;

	/** Per game day, compounding. 0 = off. 0.005 ≈ ×6 a game year — the 90s. Only shop prices follow it; amounts
	 *  written in dialogues and missions stay as written (write shop choices without numbers when this is on). */
	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (ClampMin = "0", ClampMax = "0.05")) float DailyInflation = 0.f;

	/** Keyed by the buy event: a dialogue choice publishes Event.Economy.Buy.<Item> (Publish effect). */
	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (Categories = "Event.Economy.Buy", ForceInlineRow))
	TMap<FGameplayTag, FEconomyItem> Items;

	/** Said when he can't afford it (subtitle, no speaker). */
	UPROPERTY(EditAnywhere, config, Category = "Prices") FText CantAffordLine = NSLOCTEXT("MurdarEconomy", "CantAfford", "N-ai destui bani.");
	UPROPERTY(EditAnywhere, config, Category = "Prices") FText AmmoFullLine = NSLOCTEXT("MurdarEconomy", "AmmoFull", "Nu mai ai unde să le pui.");

	/** Pawns of these classes (and children) may leave cash when they die. Empty = nobody does. ADAPT: pedestrians,
	 *  gang NPCs; leave police out unless you want cops to be worth killing. */
	UPROPERTY(EditAnywhere, config, Category = "Cash on bodies") TArray<TSoftClassPtr<APawn>> CashDropClasses;
	UPROPERTY(EditAnywhere, config, Category = "Cash on bodies", meta = (ClampMin = "0", ClampMax = "1")) float CashDropChance = 0.6f;
	UPROPERTY(EditAnywhere, config, Category = "Cash on bodies", meta = (ClampMin = "0")) float CashDropMin = 20.f;
	UPROPERTY(EditAnywhere, config, Category = "Cash on bodies", meta = (ClampMin = "0")) float CashDropMax = 150.f;
	/** Empty = AMoneyPickup (placeholder box). Set a Blueprint child with a real wallet mesh. */
	UPROPERTY(EditAnywhere, config, Category = "Cash on bodies") TSoftClassPtr<AMoneyPickup> PickupClass;
};
