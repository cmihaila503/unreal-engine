// Gangs — who owns which streets, and what they think of him. Pure C++17, unit-tested (Handoff/Gangs/Tests).
//   Respect (-100..100) per gang, saved: killing their men costs a lot, hurting them some, paying the "taxa" and
//   doing their jobs earns it; it drifts back toward 0 over days.
//   Stance from respect with hysteresis: Hostile (they come for him on sight in their streets), Wary, Friendly.
//   Protection: walking into their streets while Wary and unpaid, a man may come to ask for the "taxa". Paid = left
//   alone for a few game days.

#pragma once

#include <algorithm>
#include <cmath>

namespace MurdarGangs
{
	enum class EStance { Hostile, Wary, Friendly };

	struct FTuning
	{
		float KillMember = -35.f;
		float HurtMember = -8.f;
		float PaidTax = 12.f;
		float JobDone = 15.f;
		float DriftPerDay = 5.f;       // toward 0
		float HostileBelow = -40.f, HostileExitAbove = -25.f;
		float FriendlyAbove = 40.f, FriendlyExitBelow = 25.f;
		float DemandChancePerEntry = 0.4f;
		float PaidDays = 3.f;
		int MembersHostile = 5, MembersWary = 3, MembersFriendly = 2;
	};

	inline float Clamp(float R) { return std::clamp(R, -100.f, 100.f); }

	enum class EDeed { KilledMember, HurtMember, PaidTax, JobDone };

	inline float Apply(float Respect, EDeed D, const FTuning& T)
	{
		switch (D)
		{
		case EDeed::KilledMember: return Clamp(Respect + T.KillMember);
		case EDeed::HurtMember:   return Clamp(Respect + T.HurtMember);
		case EDeed::PaidTax:      return Clamp(Respect + T.PaidTax);
		case EDeed::JobDone:      return Clamp(Respect + T.JobDone);
		}
		return Respect;
	}

	/** Days pass: respect fades toward 0 (grudges and favours are forgotten, slowly). */
	inline float Drift(float Respect, float Days, const FTuning& T)
	{
		const float Step = T.DriftPerDay * std::max(0.f, Days);
		if (Respect > 0.f) { return std::max(0.f, Respect - Step); }
		return std::min(0.f, Respect + Step);
	}

	/** Stance with hysteresis: entering a stance needs more than keeping it. */
	inline EStance StanceOf(float Respect, EStance Current, const FTuning& T)
	{
		if (Current == EStance::Hostile) { return Respect > T.HostileExitAbove ? (Respect >= T.FriendlyAbove ? EStance::Friendly : EStance::Wary) : EStance::Hostile; }
		if (Current == EStance::Friendly) { return Respect < T.FriendlyExitBelow ? (Respect <= T.HostileBelow ? EStance::Hostile : EStance::Wary) : EStance::Friendly; }
		if (Respect <= T.HostileBelow) { return EStance::Hostile; }
		if (Respect >= T.FriendlyAbove) { return EStance::Friendly; }
		return EStance::Wary;
	}

	/** Entering their streets: does someone come for the taxa? Only Wary and unpaid. */
	inline bool DemandsTax(EStance S, float Day, float PaidUntilDay, float Rand01, const FTuning& T)
	{
		return S == EStance::Wary && Day >= PaidUntilDay && Rand01 < T.DemandChancePerEntry;
	}

	inline float PaidUntil(float Day, const FTuning& T) { return Day + T.PaidDays; }

	/** How many of theirs hang about near him in their streets. */
	inline int MembersAround(EStance S, bool bNight, const FTuning& T)
	{
		const int Base = S == EStance::Hostile ? T.MembersHostile : S == EStance::Wary ? T.MembersWary : T.MembersFriendly;
		return bNight ? Base + 1 : Base;
	}
}
