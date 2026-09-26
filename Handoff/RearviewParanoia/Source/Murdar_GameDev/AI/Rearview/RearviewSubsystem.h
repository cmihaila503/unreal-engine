// "Paranoia în retrovizoare" — the director. Watches how the player drives at night (brake checks, inspection turns,
// lights out), tells whoever is behind him, makes the civilian behind lean on the horn, and now and then puts a
// follower on his tail (an undercover crew, a gang car). No UI: everything the player learns, he learns from the
// mirror. 10 Hz timer, nothing per frame (PROJECT_OVERVIEW §5).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Rearview/RearviewRules.h"
#include "RearviewSubsystem.generated.h"

class AMurdarVehicle;
class ARearviewTailController;

UCLASS()
class MURDAR_GAMEDEV_API URearviewSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static URearviewSubsystem* Get(const UObject* WorldContext);

	/** Night: the chapter's flag, MurdarNight, or the sun below the horizon (settings). */
	UFUNCTION(BlueprintPure, Category = "Murdar|Rearview")
	bool IsDark() const { return bDark; }

	/** MurdarNight: -1 = settings decide, 0 = day, 1 = night. */
	void SetNightOverride(int32 Mode) { NightOverride = Mode; UpdateDarkness(); }

	/** MurdarTail: start a tail now (Role 1 undercover, 2 gang); false when there is no place to put it. */
	bool StartTail(MurdarRearview::ETailRole Role);

	/** The follower tells us it's done (broke off, lost him, destroyed). */
	void NotifyTailEnded(ARearviewTailController* Tail);

	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Update();          // 10 Hz
	void UpdateDarkness();  // 1 Hz (every 10th update) and on override
	void MaybeStartTail(float Now);
	void OnBrakeCheck(AMurdarVehicle* PlayerCar);
	void OnInspectionTurn(AMurdarVehicle* PlayerCar, int32 Side);
	void HonkCivilianBehind(AMurdarVehicle* PlayerCar);
	void EnsureMirror(AMurdarVehicle* PlayerCar);
	void Publish(FGameplayTag Tag, const AActor* Source, float Magnitude = 0.f) const;
	AMurdarVehicle* PlayerCar() const;

	TUniquePtr<MurdarRearview::FBrakeCheckDetector> BrakeCheck;
	TUniquePtr<MurdarRearview::FInspectionTurnDetector> InspectionTurn;
	TArray<TWeakObjectPtr<ARearviewTailController>> Tails;

	FTimerHandle UpdateTimer;
	FRandomStream Rng;
	int32 UpdateCount = 0;
	int32 NightOverride = -1;
	bool bDark = false;
	bool bLastLights = true;
	float LastTailEndTime = -1.e6f;
	int32 BrakeChecks = 0;
	int32 InspectionTurns = 0;
	int32 TailsStarted = 0;
	TWeakObjectPtr<AMurdarVehicle> LastPlayerCar;
};
