// "Paranoia în retrovizoare" — the rules, in pure C++17 (no engine types) so they are unit-tested outside the editor
// (Handoff/RearviewParanoia/Tests). The engine side (URearviewSubsystem, ARearviewTailController) feeds them samples
// and acts on what they return. Units: km/h, seconds, degrees, centimetres.
//
//   FBrakeCheckDetector   the player taps the brakes hard from speed without stopping — "who is behind me?"
//   FInspectionTurnDetector  a sharp, sudden turn (off the main road, into a side street) — "will he follow?"
//   FManeuverTracker      the classic detection route: a U-turn, or right-right-right round the block — nobody
//                         innocent goes round a block with you
//   CanSeeTarget          what a follower can see at night: a car with its lights off is a shadow past a few metres —
//                         unless its brake lights come on
//   FTailExposure         the follower's own sense of "he's made me" — what makes a pro break off and a thug close in
//   ChooseStopResponse    he pulls over: the amateur stops behind him, the pro drives past and parks ahead, dark

#pragma once

#include <algorithm>
#include <cmath>
#include <deque>

namespace MurdarRearview
{
	// ------------------------------------------------------------------------------------------------------------
	struct FBrakeCheckConfig
	{
		float MinStartKph = 35.f;      // below this a hard stop is just traffic
		float MinDecelMps2 = 5.5f;     // ~0.55 g: harder than any normal stop
		float MinDropKph = 12.f;       // the speed has to actually fall
		float MaxDurationSeconds = 1.6f; // a tap, not a stop
		float MinEndKph = 10.f;        // ...and he doesn't come to a halt
		float CooldownSeconds = 4.f;   // one check, one reaction
	};

	/** Feed Sample(time, kph) at a steady rate (the subsystem's 10 Hz). Returns true on the sample a check is recognised. */
	class FBrakeCheckDetector
	{
	public:
		explicit FBrakeCheckDetector(const FBrakeCheckConfig& In) : C(In) {}

		bool Sample(double Now, float Kph)
		{
			const bool bHadPrev = !History.empty();
			const FS Prev = bHadPrev ? History.back() : FS{Now, Kph};
			History.push_back({Now, Kph});
			// Keep the window plus one sample interval of margin (the braking episode is judged when it has ended).
			while (!History.empty() && Now - History.front().T > C.MaxDurationSeconds + 0.5)
			{
				History.pop_front();
			}
			if (Now - LastFired < C.CooldownSeconds || !bHadPrev)
			{
				return false;
			}
			// Judge only once he has stopped braking: mid-brake we can't know yet whether it ends in a halt (an
			// emergency stop) or at speed (a check). "Stopped braking" = this sample dropped less than a quarter of the
			// check's deceleration.
			const double StepDt = Now - Prev.T;
			const float StepDecel = StepDt > 0.0 ? float((Prev.Kph - Kph) / 3.6 / StepDt) : 0.f;
			if (StepDecel > 0.25f * C.MinDecelMps2)
			{
				return false;
			}
			// Look back over the window for a speed high enough; the drop from it to now, over the time it took, must be
			// sharp, short, and not end in a stop.
			for (const FS& S : History)
			{
				const double Dt = Now - S.T;
				if (Dt <= 0.0 || Dt > C.MaxDurationSeconds + StepDt || S.Kph < C.MinStartKph)
				{
					continue;
				}
				const float Drop = S.Kph - Kph;
				const float DecelMps2 = float(Drop / 3.6 / Dt);
				if (Drop >= C.MinDropKph && DecelMps2 >= C.MinDecelMps2 && Kph >= C.MinEndKph)
				{
					LastFired = Now;
					History.clear();
					return true;
				}
			}
			return false;
		}

	private:
		struct FS { double T; float Kph; };
		FBrakeCheckConfig C;
		std::deque<FS> History;
		double LastFired = -1e9;
	};

	// ------------------------------------------------------------------------------------------------------------
	struct FInspectionTurnConfig
	{
		float MinTurnDeg = 60.f;         // a real change of street, not a bend
		float MaxTurnSeconds = 3.5f;     // done quickly
		float MinKph = 25.f;             // at speed — creeping round a corner isn't a test
		float MinYawRateDegPerSec = 45.f; // sudden: an ordinary junction turn is ~30 deg/s at 15-20 km/h
		bool bSignalCounts = true;       // indicating the turn beforehand makes it an ordinary turn
		float CooldownSeconds = 6.f;
	};

	/** Feed Sample(time, yawDeg, kph, bIndicating). YawDeg may wrap; the detector unwraps it. Returns +1 / -1 (right /
	 *  left) on the sample the turn completes, 0 otherwise. */
	class FInspectionTurnDetector
	{
	public:
		explicit FInspectionTurnDetector(const FInspectionTurnConfig& In) : C(In) {}

		int Sample(double Now, float YawDeg, float Kph, bool bIndicating)
		{
			if (bHave)
			{
				float D = YawDeg - LastYaw;
				while (D > 180.f) D -= 360.f;
				while (D < -180.f) D += 360.f;
				Unwrapped += D;
			}
			bHave = true;
			LastYaw = YawDeg;
			if (bIndicating) { LastIndicate = Now; }

			History.push_back({Now, Unwrapped, Kph});
			while (!History.empty() && Now - History.front().T > C.MaxTurnSeconds)
			{
				History.pop_front();
			}
			if (Now - LastFired < C.CooldownSeconds || History.size() < 2)
			{
				return 0;
			}
			float MinKphSeen = 1e9f;
			for (const FS& S : History) { MinKphSeen = std::min(MinKphSeen, S.Kph); }
			if (MinKphSeen < C.MinKph)
			{
				return 0;
			}
			// The tightest stretch that already holds MinTurnDeg of turning: its rate is how sudden the turn was. (Measuring
			// over the whole window counted the straight road before the corner and made every turn look gentle.)
			float Turned = 0.f;
			double Dt = 0.0;
			for (auto It = History.rbegin(); It != History.rend(); ++It)
			{
				if (std::abs(Unwrapped - It->Yaw) >= C.MinTurnDeg)
				{
					Turned = Unwrapped - It->Yaw;
					Dt = Now - It->T;
					break;
				}
			}
			if (Dt <= 0.0 || std::abs(Turned) / float(Dt) < C.MinYawRateDegPerSec)
			{
				return 0;
			}
			if (C.bSignalCounts && Now - LastIndicate < C.MaxTurnSeconds + 2.0)
			{
				return 0; // announced: an ordinary turn
			}
			LastFired = Now;
			History.clear();
			return Turned > 0.f ? 1 : -1;
		}

	private:
		struct FS { double T; float Yaw; float Kph; };
		FInspectionTurnConfig C;
		std::deque<FS> History;
		bool bHave = false;
		float LastYaw = 0.f;
		float Unwrapped = 0.f;
		double LastFired = -1e9;
		double LastIndicate = -1e9;
	};

	// ------------------------------------------------------------------------------------------------------------
	enum class EManeuver : int { None = 0, UTurn = 1, AroundTheBlock = 2 };

	struct FManeuverConfig
	{
		float UTurnMinDeg = 150.f;       // back the way he came
		float UTurnMaxSeconds = 10.f;    // three-point turns included
		float LoopMinDeg = 300.f;        // right-right-right (or left x3): round the block
		float LoopMaxSeconds = 120.f;    // a town block at town speed
		float MinKph = 5.f;              // moving, not sitting at a light twitching the wheel
		float CooldownSeconds = 10.f;
	};

	/** Feed Sample(time, yawDeg, kph) at a steady rate. Returns the manoeuvre on the sample it completes. A loop is
	 *  cumulative turning in ONE direction (straight stretches in between are fine; turning back cancels it). */
	class FManeuverTracker
	{
	public:
		explicit FManeuverTracker(const FManeuverConfig& In) : C(In) {}

		EManeuver Sample(double Now, float YawDeg, float Kph)
		{
			float D = 0.f;
			if (bHave)
			{
				D = YawDeg - LastYaw;
				while (D > 180.f) D -= 360.f;
				while (D < -180.f) D += 360.f;
			}
			bHave = true;
			LastYaw = YawDeg;
			if (Kph < C.MinKph)
			{
				D = 0.f; // parked, or a stopped car's wheel wobble: no manoeuvre
			}
			Steps.push_back({Now, D});
			while (!Steps.empty() && Now - Steps.front().T > C.LoopMaxSeconds)
			{
				Steps.pop_front();
			}
			if (Now - LastFired < C.CooldownSeconds)
			{
				return EManeuver::None;
			}
			// Round the block: net turning in one direction over the loop window.
			float Net = 0.f;
			for (const FStep& S : Steps) { Net += S.D; }
			if (std::abs(Net) >= C.LoopMinDeg)
			{
				return Fire(Now);
			}
			// U-turn: net turning over the last UTurnMaxSeconds.
			float Recent = 0.f;
			for (auto It = Steps.rbegin(); It != Steps.rend() && Now - It->T <= C.UTurnMaxSeconds; ++It) { Recent += It->D; }
			if (std::abs(Recent) >= C.UTurnMinDeg)
			{
				LastFired = Now;
				Steps.clear(); // the U-turn is spent; it must not also count towards a loop
				return EManeuver::UTurn;
			}
			return EManeuver::None;
		}

	private:
		EManeuver Fire(double Now)
		{
			LastFired = Now;
			Steps.clear();
			return EManeuver::AroundTheBlock;
		}
		struct FStep { double T; float D; };
		FManeuverConfig C;
		std::deque<FStep> Steps;
		bool bHave = false;
		float LastYaw = 0.f;
		double LastFired = -1e9;
	};

	// ------------------------------------------------------------------------------------------------------------
	struct FVisibilityConfig
	{
		float DayRangeCm = 15000.f;        // plenty: a car is a car
		float NightLitRangeCm = 12000.f;   // his tail lights and headlights give him away far off
		float NightDarkRangeCm = 1800.f;   // lights off at night: a shape, only close
		float HighBeamBonusCm = 1500.f;    // our own high beams light up a dark car a bit further
		float NightBrakeLightRangeCm = 9000.f; // lights off but braking: two red lamps give him away
	};

	/** Can a follower see the target? No line of sight = no, whatever the rest says. Brake lights shine with the
	 *  headlights off (use the handbrake to stay dark). */
	inline bool CanSeeTarget(float DistanceCm, bool bLineOfSight, bool bDark, bool bTargetLightsOn, bool bOurHighBeams,
		const FVisibilityConfig& C, bool bTargetBrakeLights = false)
	{
		if (!bLineOfSight)
		{
			return false;
		}
		float Range = !bDark ? C.DayRangeCm : (bTargetLightsOn ? C.NightLitRangeCm : C.NightDarkRangeCm);
		if (bDark && !bTargetLightsOn && bTargetBrakeLights)
		{
			Range = std::max(Range, C.NightBrakeLightRangeCm);
		}
		if (bDark && !bTargetLightsOn && bOurHighBeams)
		{
			Range += C.HighBeamBonusCm;
		}
		return DistanceCm <= Range;
	}

	// ------------------------------------------------------------------------------------------------------------
	enum class ETailRole : int { Civilian = 0, Undercover = 1, Gang = 2 };

	struct FExposureConfig
	{
		float FollowedTurn = 0.40f;    // took the bait on an inspection turn
		float FollowedUTurn = 0.70f;   // turned round with him
		float FollowedLoop = 1.00f;    // went round the block with him: nobody innocent does that
		float StoppedBehind = 0.30f;   // stopped behind him when he pulled over
		float HeldOnBrakeCheck = 0.25f; // didn't react like a civilian (no horn, no overtake) — he noticed that
		float RushedAfterDark = 0.35f; // had to floor it with high beams to find him again
		float DecayPerSecond = 0.004f; // a quiet stretch calms him a little
		float BlownAt = 0.8f;          // at this, he knows he is made
	};

	/** The follower's belief that the player has spotted him (0..1). Skill (0..1) makes a pro read it earlier and
	 *  break off; a thug with low skill just gets angrier. */
	class FTailExposure
	{
	public:
		explicit FTailExposure(const FExposureConfig& In) : C(In) {}

		void Add(float Amount) { Value = std::min(1.f, Value + std::max(0.f, Amount)); }
		void Tick(float Dt) { Value = std::max(0.f, Value - C.DecayPerSecond * Dt); }
		float Get() const { return Value; }

		/** Blown for this follower: a skilled one reads it sooner (threshold scaled down by up to 40 %). */
		bool IsBlown(float Skill01) const
		{
			const float S = std::min(std::max(Skill01, 0.f), 1.f);
			return Value >= C.BlownAt * (1.f - 0.4f * S);
		}

	private:
		FExposureConfig C;
		float Value = 0.f;
	};

	/** What a follower does with an inspection turn: follow it (and pay exposure), or drive on. Skill01 × this role's
	 *  discipline is the chance to drive on. Roll01 is a uniform random number the caller supplies (seedable). */
	inline bool TakesTheBait(ETailRole Role, float Skill01, float Roll01)
	{
		switch (Role)
		{
		case ETailRole::Undercover: return Roll01 >= std::min(std::max(Skill01, 0.f), 1.f) * 0.8f; // a pro often drives on
		case ETailRole::Gang:       return true;                                                  // they don't care
		default:                    return false;                                                 // a civilian has his own way
		}
	}

	// ------------------------------------------------------------------------------------------------------------
	enum class EStopResponse : int { StopBehind = 0, ParkAhead = 1, DriveOn = 2 };

	/** He pulls over. A civilian drives on (his own way). A pro usually drives past and parks further on, lights off,
	 *  to pick him up again; a rookie stops behind him — the tell. The gang stops behind, lights on: it wants to be
	 *  seen. Roll01 is uniform random, supplied by the caller. */
	inline EStopResponse ChooseStopResponse(ETailRole Role, float Skill01, float Roll01)
	{
		switch (Role)
		{
		case ETailRole::Undercover: return Roll01 < std::min(std::max(Skill01, 0.f), 1.f) ? EStopResponse::ParkAhead : EStopResponse::StopBehind;
		case ETailRole::Gang:       return EStopResponse::StopBehind;
		default:                    return EStopResponse::DriveOn;
		}
	}

	/** A manoeuvre's exposure for a follower who went through it with him. */
	inline float ManeuverExposure(EManeuver M, const FExposureConfig& C)
	{
		return M == EManeuver::AroundTheBlock ? C.FollowedLoop : M == EManeuver::UTurn ? C.FollowedUTurn : 0.f;
	}

	/** Will this follower go through the manoeuvre with him? A U-turn or a loop is an obvious test; a skilled pro
	 *  refuses it (drives on) far more often than a plain turn. Gang: always. Civilian: never. */
	inline bool FollowsManeuver(ETailRole Role, float Skill01, float Roll01)
	{
		switch (Role)
		{
		case ETailRole::Undercover: return Roll01 >= 0.2f + 0.75f * std::min(std::max(Skill01, 0.f), 1.f);
		case ETailRole::Gang:       return true;
		default:                    return false;
		}
	}
}
