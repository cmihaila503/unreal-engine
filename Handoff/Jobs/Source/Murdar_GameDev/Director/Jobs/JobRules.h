// Jobs — the repeatable work that pays: deliveries, smuggling to the border, bringing a car, collecting a debt.
// Pure C++17, unit-tested (Handoff/Jobs/Tests). A job becomes a MurdarMission::FMissionSpec, so it runs on the same
// tested runtime as the story missions (Handoff/Missions).
//
// The 90s way to get one: the pager beeps ("Sună-l pe Vali"), you find a payphone, you call. A page nobody answers
// expires. Pages come only when he's free: no job, no mission, no police on him.

#pragma once

#if __has_include("Director/Missions/MissionRules.h")
#include "Director/Missions/MissionRules.h" // in the project (Handoff/Missions)
#else
#include "MissionRules.h"                   // the unit test's include path
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace MurdarJobs
{
	enum class EKind { Delivery, Smuggling, CarDelivery, Collection };

	struct FPoint { float X = 0.f, Y = 0.f, Z = 0.f; bool bBorder = false; };

	struct FTuning
	{
		float PickupMinCm = 30000.f, PickupMaxCm = 150000.f;   // 300 m .. 1.5 km from him
		float DropMinCm = 80000.f, DropMaxCm = 300000.f;       // 800 m .. 3 km from the pickup
		float BasePay = 100.f;
		float PayPerKm = 120.f;
		float SmugglingMultiplier = 2.5f;
		float CarDeliveryBase = 600.f;
		float CollectionBase = 250.f;
		float AverageKph = 45.f;       // city driving, for time limits
		float TimeSlack = 1.35f;
		float TimeExtraSeconds = 60.f;
		float PageMinSeconds = 240.f, PageMaxSeconds = 600.f;
		float PageExpirySeconds = 300.f;
		float DebtorPaysChance = 0.6f;
	};

	inline float Dist(const FPoint& A, const FPoint& B)
	{
		const float Dx = A.X - B.X, Dy = A.Y - B.Y, Dz = A.Z - B.Z;
		return std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
	}

	/** Index of a point whose distance from From is in [Min, Max], picked by Rand01 among those; the nearest-to-range
	 *  one when none is inside; -1 when the list is empty. Skip = an index to leave out. */
	inline int PickPoint(const std::vector<FPoint>& Pts, const FPoint& From, float MinCm, float MaxCm, float Rand01, int Skip = -1, int BorderOnly = -1)
	{
		std::vector<int> In;
		int Best = -1;
		float BestOff = 1e30f;
		for (int i = 0; i < int(Pts.size()); ++i)
		{
			if (i == Skip) { continue; }
			if (BorderOnly >= 0 && Pts[i].bBorder != (BorderOnly == 1)) { continue; }
			const float D = Dist(Pts[i], From);
			if (D >= MinCm && D <= MaxCm) { In.push_back(i); }
			const float Off = D < MinCm ? MinCm - D : (D > MaxCm ? D - MaxCm : 0.f);
			if (Off < BestOff) { BestOff = Off; Best = i; }
		}
		if (!In.empty()) { return In[std::min(int(In.size()) - 1, int(std::clamp(Rand01, 0.f, 1.f) * In.size()))]; }
		return Best;
	}

	struct FJob
	{
		EKind Kind = EKind::Delivery;
		int Pickup = -1;      // Delivery / Smuggling: where the package is; Collection: where the debtor is
		int Drop = -1;        // Delivery / Smuggling / CarDelivery: where it goes
		float Pay = 0.f;
		float TimeLimit = 0.f;
		bool bNoPolice = false;
		bool bValid = false;
	};

	inline float TimeFor(float DistanceCm, const FTuning& T)
	{
		return DistanceCm / (T.AverageKph / 0.036f) * T.TimeSlack + T.TimeExtraSeconds;
	}

	inline float RoundPay(float P) { return std::round(P / 10.f) * 10.f; }

	/** A job of this kind from where he stands. Rand in [0,1): two independent draws. */
	inline FJob Generate(EKind Kind, const std::vector<FPoint>& Pickups, const std::vector<FPoint>& Drops, const FPoint& Player,
		float RandA, float RandB, float PayMultiplier, const FTuning& T)
	{
		FJob J;
		J.Kind = Kind;
		switch (Kind)
		{
		case EKind::Delivery:
		case EKind::Smuggling:
		{
			J.Pickup = PickPoint(Pickups, Player, T.PickupMinCm, T.PickupMaxCm, RandA);
			if (J.Pickup < 0) { return J; }
			J.Drop = PickPoint(Drops, Pickups[J.Pickup], T.DropMinCm, T.DropMaxCm, RandB, -1, Kind == EKind::Smuggling ? 1 : 0);
			if (J.Drop < 0) { return J; }
			const float D = Dist(Pickups[J.Pickup], Drops[J.Drop]);
			J.Pay = T.BasePay + T.PayPerKm * D / 100000.f;
			if (Kind == EKind::Smuggling) { J.Pay *= T.SmugglingMultiplier; J.bNoPolice = true; }
			J.TimeLimit = TimeFor(Dist(Player, Pickups[J.Pickup]) + D, T);
			break;
		}
		case EKind::CarDelivery:
			J.Drop = PickPoint(Drops, Player, T.DropMinCm, T.DropMaxCm, RandB, -1, 0);
			if (J.Drop < 0) { return J; }
			J.Pay = T.CarDeliveryBase;
			J.TimeLimit = 0.f; // find the car first: no clock
			break;
		case EKind::Collection:
			J.Pickup = PickPoint(Pickups, Player, T.PickupMinCm, T.PickupMaxCm, RandA);
			if (J.Pickup < 0) { return J; }
			J.Pay = T.CollectionBase;
			J.TimeLimit = TimeFor(Dist(Player, Pickups[J.Pickup]), T) + 120.f;
			break;
		}
		J.Pay = RoundPay(J.Pay * std::max(0.1f, PayMultiplier));
		J.bValid = true;
		return J;
	}

	/** Car delivery pays for what arrives. */
	inline float CarDeliveryPay(float Pay, float Damage01) { return RoundPay(Pay * (1.f - 0.8f * std::clamp(Damage01, 0.f, 1.f))); }

	/** The job as a mission spec. Synthetic facts the job subsystem answers: "Job.InTargetCar". */
	inline MurdarMission::FMissionSpec ToSpec(const FJob& J, const std::vector<FPoint>& Pickups, const std::vector<FPoint>& Drops)
	{
		MurdarMission::FMissionSpec S;
		S.Id = "Job";
		S.TimeLimitSeconds = J.TimeLimit;
		if (J.bNoPolice) { S.FailOnEvents.push_back("Event.Police.PursuitStarted"); }
		auto Reach = [](const std::string& Id, const FPoint& P, float R)
		{
			MurdarMission::FObjectiveSpec O;
			O.Id = Id; O.bReach = true; O.ReachLocation = { P.X, P.Y, P.Z }; O.ReachRadiusCm = R;
			return O;
		};
		switch (J.Kind)
		{
		case EKind::Delivery:
		case EKind::Smuggling:
			S.Objectives.push_back(Reach("pickup", Pickups[J.Pickup], 500.f));
			S.Objectives.push_back(Reach("drop", Drops[J.Drop], 600.f));
			break;
		case EKind::CarDelivery:
		{
			MurdarMission::FObjectiveSpec Get; Get.Id = "get_car"; Get.RequiredFacts.push_back("Job.InTargetCar");
			S.Objectives.push_back(Get);
			MurdarMission::FObjectiveSpec O = Reach("drop", Drops[J.Drop], 700.f);
			O.RequiredFacts.push_back("Job.InTargetCar");
			S.Objectives.push_back(O);
			break;
		}
		case EKind::Collection:
		{
			MurdarMission::FObjectiveSpec O; O.Id = "collect"; O.CompleteOnEvent = "Event.Job.Collected";
			S.Objectives.push_back(O);
			break;
		}
		}
		return S;
	}

	/** Pager: seconds to the next page; Rand01 in the range. */
	inline float NextPageIn(float Rand01, const FTuning& T) { return T.PageMinSeconds + (T.PageMaxSeconds - T.PageMinSeconds) * std::clamp(Rand01, 0.f, 1.f); }

	inline bool CanPage(bool bJobRunning, bool bMissionRunning, int Wanted) { return !bJobRunning && !bMissionRunning && Wanted == 0; }

	/** The debtor: pays when asked, or runs; caught (asked again, close) he always pays. */
	inline bool DebtorPays(bool bCaught, float Rand01, const FTuning& T) { return bCaught || Rand01 < T.DebtorPaysChance; }
}
