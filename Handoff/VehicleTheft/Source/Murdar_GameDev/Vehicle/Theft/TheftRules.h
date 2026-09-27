// Vehicle theft — locks, breaking in, carjacking, and when the theft reaches the police. Pure C++17, unit-tested
// (Handoff/VehicleTheft/Tests).
//
// Today any free car can be entered and a traffic car can't be entered at all (AMurdarVehicle::Enter refuses an
// AI-driven car, though the HUD offers "Urcă în"). Here: a parked car may be locked (break the window and hotwire it:
// seconds standing at the door, noise, maybe an alarm); a traffic car stopped or crawling can be carjacked (the driver
// is pulled out and protests). Every theft is reported sooner or later — at once by a victim or a witness, much later
// by an owner who finds the empty kerb — and a reported car recognised by a patrol is a crime (pull over, not a war).

#pragma once

#include <algorithm>

namespace MurdarTheft
{
	struct FTuning
	{
		float LockChanceDay = 0.55f;
		float LockChanceNight = 0.8f;
		float AlarmChance = 0.3f;          // a locked car broken into: the alarm goes
		float BreakInSeconds = 1.2f;       // the window
		float HotwireSeconds = 2.5f;       // the wires under the dash
		float CancelDistanceCm = 250.f;    // walk away and it's off
		float CarjackMaxKph = 12.f;        // stopped at a light, crawling in a queue
		float CarjackReachCm = 300.f;
		float WitnessRadiusCm = 2000.f;    // someone this close sees a window go in
		float AlarmWitnessRadiusCm = 5000.f; // an alarm is heard further
		float ReportVictimMin = 15.f, ReportVictimMax = 35.f;     // carjack: the driver finds a phone
		float ReportWitnessMin = 30.f, ReportWitnessMax = 75.f;   // a passer-by
		float ReportOwnerMin = 240.f, ReportOwnerMax = 480.f;     // nobody saw: the owner comes back to an empty kerb
		float CrimeSeverity = 15.f;        // >= HeatStop (12): a patrol that recognises it pulls you over
	};

	inline bool RollLocked(float Rand01, bool bNight, const FTuning& T)
	{
		return Rand01 < (bNight ? T.LockChanceNight : T.LockChanceDay);
	}

	enum class EMethod { Unlocked, BreakIn, Carjack };

	struct FCarjackCheck
	{
		bool bAIDriven = false;
		bool bPolice = false;       // never: that's a different game (and a different crime)
		bool bHasTrafficDriver = false;
		float SpeedKph = 0.f;
		float DistanceCm = 0.f;
	};

	inline bool CanCarjack(const FCarjackCheck& C, const FTuning& T)
	{
		return C.bAIDriven && !C.bPolice && C.bHasTrafficDriver && C.SpeedKph <= T.CarjackMaxKph && C.DistanceCm <= T.CarjackReachCm;
	}

	/** Breaking into a locked car: window, then wires. Moving away cancels; the car is yours when it's Done. */
	class FBreakIn
	{
	public:
		enum class EState { Idle, Window, Wires, Done, Cancelled };

		void Start(float Now) { State = EState::Window; StartedAt = Now; }

		EState Update(float Now, float DistanceToDoorCm, const FTuning& T)
		{
			if (State != EState::Window && State != EState::Wires) { return State; }
			if (DistanceToDoorCm > T.CancelDistanceCm) { State = EState::Cancelled; return State; }
			const float E = Now - StartedAt;
			if (E >= T.BreakInSeconds + T.HotwireSeconds) { State = EState::Done; }
			else if (E >= T.BreakInSeconds) { State = EState::Wires; }
			return State;
		}

		/** True once, on the frame the window breaks (noise, glass, the alarm roll). */
		bool ConsumeWindowBroken(float Now, const FTuning& T)
		{
			if (bWindowReported || State == EState::Idle || State == EState::Cancelled) { return false; }
			if (Now - StartedAt >= T.BreakInSeconds) { bWindowReported = true; return true; }
			return false;
		}

		float Progress01(float Now, const FTuning& T) const
		{
			const float Total = T.BreakInSeconds + T.HotwireSeconds;
			if (State == EState::Done) { return 1.f; }
			if (State == EState::Idle || State == EState::Cancelled || Total <= 0.f) { return 0.f; }
			return std::clamp((Now - StartedAt) / Total, 0.f, 1.f);
		}

		EState Get() const { return State; }

	private:
		EState State = EState::Idle;
		float StartedAt = 0.f;
		bool bWindowReported = false;
	};

	/** Seconds until the theft reaches the police. Rand01 picks inside the range. */
	inline float ReportDelay(EMethod M, bool bWitnessed, float Rand01, const FTuning& T)
	{
		const float R = std::clamp(Rand01, 0.f, 1.f);
		auto Lerp = [R](float A, float B) { return A + (B - A) * R; };
		if (M == EMethod::Carjack) { return Lerp(T.ReportVictimMin, T.ReportVictimMax); }
		if (bWitnessed) { return Lerp(T.ReportWitnessMin, T.ReportWitnessMax); }
		return Lerp(T.ReportOwnerMin, T.ReportOwnerMax);
	}

	/** Was it seen? Nearest person's distance (< 0 = nobody around); an alarm carries further. */
	inline bool Witnessed(float NearestPersonCm, bool bAlarm, const FTuning& T)
	{
		if (NearestPersonCm < 0.f) { return false; }
		return NearestPersonCm <= (bAlarm ? T.AlarmWitnessRadiusCm : T.WitnessRadiusCm);
	}

	/** One stolen car's paper trail. */
	struct FStolenCar
	{
		float ReportAt = 0.f;
		bool bReported = false;
		bool bRecognised = false; // a patrol matched it (crime reported once per car)
		bool bCleaned = false;    // resprayed (Handoff/Garage): the description no longer fits

		/** True once, when the report goes in. */
		bool UpdateReport(float Now)
		{
			if (bReported || bCleaned || Now < ReportAt) { return false; }
			bReported = true;
			return true;
		}

		/** True once: a patrol has eyes on him in this car after it was reported. */
		bool UpdateRecognition(bool bPoliceSeeHimNow, bool bHeDrivesIt)
		{
			if (!bReported || bRecognised || bCleaned || !bPoliceSeeHimNow || !bHeDrivesIt) { return false; }
			bRecognised = true;
			return true;
		}
	};
}
