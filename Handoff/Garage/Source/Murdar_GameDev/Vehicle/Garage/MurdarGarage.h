// A garage placed in the map: a bay you drive into, a spot where cars come out, and a counter (an Interactable).
//   Respray — stop in the bay, unseen by the police, with the money: new colour, repaired, the paper trail ends.
//   Storage — leave a car in the bay and walk out: it is kept (saved). At the counter: „Scoate …” brings the next out.
//   Impound — where the police put your car after an arrest. At the counter: pay and take it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MurdarGarage.generated.h"

class UBoxComponent;
class UArrowComponent;
class UInteractableComponent;
class AMurdarVehicle;
class APawn;
struct FStoredCarRecord;

UENUM(BlueprintType)
enum class EGarageKind : uint8 { Respray, Storage, Impound };

UCLASS()
class MURDAR_GAMEDEV_API AMurdarGarage : public AActor
{
	GENERATED_BODY()

public:
	AMurdarGarage();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garage") EGarageKind Kind = EGarageKind::Respray;
	/** Unique across the game: saved records point at it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garage") FName GarageId = TEXT("Garage");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garage", meta = (ClampMin = "1", ClampMax = "10", EditCondition = "Kind == EGarageKind::Storage")) int32 Slots = 2;

	UPROPERTY(VisibleAnywhere, Category = "Garage") TObjectPtr<UBoxComponent> Bay;
	UPROPERTY(VisibleAnywhere, Category = "Garage") TObjectPtr<UArrowComponent> SpawnPoint;
	UPROPERTY(VisibleAnywhere, Category = "Garage") TObjectPtr<UInteractableComponent> Counter;

	/** Put a stored / impounded car back into the world at SpawnPoint. */
	AMurdarVehicle* SpawnFromRecord(const FStoredCarRecord& Record) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Tick4Hz();
	void TickRespray(AMurdarVehicle* Car, float Now);
	void FinishRespray();
	void TickStorage(APawn* Player);
	void RefreshCounter();
	UFUNCTION() void OnCounterUsed(UInteractableComponent* Interactable, APawn* User);
	TArray<int32> RecordIndices(bool bImpound) const;

	FTimerHandle TickTimer, JobTimer;
	float StoppedSince = -1.f;
	bool bJobDoneThisVisit = false;
	bool bJobRunning = false;
	TWeakObjectPtr<AMurdarVehicle> JobCar;
	TWeakObjectPtr<AMurdarVehicle> LeftInBay;
	int32 NextToRetrieve = 0;
};
