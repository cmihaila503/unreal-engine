// Weather — what the sky does, and what that does to the streets. Pure C++17, unit-tested (Handoff/Weather/Tests).
//   A Markov chain picks the next state each game hour (clear tends to stay clear, a storm passes into rain).
//   States blend over minutes, never snap. Roads get wet with rain and dry afterwards (slower at night).
//   Derived: grip (wet asphalt), fewer people outside, fog, headlights sooner, rain on the windscreen.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace MurdarWeather
{
	enum class EState { Clear = 0, Cloudy, Rain, Storm, Fog, Count };
	constexpr int N = int(EState::Count);

	using FRow = std::array<float, N>;
	using FMatrix = std::array<FRow, N>;

	/** Hourly transition weights (rows need not sum to 1). Late-summer Moldavia: mostly fine, afternoon storms. */
	inline FMatrix DefaultMatrix()
	{
		FMatrix M{};
		//          Clear  Cloudy Rain  Storm Fog
		M[0] = FRow{ 0.80f, 0.17f, 0.02f, 0.01f, 0.00f };
		M[1] = FRow{ 0.30f, 0.45f, 0.18f, 0.05f, 0.02f };
		M[2] = FRow{ 0.05f, 0.40f, 0.45f, 0.08f, 0.02f };
		M[3] = FRow{ 0.00f, 0.15f, 0.70f, 0.15f, 0.00f };
		M[4] = FRow{ 0.40f, 0.40f, 0.05f, 0.00f, 0.15f };
		return M;
	}

	/** The next state from Current by the row's weights; Rand01 in [0,1). Fog only forms at night / dawn. */
	inline EState Next(EState Current, float Rand01, bool bFogPossible, const FMatrix& M)
	{
		FRow Row = M[int(Current)];
		if (!bFogPossible) { Row[int(EState::Fog)] = 0.f; }
		float Sum = 0.f;
		for (float W : Row) { Sum += std::max(0.f, W); }
		if (Sum <= 0.f) { return Current; }
		float R = std::clamp(Rand01, 0.f, 0.999999f) * Sum;
		for (int i = 0; i < N; ++i)
		{
			R -= std::max(0.f, Row[i]);
			if (R < 0.f) { return EState(i); }
		}
		return Current;
	}

	/** What each state asks for, at full strength. */
	struct FLook
	{
		float Rain = 0.f;      // 0..1 falling rain
		float Cloud = 0.f;     // 0..1 sky cover (darker)
		float Fog = 0.f;       // 0..1 extra fog density
		float Wind = 0.f;      // 0..1
	};

	inline FLook LookOf(EState S)
	{
		switch (S)
		{
		case EState::Clear:  return { 0.f, 0.1f, 0.f, 0.1f };
		case EState::Cloudy: return { 0.f, 0.7f, 0.05f, 0.3f };
		case EState::Rain:   return { 0.6f, 0.9f, 0.15f, 0.4f };
		case EState::Storm:  return { 1.f, 1.f, 0.2f, 0.9f };
		case EState::Fog:    return { 0.f, 0.5f, 1.f, 0.f };
		default:             return {};
		}
	}

	/** Moves Current toward Target at Rate per second (both ways). */
	inline FLook Blend(FLook C, const FLook& T, float Dt, float Rate)
	{
		auto Step = [Dt, Rate](float A, float B) { const float D = B - A, M = Rate * Dt; return std::fabs(D) <= M ? B : A + (D > 0.f ? M : -M); };
		C.Rain = Step(C.Rain, T.Rain); C.Cloud = Step(C.Cloud, T.Cloud); C.Fog = Step(C.Fog, T.Fog); C.Wind = Step(C.Wind, T.Wind);
		return C;
	}

	struct FTuning
	{
		float WetPerSecondAtFullRain = 1.f / 120.f;   // soaked in two minutes of real downpour
		float DryPerSecondDay = 1.f / 600.f;          // ten minutes of sun
		float DryPerSecondNight = 1.f / 1800.f;
		float WetGrip = 0.75f;                         // asphalt friction scale when soaked
		float RainPeople = 0.35f;                      // share of pedestrians out in a storm
		float BlendRate = 1.f / 90.f;                  // a change takes ~1.5 real minutes
	};

	/** Road wetness: rises with rain, dries when it stops. */
	inline float UpdateWetness(float Wet, float Rain, bool bNight, float Dt, const FTuning& T)
	{
		if (Rain > 0.05f) { Wet += Rain * T.WetPerSecondAtFullRain * Dt; }
		else { Wet -= (bNight ? T.DryPerSecondNight : T.DryPerSecondDay) * Dt; }
		return std::clamp(Wet, 0.f, 1.f);
	}

	inline float GripScale(float Wet, const FTuning& T) { return 1.f - (1.f - T.WetGrip) * std::clamp(Wet, 0.f, 1.f); }

	/** People outside: fewer in rain and fog. */
	inline float PeopleScale(const FLook& L, const FTuning& T)
	{
		const float Bad = std::max(L.Rain, 0.5f * L.Fog);
		return 1.f - (1.f - T.RainPeople) * std::clamp(Bad, 0.f, 1.f);
	}

	/** Extra darkness for headlights: a storm at noon is dim. 0 = none. */
	inline float Gloom(const FLook& L) { return std::clamp(0.5f * L.Cloud + 0.3f * L.Rain + 0.4f * L.Fog, 0.f, 1.f); }
}
