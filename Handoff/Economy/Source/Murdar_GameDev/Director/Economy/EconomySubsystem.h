// The wallet. Cash is the narrative value Stat.Money (saved with everything else); this subsystem is the one place
// that moves it with rules: all-or-nothing purchases, bribes taken when the police accept them (Event.Police.Bribed),
// losses for consequences (arrest, hospital), cash left on bodies. Every change publishes Event.Economy.Changed.
// Nothing on the HUD: the amount is in the pause menu and the MurdarMoney cheat ("felt, not shown").

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "EconomySubsystem.generated.h"

struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UEconomySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEconomySubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Murdar|Economy") float GetCash() const;
	UFUNCTION(BlueprintPure, Category = "Murdar|Economy") bool CanAfford(float Amount) const { return GetCash() + 1e-3f >= Amount; }
	/** "1.250 lei" for the menu. */
	UFUNCTION(BlueprintPure, Category = "Murdar|Economy") FText GetCashText() const;

	UFUNCTION(BlueprintCallable, Category = "Murdar|Economy") void Earn(float Amount, FName Reason);
	/** All or nothing: false (nothing taken) when he doesn't have it. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Economy") bool Spend(float Amount, FName Reason);
	/** Gone whether he has it or not (taken by force); never below zero. Returns what was actually taken. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Economy") float Take(float Amount, FName Reason);
	/** Arrest / hospital: a fraction of the cash, at least MinLoss. Returns what was lost. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Economy") float LoseFraction(float Fraction, float MinLoss, FName Reason);

	/** Today's price of an item (Event.Economy.Buy.* key in the settings); -1 if unknown. */
	UFUNCTION(BlueprintPure, Category = "Murdar|Economy") float GetPrice(FGameplayTag Item) const;
	UFUNCTION(BlueprintCallable, Category = "Murdar|Economy") bool Buy(FGameplayTag Item);

	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SetCash(float NewCash, float Delta, FName Reason);
	void OnDied(const FGameEvent& Event);
	void Say(const FText& Line) const;
	int32 Day() const;

	TArray<int32> BusHandles;
};
