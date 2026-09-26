// Consequences — what dying or being arrested costs. Pure C++17, unit-tested (Handoff/Consequences/Tests).
//
// Today both send the player back to the last checkpoint (UChapterDirector). That stays for story chapters. In the open
// city the game goes on instead, like GTA: death = the hospital (hours pass, a bill), arrest = the police station
// (hours in a cell that grow with heat and with every earlier arrest, a fine, firearms taken). Health comes back by
// itself only part of the way: the rest is a doctor. Nothing here is shown as a number on the HUD.

#pragma once

#include <cmath>
#include <vector>

namespace MurdarConsequence
{
	enum class EOutcome { Checkpoint, Hospital, Station };

	struct FContext
	{
		bool bArrest = false;          // else: death
		bool bStoryMode = false;       // the chapter wants the checkpoint (Fact.Consequences.Checkpoint)
		bool bHasHospital = false;     // a Hospital spawn point exists in this world
		bool bHasStation = false;      // a PoliceStation spawn point exists
	};

	/** The checkpoint whenever the chapter asks for it or the map has nowhere to send him — the old behaviour. */
	inline EOutcome Decide(const FContext& C)
	{
		if (C.bStoryMode) { return EOutcome::Checkpoint; }
		if (C.bArrest) { return C.bHasStation ? EOutcome::Station : EOutcome::Checkpoint; }
		return C.bHasHospital ? EOutcome::Hospital : EOutcome::Checkpoint;
	}

	struct FTuning
	{
		float HospitalHours = 6.f;
		float HospitalFeeFraction = 0.1f;
		float HospitalFeeMin = 100.f;

		float CellHoursBase = 8.f;
		float CellHoursPerHeat = 0.2f;   // heat 0..100: a lethal chase adds up to 20 h
		float CellHoursMax = 72.f;
		float FineBase = 100.f;
		float FinePerHeat = 5.f;
		float RepeatFactor = 0.25f;      // each earlier arrest: +25 % time and fine
		float ConfiscateFromHeat = 0.f;  // firearms are taken at or above this heat (0 = always, > 100 = never)
	};

	struct FHospital
	{
		float Hours = 0.f;
		float Bill = 0.f;
	};

	/** The bill: a fraction of the cash, at least the minimum, never more than he has (they patch you up anyway). */
	inline FHospital HospitalStay(float Cash, const FTuning& T)
	{
		FHospital H;
		H.Hours = T.HospitalHours;
		if (Cash > 0.f)
		{
			float Bill = std::floor(Cash * T.HospitalFeeFraction);
			if (Bill < T.HospitalFeeMin) { Bill = T.HospitalFeeMin; }
			H.Bill = Bill > Cash ? Cash : Bill;
		}
		return H;
	}

	struct FSentence
	{
		float CellHours = 0.f;
		float Fine = 0.f;          // what is taken (never more than the cash)
		float FineOwed = 0.f;      // what it would have been
		bool bConfiscate = false;
	};

	inline FSentence StationSentence(float Heat, int PriorArrests, float Cash, const FTuning& T)
	{
		const float H = Heat < 0.f ? 0.f : (Heat > 100.f ? 100.f : Heat);
		const float Repeat = 1.f + T.RepeatFactor * float(PriorArrests < 0 ? 0 : PriorArrests);
		FSentence S;
		S.CellHours = (T.CellHoursBase + T.CellHoursPerHeat * H) * Repeat;
		if (S.CellHours > T.CellHoursMax) { S.CellHours = T.CellHoursMax; }
		S.FineOwed = std::round((T.FineBase + T.FinePerHeat * H) * Repeat);
		S.Fine = Cash <= 0.f ? 0.f : (S.FineOwed > Cash ? Cash : S.FineOwed);
		S.bConfiscate = H >= T.ConfiscateFromHeat;
		return S;
	}

	struct FRegen
	{
		float DelaySeconds = 6.f;   // after the last hit
		float PerSecond = 2.f;      // health points
		float CapFraction = 0.6f;   // comes back by itself only this far; the rest is a doctor
	};

	/** Health after Dt seconds of natural recovery. Never lowers health, never past the cap. */
	inline float Regenerate(float Health, float MaxHealth, float SinceDamage, float Dt, const FRegen& R)
	{
		const float Cap = MaxHealth * R.CapFraction;
		if (Health <= 0.f || SinceDamage < R.DelaySeconds || Health >= Cap || Dt <= 0.f) { return Health; }
		const float Next = Health + R.PerSecond * Dt;
		return Next > Cap ? Cap : Next;
	}

	/** Index of the smallest distance, or -1. */
	inline int PickNearest(const std::vector<float>& DistSq)
	{
		int Best = -1;
		for (int i = 0; i < int(DistSq.size()); ++i) { if (Best < 0 || DistSq[i] < DistSq[Best]) { Best = i; } }
		return Best;
	}
}
