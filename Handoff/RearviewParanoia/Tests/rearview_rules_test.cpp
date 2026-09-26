// Unit tests for RearviewRules.h — plain C++17, no engine.
//   g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/AI/Rearview rearview_rules_test.cpp -o rv && ./rv

#include "RearviewRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarRearview;

static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static void Run(const char* Name, const std::function<void()>& Fn)
{
	const int Before = Failures;
	Fn();
	std::printf("%s %s\n", Failures == Before ? "ok  " : "FAIL", Name);
}

// Speed profile helper: sample a function of time at 10 Hz, return how many times the detector fired.
static int DriveBrake(FBrakeCheckDetector& D, double From, double To, const std::function<float(double)>& Kph)
{
	int Fired = 0;
	for (double T = From; T <= To + 1e-9; T += 0.1) { Fired += D.Sample(T, Kph(T)) ? 1 : 0; }
	return Fired;
}

static int DriveTurn(FInspectionTurnDetector& D, double From, double To, const std::function<float(double)>& Yaw,
	float Kph, const std::function<bool(double)>& Indicating, int& Side)
{
	int Fired = 0;
	for (double T = From; T <= To + 1e-9; T += 0.1)
	{
		const int S = D.Sample(T, Yaw(T), Kph, Indicating(T));
		if (S != 0) { ++Fired; Side = S; }
	}
	return Fired;
}

int main()
{
	const FBrakeCheckConfig BC;
	const FInspectionTurnConfig TC;
	const FVisibilityConfig VC;
	const FExposureConfig EC;

	Run("brake check: a hard tap from 70 to 45 km/h is recognised once", [&]
	{
		FBrakeCheckDetector D(BC);
		const int N = DriveBrake(D, 0.0, 6.0, [](double T) { return T < 2.0 ? 70.f : (T < 3.0 ? float(70.0 - (T - 2.0) * 25.0) : 45.f); });
		CHECK(N == 1);
	});

	Run("brake check: a normal stop at a light is not a check", [&]
	{
		FBrakeCheckDetector D(BC);
		// 50 -> 0 over 4 s (3.5 m/s^2) and it ends at a halt
		CHECK(DriveBrake(D, 0.0, 8.0, [](double T) { return T < 2.0 ? 50.f : float(std::max(0.0, 50.0 - (T - 2.0) * 12.5)); }) == 0);
	});

	Run("brake check: an emergency stop to zero is not a check", [&]
	{
		FBrakeCheckDetector D(BC);
		CHECK(DriveBrake(D, 0.0, 6.0, [](double T) { return T < 2.0 ? 60.f : float(std::max(0.0, 60.0 - (T - 2.0) * 40.0)); }) == 0);
	});

	Run("brake check: slow crawl is never a check; cooldown stops double counting", [&]
	{
		FBrakeCheckDetector A(BC);
		CHECK(DriveBrake(A, 0.0, 4.0, [](double T) { return T < 1.0 ? 30.f : 15.f; }) == 0);
		FBrakeCheckDetector B(BC);
		// two taps 2 s apart: the second falls inside the cooldown
		const int N = DriveBrake(B, 0.0, 8.0, [](double T)
		{
			if (T < 1.0) return 80.f;
			if (T < 1.8) return float(80.0 - (T - 1.0) * 40.0);
			if (T < 3.0) return 48.f;
			if (T < 3.8) return float(48.0 - (T - 3.0) * 30.0);
			return 24.f;
		});
		CHECK(N == 1);
	});

	Run("inspection turn: a sudden 90 degree right at 35 km/h, no indicator", [&]
	{
		FInspectionTurnDetector D(TC);
		int Side = 0;
		const int N = DriveTurn(D, 0.0, 6.0, [](double T) { return T < 2.0 ? 0.f : (T < 3.5 ? float((T - 2.0) * 60.0) : 90.f); },
			35.f, [](double) { return false; }, Side);
		CHECK(N == 1);
		CHECK(Side == 1);
	});

	Run("inspection turn: the same turn, indicated, is ordinary", [&]
	{
		FInspectionTurnDetector D(TC);
		int Side = 0;
		CHECK(DriveTurn(D, 0.0, 6.0, [](double T) { return T < 2.0 ? 0.f : (T < 3.5 ? float((T - 2.0) * 60.0) : 90.f); },
			35.f, [](double T) { return T > 0.5 && T < 2.0; }, Side) == 0);
	});

	Run("inspection turn: a long gentle bend is not one; yaw wrap-around works (left through 180)", [&]
	{
		FInspectionTurnDetector A(TC);
		int Side = 0;
		// 90 degrees over 12 s: too slow
		CHECK(DriveTurn(A, 0.0, 14.0, [](double T) { return float(std::min(T, 12.0) * 7.5); }, 50.f, [](double) { return false; }, Side) == 0);
		FInspectionTurnDetector B(TC);
		// heading 170 -> -100 (i.e. turning left by 90 through the wrap)
		const int N = DriveTurn(B, 0.0, 6.0, [](double T)
		{
			double Y = T < 2.0 ? 170.0 : (T < 3.5 ? 170.0 + (T - 2.0) * 60.0 : 260.0);
			if (Y > 180.0) Y -= 360.0;
			return float(Y);
		}, 30.f, [](double) { return false; }, Side);
		CHECK(N == 1);
		CHECK(Side == 1); // +90 in unwrapped degrees (yaw increasing): same convention as the right turn above
	});

	Run("inspection turn: too slow a car does not count", [&]
	{
		FInspectionTurnDetector D(TC);
		int Side = 0;
		CHECK(DriveTurn(D, 0.0, 6.0, [](double T) { return T < 2.0 ? 0.f : (T < 3.5 ? float((T - 2.0) * 60.0) : 90.f); },
			10.f, [](double) { return false; }, Side) == 0);
	});

	Run("visibility: lights off at night hides the car past a few metres", [&]
	{
		CHECK(CanSeeTarget(5000.f, true, true, true, false, VC));    // lit, 50 m: seen
		CHECK(!CanSeeTarget(5000.f, true, true, false, false, VC));  // dark, 50 m: lost
		CHECK(CanSeeTarget(1500.f, true, true, false, false, VC));   // dark, 15 m: a shape
		CHECK(CanSeeTarget(3000.f, true, true, false, true, VC));    // our high beams reach 33 m
		CHECK(CanSeeTarget(5000.f, true, false, false, false, VC));  // daytime: lights don't matter
		CHECK(!CanSeeTarget(100.f, false, false, true, false, VC));  // no line of sight: nothing
	});

	Run("exposure: a pro reads it sooner than a thug; quiet time calms", [&]
	{
		FTailExposure E(EC);
		E.Add(EC.FollowedTurn);
		E.Add(EC.HeldOnBrakeCheck);
		CHECK(!E.IsBlown(0.f));        // 0.65 < 0.8
		CHECK(E.IsBlown(1.f));         // 0.65 >= 0.48
		E.Tick(60.f);                  // -0.24
		CHECK(E.Get() < 0.45f && E.Get() > 0.35f);
		E.Add(10.f);
		CHECK(E.Get() == 1.f);         // clamped
	});

	Run("bait: gang always follows, civilians never 'follow', pros often drive on", [&]
	{
		CHECK(TakesTheBait(ETailRole::Gang, 1.f, 0.f));
		CHECK(!TakesTheBait(ETailRole::Civilian, 0.f, 0.99f));
		CHECK(!TakesTheBait(ETailRole::Undercover, 1.f, 0.5f)); // skilled: 80 % drive on
		CHECK(TakesTheBait(ETailRole::Undercover, 1.f, 0.9f));
		CHECK(TakesTheBait(ETailRole::Undercover, 0.f, 0.1f));  // a rookie always follows
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
