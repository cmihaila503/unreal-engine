// Project Settings > Game > Murdar Radio. Stations, their content, and which events become news.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "RadioSettings.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct FRadioTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Track") TSoftObjectPtr<USoundBase> Sound;
	/** A licensed song (not ours): left out in streamer mode. */
	UPROPERTY(EditAnywhere, config, Category = "Track") bool bLicensed = false;
};

USTRUCT(BlueprintType)
struct FRadioNewsSet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, config, Category = "News") TArray<TSoftObjectPtr<USoundBase>> Clips;
};

USTRUCT(BlueprintType)
struct FRadioStation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Station") FText Name = NSLOCTEXT("MurdarRadio", "Station", "Radio Bahlui FM");
	UPROPERTY(EditAnywhere, config, Category = "Station") TArray<FRadioTrack> Songs;
	UPROPERTY(EditAnywhere, config, Category = "Station") TArray<TSoftObjectPtr<USoundBase>> DjLinks;
	UPROPERTY(EditAnywhere, config, Category = "Station") TArray<TSoftObjectPtr<USoundBase>> Ads;
	/** Read when there's no story. */
	UPROPERTY(EditAnywhere, config, Category = "Station") TArray<TSoftObjectPtr<USoundBase>> GenericNews;
	/** A story (News.*) -> what this station says about it. */
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (ForceInlineRow)) TMap<FGameplayTag, FRadioNewsSet> Stories;
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (Units = "s")) float NewsSlotSeconds = 30.f;
	UPROPERTY(EditAnywhere, config, Category = "Station") int32 SongsPerDj = 2;
	UPROPERTY(EditAnywhere, config, Category = "Station") int32 SongsPerAd = 3;
	UPROPERTY(EditAnywhere, config, Category = "Station") int32 SongsPerNews = 6;
};

USTRUCT(BlueprintType)
struct FRadioNewsTrigger
{
	GENERATED_BODY()
	/** Bus event (Event.Police.PursuitEnded, Event.Consequence.Station, Event.Vehicle.ReportedStolen...). */
	UPROPERTY(EditAnywhere, config, Category = "News") FGameplayTag Event;
	UPROPERTY(EditAnywhere, config, Category = "News", meta = (Categories = "News")) FGameplayTag Story;
	UPROPERTY(EditAnywhere, config, Category = "News") int32 Priority = 1;
	/** Stale after this (real seconds). */
	UPROPERTY(EditAnywhere, config, Category = "News", meta = (Units = "s")) float FreshSeconds = 900.f;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Radio"))
class MURDAR_GAMEDEV_API URadioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(EditAnywhere, config, Category = "Stations") TArray<FRadioStation> Stations;
	UPROPERTY(EditAnywhere, config, Category = "News") TArray<FRadioNewsTrigger> NewsTriggers;
	/** ADAPT: keys free in the car. */
	UPROPERTY(EditAnywhere, config, Category = "Input") TArray<FKey> NextKeys = { EKeys::Period, EKeys::Gamepad_DPad_Right };
	UPROPERTY(EditAnywhere, config, Category = "Input") TArray<FKey> PrevKeys = { EKeys::Comma, EKeys::Gamepad_DPad_Left };
	/** A burst of static between stations. */
	UPROPERTY(EditAnywhere, config, Category = "Sound") TSoftObjectPtr<USoundBase> TuneStatic;
	UPROPERTY(EditAnywhere, config, Category = "Sound", meta = (ClampMin = "0", ClampMax = "2")) float Volume = 0.8f;
	/** A 90s car speaker: highs cut. 0 = no filter. */
	UPROPERTY(EditAnywhere, config, Category = "Sound", meta = (Units = "Hz", ClampMin = "0", ClampMax = "20000")) float SpeakerLowPassHz = 7000.f;
	/** Volume under the score at full music intensity, and under a conversation. */
	UPROPERTY(EditAnywhere, config, Category = "Sound", meta = (ClampMin = "0", ClampMax = "1")) float UnderScore = 0.25f;
	UPROPERTY(EditAnywhere, config, Category = "Sound", meta = (ClampMin = "0", ClampMax = "1")) float UnderDialogue = 0.3f;
};
