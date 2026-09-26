// Heat model and witness report queue — pure C++17, no engine types, so the rules can be unit-tested outside the
// editor (Handoff/Heat/Tests). The engine side (UFactionMemorySubsystem, UWitnessReportSubsystem) owns the
// UObjects, the tags and the clock, and calls into this. Units: heat 0..100, time in seconds, distance in cm.
//
// Design (Docs/HEAT_SYSTEM.md): one global heat scalar, improved —
//   * decay waits after the last sighting/crime, and is slower after a serious crime and while police search;
//   * the same crime repeated within a window gives diminishing heat (no spam from continuous speeding);
//   * wanted level has hysteresis (no Stop<->Pursuit flapping at 40);
//   * civilian witnesses report later (queue); the first report of an incident adds its heat, later reports of the
//     same incident only corroborate; police eyes are instant.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace MurdarHeat
{
	enum class EWanted : uint8_t { None = 0, Stop = 1, Pursuit = 2, Lethal = 3 };

	struct FConfig
	{
		// Wanted thresholds — owned by the faction memory today (12/40/75); the engine side copies them in.
		float StopHeat = 12.f;
		float PursuitHeat = 40.f;
		float LethalHeat = 75.f;
		/** A level is kept until heat falls this far below its threshold. */
		float WantedHysteresisHeat = 5.f;

		/** Out-of-sight decay (the existing 0.6/s). */
		float DecayPerSecond = 0.6f;
		/** No decay for this long after the police last saw him or a crime was reported. */
		float DecayDelaySeconds = 10.f;
		/** Decay multiplier after the worst possible crime (lerp from 1 at severity 0). */
		float SevereCrimeDecayScale = 0.25f;
		/** How long the worst recent crime keeps slowing the decay. */
		float SeverityMemorySeconds = 300.f;
		/** Heat of the worst crime in the table (murder, 80) — severity01 = crime heat / this. */
		float MaxCrimeHeat = 80.f;
		/** Decay multiplier while any unit is searching for him. */
		float SearchDecayScale = 0.5f;

		/** Repeats of the same crime type inside this window give diminishing heat. */
		float RepeatWindowSeconds = 60.f;
		/** Each repeat multiplies the gain by this (0.5: 100 %, 50 %, 25 % ...). */
		float RepeatFactor = 0.5f;
		/** Never less than this fraction of the base heat. */
		float RepeatFloor = 0.1f;

		/** A second, third ... witness of the same incident adds this fraction of the incident's heat. */
		float CorroborationFraction = 0.25f;
		/** Corroborations counted per incident. */
		int MaxCorroborations = 2;

		/** Two crime observations of the same type this close in space and time are one incident. */
		float IncidentMergeDistanceCm = 1500.f;
		float IncidentMergeSeconds = 5.f;

		/** Heat change history kept for the debug log. */
		int HistorySize = 16;
	};

	struct FHeatChange
	{
		double Time = 0.0;
		float Delta = 0.f;
		float HeatAfter = 0.f;
		std::string Reason;
	};

	// ------------------------------------------------------------------------------------------------------------

	class FModel
	{
	public:
		explicit FModel(const FConfig& InConfig) : Config(InConfig) {}

		float GetHeat() const { return Heat; }
		EWanted GetWanted() const { return Wanted; }
		const std::deque<FHeatChange>& GetHistory() const { return History; }

		/** Cheats / load: sets heat directly; the wanted level snaps without hysteresis. */
		void SetHeat(float Value, double Now, const std::string& Reason)
		{
			const float Before = Heat;
			Heat = Clamp01To100(Value);
			Wanted = LevelFor(Heat);
			Record(Now, Heat - Before, Reason);
		}

		/** A crime the police now know about (seen by police, or the first report of an incident).
		 *  CrimeType: any stable id per crime tag. Returns the heat actually added. */
		float AddCrime(int CrimeType, float BaseHeat, double Now, const std::string& Reason)
		{
			PruneRepeats(Now);
			int Repeats = 0;
			for (const FRepeat& R : Repeats_)
			{
				Repeats += R.Type == CrimeType ? 1 : 0;
			}
			const float Factor = std::max(std::pow(Config.RepeatFactor, float(Repeats)), Config.RepeatFloor);
			Repeats_.push_back({CrimeType, Now});

			if (BaseHeat > WorstRecentCrimeHeat || Now - WorstRecentCrimeTime > Config.SeverityMemorySeconds)
			{
				WorstRecentCrimeHeat = BaseHeat;
				WorstRecentCrimeTime = Now;
			}
			LastActivityTime = Now;
			return Add(BaseHeat * Factor, Now, Reason);
		}

		/** Another witness of an already-known incident. Index = 1 for the second witness, 2 for the third ... */
		float AddCorroboration(float IncidentHeat, int Index, double Now, const std::string& Reason)
		{
			if (Index < 1 || Index > Config.MaxCorroborations)
			{
				return 0.f;
			}
			LastActivityTime = Now;
			return Add(IncidentHeat * Config.CorroborationFraction, Now, Reason);
		}

		/** Police eyes on him (resets the decay delay). */
		void NotifySighting(double Now) { LastActivityTime = Now; }

		void Tick(double Now, float DeltaSeconds, bool bSeenByPolice, bool bSearchActive)
		{
			if (bSeenByPolice)
			{
				LastActivityTime = Now; // the existing rule: no decay at all while they can see you
				return;
			}
			if (Now - LastActivityTime < Config.DecayDelaySeconds || Heat <= 0.f)
			{
				return;
			}
			const bool bSevere = Now - WorstRecentCrimeTime <= Config.SeverityMemorySeconds;
			const float Severity01 = bSevere && Config.MaxCrimeHeat > 0.f
				? std::min(WorstRecentCrimeHeat / Config.MaxCrimeHeat, 1.f) : 0.f;
			const float SeverityScale = 1.f + (Config.SevereCrimeDecayScale - 1.f) * Severity01;
			const float SearchScale = bSearchActive ? Config.SearchDecayScale : 1.f;

			const float Before = Heat;
			Heat = std::max(0.f, Heat - Config.DecayPerSecond * SeverityScale * SearchScale * DeltaSeconds);
			UpdateWanted();
			// Decay is not recorded per tick (it would flood the history); level changes are.
			if (LevelFor(Before) != LevelFor(Heat) || (Before > 0.f && Heat == 0.f))
			{
				Record(Now, Heat - Before, "decay");
			}
		}

		/** Pure function of the thresholds, no hysteresis. */
		EWanted LevelFor(float H) const
		{
			if (H >= Config.LethalHeat) return EWanted::Lethal;
			if (H >= Config.PursuitHeat) return EWanted::Pursuit;
			if (H >= Config.StopHeat) return EWanted::Stop;
			return EWanted::None;
		}

	private:
		struct FRepeat { int Type; double Time; };

		static float Clamp01To100(float V) { return std::min(std::max(V, 0.f), 100.f); }

		float Threshold(EWanted Level) const
		{
			switch (Level)
			{
			case EWanted::Stop: return Config.StopHeat;
			case EWanted::Pursuit: return Config.PursuitHeat;
			case EWanted::Lethal: return Config.LethalHeat;
			default: return 0.f;
			}
		}

		void UpdateWanted()
		{
			// Up: immediately at the threshold. Down: only below threshold − hysteresis, one level at a time
			// re-checked in the loop so a big drop can pass several levels.
			const EWanted Target = LevelFor(Heat);
			if (Target > Wanted)
			{
				Wanted = Target;
				return;
			}
			while (Wanted != EWanted::None && Heat < Threshold(Wanted) - Config.WantedHysteresisHeat)
			{
				Wanted = EWanted(uint8_t(Wanted) - 1);
			}
		}

		float Add(float Amount, double Now, const std::string& Reason)
		{
			const float Before = Heat;
			Heat = Clamp01To100(Heat + Amount);
			UpdateWanted();
			Record(Now, Heat - Before, Reason);
			return Heat - Before;
		}

		void PruneRepeats(double Now)
		{
			Repeats_.erase(std::remove_if(Repeats_.begin(), Repeats_.end(),
				[&](const FRepeat& R) { return Now - R.Time > Config.RepeatWindowSeconds; }), Repeats_.end());
		}

		void Record(double Now, float Delta, const std::string& Reason)
		{
			History.push_back({Now, Delta, Heat, Reason});
			while (int(History.size()) > std::max(Config.HistorySize, 1))
			{
				History.pop_front();
			}
		}

		FConfig Config;
		float Heat = 0.f;
		EWanted Wanted = EWanted::None;
		double LastActivityTime = -1e9;
		float WorstRecentCrimeHeat = 0.f;
		double WorstRecentCrimeTime = -1e9;
		std::vector<FRepeat> Repeats_;
		std::deque<FHeatChange> History;
	};

	// ------------------------------------------------------------------------------------------------------------

	struct FVec { float X = 0.f, Y = 0.f, Z = 0.f; };

	/** What a delivered report tells the engine side to do. */
	struct FDelivery
	{
		uint64_t IncidentId = 0;
		uint64_t WitnessId = 0;
		int CrimeType = 0;
		float BaseHeat = 0.f;
		FVec Location;          // where the crime happened — NOT where he is now
		double CrimeTime = 0.0; // when it happened — the police fix is this old
		int CorroborationIndex = 0; // 0 = first report of the incident (adds crime heat), 1.. = corroboration
	};

	/** Civilian witnesses' reports on their way to the police. The queue knows nothing about how a witness gets
	 *  there (phone booth, a patrol, a station — the foot AI's job later); it only holds a due time, which the
	 *  engine side can bring forward (witness reached a phone) or cancel (witness dead / silenced). */
	class FReportQueue
	{
	public:
		explicit FReportQueue(const FConfig& InConfig) : Config(InConfig) {}

		/** Finds the incident this observation belongs to (same type, close in space and time) or makes one. */
		uint64_t FindOrCreateIncident(int CrimeType, const FVec& Where, double When)
		{
			const float MergeSq = Config.IncidentMergeDistanceCm * Config.IncidentMergeDistanceCm;
			for (FIncident& I : Incidents)
			{
				const float DX = I.Where.X - Where.X, DY = I.Where.Y - Where.Y, DZ = I.Where.Z - Where.Z;
				if (I.CrimeType == CrimeType && std::abs(I.When - When) <= Config.IncidentMergeSeconds
					&& DX * DX + DY * DY + DZ * DZ <= MergeSq)
				{
					return I.Id;
				}
			}
			Incidents.push_back({++LastIncidentId, CrimeType, Where, When, 0});
			return LastIncidentId;
		}

		/** A witness will report this incident at DueTime. One pending report per witness per incident. */
		void Queue(uint64_t WitnessId, uint64_t IncidentId, float BaseHeat, double DueTime)
		{
			for (const FPending& P : Pending)
			{
				if (P.WitnessId == WitnessId && P.IncidentId == IncidentId)
				{
					return;
				}
			}
			Pending.push_back({WitnessId, IncidentId, BaseHeat, DueTime});
		}

		/** Police saw it themselves: the heat went in directly; queued civilian reports only corroborate. */
		void MarkIncidentKnown(uint64_t IncidentId)
		{
			if (FIncident* I = Find(IncidentId))
			{
				I->Reports = std::max(I->Reports, 1);
			}
		}

		/** Bring a witness's reports forward (reached a phone / a patrol). */
		void Expedite(uint64_t WitnessId, double NewDueTime)
		{
			for (FPending& P : Pending)
			{
				if (P.WitnessId == WitnessId)
				{
					P.DueTime = std::min(P.DueTime, NewDueTime);
				}
			}
		}

		/** Witness died, was silenced or paid off: their reports never arrive. */
		void CancelWitness(uint64_t WitnessId)
		{
			Pending.erase(std::remove_if(Pending.begin(), Pending.end(),
				[&](const FPending& P) { return P.WitnessId == WitnessId; }), Pending.end());
		}

		bool HasPending(uint64_t WitnessId) const
		{
			return std::any_of(Pending.begin(), Pending.end(), [&](const FPending& P) { return P.WitnessId == WitnessId; });
		}

		/** Reports due by Now, in due order. Old incidents with nothing pending are forgotten. */
		std::vector<FDelivery> Collect(double Now) { return Deliver([&](const FPending& P) { return P.DueTime <= Now; }, Now); }

		/** Everything still pending, delivered now — called before a save so saving can't erase a witness. */
		std::vector<FDelivery> FlushAll(double Now) { return Deliver([](const FPending&) { return true; }, Now); }

		size_t NumPending() const { return Pending.size(); }
		size_t NumIncidents() const { return Incidents.size(); }

	private:
		struct FIncident { uint64_t Id; int CrimeType; FVec Where; double When; int Reports; };
		struct FPending { uint64_t WitnessId; uint64_t IncidentId; float BaseHeat; double DueTime; };

		FIncident* Find(uint64_t Id)
		{
			for (FIncident& I : Incidents)
			{
				if (I.Id == Id) return &I;
			}
			return nullptr;
		}

		template <typename Pred>
		std::vector<FDelivery> Deliver(Pred IsDue, double Now)
		{
			std::vector<FPending> Due;
			std::vector<FPending> Keep;
			for (const FPending& P : Pending)
			{
				(IsDue(P) ? Due : Keep).push_back(P);
			}
			Pending.swap(Keep);
			std::stable_sort(Due.begin(), Due.end(), [](const FPending& A, const FPending& B) { return A.DueTime < B.DueTime; });

			std::vector<FDelivery> Out;
			for (const FPending& P : Due)
			{
				FIncident* I = Find(P.IncidentId);
				if (!I)
				{
					continue;
				}
				Out.push_back({I->Id, P.WitnessId, I->CrimeType, P.BaseHeat, I->Where, I->When, I->Reports});
				++I->Reports;
			}

			// Forget incidents nobody is still reporting, once they are too old to merge with.
			Incidents.erase(std::remove_if(Incidents.begin(), Incidents.end(), [&](const FIncident& I)
			{
				const bool bPending = std::any_of(Pending.begin(), Pending.end(),
					[&](const FPending& P) { return P.IncidentId == I.Id; });
				return !bPending && Now - I.When > Config.IncidentMergeSeconds;
			}), Incidents.end());
			return Out;
		}

		FConfig Config;
		std::vector<FIncident> Incidents;
		std::vector<FPending> Pending;
		uint64_t LastIncidentId = 0;
	};
}
