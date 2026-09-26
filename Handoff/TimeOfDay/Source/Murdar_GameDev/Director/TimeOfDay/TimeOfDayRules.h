// Time of day — the rules, pure C++17 (unit-tested in Handoff/TimeOfDay/Tests). The engine side
// (UTimeOfDaySubsystem) owns the clock's storage, the sun actor and the events.
//   FGameClock        minutes since midnight + day number, advanced by real seconds at a configurable rate
//   SunAngles         the sun's pitch/yaw for an hour (pitch < 0 = above the horizon, the engine's convention)
//   LightLevel01      0 at night, 1 at day, smooth through dawn and dusk (twilight)
//   HourlyCurve       24 values (people, traffic...) interpolated at a fractional hour, wrapping at midnight

#pragma once

#include <array>
#include <cmath>

namespace MurdarTime
{
	constexpr float MinutesPerDay = 1440.f;
	constexpr float Pi = 3.14159265358979f;

	struct FSunConfig
	{
		float SunriseHour = 6.0f;
		float SunsetHour = 20.0f;
		float MaxElevationDeg = 62.f;   // summer noon in Bucharest is ~67; a little lower reads better
		float NightDepthDeg = 30.f;     // how far below the horizon the sun goes at midnight
		float TwilightHours = 0.75f;    // dawn / dusk length on each side of sunrise / sunset
		float SunriseYawDeg = 90.f;     // east
		float SunsetYawDeg = 270.f;     // west
	};

	class FGameClock
	{
	public:
		FGameClock(float StartMinutes = 480.f, int StartDay = 0) : Minutes(Wrap(StartMinutes)), Day(StartDay) {}

		/** RealSecondsPerGameDay: e.g. 2880 = 48 real minutes per game day. Returns true if the day rolled over. */
		bool Advance(float RealSeconds, float RealSecondsPerGameDay)
		{
			if (RealSecondsPerGameDay <= 0.f || RealSeconds <= 0.f)
			{
				return false;
			}
			Minutes += RealSeconds * (MinutesPerDay / RealSecondsPerGameDay);
			bool bRolled = false;
			while (Minutes >= MinutesPerDay)
			{
				Minutes -= MinutesPerDay;
				++Day;
				bRolled = true;
			}
			return bRolled;
		}

		void Set(float NewMinutes) { Minutes = Wrap(NewMinutes); }
		float GetMinutes() const { return Minutes; }
		float GetHour() const { return Minutes / 60.f; }
		int GetDay() const { return Day; }
		void SetDay(int D) { Day = D; }

		static float Wrap(float M)
		{
			M = std::fmod(M, MinutesPerDay);
			return M < 0.f ? M + MinutesPerDay : M;
		}

	private:
		float Minutes = 480.f;
		int Day = 0;
	};

	struct FSunAngles { float PitchDeg = 0.f; float YawDeg = 0.f; };

	/** Elevation follows a half sine over the day and a shallower half sine under the horizon at night. */
	inline FSunAngles SunAngles(float Hour, const FSunConfig& C)
	{
		const float DayLen = C.SunsetHour - C.SunriseHour;
		const float NightLen = 24.f - DayLen;
		FSunAngles A;
		if (Hour >= C.SunriseHour && Hour <= C.SunsetHour)
		{
			const float T = (Hour - C.SunriseHour) / DayLen; // 0..1 over the day
			A.PitchDeg = -C.MaxElevationDeg * std::sin(Pi * T);
			A.YawDeg = C.SunriseYawDeg + (C.SunsetYawDeg - C.SunriseYawDeg) * T;
		}
		else
		{
			const float SinceSunset = Hour > C.SunsetHour ? Hour - C.SunsetHour : Hour + 24.f - C.SunsetHour;
			const float T = SinceSunset / NightLen; // 0..1 over the night
			A.PitchDeg = C.NightDepthDeg * std::sin(Pi * T);
			A.YawDeg = C.SunsetYawDeg; // parked in the west; nobody sees it
		}
		return A;
	}

	/** 1 in full day, 0 in full night, smoothstep through the twilight around sunrise and sunset. */
	inline float LightLevel01(float Hour, const FSunConfig& C)
	{
		auto Smooth = [](float X) { X = X < 0.f ? 0.f : (X > 1.f ? 1.f : X); return X * X * (3.f - 2.f * X); };
		const float Tw = C.TwilightHours > 0.01f ? C.TwilightHours : 0.01f;
		const float Dawn = Smooth((Hour - (C.SunriseHour - Tw)) / (2.f * Tw));
		const float Dusk = 1.f - Smooth((Hour - (C.SunsetHour - Tw)) / (2.f * Tw));
		return Dawn < Dusk ? Dawn : Dusk;
	}

	/** Night for gameplay: the light is below this. Default half-way through twilight. */
	inline bool IsNight(float Hour, const FSunConfig& C, float NightBelow = 0.5f)
	{
		return LightLevel01(Hour, C) < NightBelow;
	}

	/** 24 hourly values, linear between hours, 23:xx interpolates towards 00:00. */
	inline float HourlyCurve(const std::array<float, 24>& Values, float Hour)
	{
		float H = std::fmod(Hour, 24.f);
		if (H < 0.f) H += 24.f;
		const int I = int(H) % 24;
		const int J = (I + 1) % 24;
		const float F = H - float(int(H));
		return Values[I] + (Values[J] - Values[I]) * F;
	}
}
