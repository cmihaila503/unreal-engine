// Cash on the ground (a wallet by a body, a roll of notes in a stash). Walk into it: it goes into the wallet.
// Same overlap setup as AAmmoPickup: overlaps pawns only, blocks nothing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "MoneyPickup.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class MURDAR_GAMEDEV_API AMoneyPickup : public AActor
{
	GENERATED_BODY()

public:
	AMoneyPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money", meta = (ClampMin = "0")) float Amount = 100.f;

	/** Gone after this long (someone else took it). 0 = stays. Dropped wallets set it; placed stashes don't. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money", meta = (Units = "s", ClampMin = "0")) float LifeSeconds = 0.f;

	/** Placed stash, once ever (saved): the fact is set on pickup and the stash is gone on the next load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money", meta = (Categories = "Fact")) FGameplayTag TakenFact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money") TObjectPtr<USoundBase> PickupSound;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Money") TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Money") TObjectPtr<UStaticMeshComponent> Mesh;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
