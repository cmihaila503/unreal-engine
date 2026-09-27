// "Who is near here?" without scanning the world. Before this, every traffic car walked TActorIterator over every
// car and every character three times per think (FindLeader, NonTrafficClear, OppositeClear): 8 cars and 24 people
// cost ~2 300 actor visits and ~6 900 path projections a second, and it grows with the square of the population -
// one of the reasons the density had to stay low. Now one pass over the world every RebuildSeconds fills a grid,
// and a query returns only what is in the cells round a point.
//
// The grid holds the positions at rebuild time: a query widens its radius by what anything can travel until the
// next rebuild (MaxSpeedCms * RebuildSeconds), and callers keep reading the live GetActorLocation() of what it returns.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Quality/AIQualityRules.h"
#include "AISpatialIndex.generated.h"

class AMurdarVehicle;
class ACharacter;

UCLASS()
class MURDAR_GAMEDEV_API UAISpatialIndex : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAISpatialIndex* Get(const UObject* WorldContext);

	/** Cars (any: traffic, parked, police, the player's) within RadiusCm of Where (2D). */
	void QueryVehicles(const FVector& Where, float RadiusCm, TArray<AMurdarVehicle*>& Out) const;
	/** Characters on foot within RadiusCm (hidden ones and those attached to a car - sitting in it - are left out). */
	void QueryCharacters(const FVector& Where, float RadiusCm, TArray<ACharacter*>& Out) const;
	/** Every car / character of the last rebuild (for the rare caller that really wants all). */
	const TArray<TWeakObjectPtr<AMurdarVehicle>>& AllVehicles() const { return Vehicles; }

	int32 NumVehicles() const { return Vehicles.Num(); }
	int32 NumCharacters() const { return Characters.Num(); }
	/** Actor visits saved since BeginPlay (what the old scans would have cost minus what the queries cost). */
	int64 GetVisitsSaved() const { return VisitsSaved; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAISpatialIndex, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Rebuild();
	/** The fastest anything moves (cm/s): a car at 160 km/h. */
	static constexpr float MaxSpeedCms = 4500.f;

	MurdarAIQ::FSpatialHash2D VehicleGrid{ 2000.f };
	MurdarAIQ::FSpatialHash2D CharacterGrid{ 2000.f };
	TArray<TWeakObjectPtr<AMurdarVehicle>> Vehicles;
	TArray<TWeakObjectPtr<ACharacter>> Characters;
	float SinceRebuild = 1000.f;
	float CellCm = 2000.f;
	mutable std::vector<int> Scratch;
	mutable int64 VisitsSaved = 0;
};
