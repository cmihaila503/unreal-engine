// Dynamic music — which mood, how loud each layer. Pure C++17, unit-tested (Handoff/Music/Tests).
//
// Mostly silence (the city, the engine, the radio later). Music comes when something is at stake and it says only
// how much: being followed (suspense), running from the police (pursuit), guns (combat), and the breath after
// (aftermath). Going up is immediate — the first shot must land with the music. Coming down waits, so a chase that
// pauses for five seconds behind a block doesn't drop the score and pick it up again.

#pragma once

#include <algorithm>
#include <cmath>

namespace MurdarMusic
{
	enum class EMood { Silence = 0, Unease, Suspense, Aftermath, Pursuit, Combat };

	/** Higher = more urgent. Aftermath ranks under the chase it follows, above suspense. */
	inline int Rank(EMood M) { return int(M); }

	struct FInputs
	{
		float Stress01 = 0.f;            // UTensionSubsystem
		int Wanted = 0;                  // EWantedLevel: 0 none, 1 stop, 2 pursuit, 3 lethal
		float SinceShot = 1e9f;          // seconds since a shot near him
		bool bTailed = false;            // a rearview follower is behind him
		float SincePursuitEnded = 1e9f;  // seconds since a chase ended (escaped or not)
	};

	struct FTuning
	{
		float CombatShotWindow = 12.f;   // a shot keeps combat music this long
		float AftermathSeconds = 20.f;
		float UneaseStress = 0.5f;
		float DownDelay = 6.f;           // target must stay lower this long before the mood drops
		float MinDwell = 8.f;            // and the mood must have played at least this long
		float RiseRate = 0.8f;           // intensity per second going up
		float FallRate = 0.12f;          // and down (a slow release)
	};

	inline EMood TargetMood(const FInputs& In, const FTuning& T)
	{
		if (In.Wanted >= 3 || In.SinceShot < T.CombatShotWindow) { return EMood::Combat; }
		if (In.Wanted == 2) { return EMood::Pursuit; }
		if (In.SincePursuitEnded < T.AftermathSeconds) { return EMood::Aftermath; }
		if (In.bTailed) { return EMood::Suspense; }
		if (In.Stress01 >= T.UneaseStress || In.Wanted == 1) { return EMood::Unease; }
		return EMood::Silence;
	}

	/** 0..1 for a mood, nudged by stress inside a band (a tense suspense plays a bit fuller than a calm one). */
	inline float Intensity(EMood M, float Stress01)
	{
		static const float Base[] = { 0.f, 0.2f, 0.4f, 0.3f, 0.75f, 0.95f };
		if (M == EMood::Silence) { return 0.f; }
		const float S = std::clamp(Stress01, 0.f, 1.f);
		return std::clamp(Base[int(M)] + 0.1f * (S - 0.5f), 0.f, 1.f);
	}

	/** Up at once, down only after the lower target has held for DownDelay and the current mood for MinDwell. */
	class FMoodMachine
	{
	public:
		/** True when the mood changed. */
		bool Update(EMood Target, float Now, const FTuning& T)
		{
			if (Rank(Target) > Rank(Current))
			{
				Set(Target, Now);
				return true;
			}
			if (Rank(Target) == Rank(Current))
			{
				LowerSince = -1.f;
				return false;
			}
			if (LowerSince < 0.f || Rank(Target) != Rank(LowerTarget)) { LowerSince = Now; LowerTarget = Target; }
			if (Now - LowerSince >= T.DownDelay && Now - Since >= T.MinDwell)
			{
				Set(Target, Now);
				return true;
			}
			return false;
		}

		EMood Get() const { return Current; }
		float PlayingFor(float Now) const { return Now - Since; }

	private:
		EMood Current = EMood::Silence;
		EMood LowerTarget = EMood::Silence;
		float Since = 0.f;
		float LowerSince = -1.f;

		void Set(EMood M, float Now) { Current = M; Since = Now; LowerSince = -1.f; }
	};

	/** Moves Value toward Target at the rise / fall rate. */
	inline float Approach(float Value, float Target, float Dt, const FTuning& T)
	{
		if (Target > Value) { return std::min(Target, Value + T.RiseRate * Dt); }
		return std::max(Target, Value - T.FallRate * Dt);
	}

	/** A layer's gain at an intensity: silent under Start, full at Full, linear between. */
	inline float LayerGain(float Intensity01, float Start, float Full)
	{
		if (Full <= Start) { return Intensity01 >= Start ? 1.f : 0.f; }
		return std::clamp((Intensity01 - Start) / (Full - Start), 0.f, 1.f);
	}
}
