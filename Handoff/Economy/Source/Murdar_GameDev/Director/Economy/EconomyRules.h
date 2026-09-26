// Economy — the arithmetic of lei. Pure C++17, unit-tested (Handoff/Economy/Tests).
// Cash is one number (Stat.Money on the narrative state, so it saves). Prices are round like real prices (nobody asks
// 437 lei), may inflate per game day (Romania, the 90s), and nothing ever takes the wallet below zero.

#pragma once

#include <cmath>
#include <string>

namespace MurdarEconomy
{
	/** Rounds to what a price tag would say: 1 up to 20, then 5, 10, 50, 100, 500. Never below 1 for a positive price. */
	inline float NicePrice(float Raw)
	{
		if (Raw <= 0.f) { return 0.f; }
		const float Step = Raw < 20.f ? 1.f : Raw < 100.f ? 5.f : Raw < 500.f ? 10.f : Raw < 2000.f ? 50.f : Raw < 10000.f ? 100.f : 500.f;
		const float R = std::round(Raw / Step) * Step;
		return R < 1.f ? 1.f : R;
	}

	/** Base price after Day days of compounding daily inflation (0 = none), rounded to a nice price. */
	inline float Inflated(float Base, int Day, float DailyRate)
	{
		if (Base <= 0.f) { return 0.f; }
		const float Factor = (DailyRate > 0.f && Day > 0) ? std::pow(1.f + DailyRate, float(Day)) : 1.f;
		return NicePrice(Base * Factor);
	}

	struct FPayResult
	{
		bool bPaid = false;
		float Cash = 0.f;      // after
		float Shortfall = 0.f; // how much was missing (0 when paid)
	};

	/** Buying: all or nothing. */
	inline FPayResult TryPay(float Cash, float Price)
	{
		FPayResult R;
		if (Price <= 0.f) { R.bPaid = true; R.Cash = Cash; return R; }
		if (Cash + 1e-3f >= Price) { R.bPaid = true; R.Cash = Cash - Price; if (R.Cash < 0.f) { R.Cash = 0.f; } return R; }
		R.Cash = Cash;
		R.Shortfall = Price - Cash;
		return R;
	}

	/** Money already gone (the cop took it, it was stolen): as much as there is. Shortfall = what wasn't there. */
	inline FPayResult Take(float Cash, float Amount)
	{
		FPayResult R;
		R.bPaid = Amount <= Cash + 1e-3f;
		R.Cash = Amount >= Cash ? 0.f : Cash - Amount;
		R.Shortfall = Amount > Cash ? Amount - Cash : 0.f;
		return R;
	}

	/** A fraction of the cash, rounded down to whole lei, at least MinLoss (capped at what there is). Arrest, hospital. */
	inline float Loss(float Cash, float Fraction, float MinLoss = 0.f)
	{
		if (Cash <= 0.f) { return 0.f; }
		float L = std::floor(Cash * (Fraction < 0.f ? 0.f : (Fraction > 1.f ? 1.f : Fraction)));
		if (L < MinLoss) { L = MinLoss; }
		return L > Cash ? Cash : L;
	}

	/** Cash on a body: Min..Max by Rand01, rounded like money in a pocket (to 5). */
	inline float PocketCash(float Min, float Max, float Rand01)
	{
		if (Max < Min) { const float T = Min; Min = Max; Max = T; }
		const float R = Rand01 < 0.f ? 0.f : (Rand01 > 1.f ? 1.f : Rand01);
		const float Raw = Min + (Max - Min) * R;
		return std::round(Raw / 5.f) * 5.f;
	}

	/** "1.250 lei" — Romanian thousands separator; for the menu and the cheat, never on the HUD. */
	inline std::string FormatLei(float Amount)
	{
		const long long V = (long long)std::llround(Amount < 0.f ? -Amount : Amount);
		std::string Digits = std::to_string(V), Out;
		for (int i = 0; i < int(Digits.size()); ++i)
		{
			if (i > 0 && (int(Digits.size()) - i) % 3 == 0) { Out += '.'; }
			Out += Digits[i];
		}
		return (Amount < -0.5f ? "-" : "") + Out + " lei";
	}
}
