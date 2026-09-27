// AI quality — the rules that keep traffic, pedestrians and gunmen from getting stuck, deadlocking or standing in
// the open. Pure C++17, unit-tested (Handoff/AIQuality/Tests). Written after reading the real AI source (27 Sep):
//  - traffic waits for ever behind a person standing in the road or a body (only cars count as obstacles);
//  - a car that is "committed" to a junction and then stops inside it (queue, stranded, a row with the player) keeps
//    every crossing movement waiting - the junction locks up;
//  - a police car parked with its siren on near a junction stops that junction for good;
//  - nothing watches for "wants to move, has not moved" except the traffic's own nose-in-a-wall check, and foot NPCs
//    have no stuck check at all (they lean on a wall, or stand at an edge, for ever);
//  - stuck or stranded actors near the player are never recycled (only far ones are);
//  - every traffic car scans every car and every person in the world three times per think (O(N^2));
//  - gunmen strafe in the open; hiding and fleeing pick points that may be inside a wall or across the road.
// The Unreal side (AISpatialIndex, AIWatchdogSubsystem, CoverSubsystem, the README patches) only calls these.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace MurdarAIQ
{
	// -----------------------------------------------------------------------------------------------------------------
	// 1. Progress watchdog: "wants to move, has not moved"
	// -----------------------------------------------------------------------------------------------------------------

	struct FWatchTuning
	{
		float ProgressCm = 150.f;   // moving this far from the anchor = progress, the clock restarts
		float Level1Seconds = 4.f;  // nudge: back off / sidestep
		float Level2Seconds = 12.f; // break the rule that holds us: drive round, force a navmesh path, go at the junction
		float Level3Seconds = 30.f; // give up: stranded / new goal; recycled when nobody sees it
		float LegitWaitMaxSeconds = 90.f; // a red light, a queue: not stuck - unless it never ends
	};

	/**
	 * One per agent. Update() every think with where we are and whether we want to move. A legitimate wait (red light,
	 * a queue that itself waits legitimately, a scripted hold) does not count - until LegitWaitMaxSeconds, after which
	 * a wait that never ends is a deadlock like any other.
	 */
	struct FWatchdog
	{
		float AnchorX = 0.f, AnchorY = 0.f;
		double AnchorTime = 0.0;
		double StallStart = -1.0;   // when we started wanting to move without moving; -1 not stalled
		double LegitStart = -1.0;   // when the current legitimate wait began
		bool bInit = false;
		int Level = 0;
		int Escalations = 0;        // how many times Level went up (for the soak report)

		void Reset(double Now, float X, float Y)
		{
			AnchorX = X; AnchorY = Y; AnchorTime = Now; StallStart = -1.0; LegitStart = -1.0; Level = 0; bInit = true;
		}

		/** Returns the level (0..3). OutEscalated = it went up this call. */
		int Update(double Now, float X, float Y, bool bWantsToMove, bool bLegitWait, const FWatchTuning& T, bool& OutEscalated)
		{
			OutEscalated = false;
			if (!bInit) { Reset(Now, X, Y); } // and count from now if we already want to move
			const float Dx = X - AnchorX, Dy = Y - AnchorY;
			if (Dx * Dx + Dy * Dy > T.ProgressCm * T.ProgressCm)
			{
				Reset(Now, X, Y);
				return 0;
			}
			if (!bWantsToMove)
			{
				StallStart = -1.0; LegitStart = -1.0; Level = 0;
				return 0;
			}
			if (bLegitWait)
			{
				if (LegitStart < 0.0) { LegitStart = Now; }
				if (Now - LegitStart < T.LegitWaitMaxSeconds) { StallStart = -1.0; Level = 0; return 0; }
				// the wait never ended: counted from when it should have
				if (StallStart < 0.0) { StallStart = LegitStart + T.LegitWaitMaxSeconds; }
			}
			else
			{
				LegitStart = -1.0;
				if (StallStart < 0.0) { StallStart = Now; }
			}
			const double Stalled = Now - StallStart;
			const int NewLevel = Stalled >= T.Level3Seconds ? 3 : Stalled >= T.Level2Seconds ? 2 : Stalled >= T.Level1Seconds ? 1 : 0;
			if (NewLevel > Level) { OutEscalated = true; ++Escalations; }
			Level = NewLevel;
			return Level;
		}

		double StalledFor(double Now) const { return StallStart < 0.0 ? 0.0 : Now - StallStart; }
	};

	// -----------------------------------------------------------------------------------------------------------------
	// 2. Junctions
	// -----------------------------------------------------------------------------------------------------------------

	struct FJunctionOther
	{
		bool bCommitted = false;    // inside, or past the point where it could stop
		bool bObstacle = false;     // stranded, a row with the player, held (IsObstacle)
		float SpeedCms = 0.f;
		float StillSeconds = 0.f;   // how long it has stood (speed < 50 cm/s)
		int WatchLevel = 0;
	};

	/**
	 * Does a committed car on a crossing movement keep us at our line? Only while it is actually going through. One that
	 * has stopped in the box (stranded, a row, a queue past the junction, its own watchdog up) no longer reserves the
	 * junction: our own leader check still stops us if it is physically on our path. Before this, one stopped car
	 * locked every crossing movement for ever.
	 */
	inline bool CommittedBlocksEntry(const FJunctionOther& O, float StillLimitSeconds = 3.f)
	{
		if (!O.bCommitted) { return false; }
		if (O.bObstacle || O.WatchLevel >= 2) { return false; }
		if (O.SpeedCms < 50.f && O.StillSeconds > StillLimitSeconds) { return false; }
		return true;
	}

	/**
	 * A siren closes the junction only while it is coming: moving (> 15 km/h) and closing on the junction. A police car
	 * parked at a scene with its lights on used to stop the junction for good.
	 */
	inline bool SirenHoldsJunction(float DistCm, float SirenSpeedCms, float ClosingCms, float HoldRadiusCm)
	{
		return DistCm < HoldRadiusCm && SirenSpeedCms > 420.f && ClosingCms > 100.f;
	}

	/**
	 * Watchdog level 2 at a junction line with nobody actually moving through it: go (first come, first served already
	 * failed). Only when nothing crossing is moving and there is no red light.
	 */
	inline bool ForceJunction(int MyLevel, bool bRedLight, bool bAnyCrossingMoving)
	{
		return MyLevel >= 2 && !bRedLight && !bAnyCrossingMoving;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// 3. Something standing on our path (traffic)
	// -----------------------------------------------------------------------------------------------------------------

	enum class EObstacleKind
	{
		Queue,          // a traffic car that is driving (waiting at a light, in a queue)
		Car,            // a car nobody drives, a breakdown, a stranded one
		Person,         // a pedestrian standing in the road
		Player,         // the player on foot in the road
		Body            // dead, or down in ragdoll
	};

	enum class EObstacleAction { Follow, Honk, DriveRound };

	struct FObstacleTuning
	{
		float CarRoundSeconds = 4.f;          // (the existing BypassAfterSeconds)
		float CarRoundNearJunctionSeconds = 10.f;
		float PersonHonkSeconds = 2.f;
		float PersonRoundSeconds = 5.f;
		float PlayerHonkSeconds = 1.5f;
		float PlayerRoundSeconds = 8.f;
		float BodyRoundSeconds = 1.5f;
	};

	/**
	 * What to do about what stands ahead of us for StillSeconds. A queue is followed - unless the car at its head is
	 * itself stuck (its watchdog at 2+): then it is an obstacle like a breakdown. bNearJunction: within 25 m of a line or
	 * in the box - driving round there is allowed, but only after a longer wait.
	 */
	inline EObstacleAction ObstacleAction(EObstacleKind Kind, float StillSeconds, bool bNearJunction, int LeaderWatchLevel, const FObstacleTuning& T)
	{
		switch (Kind)
		{
		case EObstacleKind::Queue:
			if (LeaderWatchLevel < 2) { return EObstacleAction::Follow; }
			[[fallthrough]];
		case EObstacleKind::Car:
		{
			const float After = bNearJunction ? T.CarRoundNearJunctionSeconds : T.CarRoundSeconds;
			if (StillSeconds >= After) { return EObstacleAction::DriveRound; }
			return StillSeconds >= 3.f ? EObstacleAction::Honk : EObstacleAction::Follow;
		}
		case EObstacleKind::Person:
			if (StillSeconds >= T.PersonRoundSeconds) { return EObstacleAction::DriveRound; }
			return StillSeconds >= T.PersonHonkSeconds ? EObstacleAction::Honk : EObstacleAction::Follow;
		case EObstacleKind::Player:
			if (StillSeconds >= T.PlayerRoundSeconds) { return EObstacleAction::DriveRound; }
			return StillSeconds >= T.PlayerHonkSeconds ? EObstacleAction::Honk : EObstacleAction::Follow;
		case EObstacleKind::Body:
			return StillSeconds >= T.BodyRoundSeconds ? EObstacleAction::DriveRound : EObstacleAction::Follow;
		}
		return EObstacleAction::Follow;
	}

	/**
	 * Backing off whatever we are stuck on: steer so the nose swings back towards the lane. LateralCm is our offset from
	 * the lane centre (+ right). Reversing, the wheel works the other way round: right of the lane -> wheel right.
	 * Returns steer in -1..1. The old back-off went straight back and drove into the same thing again.
	 */
	inline float ReverseSteer(float LateralCm, float HeadingErrDeg)
	{
		// HeadingErrDeg: lane direction minus ours, + means the lane goes to our right.
		const float FromLat = std::clamp(LateralCm / 250.f, -1.f, 1.f);
		const float FromHead = std::clamp(-HeadingErrDeg / 30.f, -1.f, 1.f);
		return std::clamp(0.6f * FromLat + 0.6f * FromHead, -1.f, 1.f);
	}

	/** On its side or roof. */
	inline bool IsFlipped(float RollDeg, float PitchDeg) { return std::fabs(RollDeg) > 70.f || std::fabs(PitchDeg) > 70.f; }

	// -----------------------------------------------------------------------------------------------------------------
	// 4. On foot
	// -----------------------------------------------------------------------------------------------------------------

	enum class EFootFix { None, Sidestep, ForcePath, NewGoal };

	/** The foot watchdog's level -> what the controller does. Level 3 also reports: unseen, the actor is recycled. */
	inline EFootFix FootFix(int Level)
	{
		switch (Level)
		{
		case 1: return EFootFix::Sidestep;
		case 2: return EFootFix::ForcePath;
		case 3: return EFootFix::NewGoal;
		default: return EFootFix::None;
		}
	}

	/** Sidestep side for attempt N: right, left, right... (keep right first, as people do). */
	inline int SidestepSide(int Attempt) { return (Attempt % 2 == 0) ? 1 : -1; }

	/**
	 * Two people walking at each other on a pavement: both keep right. Everything in the walker's frame: Fwd is our
	 * direction (unit), the other is at (RelX forward, RelY right) and walks with OtherFwdDot = dot(its dir, ours).
	 * Returns how far (cm) to shift right this step; 0 when there is nothing to do. Also used for the player (who does
	 * not move aside): then we take the whole gap.
	 */
	inline float PassOffset(float RelX, float RelY, float OtherFwdDot, bool bOtherYields)
	{
		if (RelX < 40.f || RelX > 450.f) { return 0.f; }      // behind us or far
		const float Needed = 90.f;                             // shoulder to shoulder
		if (std::fabs(RelY) >= Needed) { return 0.f; }
		const bool bOncoming = OtherFwdDot < -0.3f;
		const bool bStanding = OtherFwdDot > -0.3f && OtherFwdDot < 0.3f;
		if (!bOncoming && !bStanding) { return 0.f; }          // walking our way: we overtake only by the normal path
		// Pass on its left side (we go right) unless it already stands to our right: then go left of it.
		const float Share = (bOncoming && bOtherYields) ? 0.5f : 1.f;
		const float Want = Needed - std::fabs(RelY);
		const float Side = RelY > 25.f ? -1.f : 1.f;
		return Side * Want * Share;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// 5. Where to run, where to hide, where to take cover
	// -----------------------------------------------------------------------------------------------------------------

	struct FSpot
	{
		bool bOnNavmesh = false;
		bool bReachable = false;   // a complete navmesh path exists
		float PathCm = 0.f;        // its length
		float StraightCm = 0.f;    // straight-line distance to it
		bool bOnRoad = false;      // on a traffic lane
		bool bHidden = false;      // the threat cannot see chest height there
		bool bCanPeek = false;     // a step to the side (or standing up) sees the threat
		float ThreatDistCm = 0.f;  // from the threat
		float AwayDot = 0.f;       // dot(direction to the spot, direction away from the threat), -1..1
		float AllyDistCm = 1e9f;   // to the nearest spot another NPC has claimed
	};

	/** A detour longer than this (x straight + slack) is not "over there", it is round the block. */
	inline bool PathSensible(const FSpot& S) { return S.bOnNavmesh && S.bReachable && S.PathCm <= S.StraightCm * 2.5f + 500.f; }

	/** Fleeing: away from the threat, reachable, not into the road. <0 = rejected. */
	inline float ScoreFlee(const FSpot& S)
	{
		if (!PathSensible(S) || S.AwayDot < 0.2f) { return -1.f; }
		return 1000.f * S.AwayDot + S.ThreatDistCm * 0.5f - S.PathCm * 0.2f - (S.bOnRoad ? 1500.f : 0.f) + (S.bHidden ? 800.f : 0.f);
	}

	/** Hiding (out of ammo, hurt): out of sight first, then far from him, then close to us. <0 = rejected. */
	inline float ScoreHide(const FSpot& S)
	{
		if (!PathSensible(S)) { return -1.f; }
		return (S.bHidden ? 10000.f : 0.f) + S.ThreatDistCm - 0.5f * S.PathCm - (S.bOnRoad ? 3000.f : 0.f);
	}

	struct FCoverTuning
	{
		float PreferredRangeCm = 1100.f;
		float AllySpacingCm = 300.f;
		float MaxPathCm = 1500.f;   // not across the map to a better wall
	};

	/** Cover for a fight: hidden at chest height, able to peek, near our range, not on top of a mate. <0 = rejected. */
	inline float ScoreCover(const FSpot& S, const FCoverTuning& T)
	{
		if (!PathSensible(S) || !S.bHidden || S.PathCm > T.MaxPathCm) { return -1.f; }
		float Score = 5000.f;
		if (S.bCanPeek) { Score += 4000.f; }
		Score -= std::fabs(S.ThreatDistCm - T.PreferredRangeCm) * 1.5f;
		Score -= S.PathCm;
		if (S.AllyDistCm < T.AllySpacingCm) { Score -= 4000.f; }
		if (S.bOnRoad) { Score -= 1500.f; }
		return Score;
	}

	/** Best index by a score function; -1 when all are rejected. */
	template <class F>
	inline int PickBest(const std::vector<FSpot>& Spots, F&& Score)
	{
		int Best = -1;
		float BestScore = 0.f;
		for (int i = 0; i < static_cast<int>(Spots.size()); ++i)
		{
			const float Sc = Score(Spots[i]);
			if (Sc < 0.f) { continue; }
			if (Best < 0 || Sc > BestScore) { Best = i; BestScore = Sc; }
		}
		return Best;
	}

	/**
	 * In cover: hide, peek and shoot a burst, hide again. Moves on when flanked (seen in cover) or after a while in the
	 * same place (so a fight moves). Phase durations are the NPC's own burst/pause timings.
	 */
	enum class ECoverPhase { Moving, Hidden, Peek, Leave };

	struct FCoverCycle
	{
		ECoverPhase Phase = ECoverPhase::Moving;
		double PhaseStart = 0.0;
		double ArrivedAt = -1.0;

		void Start(double Now) { Phase = ECoverPhase::Moving; PhaseStart = Now; ArrivedAt = -1.0; }

		ECoverPhase Update(double Now, bool bAtSpot, bool bSeenInCover, bool bReloading, float HideSeconds, float PeekSeconds, float MaxStaySeconds)
		{
			switch (Phase)
			{
			case ECoverPhase::Moving:
				if (bAtSpot) { Phase = ECoverPhase::Hidden; PhaseStart = Now; ArrivedAt = Now; }
				break;
			case ECoverPhase::Hidden:
				if (bSeenInCover || (ArrivedAt >= 0.0 && Now - ArrivedAt > MaxStaySeconds)) { Phase = ECoverPhase::Leave; PhaseStart = Now; }
				else if (!bReloading && Now - PhaseStart >= HideSeconds) { Phase = ECoverPhase::Peek; PhaseStart = Now; }
				break;
			case ECoverPhase::Peek:
				if (bReloading || Now - PhaseStart >= PeekSeconds) { Phase = ECoverPhase::Hidden; PhaseStart = Now; }
				break;
			case ECoverPhase::Leave:
				break;
			}
			return Phase;
		}
	};

	// -----------------------------------------------------------------------------------------------------------------
	// 6. Recycling the stuck
	// -----------------------------------------------------------------------------------------------------------------

	enum class ERecycle { Keep, Remove };

	struct FRecycleInput
	{
		int WatchLevel = 0;
		bool bStranded = false;        // traffic's own "gave up" (stranded, flipped, wrecked and left)
		bool bVisible = false;         // on screen for the player
		float DistCm = 0.f;            // from the player
		bool bInChase = false;         // a police unit in anything but Patrol / Disabled / StandDown
		bool bPlayerOwned = false;     // his car, his passenger
		bool bMission = false;         // tagged by a mission / job / street event
	};

	/** Stuck for good, or stranded, and nobody is watching: take it away (the population puts a fresh one elsewhere). */
	inline ERecycle Recycle(const FRecycleInput& I, float MinDistCm = 1500.f)
	{
		if (I.bPlayerOwned || I.bMission || I.bInChase) { return ERecycle::Keep; }
		if (!(I.WatchLevel >= 3 || I.bStranded)) { return ERecycle::Keep; }
		if (I.bVisible || I.DistCm < MinDistCm) { return ERecycle::Keep; }
		return ERecycle::Remove;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// 7. Spatial hash: "who is near here" without scanning the world
	// -----------------------------------------------------------------------------------------------------------------

	/**
	 * A 2D grid rebuilt ten times a second from one pass over the world. Stores ids (indices into the caller's list).
	 * Buckets are kept between rebuilds (cleared, not freed), so a rebuild allocates nothing once warm.
	 */
	class FSpatialHash2D
	{
	public:
		explicit FSpatialHash2D(float InCellCm = 2000.f) : CellCm(InCellCm) {}

		static int64_t Key(int32_t Ix, int32_t Iy) { return (static_cast<int64_t>(Ix) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(Iy)); }
		int32_t CellOf(float V) const { return static_cast<int32_t>(std::floor(V / CellCm)); }

		void Clear()
		{
			for (auto& KV : Buckets) { KV.second.clear(); }
			Count = 0;
		}

		void Insert(int Id, float X, float Y)
		{
			Buckets[Key(CellOf(X), CellOf(Y))].push_back(FEntry{ Id, X, Y });
			++Count;
		}

		/** Ids within RadiusCm of (X, Y) (by their inserted position). */
		void Query(float X, float Y, float RadiusCm, std::vector<int>& Out) const
		{
			Out.clear();
			const int32_t X0 = CellOf(X - RadiusCm), X1 = CellOf(X + RadiusCm);
			const int32_t Y0 = CellOf(Y - RadiusCm), Y1 = CellOf(Y + RadiusCm);
			const float R2 = RadiusCm * RadiusCm;
			for (int32_t Ix = X0; Ix <= X1; ++Ix)
			{
				for (int32_t Iy = Y0; Iy <= Y1; ++Iy)
				{
					const auto It = Buckets.find(Key(Ix, Iy));
					if (It == Buckets.end()) { continue; }
					for (const FEntry& E : It->second)
					{
						const float Dx = E.X - X, Dy = E.Y - Y;
						if (Dx * Dx + Dy * Dy <= R2) { Out.push_back(E.Id); }
					}
				}
			}
		}

		int Num() const { return Count; }
		size_t NumBuckets() const { return Buckets.size(); }

	private:
		struct FEntry { int Id; float X; float Y; };
		float CellCm;
		int Count = 0;
		std::unordered_map<int64_t, std::vector<FEntry>> Buckets;
	};

	// -----------------------------------------------------------------------------------------------------------------
	// 8. Soak test: measure it, don't guess
	// -----------------------------------------------------------------------------------------------------------------

	enum class EAgent { Traffic = 0, Pedestrian = 1, Police = 2, Gunman = 3 };
	constexpr int NumAgentKinds = 4;

	inline const char* AgentName(EAgent K)
	{
		switch (K)
		{
		case EAgent::Traffic: return "traffic";
		case EAgent::Pedestrian: return "pedestrian";
		case EAgent::Police: return "police";
		case EAgent::Gunman: return "gunman";
		}
		return "?";
	}

	/** A junction with this many cars at watchdog level 2+ is a gridlock. */
	inline bool IsGridlock(const std::vector<int>& LevelsAtJunction, int MinCars = 3)
	{
		int N = 0;
		for (int L : LevelsAtJunction) { if (L >= 2) { ++N; } }
		return N >= MinCars;
	}

	struct FSoakStats
	{
		int Escalations[NumAgentKinds][4] = {};  // [kind][level 1..3], index 0 unused
		int Recycled[NumAgentKinds] = {};
		int Gridlocks = 0;
		double Seconds = 0.0;

		void Add(EAgent K, int Level) { if (Level >= 1 && Level <= 3) { ++Escalations[static_cast<int>(K)][Level]; } }

		/** Level-3 events of one kind per 10 minutes. */
		double Level3Per10Min(EAgent K) const
		{
			return Seconds > 0.0 ? Escalations[static_cast<int>(K)][3] * 600.0 / Seconds : 0.0;
		}

		/**
		 * Pass: no gridlock at all, and at most MaxL3Per10Min give-ups per kind per 10 minutes (a few are fine: a car
		 * really wedged in a ditch). The numbers to beat are written into Docs/AI_SOAK_REPORT.md from the BEFORE run.
		 */
		bool Passes(double MaxL3Per10Min = 2.0) const
		{
			if (Gridlocks > 0) { return false; }
			for (int k = 0; k < NumAgentKinds; ++k)
			{
				if (Level3Per10Min(static_cast<EAgent>(k)) > MaxL3Per10Min) { return false; }
			}
			return true;
		}

		std::string Summary() const
		{
			std::string S = "soak " + std::to_string(static_cast<int>(Seconds)) + " s | gridlocks " + std::to_string(Gridlocks);
			for (int k = 0; k < NumAgentKinds; ++k)
			{
				S += " | ";
				S += AgentName(static_cast<EAgent>(k));
				S += " L1 " + std::to_string(Escalations[k][1]) + " L2 " + std::to_string(Escalations[k][2]) + " L3 " + std::to_string(Escalations[k][3]);
				S += " recycled " + std::to_string(Recycled[k]);
			}
			S += Passes() ? " | PASS" : " | FAIL";
			return S;
		}
	};
}
