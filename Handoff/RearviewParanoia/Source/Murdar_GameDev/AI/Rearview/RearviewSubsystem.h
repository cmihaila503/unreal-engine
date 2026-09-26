// "Paranoia în retrovizoare" — the director. Watches how the player drives at night (brake checks, sudden turns,
// U-turns, a loop round the block, pulling over, lights out), tells whoever is behind him, makes the civilian behind
// honk or flash, and now and then puts somebody on his tail: an undercover crew, a gang car — or, as often, nobody at
// all, just an ordinary car that happens to go his way. No UI: everything the player learns, he learns from the mirror.
// 10 Hz timer, nothing per frame (PROJECT_OVERVIEW §5).

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

	/** The player's foot is on the brake (not the handbrake): his brake lights are on. */
	bool IsPlayerBraking() const { return bPlayerBraking; }

	/** MurdarNight: -1 = settings decide, 0 = day, 1 = night. */
	void SetNightOverride(int32 Mode) { NightOverride = Mode; UpdateDarkness(); }

	/** MurdarTail: put somebody behind him now (Civilian = a decoy, an ordinary car). False when there is no place. */
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
	void UpdateDarkness();  // 1 Hz and on override
	void ResetDetectors();
	void MaybeStartTail(float Now);
	void UpdatePlayerBrakeLights(AMurdarVehicle* Car);
	void OnBrakeCheck(AMurdarVehicle* PlayerCar);
	void OnInspectionTurn(AMurdarVehicle* PlayerCar, int32 Side);
	void OnManeuver(AMurdarVehicle* PlayerCar, MurdarRearview::EManeuver Maneuver);
	void CivilianReactsToBrakeCheck(AMurdarVehicle* PlayerCar);
	void FlashHeadlights(AMurdarVehicle* Car, int32 BlinksLeft);
	void EnsureMirror(AMurdarVehicle* PlayerCar);
	bool FindSpotBehind(const AMurdarVehicle* PlayerCar, FTransform& Out) const;
	bool SpawnDecoy(const FTransform& Where);
	void Publish(FGameplayTag Tag, const AActor* Source, float Magnitude = 0.f) const;
	AMurdarVehicle* PlayerCar() const;

	TUniquePtr<MurdarRearview::FBrakeCheckDetector> BrakeCheck;
	TUniquePtr<MurdarRearview::FInspectionTurnDetector> InspectionTurn;
	TUniquePtr<MurdarRearview::FManeuverTracker> Maneuvers;
	TArray<TWeakObjectPtr<ARearviewTailController>> Tails;

	FTimerHandle UpdateTimer;
	FRandomStream Rng;
	int32 UpdateCount = 0;
	int32 NightOverride = -1;
	bool bDark = false;
	bool bLastLights = true;
	bool bPlayerBraking = false;
	float LastTailEndTime = -1.e6f;
	int32 BrakeChecks = 0;
	int32 InspectionTurns = 0;
	int32 UTurns = 0;
	int32 Loops = 0;
	int32 TailsStarted = 0;
	int32 DecoysStarted = 0;
	TWeakObjectPtr<AMurdarVehicle> LastPlayerCar;
};
