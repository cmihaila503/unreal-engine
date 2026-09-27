// Repeatable work. The pager beeps when he's free (no job, no mission, no police); the page names a contact; a
// payphone (an Interactable with Interact.Payphone) answers it and the contact gives a job generated from where he
// stands (MurdarJobs, unit-tested): a delivery, smuggling to the border, a car someone wants, a debt to collect.
// The job runs on MurdarMission::FMissionRuntime (the story missions' runtime) and publishes Event.Mission.* with the
// payload Job, so saving, sleeping and the music treat it like a mission. Pay goes through the economy. 4 Hz.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Jobs/JobRules.h"
#include "JobSubsystem.generated.h"

class AJobPoint;
class APawn;
class UVehicleDefinition;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UJobSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UJobSubsystem* Get(const UObject* WorldContext);

	/** Cheat / story: page now, or start a job of this kind now (-1 = the contact's choice). */
	void PageNow();
	bool StartJob(int32 ContactIndex, int32 Kind = -1);
	void AbortJob(const FString& Reason);
	bool IsRunning() const { return Runtime.IsValid() && Runtime->GetState() == MurdarMission::EState::Running; }
	/** Where the current stage sends him (the paper map circles it). False when there is no place to go. */
	bool GetTargetLocation(FVector& OutLocation, FText& OutName) const;
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void GatherPoints();
	void OnBusEvent(const FGameEvent& E);
	void Tick4Hz();
	void TickPager(float Now);
	void Apply(const std::vector<MurdarMission::FStep>& Steps);
	void SpawnDebtor();
	void Finish(bool bSuccess, const FString& Reason);
	bool InTargetCar() const;
	FText PlaceOf(int32 Index, bool bDrop) const;
	void Say(const FText& Line) const;
	void Publish(const TCHAR* TagName, float Magnitude = 0.f) const;

	TArray<TWeakObjectPtr<AJobPoint>> PickupActors, DropActors;
	std::vector<MurdarJobs::FPoint> Pickups, Drops;

	MurdarJobs::FJob Job;
	int32 JobContact = -1;
	TUniquePtr<MurdarMission::FMissionRuntime> Runtime;
	TSoftObjectPtr<UVehicleDefinition> WantedCar;
	TWeakObjectPtr<APawn> Debtor;
	bool bDebtorAsked = false;

	int32 PendingContact = -1;
	float NextPageAt = 0.f;
	float PageExpiresAt = 0.f;
	int32 StoryMissions = 0;
	int32 BusHandle = 0;
	bool bInHandler = false;
	FTimerHandle TickTimer;
};
