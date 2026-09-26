// AI LOD — who deserves CPU this frame. Pure C++17, unit-tested (Handoff/AILod/Tests).
//
// Every pedestrian ticks its brain at 20 Hz and every car its signals at 20 Hz today, whether it is next to the player
// or 300 m behind a block. Significance ranks them (near + in view first; anyone in a fight or a chase, or tagged for
// a mission, always on top), fills a budget per tier, and turns the tier into tick intervals. Promotion is immediate
// (something walks into view: full rate now); demotion waits a few passes, so an agent at a tier boundary doesn't flip
// every quarter second.
//
// Also: the streaming radius of the player's car, which grows with speed (World Partition), with hysteresis.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace MurdarLod
{
	enum class ETier { Full = 0, Reduced = 1, Minimal = 2, Dormant = 3 };

	struct FAgent
	{
		float DistanceCm = 0.f;
		bool bInView = false;   // inside the camera cone (not occlusion — cheap on purpose)
		bool bEngaged = false;  // chasing, fighting, arresting: never reduced
		bool bPinned = false;   // a mission / story actor
	};

	struct FBudget
	{
		int FullMax = 24;                 // at most this many at full rate
		int ReducedMax = 60;
		float FullMaxDistanceCm = 6000.f; // full rate only this close...
		float ReducedMaxDistanceCm = 15000.f;
		float DormantDistanceCm = 30000.f; // beyond this and out of view: dormant
		float OutOfViewWeight = 0.35f;    // behind the camera counts as this much "closer"
		int DemotePasses = 3;             // a lower tier must hold this many passes before it applies
	};

	/** Higher = more deserving. Engaged / pinned are above anything distance can give. */
	inline float Score(const FAgent& A, const FBudget& B)
	{
		if (A.bEngaged || A.bPinned) { return 1000.f; }
		const float Near = 1.f / (1.f + std::max(0.f, A.DistanceCm) / 2000.f);
		return Near * (A.bInView ? 1.f : B.OutOfViewWeight);
	}

	/** Tier for each agent this pass (no hysteresis). */
	inline std::vector<ETier> AssignTiers(const std::vector<FAgent>& Agents, const FBudget& B)
	{
		const int N = int(Agents.size());
		std::vector<int> Order(N);
		for (int i = 0; i < N; ++i) { Order[i] = i; }
		std::stable_sort(Order.begin(), Order.end(), [&](int L, int R) { return Score(Agents[L], B) > Score(Agents[R], B); });

		std::vector<ETier> Out(N, ETier::Minimal);
		int Full = 0, Reduced = 0;
		for (int i : Order)
		{
			const FAgent& A = Agents[i];
			if (A.bEngaged || A.bPinned) { Out[i] = ETier::Full; continue; } // outside the budget: a chase never starves the street
			if (!A.bInView && A.DistanceCm > B.DormantDistanceCm) { Out[i] = ETier::Dormant; continue; }
			if (Full < B.FullMax && A.DistanceCm <= B.FullMaxDistanceCm) { Out[i] = ETier::Full; ++Full; continue; }
			if (Reduced < B.ReducedMax && A.DistanceCm <= B.ReducedMaxDistanceCm) { Out[i] = ETier::Reduced; ++Reduced; continue; }
			Out[i] = ETier::Minimal;
		}
		return Out;
	}

	/** Per agent: up at once, down after DemotePasses consecutive passes asking for a lower tier. */
	struct FTierState
	{
		ETier Current = ETier::Full;
		int LowerPasses = 0;

		/** True when Current changed. */
		bool Update(ETier Wanted, int DemotePasses)
		{
			if (int(Wanted) < int(Current)) { Current = Wanted; LowerPasses = 0; return true; }
			if (Wanted == Current) { LowerPasses = 0; return false; }
			if (++LowerPasses >= DemotePasses) { Current = Wanted; LowerPasses = 0; return true; }
			return false;
		}
	};

	enum class EKind { Pedestrian, Car };

	struct FIntervals
	{
		float Brain = 0.f;     // pedestrian component / NPC think (s); 0 = every frame
		float Movement = 0.f;  // character movement
		float Signals = 0.f;   // car lights, indicators
		bool bAnimOnlyWhenRendered = false;
		bool bDriverReduced = false; // traffic driver at a slower rate (only far and out of view)
	};

	inline FIntervals Intervals(ETier T, EKind K)
	{
		FIntervals I;
		switch (T)
		{
		case ETier::Full:    I.Brain = 0.05f; I.Movement = 0.f;   I.Signals = 0.05f; break;
		case ETier::Reduced: I.Brain = 0.15f; I.Movement = 0.f;   I.Signals = 0.1f;  I.bAnimOnlyWhenRendered = true; break;
		case ETier::Minimal: I.Brain = 0.4f;  I.Movement = 0.05f; I.Signals = 0.25f; I.bAnimOnlyWhenRendered = true; break;
		case ETier::Dormant: I.Brain = 1.f;   I.Movement = 0.1f;  I.Signals = 1.f;   I.bAnimOnlyWhenRendered = true; I.bDriverReduced = true; break;
		}
		if (K == EKind::Car) { I.Movement = 0.f; } // physics cars: never throttle the simulation
		return I;
	}

	// ------------------------------------------------------------ streaming

	struct FStreamingTuning
	{
		float BaseRadiusCm = 25000.f;   // on foot / slow
		float MaxRadiusCm = 60000.f;    // flat out
		float FullAtKph = 140.f;
		float StepCm = 5000.f;          // radius changes in steps (each change re-evaluates streaming)
		float HysteresisCm = 2500.f;    // and only when the target is this far past the current step
	};

	/** Radius for a speed, quantised to steps; changes only when the target leaves the current step by the hysteresis. */
	inline float StreamingRadius(float CurrentRadius, float Kph, const FStreamingTuning& T)
	{
		const float A = std::clamp(Kph / std::max(1.f, T.FullAtKph), 0.f, 1.f);
		const float Target = T.BaseRadiusCm + (T.MaxRadiusCm - T.BaseRadiusCm) * A;
		if (std::fabs(Target - CurrentRadius) < T.StepCm * 0.5f + T.HysteresisCm) { return CurrentRadius; }
		const float Stepped = T.BaseRadiusCm + std::round((Target - T.BaseRadiusCm) / T.StepCm) * T.StepCm;
		return std::clamp(Stepped, T.BaseRadiusCm, T.MaxRadiusCm);
	}
}
