// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/AI/Gangs gang_rules_test.cpp -o gg && ./gg
#include "GangRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarGangs;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

int main()
{
	const FTuning T;

	Run("deeds move respect, within limits", [&]
	{
		CHECK(Near(Apply(0.f, EDeed::KilledMember, T), -35.f));
		CHECK(Near(Apply(-90.f, EDeed::KilledMember, T), -100.f));
		CHECK(Near(Apply(95.f, EDeed::JobDone, T), 100.f));
		CHECK(Near(Apply(0.f, EDeed::PaidTax, T), 12.f));
		CHECK(Near(Apply(10.f, EDeed::HurtMember, T), 2.f));
	});

	Run("grudges fade toward zero, never past it", [&]
	{
		CHECK(Near(Drift(-60.f, 2.f, T), -50.f));
		CHECK(Near(Drift(30.f, 10.f, T), 0.f));
		CHECK(Near(Drift(-3.f, 1.f, T), 0.f));
		CHECK(Near(Drift(20.f, -5.f, T), 20.f));
	});

	Run("stance with hysteresis", [&]
	{
		CHECK(StanceOf(0.f, EStance::Wary, T) == EStance::Wary);
		CHECK(StanceOf(-40.f, EStance::Wary, T) == EStance::Hostile);
		CHECK(StanceOf(-30.f, EStance::Hostile, T) == EStance::Hostile);   // still hostile until above -25
		CHECK(StanceOf(-20.f, EStance::Hostile, T) == EStance::Wary);
		CHECK(StanceOf(40.f, EStance::Wary, T) == EStance::Friendly);
		CHECK(StanceOf(30.f, EStance::Friendly, T) == EStance::Friendly);
		CHECK(StanceOf(20.f, EStance::Friendly, T) == EStance::Wary);
		CHECK(StanceOf(-50.f, EStance::Friendly, T) == EStance::Hostile);  // a massacre skips Wary
		CHECK(StanceOf(50.f, EStance::Hostile, T) == EStance::Friendly);
	});

	Run("the taxa", [&]
	{
		CHECK(DemandsTax(EStance::Wary, 5.f, 0.f, 0.1f, T));
		CHECK(!DemandsTax(EStance::Wary, 5.f, 0.f, 0.9f, T));
		CHECK(!DemandsTax(EStance::Wary, 5.f, PaidUntil(4.f, T), 0.1f, T)); // paid until day 7
		CHECK(DemandsTax(EStance::Wary, 7.f, PaidUntil(4.f, T), 0.1f, T));
		CHECK(!DemandsTax(EStance::Hostile, 5.f, 0.f, 0.f, T) && !DemandsTax(EStance::Friendly, 5.f, 0.f, 0.f, T));
	});

	Run("how many hang about", [&]
	{
		CHECK(MembersAround(EStance::Hostile, false, T) == 5 && MembersAround(EStance::Hostile, true, T) == 6);
		CHECK(MembersAround(EStance::Friendly, false, T) < MembersAround(EStance::Wary, false, T));
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
