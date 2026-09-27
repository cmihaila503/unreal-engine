// Project Settings > Game > Murdar Weather.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Director/Weather/WeatherRules.h"
#include "WeatherSettings.generated.h"

class UMaterialParameterCollection;
class UNiagaraSystem;
class UPhysicalMaterial;
class USoundBase;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Weather"))
class MURDAR_GAMEDEV_API UWeatherSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(EditAnywhere, config, Category = "Weather") bool bEnabled = true;
	/** A change takes about this long to blend in. */
	UPROPERTY(EditAnywhere, config, Category = "Weather", meta = (Units = "s", ClampMin = "5", ClampMax = "600")) float BlendSeconds = 90.f;
	/** Soaked asphalt grips this much (friction scale). */
	UPROPERTY(EditAnywhere, config, Category = "Roads", meta = (ClampMin = "0.4", ClampMax = "1")) float WetGrip = 0.75f;
	/** Road physical materials whose Friction the wetness scales (asphalt, cobbles; not mud). Restored at the end. */
	UPROPERTY(EditAnywhere, config, Category = "Roads") TArray<TSoftObjectPtr<UPhysicalMaterial>> WetAffected;
	UPROPERTY(EditAnywhere, config, Category = "People", meta = (ClampMin = "0", ClampMax = "1")) float PeopleInStorm = 0.35f;

	/** Scalars written: Rain, Wetness, Cloud, Fog, Wind (0..1). Road / puddle / wiper materials read them. */
	UPROPERTY(EditAnywhere, config, Category = "Look") TSoftObjectPtr<UMaterialParameterCollection> Parameters;
	/** Rain around the camera, user float "Intensity" (0..1). ADAPT: Niagara in Build.cs. */
	UPROPERTY(EditAnywhere, config, Category = "Look") TSoftObjectPtr<UNiagaraSystem> RainSystem;
	/** Height-fog density multiplier at full fog (on top of the level's own density). */
	UPROPERTY(EditAnywhere, config, Category = "Look", meta = (ClampMin = "1", ClampMax = "20")) float FogMultiplier = 6.f;
	UPROPERTY(EditAnywhere, config, Category = "Sound") TSoftObjectPtr<USoundBase> RainLoop;
	UPROPERTY(EditAnywhere, config, Category = "Sound") TSoftObjectPtr<USoundBase> Thunder;

	MurdarWeather::FTuning ToTuning() const
	{
		MurdarWeather::FTuning T;
		T.WetGrip = WetGrip; T.RainPeople = PeopleInStorm; T.BlendRate = 1.f / FMath::Max(BlendSeconds, 1.f);
		return T;
	}
};
