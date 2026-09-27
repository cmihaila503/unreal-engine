// The paper map page: the drawing, pencil marks for the places he knows, the job's place circled in red, a cross
// where he stands. Mouse wheel / triggers zoom, drag / stick / arrows pan, Esc / B / M back. Custom OnPaint, no assets
// beyond the drawing. View maths: MurdarMap (unit-tested).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/PaperMap/PaperMapRules.h"

struct FPaperMapMark
{
	MurdarMap::FV2 UV;
	FText Label;
	FLinearColor Color;
	bool bCircle = false; // the job's place
	bool bCross = false;  // him
};

class SPaperMap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPaperMap) {}
		SLATE_ARGUMENT(UTexture2D*, Texture)
		SLATE_ARGUMENT(TArray<FPaperMapMark>, Marks)
		SLATE_ARGUMENT(MurdarMap::FV2, OpenAt)
		SLATE_ARGUMENT(float, MaxZoom)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 800.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Mouse) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Mouse) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Mouse) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Mouse) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;

private:
	float Aspect(const FGeometry& G) const { const FVector2D S = G.GetLocalSize(); return S.Y > 0.0 ? float(S.X / S.Y) : 1.f; }
	void Pan(float Dx01, float Dy01, float AspectRatio);

	FSlateBrush PaperBrush;
	TArray<FPaperMapMark> Marks;
	MurdarMap::FView View;
	MurdarMap::FViewLimits Limits;
	FSimpleDelegate OnBack;
	bool bDragging = false;
	mutable float LastAspect = 1.f;
};
