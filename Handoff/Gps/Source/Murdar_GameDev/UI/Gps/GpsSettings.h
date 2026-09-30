// Project Settings > Game > Murdar GPS. The map drawing and its world corners are the Paper Map's
// (UPaperMapSettings::MapTexture / WorldAtUV0 / WorldAtUV1): one map, two views.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UI/Gps/GpsRules.h"
#include "GpsSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar GPS"))
class MURDAR_GAMEDEV_API UGpsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	static const UGpsSettings* Get() { return GetDefault<UGpsSettings>(); }

	/** The minimap in play. Off = the map-only way (pause menu map still has pins and routes). */
	UPROPERTY(EditAnywhere, config, Category = "Minimap") bool bMinimap = true;
	/** Draw the routes on the minimap (off = pins only, no line: find your own way). */
	UPROPERTY(EditAnywhere, config, Category = "Minimap") bool bRouteOnMinimap = true;
	UPROPERTY(EditAnywhere, config, Category = "Minimap", meta = (ClampMin = "100", ClampMax = "600")) float SizePx = 230.f;
	/** From the bottom-left corner of the screen (GTA's place). */
	UPROPERTY(EditAnywhere, config, Category = "Minimap") FVector2D MarginPx = FVector2D(40.f, 40.f);
	UPROPERTY(EditAnywhere, config, Category = "Minimap", meta = (Units = "cm")) float RadiusAtRestCm = 9000.f;
	UPROPERTY(EditAnywhere, config, Category = "Minimap", meta = (Units = "cm")) float RadiusAtSpeedCm = 25000.f;
	UPROPERTY(EditAnywhere, config, Category = "Minimap") float FullSpeedKph = 110.f;
	UPROPERTY(EditAnywhere, config, Category = "Minimap", meta = (ClampMin = "0", ClampMax = "1")) float Opacity = 0.9f;

	UPROPERTY(EditAnywhere, config, Category = "Colours") FLinearColor MissionColor = FLinearColor(0.95f, 0.76f, 0.19f, 1.f);  // yellow
	UPROPERTY(EditAnywhere, config, Category = "Colours") FLinearColor WaypointColor = FLinearColor(0.64f, 0.36f, 0.94f, 1.f); // purple
	UPROPERTY(EditAnywhere, config, Category = "Colours") FLinearColor PlayerColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, config, Category = "Colours", meta = (ClampMin = "1", ClampMax = "12")) float RouteThicknessPx = 4.f;

	/** Pressing "place" this close to your waypoint removes it. */
	UPROPERTY(EditAnywhere, config, Category = "Pins", meta = (Units = "cm")) float RemoveRadiusCm = 3000.f;
	/** Your waypoint clears itself when you get this close. */
	UPROPERTY(EditAnywhere, config, Category = "Pins", meta = (Units = "cm")) float ArriveCm = 2500.f;

	UPROPERTY(EditAnywhere, config, Category = "Routing", meta = (Units = "cm")) float OffRouteCm = 3000.f;
	UPROPERTY(EditAnywhere, config, Category = "Routing", meta = (Units = "s")) float MinRerouteSeconds = 2.f;
	/** How far from the lanes a start / end may be and still route along them (else: straight line). */
	UPROPERTY(EditAnywhere, config, Category = "Routing", meta = (Units = "cm")) float LaneSnapCm = 4000.f;

	MurdarGps::FMinimapTuning Minimap() const
	{
		MurdarGps::FMinimapTuning T;
		T.RadiusAtRestCm = RadiusAtRestCm; T.RadiusAtSpeedCm = RadiusAtSpeedCm; T.FullSpeedKph = FullSpeedKph;
		return T;
	}

	MurdarGps::FRouteTuning Routing() const
	{
		MurdarGps::FRouteTuning T;
		T.OffRouteCm = OffRouteCm; T.MinRerouteSeconds = MinRerouteSeconds;
		return T;
	}
};
