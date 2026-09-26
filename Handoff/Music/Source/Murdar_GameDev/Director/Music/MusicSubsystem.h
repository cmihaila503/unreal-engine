// The score. 10 Hz: reads stress (UTensionSubsystem), wanted level (UFactionMemorySubsystem) and what the bus said
// (shots near him, rearview tails, chases ending, conversations), picks a mood with MurdarMusic::FMoodMachine
// (unit-tested: up at once, down after a wait), glides the intensity and drives either a MetaSound's parameters or
// the stems' volumes. Stings on events. Nothing is shown; nothing is saved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Music/MusicRules.h"
#include "MusicSubsystem.generated.h"

class UAudioComponent;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UMusicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMusicSubsystem* Get(const UObject* WorldContext);

	/** Cutscenes / menus: force a mood (or clear with bForce false). */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Music") void ForceMood(int32 Mood, bool bForce);

	float GetIntensity() const { return Intensity; }
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Tick10Hz();
	MurdarMusic::FTuning Tuning() const;
	MurdarMusic::FInputs GatherInputs(float Now) const;
	void EnsurePlaying();
	void StopAll();
	void ApplyOutput();
	void OnBusEvent(const FGameEvent& Event);
	void PlaySting(const FGameplayTag& EventTag);

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Score;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> StemComponents;

	MurdarMusic::FMoodMachine Machine;
	float Intensity = 0.f;
	float LastShotTime = -1e6f;
	float TailSince = -1e6f;
	bool bTailed = false;
	float PursuitEndedAt = -1e6f;
	int32 ActiveDialogues = 0;
	float SilentSince = 0.f;
	int32 ForcedMood = -1;
	TMap<FGameplayTag, float> StingNextAllowed;
	int32 BusHandle = 0;
	FTimerHandle TickTimer;
};
