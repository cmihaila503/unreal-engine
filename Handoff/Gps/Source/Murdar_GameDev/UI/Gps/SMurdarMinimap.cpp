#include "UI/Gps/SMurdarMinimap.h"
#include "UI/Gps/GpsSubsystem.h"
#include "UI/Gps/GpsSettings.h"
#include "UI/PaperMap/PaperMapSettings.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	MurdarGps::FV2 V2(const FVector& V) { return { static_cast<float>(V.X), static_cast<float>(V.Y) }; }
}

void SMurdarMinimap::Construct(const FArguments& InArgs)
{
	Gps = InArgs._Gps;
	SetVisibility(EVisibility::HitTestInvisible);
	if (const UPaperMapSettings* P = GetDefault<UPaperMapSettings>())
	{
		if (UTexture2D* Tex = P->MapTexture.LoadSynchronous())
		{
			MapBrush.SetResourceObject(Tex);
			MapBrush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
			MapBrush.DrawAs = ESlateBrushDrawType::Image;
		}
	}
	WhiteBrush = *FCoreStyle::Get().GetBrush("GenericWhiteBox");
}

FVector2D SMurdarMinimap::ComputeDesiredSize(float) const
{
	const UGpsSettings* S = UGpsSettings::Get();
	const float Size = S ? S->SizePx : 230.f;
	return FVector2D(Size, Size);
}

bool SMurdarMinimap::ShouldShow() const
{
	const UGpsSubsystem* G = Gps.Get();
	const UWorld* World = G ? G->GetWorld() : nullptr;
	if (!World || UGameplayStatics::IsGamePaused(World)) { return false; }
	FVector L; float Y = 0.f, K = 0.f;
	if (!G->GetPlayer(L, Y, K)) { return false; }
	// ADAPT: hide during cutscenes - UCutsceneSubsystem (Handoff/Cutscenes) IsPlaying(); and while the loading screen is up.
	return true;
}

void SMurdarMinimap::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SLeafWidget::Tick(Geometry, CurrentTime, DeltaTime);
	const UGpsSubsystem* G = Gps.Get();
	const UGpsSettings* S = UGpsSettings::Get();
	FVector L; float Yaw = 0.f, Kph = 0.f;
	if (G && S && G->GetPlayer(L, Yaw, Kph))
	{
		const MurdarGps::FMinimapTuning T = S->Minimap();
		RadiusCm = MurdarGps::SmoothRadius(RadiusCm, MurdarGps::TargetRadius(Kph, T), DeltaTime, T);
	}
}

int32 SMurdarMinimap::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const UGpsSubsystem* G = Gps.Get();
	const UGpsSettings* S = UGpsSettings::Get();
	const UPaperMapSettings* P = GetDefault<UPaperMapSettings>();
	FVector PlayerLoc; float Yaw = 0.f, Kph = 0.f;
	if (!G || !S || !P || !ShouldShow() || !G->GetPlayer(PlayerLoc, Yaw, Kph)) { return Layer; }

	// Where the widget sits: bottom-left of the viewport, SizePx square. The widget fills the viewport (it was added
	// as viewport content), so draw in a sub-rectangle of its geometry.
	const FVector2D Full = Geometry.GetLocalSize();
	const float Size = S->SizePx;
	const FVector2D Origin(S->MarginPx.X, Full.Y - S->MarginPx.Y - Size);
	const FVector2D Centre = Origin + FVector2D(Size * 0.5f);
	const FGeometry Box = Geometry.MakeChild(FVector2D(Size, Size), FSlateLayoutTransform(Origin));
	const FLinearColor Tint(1.f, 1.f, 1.f, S->Opacity);
	const MurdarGps::FV2 Me = V2(PlayerLoc);

	Out.PushClip(FSlateClippingZone(Box));

	// 1. Background (so the map's edge is not see-through), then the map region around him, rotated heading-up.
	FSlateDrawElement::MakeBox(Out, Layer, Box.ToPaintGeometry(), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.08f, 0.09f, 0.1f, S->Opacity));
	if (MapBrush.GetResourceObject())
	{
		const FVector2D W0 = P->WorldAtUV0, W1 = P->WorldAtUV1;
		const double SpanX = W1.X - W0.X, SpanY = W1.Y - W0.Y;
		if (SpanX != 0.0 && SpanY != 0.0)
		{
			// The square drawn is the minimap's diagonal wide, so rotation never shows its corners.
			const float DrawR = RadiusCm * 1.42f;
			const FVector2f U0((PlayerLoc.X - DrawR - W0.X) / SpanX, (PlayerLoc.Y - DrawR - W0.Y) / SpanY);
			const FVector2f U1((PlayerLoc.X + DrawR - W0.X) / SpanX, (PlayerLoc.Y + DrawR - W0.Y) / SpanY);
			FSlateBrush Region = MapBrush;
			Region.SetUVRegion(FBox2f(U0, U1));
			const float DrawPx = Size * 1.42f;
			const FGeometry MapGeo = Geometry.MakeChild(FVector2D(DrawPx, DrawPx), FSlateLayoutTransform(Centre - FVector2D(DrawPx * 0.5f)));
			// Heading up: world +X (texture right) must point up when Yaw = 0 -> rotate by -(90 + Yaw) degrees.
			// ADAPT (test MM-02): if the map turns the wrong way when you steer, flip the sign.
			const float AngleRad = FMath::DegreesToRadians(-(90.f + Yaw));
			FSlateDrawElement::MakeRotatedBox(Out, Layer + 1, MapGeo.ToPaintGeometry(), &Region, ESlateDrawEffect::None, AngleRad,
				TOptional<FVector2D>(), FSlateDrawElement::RelativeToElement, Tint);
		}
	}

	// World -> widget pixels (same maths the tests check).
	auto ToPx = [&](const MurdarGps::FV2& World) -> FVector2D
	{
		const MurdarGps::FV2 M = MurdarGps::ToMinimap(World, Me, Yaw, RadiusCm);
		return Centre + FVector2D(M.X, M.Y) * (Size * 0.5f);
	};

	// 2. Routes: your waypoint's under the mission's (both visible where they part).
	if (S->bRouteOnMinimap)
	{
		const EGpsPin Order[] = { EGpsPin::Waypoint, EGpsPin::Mission };
		for (EGpsPin K : Order)
		{
			const std::vector<MurdarGps::FV2> Pts = MurdarGps::VisibleRoute(G->Route(K), Me, RadiusCm);
			if (Pts.size() < 2) { continue; }
			TArray<FVector2D> Line;
			for (const MurdarGps::FV2& W : Pts) { Line.Add(ToPx(W)); }
			const FLinearColor C = K == EGpsPin::Mission ? S->MissionColor : S->WaypointColor;
			FSlateDrawElement::MakeLines(Out, Layer + 2, Geometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, C, true, S->RouteThicknessPx);
		}
	}

	// 3. Pins: a diamond; on the edge when outside.
	for (EGpsPin K : { EGpsPin::Waypoint, EGpsPin::Mission })
	{
		FVector Where; FText Label;
		if (!G->GetPin(K, Where, Label)) { continue; }
		const MurdarGps::FEdge E = MurdarGps::ClampToEdge(MurdarGps::ToMinimap(V2(Where), Me, Yaw, RadiusCm));
		const FVector2D At = Centre + FVector2D(E.At.X, E.At.Y) * (Size * 0.5f);
		const float R = E.bOnEdge ? 6.f : 8.f;
		const FLinearColor C = K == EGpsPin::Mission ? S->MissionColor : S->WaypointColor;
		const TArray<FVector2D> Diamond = { At + FVector2D(0, -R), At + FVector2D(R, 0), At + FVector2D(0, R), At + FVector2D(-R, 0), At + FVector2D(0, -R) };
		FSlateDrawElement::MakeLines(Out, Layer + 3, Geometry.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, C, true, 3.f);
	}

	// 4. Him: an arrow pointing up (heading-up map), and an N for north.
	{
		const float A = 9.f;
		const TArray<FVector2D> Arrow = { Centre + FVector2D(0, -A), Centre + FVector2D(A * 0.7f, A), Centre + FVector2D(0, A * 0.45f), Centre + FVector2D(-A * 0.7f, A), Centre + FVector2D(0, -A) };
		FSlateDrawElement::MakeLines(Out, Layer + 4, Geometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, S->PlayerColor, true, 2.5f);
		// "N" on the rim, towards world +X. ADAPT: point it at the real north of the Iași map once the drawing's
		// orientation is set (north = the direction the top of the map drawing faces in the world).
		const MurdarGps::FEdge N = MurdarGps::ClampToEdge(MurdarGps::ToMinimap({ Me.X + 1e6f, Me.Y }, Me, Yaw, RadiusCm), 0.86f);
		const FVector2D NAt = Centre + FVector2D(N.At.X, N.At.Y) * (Size * 0.5f) - FVector2D(5, 8);
		FSlateDrawElement::MakeText(Out, Layer + 4, Geometry.ToPaintGeometry(FVector2D(16, 16), FSlateLayoutTransform(NAt)), FText::FromString(TEXT("N")),
			FCoreStyle::GetDefaultFontStyle("Bold", 11), ESlateDrawEffect::None, FLinearColor::White);
	}

	Out.PopClip();

	// 5. A thin frame.
	const TArray<FVector2D> Frame = { Origin, Origin + FVector2D(Size, 0), Origin + FVector2D(Size, Size), Origin + FVector2D(0, Size), Origin };
	FSlateDrawElement::MakeLines(Out, Layer + 5, Geometry.ToPaintGeometry(), Frame, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.8f), true, 3.f);
	return Layer + 5;
}
