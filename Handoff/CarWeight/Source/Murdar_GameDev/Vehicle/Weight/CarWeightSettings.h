// Project Settings > Game > Murdar Car Weight. One switch per fix, so each can be felt on its own.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Vehicle/Weight/CarWeightRules.h"
#include "CarWeightSettings.generated.h"

class UPhysicalMaterial;
class USoundBase;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Car Weight"))
class MURDAR_GAMEDEV_API UCarWeightSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	static const UCarWeightSettings* Get() { return GetDefault<UCarWeightSettings>(); }

	// --- Switches ---
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bAirGravity = true;
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bAirStabilize = true;
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bLandingSettle = true;
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bHitStop = true;
	/** Body physical material (no bounce) + capped depenetration, on every car at BeginPlay. */
	UPROPERTY(EditAnywhere, config, Category = "Switches") bool bBodyContact = true;

	// --- Air ---
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (Units = "s", ClampMin = "0", ClampMax = "0.5")) float AirborneAfterSeconds = 0.08f;
	/** Total gravity while all four wheels are off (1 = real). 1.5-2 reads as a heavy car at game camera distance. */
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (ClampMin = "1", ClampMax = "3")) float GravityMultiplier = 1.8f;
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (Units = "s", ClampMin = "0", ClampMax = "1")) float RampSeconds = 0.25f;
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (ClampMin = "0", ClampMax = "10")) float PitchRollDamping = 2.5f;
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (ClampMin = "0", ClampMax = "5")) float LevelingTorque = 0.6f;
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (Units = "deg", ClampMin = "0", ClampMax = "90")) float MaxLevelTiltDeg = 50.f;
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (Units = "s", ClampMin = "0", ClampMax = "1")) float SettleSeconds = 0.35f;
	UPROPERTY(EditAnywhere, config, Category = "Air", meta = (ClampMin = "0", ClampMax = "20")) float SettleDamping = 6.f;

	// --- Contact ---
	/** Restitution 0 (Combine Min), Friction ~0.3 (Combine Average): the car stops at a wall and slides along it. */
	UPROPERTY(EditAnywhere, config, Category = "Contact") TSoftObjectPtr<UPhysicalMaterial> BodyMaterial;
	UPROPERTY(EditAnywhere, config, Category = "Contact", meta = (Units = "cm/s", ClampMin = "50", ClampMax = "2000")) float MaxDepenetrationVelocity = 200.f;

	// --- Impact ---
	UPROPERTY(EditAnywhere, config, Category = "Impact", meta = (ClampMin = "0", ClampMax = "1")) float HitStopMinSeverity = 0.35f;
	UPROPERTY(EditAnywhere, config, Category = "Impact", meta = (Units = "s", ClampMin = "0", ClampMax = "0.2")) float HitStopMaxSeconds = 0.07f;
	UPROPERTY(EditAnywhere, config, Category = "Impact", meta = (ClampMin = "0.01", ClampMax = "1")) float HitStopDilation = 0.05f;
	UPROPERTY(EditAnywhere, config, Category = "Impact", meta = (Units = "s", ClampMin = "0", ClampMax = "5")) float HitStopCooldown = 1.f;
	UPROPERTY(EditAnywhere, config, Category = "Impact") float ShakeMin = 0.3f;
	UPROPERTY(EditAnywhere, config, Category = "Impact") float ShakeMax = 1.6f;
	/** A low "weight" thud under the crash sound (a sub-bass hit). Optional. */
	UPROPERTY(EditAnywhere, config, Category = "Impact") TSoftObjectPtr<USoundBase> ThumpSound;

	MurdarWeight::FAirTuning Air() const
	{
		MurdarWeight::FAirTuning T;
		T.AirborneAfterSeconds = AirborneAfterSeconds; T.GravityMultiplier = GravityMultiplier; T.RampSeconds = RampSeconds;
		T.PitchRollDamping = PitchRollDamping; T.LevelingTorque = LevelingTorque; T.MaxLevelTiltDeg = MaxLevelTiltDeg;
		T.SettleSeconds = SettleSeconds; T.SettleDamping = SettleDamping;
		return T;
	}

	MurdarWeight::FImpactTuning Impact(float MinDeltaV, float FullDeltaV) const
	{
		MurdarWeight::FImpactTuning T;
		T.MinDeltaVCms = MinDeltaV; T.FullDeltaVCms = FullDeltaV;
		T.HitStopMinSeverity = HitStopMinSeverity; T.HitStopMaxSeconds = HitStopMaxSeconds; T.HitStopDilation = HitStopDilation;
		T.HitStopCooldown = HitStopCooldown; T.ShakeMin = ShakeMin; T.ShakeMax = ShakeMax;
		return T;
	}
};
