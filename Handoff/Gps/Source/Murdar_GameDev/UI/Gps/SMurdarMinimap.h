// The minimap, bottom-left, heading-up (GTA): the city drawing around you, rotated so you always drive "up"; your
// arrow in the middle; the mission route (yellow) and your waypoint route (purple) at the same time; the two pins,
// held on the edge when they are off the map; zooms out with speed. Pure Slate OnPaint - no render target, no scene
// capture (a live top-down camera costs a whole second scene render every frame).
// Maths: MurdarGps (unit-tested). Drawing: UPaperMapSettings::MapTexture and its world corners.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"

class UGpsSubsystem;

class SMurdarMinimap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMurdarMinimap) {}
		SLATE_ARGUMENT(UGpsSubsystem*, Gps)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override;

private:
	/** Hidden in menus, cutscenes, the loading screen and while the player has no pawn. */
	bool ShouldShow() const;

	TWeakObjectPtr<UGpsSubsystem> Gps;
	FSlateBrush MapBrush;
	FSlateBrush WhiteBrush;
	float RadiusCm = 9000.f;
};
