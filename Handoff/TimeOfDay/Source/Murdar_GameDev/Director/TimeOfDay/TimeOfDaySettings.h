// Project Settings > Game > Murdar Time of Day. Every tunable of the clock and the sun, with unit and reason.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Director/TimeOfDay/TimeOfDayRules.h"
#include "TimeOfDaySettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Time of Day"))
class MURDAR_GAMEDEV_API UTimeOfDaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Real seconds per game day. 2880 = 48 real minutes (GTA-like pace: a mission can see dusk). 0 = frozen. */
	UPROPERTY(EditAnywhere, config, Category = "Clock", meta = (Units = "s", ClampMin = "0", ClampMax = "86400"))
	float RealSecondsPerGameDay = 2880.f;

	/** Where a new game starts when no chapter sets it. */
	UPROPERTY(EditAnywhere, config, Category = "Clock", meta = (ClampMin = "0", ClampMax = "23.99"))
	float StartHour = 8.f;

	/** Clock step. Timers run on game time, so pause and slow motion stop / slow the clock too. */
	UPROPERTY(EditAnywhere, config, Category = "Clock", meta = (Units = "s", ClampMin = "0.05", ClampMax = "2"))
	float UpdateIntervalSeconds = 0.25f;

	/** The clock is written into the narrative state (saved) every this many game minutes. */
	UPROPERTY(EditAnywhere, config, Category = "Clock", meta = (ClampMin = "1", ClampMax = "120"))
	float SaveEveryGameMinutes = 10.f;

	// ---- Sun (Bucharest, summer-ish; one fixed season — no calendar) ----
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (ClampMin = "3", ClampMax = "10")) float SunriseHour = 6.f;
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (ClampMin = "15", ClampMax = "22")) float SunsetHour = 20.f;
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (Units = "deg", ClampMin = "10", ClampMax = "90")) float MaxElevationDeg = 62.f;
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (Units = "deg", ClampMin = "5", ClampMax = "90")) float NightDepthDeg = 30.f;
	/** Dawn / dusk length on each side of sunrise / sunset. */
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (ClampMin = "0.1", ClampMax = "2")) float TwilightHours = 0.75f;
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (Units = "deg")) float SunriseYawDeg = 90.f;
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (Units = "deg")) float SunsetYawDeg = 270.f;
	/** "Night" for gameplay (rearview, headlights) below this light level. 0.5 = halfway through twilight. */
	UPROPERTY(EditAnywhere, config, Category = "Sun", meta = (ClampMin = "0.05", ClampMax = "0.95")) float NightBelowLight = 0.5f;
	/** Move the level's first directional light. Off if a level wants a fixed sky (the clock still runs). */
	UPROPERTY(EditAnywhere, config, Category = "Sun") bool bDriveSun = true;

	// ---- Who is out (multipliers on the population caps, one per hour 00..23) ----
	/** Pedestrians: empty streets at 3 am, busy at 8 and 18. */
	UPROPERTY(EditAnywhere, config, Category = "Density", EditFixedSize)
	TArray<float> PeopleByHour = { 0.10f, 0.08f, 0.05f, 0.05f, 0.05f, 0.10f, 0.35f, 0.75f, 1.00f, 0.85f, 0.80f, 0.85f,
	                               0.95f, 0.90f, 0.85f, 0.90f, 1.00f, 1.00f, 1.00f, 0.90f, 0.70f, 0.50f, 0.30f, 0.18f };
	/** Traffic: a little earlier and later than people (shift workers, taxis). */
	UPROPERTY(EditAnywhere, config, Category = "Density", EditFixedSize)
	TArray<float> TrafficByHour = { 0.20f, 0.15f, 0.10f, 0.10f, 0.15f, 0.30f, 0.60f, 1.00f, 1.00f, 0.80f, 0.70f, 0.75f,
	                                0.80f, 0.75f, 0.75f, 0.85f, 1.00f, 1.00f, 0.90f, 0.75f, 0.55f, 0.45f, 0.35f, 0.25f };

	/** AI cars' headlights follow the night; checked this often (cheap sweep, no patch to the drivers needed). */
	UPROPERTY(EditAnywhere, config, Category = "Lights", meta = (Units = "s", ClampMin = "1", ClampMax = "30"))
	float HeadlightSweepSeconds = 5.f;

	MurdarTime::FSunConfig ToSun() const
	{
		MurdarTime::FSunConfig C;
		C.SunriseHour = SunriseHour; C.SunsetHour = SunsetHour; C.MaxElevationDeg = MaxElevationDeg; C.NightDepthDeg = NightDepthDeg;
		C.TwilightHours = TwilightHours; C.SunriseYawDeg = SunriseYawDeg; C.SunsetYawDeg = SunsetYawDeg;
		return C;
	}
	static std::array<float, 24> ToCurve(const TArray<float>& V)
	{
		std::array<float, 24> A{};
		for (int32 i = 0; i < 24; ++i) { A[i] = V.IsValidIndex(i) ? V[i] : 1.f; }
		return A;
	}
};
