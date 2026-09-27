// Safehouse — a bed, a stash, a wardrobe, a door the police don't open. Pure C++17, unit-tested
// (Handoff/Safehouse/Tests).
//   Sleep: only when things are quiet (the same rule as saving); wakes at the next morning, or after a nap if it is
//          already day; heals; saves.
//   Stash: money kept here isn't on him, so an arrest or a hospital bill can't take it.
//   Hiding: inside, unseen, the heat cools several times faster — the door is shut and the lights are off.

#pragma once

#include <algorithm>
#include <cmath>

namespace MurdarSafehouse
{
	struct FTuning
	{
		float WakeHour = 8.f;       // mornings
		float NapHours = 4.f;       // sleeping by day
		float MinSleepHours = 3.f;  // a "night" after 5 am still lasts this long
		float CombatWindow = 20.f;  // seconds after a shot near him
		float HideDecayMultiplier = 3.f;
		float HideUnseenSeconds = 10.f; // inside and unseen this long before the hiding bonus starts
	};

	enum class EBlock { None, Wanted, Combat, Mission };

	inline EBlock CanSleep(int Wanted, float SinceShot, bool bMissionActive, const FTuning& T)
	{
		if (Wanted > 0) { return EBlock::Wanted; }
		if (SinceShot < T.CombatWindow) { return EBlock::Combat; }
		if (bMissionActive) { return EBlock::Mission; }
		return EBlock::None;
	}

	/** Hours to skip. Night (from 20:00 to WakeHour): until WakeHour, at least MinSleepHours. Day: a nap. */
	inline float SleepHours(float Hour, const FTuning& T)
	{
		const float H = std::fmod(std::fmod(Hour, 24.f) + 24.f, 24.f);
		const bool bNight = H >= 20.f || H < T.WakeHour;
		if (!bNight) { return T.NapHours; }
		float Until = T.WakeHour - H;
		if (Until < 0.f) { Until += 24.f; }
		return std::max(Until, T.MinSleepHours);
	}

	struct FMoney { float Cash = 0.f, Stash = 0.f; };

	/** Everything in the pocket goes into the stash (or Amount, if smaller). */
	inline FMoney Deposit(FMoney M, float Amount = 1e30f)
	{
		const float A = std::clamp(Amount, 0.f, M.Cash);
		M.Cash -= A; M.Stash += A;
		return M;
	}

	inline FMoney Withdraw(FMoney M, float Amount = 1e30f)
	{
		const float A = std::clamp(Amount, 0.f, M.Stash);
		M.Stash -= A; M.Cash += A;
		return M;
	}

	/** Extra heat decay per second while hiding (on top of the normal decay). */
	inline float HideExtraDecay(bool bInside, float SinceSeen, float NormalDecayPerSecond, const FTuning& T)
	{
		if (!bInside || SinceSeen < T.HideUnseenSeconds) { return 0.f; }
		return NormalDecayPerSecond * std::max(0.f, T.HideDecayMultiplier - 1.f);
	}

	/** Wardrobe: next outfit, wrapping. */
	inline int NextOutfit(int Current, int Count) { return Count <= 0 ? 0 : ((Current + 1) % Count + Count) % Count; }
}
