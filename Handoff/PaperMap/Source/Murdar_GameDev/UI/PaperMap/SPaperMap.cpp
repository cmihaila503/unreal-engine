#include "UI/PaperMap/SPaperMap.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	FVector2D ToLocal(const MurdarMap::FV2& V01, const FGeometry& G) { return FVector2D(V01.X, V01.Y) * G.GetLocalSize(); }

	void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& C, float R, const FLinearColor& Color)
	{
		TArray<FVector2D> Pts;
		for (int32 i = 0; i <= 20; ++i)
		{
			const float A = 2.f * PI * i / 20.f;
			Pts.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		// ADAPT: in 5.8 MakeLines may take TArray<FVector2f>; convert if the FVector2D overload is gone.
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, 3.f);
	}
}

void SPaperMap::Construct(const FArguments& InArgs)
{
	SetClipping(EWidgetClipping::ClipToBounds);
	PaperBrush.SetResourceObject(InArgs._Texture);
	PaperBrush.DrawAs = ESlateBrushDrawType::Image;
	Marks = InArgs._Marks;
	Limits.MaxZoom = FMath::Max(1.f, InArgs._MaxZoom);
	OnBack = InArgs._OnBack;
	View = MurdarMap::OpenAt(InArgs._OpenAt, 16.f / 9.f, Limits);
}

int32 SPaperMap::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	LastAspect = Aspect(G);
	const MurdarMap::FView V = MurdarMap::Clamp(View, LastAspect, Limits);

	// Background (the table under the paper), then the paper where it falls on screen.
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.08f, 0.06f, 0.05f));
	const FVector2D TopLeft = ToLocal(MurdarMap::UVToViewport({ 0.f, 0.f }, V, LastAspect), G);
	const FVector2D BottomRight = ToLocal(MurdarMap::UVToViewport({ 1.f, 1.f }, V, LastAspect), G);
	if (PaperBrush.GetResourceObject())
	{
		FSlateDrawElement::MakeBox(Out, Layer + 1, G.ToPaintGeometry(BottomRight - TopLeft, FSlateLayoutTransform(TopLeft)), &PaperBrush);
	}

	// Pencil marks.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Italic", 14);
	for (const FPaperMapMark& M : Marks)
	{
		const FVector2D P = ToLocal(MurdarMap::UVToViewport(M.UV, V, LastAspect), G);
		if (M.bCross)
		{
			const float S = 10.f;
			FSlateDrawElement::MakeLines(Out, Layer + 2, G.ToPaintGeometry(), { P + FVector2D(-S, -S), P + FVector2D(S, S) }, ESlateDrawEffect::None, M.Color, true, 3.f);
			FSlateDrawElement::MakeLines(Out, Layer + 2, G.ToPaintGeometry(), { P + FVector2D(-S, S), P + FVector2D(S, -S) }, ESlateDrawEffect::None, M.Color, true, 3.f);
		}
		else if (M.bCircle)
		{
			DrawCircle(Out, Layer + 2, G, P, 22.f, M.Color);
		}
		else
		{
			FSlateDrawElement::MakeBox(Out, Layer + 2, G.ToPaintGeometry(FVector2D(8.f, 8.f), FSlateLayoutTransform(P - FVector2D(4.f, 4.f))),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, M.Color);
		}
		if (!M.Label.IsEmpty())
		{
			FSlateDrawElement::MakeText(Out, Layer + 3, G.ToPaintGeometry(FVector2D(300.f, 24.f), FSlateLayoutTransform(P + FVector2D(12.f, -10.f))),
				M.Label, Font, ESlateDrawEffect::None, M.Color);
		}
	}
	return Layer + 3;
}

void SPaperMap::Pan(float Dx01, float Dy01, float AspectRatio)
{
	const float A = FMath::Max(AspectRatio, 1e-3f);
	const float SpanX = (A >= 1.f ? A : 1.f) / View.Zoom, SpanY = (A >= 1.f ? 1.f : 1.f / A) / View.Zoom;
	View.Center.X += Dx01 * SpanX;
	View.Center.Y += Dy01 * SpanY;
	View = MurdarMap::Clamp(View, A, Limits);
}

FReply SPaperMap::OnKeyDown(const FGeometry& G, const FKeyEvent& Key)
{
	const FKey K = Key.GetKey();
	const float Step = 0.08f;
	if (K == EKeys::Escape || K == EKeys::BackSpace || K == EKeys::M || K == EKeys::Gamepad_FaceButton_Right) { OnBack.ExecuteIfBound(); return FReply::Handled(); }
	if (K == EKeys::Left || K == EKeys::A || K == EKeys::Gamepad_DPad_Left) { Pan(-Step, 0.f, Aspect(G)); return FReply::Handled(); }
	if (K == EKeys::Right || K == EKeys::D || K == EKeys::Gamepad_DPad_Right) { Pan(Step, 0.f, Aspect(G)); return FReply::Handled(); }
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Gamepad_DPad_Up) { Pan(0.f, -Step, Aspect(G)); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down) { Pan(0.f, Step, Aspect(G)); return FReply::Handled(); }
	if (K == EKeys::Add || K == EKeys::Equals || K == EKeys::Gamepad_RightTrigger) { View = MurdarMap::ZoomAt(View, 1.25f, { 0.5f, 0.5f }, Aspect(G), Limits); return FReply::Handled(); }
	if (K == EKeys::Subtract || K == EKeys::Hyphen || K == EKeys::Gamepad_LeftTrigger) { View = MurdarMap::ZoomAt(View, 0.8f, { 0.5f, 0.5f }, Aspect(G), Limits); return FReply::Handled(); }
	return FReply::Unhandled();
}

FReply SPaperMap::OnMouseWheel(const FGeometry& G, const FPointerEvent& Mouse)
{
	const FVector2D Local = G.AbsoluteToLocal(Mouse.GetScreenSpacePosition()) / G.GetLocalSize();
	View = MurdarMap::ZoomAt(View, FMath::Pow(1.25f, Mouse.GetWheelDelta()), { float(Local.X), float(Local.Y) }, Aspect(G), Limits);
	return FReply::Handled();
}

FReply SPaperMap::OnMouseButtonDown(const FGeometry&, const FPointerEvent& Mouse)
{
	if (Mouse.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
	bDragging = true;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SPaperMap::OnMouseButtonUp(const FGeometry&, const FPointerEvent& Mouse)
{
	if (!bDragging || Mouse.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SPaperMap::OnMouseMove(const FGeometry& G, const FPointerEvent& Mouse)
{
	if (!bDragging) { return FReply::Unhandled(); }
	const FVector2D D = Mouse.GetCursorDelta() / G.GetLocalSize();
	Pan(-float(D.X), -float(D.Y), Aspect(G)); // the paper follows the hand
	return FReply::Handled();
}

FReply SPaperMap::OnAnalogValueChanged(const FGeometry& G, const FAnalogInputEvent& Event)
{
	const float V = Event.GetAnalogValue();
	if (FMath::Abs(V) < 0.25f) { return FReply::Unhandled(); }
	if (Event.GetKey() == EKeys::Gamepad_LeftX) { Pan(0.02f * V, 0.f, Aspect(G)); return FReply::Handled(); }
	if (Event.GetKey() == EKeys::Gamepad_LeftY) { Pan(0.f, -0.02f * V, Aspect(G)); return FReply::Handled(); }
	return FReply::Unhandled();
}
