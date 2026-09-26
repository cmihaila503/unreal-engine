// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/TimeOfDay time_of_day_test.cpp -o tod && ./tod
#include "TimeOfDayRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarTime;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(std::fabs(double(a) - double(b)) <= (e))
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	const FSunConfig C;

	Run("clock: advances at the configured rate and rolls the day", [&]
	{
		FGameClock K(23.f * 60.f, 3);
		CHECK(!K.Advance(60.f, 2880.f));       // 60 real s at 48 min/day = 30 game min
		NEAR(K.GetMinutes(), 23.f * 60.f + 30.f, 1e-3);
		CHECK(K.Advance(120.f, 2880.f));       // +60 game min: past midnight
		NEAR(K.GetMinutes(), 30.f, 1e-3);
		CHECK(K.GetDay() == 4);
		CHECK(!K.Advance(10.f, 0.f));          // frozen clock
		K.Set(-30.f);
		NEAR(K.GetMinutes(), 1410.f, 1e-3);    // wraps negatives
		K.Set(3000.f);
		NEAR(K.GetMinutes(), 120.f, 1e-3);
	});

	Run("clock: skipping hours (SkipHours) rolls several days", [&]
	{
		FGameClock K(22.f * 60.f, 1);
		CHECK(K.Advance(30.f * 60.f, MinutesPerDay)); // 30 h in a cell: 22:00 day 1 -> 04:00 day 3
		NEAR(K.GetMinutes(), 4.f * 60.f, 1e-2);
		CHECK(K.GetDay() == 3);
	});

	Run("sun: horizon at sunrise and sunset, highest at mid-day, below at night", [&]
	{
		NEAR(SunAngles(C.SunriseHour, C).PitchDeg, 0.f, 1e-3);
		NEAR(SunAngles(C.SunsetHour, C).PitchDeg, 0.f, 1e-3);
		NEAR(SunAngles(13.f, C).PitchDeg, -C.MaxElevationDeg, 1e-3); // (6+20)/2
		CHECK(SunAngles(1.f, C).PitchDeg > 0.f);
		NEAR(SunAngles(1.f, C).PitchDeg, C.NightDepthDeg, 1e-3);     // (20 + 10/2) mod 24 = 1:00
		NEAR(SunAngles(C.SunriseHour, C).YawDeg, C.SunriseYawDeg, 1e-3);
		NEAR(SunAngles(C.SunsetHour, C).YawDeg, C.SunsetYawDeg, 1e-3);
	});

	Run("light level: 0 at night, 1 by day, 0.5 exactly at sunrise / sunset", [&]
	{
		NEAR(LightLevel01(2.f, C), 0.f, 1e-4);
		NEAR(LightLevel01(12.f, C), 1.f, 1e-4);
		NEAR(LightLevel01(C.SunriseHour, C), 0.5f, 1e-4);
		NEAR(LightLevel01(C.SunsetHour, C), 0.5f, 1e-4);
		CHECK(LightLevel01(C.SunriseHour - 0.3f, C) < 0.5f);
		CHECK(LightLevel01(C.SunsetHour - 0.3f, C) > 0.5f);
		CHECK(IsNight(23.f, C));
		CHECK(!IsNight(9.f, C));
		CHECK(IsNight(C.SunsetHour + 0.5f, C));
	});

	Run("hourly curve: interpolates and wraps at midnight", [&]
	{
		std::array<float, 24> V{};
		for (int i = 0; i < 24; ++i) { V[i] = float(i); }
		NEAR(HourlyCurve(V, 5.5f), 5.5f, 1e-4);
		NEAR(HourlyCurve(V, 23.5f), 11.5f, 1e-4); // halfway 23 -> 0
		NEAR(HourlyCurve(V, 24.f), 0.f, 1e-4);
		NEAR(HourlyCurve(V, -1.f), 23.f, 1e-4);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
