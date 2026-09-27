// A pool of pedestrians (character + NPC brain). UPopulationSubsystem asks AcquirePedestrian where it used to call
// SpawnNPC and ReleasePedestrian where it destroyed them (README §Patches). Parked ones are hidden, frozen and out of
// collision; they come back reset. Warmed up one per quiet frame until Warm are parked. The dead and the ragdolled
// are never reused (a body stays a body). Cars are not pooled yet: a physics car's reset (KinetiForge drive state,
// damage, lights) needs its own pass — see README.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Pooling/PoolRules.h"
#include "ActorPoolSubsystem.generated.h"

class AMurdarCharacter;

UCLASS()
class MURDAR_GAMEDEV_API UActorPoolSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UActorPoolSubsystem* Get(const UObject* WorldContext);

	AMurdarCharacter* AcquirePedestrian(const FTransform& Where, FGameplayTag Sector);
	void ReleasePedestrian(AMurdarCharacter* Person);
	int32 NumParked() const { return Parked.Num(); }
	FString Describe() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UActorPoolSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void Park(AMurdarCharacter* Person);
	void Wake(AMurdarCharacter* Person, const FTransform& Where);

	struct FParked { TWeakObjectPtr<AMurdarCharacter> Person; double ParkedAt = 0.0; };
	TArray<FParked> Parked;
	int32 Reused = 0, Spawned = 0, Destroyed = 0;
	MurdarPool::FTuning Tuning;
};
