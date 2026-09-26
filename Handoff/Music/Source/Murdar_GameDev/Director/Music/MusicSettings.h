// Project Settings > Game > Murdar Music. Either one MetaSound driven by parameters, or synced stems with fade ranges.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "MusicSettings.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct FMusicStem
{
	GENERATED_BODY()

	/** A looping stem. All stems must have the same length and tempo: they start together and stay in step. */
	UPROPERTY(EditAnywhere, config, Category = "Stem") TSoftObjectPtr<USoundBase> Sound;
	/** Silent below this intensity (0..1)... */
	UPROPERTY(EditAnywhere, config, Category = "Stem", meta = (ClampMin = "0", ClampMax = "1")) float FadeInStart = 0.f;
	/** ...full from this one. Pad 0.05→0.3, pulse 0.3→0.5, drums 0.6→0.8, lead 0.85→1. */
	UPROPERTY(EditAnywhere, config, Category = "Stem", meta = (ClampMin = "0", ClampMax = "1")) float FullAt = 0.3f;
	UPROPERTY(EditAnywhere, config, Category = "Stem", meta = (ClampMin = "0", ClampMax = "2")) float Volume = 1.f;
};

USTRUCT(BlueprintType)
struct FMusicStingSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Sting") TArray<TSoftObjectPtr<USoundBase>> Variants;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Music"))
class MURDAR_GAMEDEV_API UMusicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Preferred: one MetaSound with float input "Intensity" (0..1) and int input "Mood" (0 silence .. 5 combat).
	 *  When set, Stems are ignored. */
	UPROPERTY(EditAnywhere, config, Category = "Score") TSoftObjectPtr<USoundBase> MusicMetaSound;
	UPROPERTY(EditAnywhere, config, Category = "Score") FName IntensityParameter = TEXT("Intensity");
	UPROPERTY(EditAnywhere, config, Category = "Score") FName MoodParameter = TEXT("Mood");

	/** Fallback: plain looping stems faded by intensity (vertical layering). */
	UPROPERTY(EditAnywhere, config, Category = "Score") TArray<FMusicStem> Stems;

	/** One-shots on bus events (Event.Police.PursuitEnded, Event.Mission.Succeeded, Event.Rearview.TailBlown...). */
	UPROPERTY(EditAnywhere, config, Category = "Stings", meta = (ForceInlineRow)) TMap<FGameplayTag, FMusicStingSet> Stings;
	UPROPERTY(EditAnywhere, config, Category = "Stings", meta = (Units = "s", ClampMin = "0")) float StingCooldownSeconds = 8.f;

	// ---- Behaviour (MurdarMusic::FTuning) ----
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "s", ClampMin = "1", ClampMax = "60")) float CombatShotWindow = 12.f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "s", ClampMin = "0", ClampMax = "120")) float AftermathSeconds = 20.f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (ClampMin = "0", ClampMax = "1")) float UneaseStress = 0.5f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "s", ClampMin = "0", ClampMax = "60")) float DownDelay = 6.f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "s", ClampMin = "0", ClampMax = "60")) float MinDwell = 8.f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (ClampMin = "0.01", ClampMax = "10")) float RiseRate = 0.8f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (ClampMin = "0.01", ClampMax = "10")) float FallRate = 0.12f;

	/** A shot counts for combat music only this close to him. */
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "cm", ClampMin = "500")) float ShotHearingCm = 4000.f;
	/** A tail with no news (no blown / attack event) is forgotten after this. */
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "s", ClampMin = "10")) float TailMemorySeconds = 120.f;
	/** Music under a conversation line, in dB (0 = no duck). */
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (Units = "dB", ClampMin = "-24", ClampMax = "0")) float DialogueDuckDb = -8.f;
	UPROPERTY(EditAnywhere, config, Category = "Behaviour", meta = (ClampMin = "0", ClampMax = "2")) float MasterVolume = 0.8f;
};
