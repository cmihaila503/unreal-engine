// Garages — respray, repair, storage, impound. Pure C++17, unit-tested (Handoff/Garage/Tests).
//
// The police describe a car by model and colour (patch to UFactionMemorySubsystem). A respray gives it a colour far
// from the old one, so the description stops matching; done out of sight during a chase it also cools the chase to a
// stop-level heat (the GTA respray). Storage keeps cars (saved); an arrest in a car sends it to the impound lot, where
// it costs money to get back.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace MurdarGarage
{
	struct FColor3 { float R = 0.f, G = 0.f, B = 0.f; };

	/** Perceptual-ish distance ("redmean"), 0..~3. Two colours a witness would call the same are under ~0.35. */
	inline float ColorDistance(const FColor3& A, const FColor3& B)
	{
		const float Rm = 0.5f * (A.R + B.R);
		const float Dr = A.R - B.R, Dg = A.G - B.G, Db = A.B - B.B;
		return std::sqrt((2.f + Rm) * Dr * Dr + 4.f * Dg * Dg + (3.f - Rm) * Db * Db);
	}

	/** Does this car still fit the description? Same model and a colour a witness would call the same. */
	inline bool MatchesDescription(bool bSameModel, const FColor3& Described, const FColor3& Now, float SameColour = 0.35f)
	{
		return bSameModel && ColorDistance(Described, Now) <= SameColour;
	}

	/** A palette colour clearly different from Current; Rand01 picks among the candidates. -1 if none qualifies. */
	inline int PickNewColour(const FColor3& Current, const std::vector<FColor3>& Palette, float Rand01, float MinDistance = 0.6f)
	{
		std::vector<int> Ok;
		for (int i = 0; i < int(Palette.size()); ++i) { if (ColorDistance(Current, Palette[i]) >= MinDistance) { Ok.push_back(i); } }
		if (Ok.empty()) { return -1; }
		const int k = std::min(int(Ok.size()) - 1, int(std::clamp(Rand01, 0.f, 1.f) * Ok.size()));
		return Ok[k];
	}

	struct FPrices
	{
		float Respray = 150.f;
		float RepairFull = 400.f;     // at damage 1
		float HotMultiplier = 2.f;    // resprayed while wanted: the man knows
		float ImpoundBase = 200.f;
		float ImpoundPerDay = 50.f;
	};

	/** Respray always repairs too (as in GTA): paint + damage. */
	inline float ResprayPrice(float Damage01, bool bWanted, const FPrices& P)
	{
		const float Base = P.Respray + P.RepairFull * std::clamp(Damage01, 0.f, 1.f);
		return std::round(Base * (bWanted ? P.HotMultiplier : 1.f));
	}

	inline float ImpoundFee(int DaysHeld, const FPrices& P)
	{
		return P.ImpoundBase + P.ImpoundPerDay * float(std::max(0, DaysHeld));
	}

	enum class EResprayResult { Ok, Seen, Moving, NoMoney };

	struct FResprayCheck
	{
		float SpeedKph = 0.f;
		float SinceSeenByPolice = 1e9f;
		float Cash = 0.f;
		float Price = 0.f;
	};

	/** Stopped inside, nobody from the police watching, and the money. */
	inline EResprayResult CanRespray(const FResprayCheck& C, float MaxKph = 3.f, float UnseenSeconds = 5.f)
	{
		if (C.SinceSeenByPolice < UnseenSeconds) { return EResprayResult::Seen; }
		if (C.SpeedKph > MaxKph) { return EResprayResult::Moving; }
		if (C.Cash + 1e-3f < C.Price) { return EResprayResult::NoMoney; }
		return EResprayResult::Ok;
	}

	/** Heat after a respray: a chase out of sight cools to just under a stop; a stop-level heat stays as it is. */
	inline float HeatAfterRespray(float Heat, float HeatStop, float HeatPursuit)
	{
		return Heat >= HeatPursuit ? HeatStop - 1.f : Heat;
	}

	/** Storage slots: index of the first free one, or -1 when full. */
	inline int FreeSlot(const std::vector<bool>& Used)
	{
		for (int i = 0; i < int(Used.size()); ++i) { if (!Used[i]) { return i; } }
		return -1;
	}
}
