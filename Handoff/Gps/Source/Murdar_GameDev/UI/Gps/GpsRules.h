// GPS — pins, routes along the road lanes, the minimap and the big map in the pause menu (GTA-style). Pure C++17,
// unit-tested (Handoff/Gps/Tests). The user asked for it (30 Sep): a minimap with the route drawn on it, the big map
// moved into the pause menu, pins you place yourself, and a mission pin and your own pin routed AT THE SAME TIME.
// This overrides the PaperMap handoff's "no minimap in play" line; the minimap has its own switch (off = the old,
// map-only way to play).
//
// The routes come from the road layer that already exists: MurdarRoad::RouteAlongLanes (ZoneGraph lane A*) - the same
// router the police use. On foot: the Human navmesh (MurdarNav::FindPath), or a straight line.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace MurdarGps
{
	struct FV2 { float X = 0.f, Y = 0.f; };

	inline float Dist(const FV2& A, const FV2& B) { return std::hypot(A.X - B.X, A.Y - B.Y); }

	// -----------------------------------------------------------------------------------------------------------------
	// Pins
	// -----------------------------------------------------------------------------------------------------------------

	/** Mission = the story mission or the job's current stage (yellow). Waypoint = the one you place (purple).
	 *  Known = safehouses, garages... shown, never routed unless you pick them. */
	enum class EPin { Mission = 0, Waypoint = 1 };
	constexpr int NumRouted = 2;

	struct FPin
	{
		bool bSet = false;
		FV2 At;
	};

	struct FPins
	{
		FPin Slot[NumRouted];

		const FPin& Get(EPin K) const { return Slot[static_cast<int>(K)]; }
		void Set(EPin K, const FV2& At) { Slot[static_cast<int>(K)] = FPin{ true, At }; }
		void Clear(EPin K) { Slot[static_cast<int>(K)] = FPin{}; }

		/**
		 * The map's "place / remove" button: pressing near your existing waypoint removes it (GTA), anywhere else moves
		 * it there. Returns true when a waypoint is set afterwards.
		 */
		bool ToggleWaypoint(const FV2& At, float RemoveRadiusCm)
		{
			FPin& W = Slot[static_cast<int>(EPin::Waypoint)];
			if (W.bSet && Dist(W.At, At) <= RemoveRadiusCm) { W = FPin{}; return false; }
			W = FPin{ true, At };
			return true;
		}
	};

	/** Your waypoint clears itself when you get there; the mission pin is the mission's to clear. */
	inline bool WaypointReached(const FPins& P, const FV2& Player, float ArriveCm)
	{
		const FPin& W = P.Get(EPin::Waypoint);
		return W.bSet && Dist(W.At, Player) <= ArriveCm;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Following a route
	// -----------------------------------------------------------------------------------------------------------------

	/** Closest point of polyline Pts to P, searched from segment From onward (a route is walked forwards). */
	struct FProjection
	{
		int Segment = 0;       // index of the segment's first point
		float T = 0.f;         // 0..1 along it
		float OffCm = 0.f;     // distance from P to the line
		float RemainingCm = 0.f;
	};

	inline FProjection Project(const std::vector<FV2>& Pts, const FV2& P, int From = 0, int Window = 40)
	{
		FProjection Best;
		Best.OffCm = 1e30f;
		if (Pts.size() < 2) { Best.OffCm = Pts.empty() ? 0.f : Dist(Pts[0], P); return Best; }
		const int Last = static_cast<int>(Pts.size()) - 2;
		const int S0 = std::clamp(From, 0, Last), S1 = std::min(Last, S0 + Window);
		for (int i = S0; i <= S1; ++i)
		{
			const FV2 A = Pts[i], B = Pts[i + 1];
			const float Dx = B.X - A.X, Dy = B.Y - A.Y;
			const float L2 = Dx * Dx + Dy * Dy;
			const float T = L2 > 0.f ? std::clamp(((P.X - A.X) * Dx + (P.Y - A.Y) * Dy) / L2, 0.f, 1.f) : 0.f;
			const FV2 Q{ A.X + Dx * T, A.Y + Dy * T };
			const float D = Dist(Q, P);
			if (D < Best.OffCm) { Best.OffCm = D; Best.Segment = i; Best.T = T; }
		}
		float Rem = Dist(Pts[Best.Segment], Pts[Best.Segment + 1]) * (1.f - Best.T);
		for (int i = Best.Segment + 1; i <= Last; ++i) { Rem += Dist(Pts[i], Pts[i + 1]); }
		Best.RemainingCm = Rem;
		return Best;
	}

	struct FRouteTuning
	{
		float OffRouteCm = 3000.f;        // farther than this from the line...
		float OffRouteSeconds = 1.5f;     // ...for this long: re-route (a wide turn is not leaving the route)
		float TargetMovedCm = 5000.f;     // the pin moved (a mission stage, a chase target): re-route
		float MinRerouteSeconds = 2.f;    // never more often (the lane A* is not free)
		float PeriodicSeconds = 20.f;     // and once in a while anyway (traffic, a new better road)
	};

	/** One route (mission or waypoint): what it follows, where the player is on it, when to ask again. */
	struct FRoute
	{
		std::vector<FV2> Pts;
		FV2 Target;
		bool bValid = false;
		int Progress = 0;          // segment index reached (drawing starts here)
		float OffSince = -1.f;     // when we left the line; -1 on it
		float PlannedAt = -1e9f;
		float RemainingCm = 0.f;

		void Adopt(const std::vector<FV2>& NewPts, const FV2& NewTarget, float Now)
		{
			Pts = NewPts; Target = NewTarget; bValid = Pts.size() >= 2; Progress = 0; OffSince = -1.f; PlannedAt = Now;
			RemainingCm = 0.f;
			for (size_t i = 0; i + 1 < Pts.size(); ++i) { RemainingCm += Dist(Pts[i], Pts[i + 1]); }
		}

		void Clear() { Pts.clear(); bValid = false; Progress = 0; OffSince = -1.f; RemainingCm = 0.f; }

		/** Update with the player's position; returns true when a re-plan is due. */
		bool Update(const FV2& Player, const FV2& CurrentTarget, float Now, const FRouteTuning& T)
		{
			const bool bCanPlan = Now - PlannedAt >= T.MinRerouteSeconds;
			if (!bValid) { return bCanPlan; }
			if (Dist(CurrentTarget, Target) > T.TargetMovedCm) { return bCanPlan; }
			const FProjection P = Project(Pts, Player, Progress);
			Progress = std::max(Progress, P.Segment);
			RemainingCm = P.RemainingCm;
			if (P.OffCm > T.OffRouteCm)
			{
				if (OffSince < 0.f) { OffSince = Now; }
				if (Now - OffSince >= T.OffRouteSeconds) { return bCanPlan; }
			}
			else { OffSince = -1.f; }
			return bCanPlan && Now - PlannedAt >= T.PeriodicSeconds;
		}
	};

	// -----------------------------------------------------------------------------------------------------------------
	// Minimap
	// -----------------------------------------------------------------------------------------------------------------

	struct FMinimapTuning
	{
		float RadiusAtRestCm = 9000.f;     // 90 m around you standing / walking
		float RadiusAtSpeedCm = 25000.f;   // 250 m at FullSpeedKph (you need to see the next junction coming)
		float FullSpeedKph = 110.f;
		float ZoomSmoothing = 1.5f;        // 1/s
	};

	inline float TargetRadius(float Kph, const FMinimapTuning& T)
	{
		const float A = std::clamp(Kph / std::max(T.FullSpeedKph, 1.f), 0.f, 1.f);
		return T.RadiusAtRestCm + (T.RadiusAtSpeedCm - T.RadiusAtRestCm) * A;
	}

	inline float SmoothRadius(float Current, float Target, float Dt, const FMinimapTuning& T)
	{
		const float K = 1.f - std::exp(-T.ZoomSmoothing * std::max(Dt, 0.f));
		return Current + (Target - Current) * K;
	}

	/**
	 * World -> minimap, heading-up: the player at the centre, his heading pointing up. Result in -1..1 per axis
	 * (x right, y down, 1 = the minimap's edge at RadiusCm). YawDeg is Unreal's yaw (0 = +X, +90 = +Y).
	 */
	inline FV2 ToMinimap(const FV2& World, const FV2& Player, float YawDeg, float RadiusCm)
	{
		const float Dx = World.X - Player.X, Dy = World.Y - Player.Y;
		const float R = YawDeg * 3.14159265f / 180.f;
		const float C = std::cos(R), S = std::sin(R);
		// Forward component (along heading) and right component (Unreal: right of +X is +Y).
		const float Fwd = Dx * C + Dy * S;
		const float Right = -Dx * S + Dy * C;
		return { Right / RadiusCm, -Fwd / RadiusCm };
	}

	/** A pin beyond the edge sits on the edge (square minimap), pointing its way. */
	struct FEdge { FV2 At; bool bOnEdge = false; };

	inline FEdge ClampToEdge(const FV2& M, float Inset = 0.92f)
	{
		const float Mx = std::max(std::fabs(M.X), std::fabs(M.Y));
		if (Mx <= Inset) { return { M, false }; }
		const float K = Inset / Mx;
		return { { M.X * K, M.Y * K }, true };
	}

	/** Route points to draw: from the player's progress on, only those that can appear on the minimap. */
	inline std::vector<FV2> VisibleRoute(const FRoute& R, const FV2& Player, float RadiusCm)
	{
		std::vector<FV2> Out;
		if (!R.bValid) { return Out; }
		const float Reach = RadiusCm * 1.5f; // the square's corners, plus a segment crossing in from outside
		Out.push_back(Player);
		const int Start = std::clamp(R.Progress + 1, 0, static_cast<int>(R.Pts.size()) - 1);
		for (int i = Start; i < static_cast<int>(R.Pts.size()); ++i)
		{
			Out.push_back(R.Pts[i]);
			if (Dist(R.Pts[i], Player) > Reach) { break; } // the first point outside ends the visible part
		}
		return Out;
	}
}
