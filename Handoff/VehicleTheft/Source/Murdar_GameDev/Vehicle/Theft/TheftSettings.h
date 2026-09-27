// Project Settings > Game > Murdar Vehicle Theft.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Vehicle/Theft/TheftRules.h"
#include "TheftSettings.generated.h"

class USoundBase;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Vehicle Theft"))
class MURDAR_GAMEDEV_API UTheftSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(EditAnywhere, config, Category = "Locks", meta = (ClampMin = "0", ClampMax = "1")) float LockChanceDay = 0.55f;
	UPROPERTY(EditAnywhere, config, Category = "Locks", meta = (ClampMin = "0", ClampMax = "1")) float LockChanceNight = 0.8f;
	UPROPERTY(EditAnywhere, config, Category = "Break-in", meta = (ClampMin = "0", ClampMax = "1")) float AlarmChance = 0.3f;
	UPROPERTY(EditAnywhere, config, Category = "Break-in", meta = (Units = "s", ClampMin = "0.1", ClampMax = "10")) float BreakInSeconds = 1.2f;
	UPROPERTY(EditAnywhere, config, Category = "Break-in", meta = (Units = "s", ClampMin = "0", ClampMax = "15")) float HotwireSeconds = 2.5f;
	UPROPERTY(EditAnywhere, config, Category = "Break-in", meta = (Units = "cm", ClampMin = "100", ClampMax = "600")) float CancelDistanceCm = 250.f;
	UPROPERTY(EditAnywhere, config, Category = "Break-in", meta = (Units = "s", ClampMin = "0", ClampMax = "120")) float AlarmSeconds = 25.f;
	UPROPERTY(EditAnywhere, config, Category = "Carjack", meta = (Units = "km/h", ClampMin = "0", ClampMax = "40")) float CarjackMaxKph = 12.f;
	UPROPERTY(EditAnywhere, config, Category = "Carjack", meta = (Units = "cm", ClampMin = "100", ClampMax = "600")) float CarjackReachCm = 300.f;
	/** The driver is pulled out: the player gets in after this (there is no animation yet). */
	UPROPERTY(EditAnywhere, config, Category = "Carjack", meta = (Units = "s", ClampMin = "0", ClampMax = "3")) float CarjackEnterDelay = 0.8f;
	UPROPERTY(EditAnywhere, config, Category = "Reports", meta = (Units = "cm")) float WitnessRadiusCm = 2000.f;
	UPROPERTY(EditAnywhere, config, Category = "Reports", meta = (Units = "cm")) float AlarmWitnessRadiusCm = 5000.f;
	UPROPERTY(EditAnywhere, config, Category = "Reports", meta = (Units = "s")) FVector2D ReportVictimSeconds = FVector2D(15.f, 35.f);
	UPROPERTY(EditAnywhere, config, Category = "Reports", meta = (Units = "s")) FVector2D ReportWitnessSeconds = FVector2D(30.f, 75.f);
	UPROPERTY(EditAnywhere, config, Category = "Reports", meta = (Units = "s")) FVector2D ReportOwnerSeconds = FVector2D(240.f, 480.f);
	/** Heat added when a patrol recognises a reported car (>= HeatStop 12: a stop, not a chase). */
	UPROPERTY(EditAnywhere, config, Category = "Reports", meta = (ClampMin = "0", ClampMax = "39")) float CrimeSeverity = 15.f;

	UPROPERTY(EditAnywhere, config, Category = "Sound") TSoftObjectPtr<USoundBase> GlassSound;
	UPROPERTY(EditAnywhere, config, Category = "Sound") TSoftObjectPtr<USoundBase> AlarmSound;

	MurdarTheft::FTuning ToTuning() const
	{
		MurdarTheft::FTuning T;
		T.LockChanceDay = LockChanceDay; T.LockChanceNight = LockChanceNight; T.AlarmChance = AlarmChance;
		T.BreakInSeconds = BreakInSeconds; T.HotwireSeconds = HotwireSeconds; T.CancelDistanceCm = CancelDistanceCm;
		T.CarjackMaxKph = CarjackMaxKph; T.CarjackReachCm = CarjackReachCm;
		T.WitnessRadiusCm = WitnessRadiusCm; T.AlarmWitnessRadiusCm = AlarmWitnessRadiusCm;
		T.ReportVictimMin = ReportVictimSeconds.X; T.ReportVictimMax = ReportVictimSeconds.Y;
		T.ReportWitnessMin = ReportWitnessSeconds.X; T.ReportWitnessMax = ReportWitnessSeconds.Y;
		T.ReportOwnerMin = ReportOwnerSeconds.X; T.ReportOwnerMax = ReportOwnerSeconds.Y;
		T.CrimeSeverity = CrimeSeverity;
		return T;
	}
};
