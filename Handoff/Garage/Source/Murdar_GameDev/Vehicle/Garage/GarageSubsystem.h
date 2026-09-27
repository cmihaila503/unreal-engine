// Garage bookkeeping that isn't one garage's: the car he drove last, sending it to the impound lot after an arrest
// (Event.Consequence.Station, Handoff/Consequences), and a random paint for every spawned car (without colours a
// respray means nothing).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GarageSubsystem.generated.h"

class AMurdarVehicle;
class AMurdarGarage;
struct FMurdarWorldState;

UCLASS()
class MURDAR_GAMEDEV_API UGarageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGarageSubsystem* Get(const UObject* WorldContext);

	/** A palette colour for a fresh car (VehicleSubsystem::SpawnVehicle calls it: README §Patches 3). */
	static void RandomPaint(AMurdarVehicle* Car);

	AMurdarVehicle* GetLastCar() const { return LastCar.Get(); }
	FMurdarWorldState* Records() const;
	int32 CurrentDay() const;
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Impound();

	TWeakObjectPtr<AMurdarVehicle> LastCar;
	FVector ArrestLocation = FVector::ZeroVector;
	bool bHasArrestLocation = false;
	TArray<int32> BusHandles;
};
