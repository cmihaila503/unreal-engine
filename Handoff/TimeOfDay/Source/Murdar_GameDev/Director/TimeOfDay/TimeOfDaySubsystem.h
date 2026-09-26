// The game clock. Moves the sun, says when it is night (rearview, headlights, music), scales how many people and cars
// are out, and saves itself in the narrative state as two values (Stat.TimeOfDay minutes, Stat.Day) — tags and
// numbers only, like everything else in the save. Timer on game time: pause stops it. No UI: the player reads the sky.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/TimeOfDay/TimeOfDayRules.h"
#include "TimeOfDaySubsystem.generated.h"

class ADirectionalLight;

UCLASS()
class MURDAR_GAMEDEV_API UTimeOfDaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTimeOfDaySubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Murdar|Time") float GetHour() const { return Clock.GetHour(); }
	UFUNCTION(BlueprintPure, Category = "Murdar|Time") int32 GetDay() const { return Clock.GetDay(); }
	UFUNCTION(BlueprintPure, Category = "Murdar|Time") float GetLightLevel01() const;
	UFUNCTION(BlueprintPure, Category = "Murdar|Time") bool IsNight() const { return bNight; }

	/** Population multipliers for the current hour (UPopulationSubsystem caps × these; README §Patches). */
	UFUNCTION(BlueprintPure, Category = "Murdar|Time") float GetPeopleScale() const;
	UFUNCTION(BlueprintPure, Category = "Murdar|Time") float GetTrafficScale() const;

	/** Story / cheats. Hour 0..24. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Time") void SetHour(float Hour);
	UFUNCTION(BlueprintCallable, Category = "Murdar|Time") void SetFrozen(bool bFreeze) { bFrozen = bFreeze; }
	/** Cheat: real seconds per game day (< 0 = back to the settings). */
	void SetDayLengthOverride(float RealSeconds) { DayLengthOverride = RealSeconds; }

	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Update();
	void ApplySun();
	void SweepHeadlights();
	void SaveToState();
	void UpdateNight(bool bPublish);
	/** Reads the clock back from the narrative state: at world start and after a load (Event.State.Loaded). */
	bool RestoreFromState();
	ADirectionalLight* Sun();

	MurdarTime::FGameClock Clock;
	TWeakObjectPtr<ADirectionalLight> CachedSun;
	FTimerHandle UpdateTimer;
	float MinutesSinceSave = 0.f;
	float SecondsSinceSweep = 0.f;
	float DayLengthOverride = -1.f;
	bool bFrozen = false;
	bool bNight = false;
	int32 LoadedHandle = 0; // UGameEventSubsystem::FHandle (int32)
};
