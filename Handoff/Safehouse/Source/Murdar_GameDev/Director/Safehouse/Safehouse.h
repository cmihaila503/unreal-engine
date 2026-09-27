// A safehouse placed in the map (Blueprint child with the flat's mesh): the interior box, and three Interactables —
// the bed (sleep: skip to morning, heal, save), the stash (put the cash away / take it), the wardrobe (next outfit,
// Handoff/Disguise reads it). Not owned yet: the bed offers to buy it (Economy). Inside and unseen, the police heat
// cools faster (MurdarSafehouse::HideExtraDecay).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Safehouse.generated.h"

class UBoxComponent;
class UInteractableComponent;
class APawn;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API ASafehouse : public AActor
{
	GENERATED_BODY()

public:
	ASafehouse();

	/** Set when owned (saved). Leave Price 0 and put the fact in the chapter's FactsOnEnter for a starting flat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safehouse", meta = (Categories = "Fact")) FGameplayTag OwnedFact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safehouse", meta = (ClampMin = "0")) float Price = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safehouse") FText DisplayName = NSLOCTEXT("MurdarSafehouse", "Flat", "garsoniera");

	UPROPERTY(VisibleAnywhere, Category = "Safehouse") TObjectPtr<UBoxComponent> Interior;
	UPROPERTY(VisibleAnywhere, Category = "Safehouse") TObjectPtr<UInteractableComponent> Bed;
	UPROPERTY(VisibleAnywhere, Category = "Safehouse") TObjectPtr<UInteractableComponent> Stash;
	UPROPERTY(VisibleAnywhere, Category = "Safehouse") TObjectPtr<UInteractableComponent> Wardrobe;

	bool IsOwned() const;
	bool IsPlayerInside() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void OnBed(UInteractableComponent* I, APawn* User);
	UFUNCTION() void OnStash(UInteractableComponent* I, APawn* User);
	UFUNCTION() void OnWardrobe(UInteractableComponent* I, APawn* User);
	void Tick1Hz();
	void Refresh();
	void WakeUp(float Hours);
	void OnBusEvent(const FGameEvent& E);

	FTimerHandle TickTimer, SleepTimer;
	int32 BusHandle = 0;
	int32 ActiveMissions = 0;
	float LastShotTime = -1e6f;
	bool bSleeping = false;
};
