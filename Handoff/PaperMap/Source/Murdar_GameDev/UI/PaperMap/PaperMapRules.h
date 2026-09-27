// Paper map — the city on paper, in the pause menu, with the places he knows. Pure C++17, unit-tested
// (Handoff/PaperMap/Tests). No minimap in play ("felt, not shown"): this is the man unfolding a map.
//   World <-> paper: the map texture covers a world rectangle (authored corners).
//   View: zoom around a point, pan, never showing past the paper's edge.
//   Markers: shown only once known (a fact), the job's places while it runs, and him (a pencil cross).

#pragma once

#include <algorithm>
#include <cmath>

namespace MurdarMap
{
	struct FV2 { float X = 0.f, Y = 0.f; };

	/** World rectangle the paper covers. The texture's U grows with world X from Min to Max, V with world Y
	 *  (Unreal's top view: +X up the screen is a rotation — ADAPT by swapping in the settings, not here). */
	struct FBounds { FV2 Min, Max; };

	inline FV2 WorldToUV(const FV2& W, const FBounds& B)
	{
		const float Sx = B.Max.X - B.Min.X, Sy = B.Max.Y - B.Min.Y;
		return { Sx != 0.f ? (W.X - B.Min.X) / Sx : 0.f, Sy != 0.f ? (W.Y - B.Min.Y) / Sy : 0.f };
	}

	inline FV2 UVToWorld(const FV2& UV, const FBounds& B)
	{
		return { B.Min.X + UV.X * (B.Max.X - B.Min.X), B.Min.Y + UV.Y * (B.Max.Y - B.Min.Y) };
	}

	inline bool OnPaper(const FV2& UV) { return UV.X >= 0.f && UV.X <= 1.f && UV.Y >= 0.f && UV.Y <= 1.f; }

	/** The view: Zoom 1 = the whole paper fits the viewport's shorter side; Center = the paper point in the middle. */
	struct FView
	{
		float Zoom = 1.f;
		FV2 Center{ 0.5f, 0.5f };
	};

	struct FViewLimits { float MinZoom = 1.f, MaxZoom = 6.f; };

	/** Keep the view on the paper: at zoom z the visible half-width is 0.5/z of the paper (per axis, given aspect). */
	inline FView Clamp(FView V, float Aspect, const FViewLimits& L)
	{
		V.Zoom = std::clamp(V.Zoom, L.MinZoom, L.MaxZoom);
		// Paper is square in UV; the viewport's shorter side shows 1/z of it, the longer side Aspect/z (Aspect >= 1).
		const float A = std::max(Aspect, 1e-3f);
		const float HalfX = 0.5f * (A >= 1.f ? A : 1.f) / V.Zoom;
		const float HalfY = 0.5f * (A >= 1.f ? 1.f : 1.f / A) / V.Zoom;
		auto Axis = [](float C, float Half) { return Half >= 0.5f ? 0.5f : std::clamp(C, Half, 1.f - Half); };
		V.Center.X = Axis(V.Center.X, HalfX);
		V.Center.Y = Axis(V.Center.Y, HalfY);
		return V;
	}

	/** Zoom by Factor keeping the paper point under Cursor (viewport 0..1) where it is. */
	inline FView ZoomAt(FView V, float Factor, const FV2& Cursor01, float Aspect, const FViewLimits& L)
	{
		const float A = std::max(Aspect, 1e-3f);
		const float SpanX = (A >= 1.f ? A : 1.f) / V.Zoom, SpanY = (A >= 1.f ? 1.f : 1.f / A) / V.Zoom;
		const FV2 Under{ V.Center.X + (Cursor01.X - 0.5f) * SpanX, V.Center.Y + (Cursor01.Y - 0.5f) * SpanY };
		FView N = V;
		N.Zoom = std::clamp(V.Zoom * Factor, L.MinZoom, L.MaxZoom);
		const float NSpanX = (A >= 1.f ? A : 1.f) / N.Zoom, NSpanY = (A >= 1.f ? 1.f : 1.f / A) / N.Zoom;
		N.Center = { Under.X - (Cursor01.X - 0.5f) * NSpanX, Under.Y - (Cursor01.Y - 0.5f) * NSpanY };
		return Clamp(N, Aspect, L);
	}

	/** Paper UV -> viewport 0..1 under this view (may fall outside 0..1 = off screen). */
	inline FV2 UVToViewport(const FV2& UV, const FView& V, float Aspect)
	{
		const float A = std::max(Aspect, 1e-3f);
		const float SpanX = (A >= 1.f ? A : 1.f) / V.Zoom, SpanY = (A >= 1.f ? 1.f : 1.f / A) / V.Zoom;
		return { 0.5f + (UV.X - V.Center.X) / SpanX, 0.5f + (UV.Y - V.Center.Y) / SpanY };
	}

	enum class EMarker { Safehouse, Garage, Payphone, Contact, JobPickup, JobDrop, Player, Custom };

	struct FMarkerState
	{
		bool bKnown = true;     // its fact is set (or it needs none)
		bool bJobActive = false; // for job markers: shown only while the job runs and that stage is current
	};

	inline bool ShowMarker(EMarker K, const FMarkerState& S)
	{
		if (K == EMarker::Player) { return true; }
		if (K == EMarker::JobPickup || K == EMarker::JobDrop) { return S.bJobActive; }
		return S.bKnown;
	}

	/** Open the map centred on him, zoomed in a little (where he is matters most). */
	inline FView OpenAt(const FV2& PlayerUV, float Aspect, const FViewLimits& L, float OpenZoom = 2.f)
	{
		FView V; V.Zoom = OpenZoom; V.Center = PlayerUV;
		return Clamp(V, Aspect, L);
	}
}
