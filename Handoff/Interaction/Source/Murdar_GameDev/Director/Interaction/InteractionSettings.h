// Project Settings > Game > Murdar Interaction. How the "E" finds its target; every value with unit and reason.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "InteractionSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Interaction"))
class MURDAR_GAMEDEV_API UInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Focus scans per second. 8 Hz: the prompt follows a turning head without a per-frame cost. */
	UPROPERTY(EditAnywhere, config, Category = "Focus", meta = (Units = "Hz", ClampMin = "2", ClampMax = "30"))
	float ScanHz = 8.f;

	/** Anything farther than this is skipped before scoring (cheap reject; per-thing ranges are smaller). */
	UPROPERTY(EditAnywhere, config, Category = "Focus", meta = (Units = "cm", ClampMin = "100", ClampMax = "2000"))
	float BroadRangeCm = 500.f;

	/** How much better another thing must score to take the focus from the current one (0..1). Stops flicker. */
	UPROPERTY(EditAnywhere, config, Category = "Focus", meta = (ClampMin = "0", ClampMax = "0.5"))
	float Stickiness = 0.15f;

	/** Facing = where the camera looks (true) or where the body faces (false). Camera: you use what you look at. */
	UPROPERTY(EditAnywhere, config, Category = "Focus")
	bool bUseCameraFacing = true;

	/** Nothing through walls: a trace from the eyes to the thing must reach it. */
	UPROPERTY(EditAnywhere, config, Category = "Focus")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, config, Category = "Focus", meta = (EditCondition = "bRequireLineOfSight"))
	TEnumAsByte<ECollisionChannel> SightChannel = ECC_Visibility;

	/** At most this many line-of-sight traces per scan (best first). */
	UPROPERTY(EditAnywhere, config, Category = "Focus", meta = (ClampMin = "1", ClampMax = "8", EditCondition = "bRequireLineOfSight"))
	int32 MaxTracesPerScan = 3;
};
