// Runs the missions: one at a time, started by bus events (a chapter trigger, later a dialogue), judged by
// MurdarMission::FMissionRuntime (unit-tested), with its effects applied through the project's own systems: facts and
// values on the narrative state, checkpoints on the chapter director, subtitles on the HUD, Event.Mission.* on the bus.
// An active mission is not saved (like a pursuit): after a load it is simply not running; done missions are facts.
// 4 Hz timer.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Missions/MissionRules.h"
#include "MissionSubsystem.generated.h"

class UMissionDefinition;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UMissionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMissionSubsystem* Get(const UObject* WorldContext);

	/** Start this mission now (cheat / story), if nothing else runs. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Mission") bool StartMission(UMissionDefinition* Mission);
	/** After a failure: back to its retry checkpoint and start again. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Mission") bool RetryLastFailed();
	UFUNCTION(BlueprintCallable, Category = "Murdar|Mission") void AbortActive(const FString& Reason);

	UFUNCTION(BlueprintPure, Category = "Murdar|Mission") UMissionDefinition* GetActive() const { return Active.Get(); }
	UFUNCTION(BlueprintPure, Category = "Murdar|Mission") bool IsDone(const UMissionDefinition* Mission) const;
	UMissionDefinition* FindByName(const FString& Name) const;
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void LoadMissions();
	void OnBusEvent(const FGameEvent& Event);
	void Tick4Hz();
	bool CanStart(const UMissionDefinition* M) const;
	MurdarMission::FMissionSpec BuildSpec(const UMissionDefinition* M) const;
	void Apply(const std::vector<MurdarMission::FStep>& Steps);
	void Subtitle(const FText& Text) const;
	void PublishMission(const TCHAR* TagName, float Magnitude = 0.f) const;

	UPROPERTY(Transient) TArray<TObjectPtr<UMissionDefinition>> Missions;
	TWeakObjectPtr<UMissionDefinition> Active;
	TWeakObjectPtr<UMissionDefinition> LastFailed;
	TUniquePtr<MurdarMission::FMissionRuntime> Runtime;
	FTimerHandle TickTimer;
	int32 BusHandle = 0;
	bool bInHandler = false;
};
