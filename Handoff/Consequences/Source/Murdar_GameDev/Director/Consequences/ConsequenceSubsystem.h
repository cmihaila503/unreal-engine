// What happens after death and arrest in the open city, and natural recovery.
// AMurdarCharacter::AfterDeath and UChapterDirector::AfterArrest ask this first (README §Patches); when it answers
// false (story chapter, or no hospital / station in the map) they go back to the checkpoint exactly as today.
// Hospital: nearest Hospital start, hours pass, a bill. Station: nearest PoliceStation start, hours in a cell that grow
// with the heat of the arrest and every earlier arrest (Stat.Arrests, saved), a fine, firearms taken. Heat is cleared.
// Recovery: 4 Hz, health comes back after a quiet moment up to a cap; a doctor (economy item) heals fully.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ConsequenceSubsystem.generated.h"

class AMurdarCharacter;
class APlayerStart;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UConsequenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UConsequenceSubsystem* Get(const UObject* WorldContext);

	/** From AfterDeath, after Revive. True = handled (he woke up in hospital); false = do the checkpoint. */
	bool HandlePlayerDeath(AMurdarCharacter* Player);
	/** From AfterArrest, after input is back. True = handled (station); false = do the checkpoint. */
	bool HandlePlayerArrest(AMurdarCharacter* Player);

	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	APlayerStart* FindNearest(FName Tag, const FVector& From) const;
	bool IsStoryMode() const;
	void WakeAt(AMurdarCharacter* Player, const APlayerStart* Start) const;
	void ClearHeat() const;
	void Confiscate(AMurdarCharacter* Player) const;
	void Recover();
	void Publish(const TCHAR* TagName, float Magnitude) const;

	TArray<int32> BusHandles;
	FTimerHandle RecoverTimer;
	float LastPlayerDamageTime = -1000.f;
	/** Heat when the police got him (Event.Police.Arrested); by AfterArrest it may already have decayed. */
	float HeatAtArrest = 0.f;
};
