// The car radio. Stations broadcast on the world clock (MurdarRadio::At), so tuning in lands mid-song; the dial
// (next / previous, through "off") is remembered (Stat.RadioStation). Only in a car. News slots read the freshest,
// most important story about him (MurdarRadio::FNewsDesk) fed by bus events. Under the score and under a conversation
// it steps back. Streamer mode (player setting) drops licensed songs.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Radio/RadioRules.h"
#include "RadioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API URadioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static URadioSubsystem* Get(const UObject* WorldContext);

	void Tune(int32 Dir);
	int32 GetStation() const { return Station; }
	bool IsPlaying() const { return bInCar && Station >= 0; }
	/** Rebuild programs (streamer mode changed in the settings). */
	void Rebuild();
	FString Describe() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(URadioSubsystem, STATGROUP_Tickables); }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnBusEvent(const FGameEvent& E);
	void StartCurrent();
	void Stop();
	USoundBase* SoundFor(const MurdarRadio::FItem& Item);
	float Phase(int32 Index) const { return 1000.f * float(Index) + 137.f; }

	std::vector<std::vector<MurdarRadio::FItem>> Programs;
	MurdarRadio::FNewsDesk Desk;
	int32 Station = 0;
	bool bInCar = false;
	bool bBuilt = false;
	int32 PlayingItem = -1;
	double ItemEndsAt = 0.0;
	int32 Dialogues = 0;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Speaker;
	int32 BusHandle = 0;
};
