// Interaction — which thing gets the "E". Pure C++17, unit-tested (Handoff/Interaction/Tests).
// A candidate is in play when it is within its range and within its angle of where the player faces; among those,
// the best is the one he is most clearly facing and closest to, with an author's priority as a tie-breaker weight.

#pragma once

#include <cmath>
#include <vector>

namespace MurdarInteract
{
	struct FCandidate
	{
		int Id = -1;
		float DistanceCm = 0.f;
		float AngleDeg = 0.f;      // between the player's facing and the direction to the thing, 0..180
		float RangeCm = 150.f;
		float MaxAngleDeg = 60.f;
		float Priority = 1.f;      // author weight: a phone beats the bench next to it
		bool bAvailable = true;    // conditions (facts, once-only, busy) already checked by the caller
	};

	/** < 0 = not in play. Otherwise higher is better: facing counts more than distance (you use what you look at). */
	inline float Score(const FCandidate& C)
	{
		if (!C.bAvailable || C.DistanceCm > C.RangeCm || C.AngleDeg > C.MaxAngleDeg || C.RangeCm <= 0.f)
		{
			return -1.f;
		}
		const float Near = 1.f - C.DistanceCm / C.RangeCm;                           // 1 at the thing, 0 at the range
		const float Facing = C.MaxAngleDeg > 0.f ? 1.f - C.AngleDeg / C.MaxAngleDeg : 1.f; // 1 dead ahead
		return (0.4f * Near + 0.6f * Facing) * (C.Priority > 0.f ? C.Priority : 0.f);
	}

	/** Index of the best candidate, or -1. Stickiness keeps the current focus unless another beats it clearly, so the
	 *  prompt doesn't flicker between two doors side by side. */
	inline int PickBest(const std::vector<FCandidate>& Cs, int CurrentId, float Stickiness = 0.15f)
	{
		int Best = -1;
		float BestScore = -1.f;
		float CurrentScore = -1.f;
		int CurrentIndex = -1;
		for (int i = 0; i < int(Cs.size()); ++i)
		{
			const float S = Score(Cs[i]);
			if (Cs[i].Id == CurrentId) { CurrentScore = S; CurrentIndex = i; }
			if (S > BestScore) { BestScore = S; Best = S >= 0.f ? i : -1; }
		}
		if (CurrentIndex >= 0 && CurrentScore >= 0.f && BestScore < CurrentScore + Stickiness)
		{
			return CurrentIndex;
		}
		return Best;
	}

	/** Angle between a facing direction and the direction to a point, in the ground plane (degrees). */
	inline float FacingAngleDeg(float FwdX, float FwdY, float ToX, float ToY)
	{
		const float Lf = std::sqrt(FwdX * FwdX + FwdY * FwdY), Lt = std::sqrt(ToX * ToX + ToY * ToY);
		if (Lf < 1e-4f || Lt < 1e-4f)
		{
			return 0.f; // standing on it: facing doesn't matter
		}
		float Cos = (FwdX * ToX + FwdY * ToY) / (Lf * Lt);
		Cos = Cos > 1.f ? 1.f : (Cos < -1.f ? -1.f : Cos);
		return std::acos(Cos) * 57.2957795f;
	}
}
