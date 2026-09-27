// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Disguise disguise_rules_test.cpp -o dg && ./dg
#include "DisguiseRules.h"

#include <cmath>
#include <cstdio>
#include <functional>

using namespace MurdarDisguise;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	const FTuning T;
	const FLookOfHim Leather{ 0, 0 }, LeatherCap{ 0, 1 }, Suit{ 2, 0 };

	Run("described, recognised; changed, not at a distance", [&]
	{
		const FDescription None;
		CHECK(Recognise(None, Suit, 5000.f, false, T));
		const FDescription D = Describe(Leather);
		CHECK(Recognise(D, Leather, 5000.f, true, T));
		CHECK(!Recognise(D, Suit, 2000.f, false, T));
		CHECK(!Recognise(D, LeatherCap, 2000.f, false, T)); // a cap is enough at a distance
		CHECK(Recognise(D, Suit, 700.f, false, T));         // close, by day: the face
		CHECK(!Recognise(D, Suit, 700.f, true, T));         // at night it isn't
		CHECK(Recognise(D, Suit, 250.f, true, T));
	});

	Run("changing only counts unseen, and cools a chase", [&]
	{
		CHECK(!ChangeCounts(2.f, T) && ChangeCounts(6.f, T));
		CHECK(std::fabs(HeatAfterChange(60.f, 12.f, 40.f) - 11.f) < 1e-4f);
		CHECK(std::fabs(HeatAfterChange(20.f, 12.f, 40.f) - 20.f) < 1e-4f);
	});

	Run("headwear cycles through what he owns", [&]
	{
		CHECK(NextHeadwear(0, 0b101u, 3) == 1);
		CHECK(NextHeadwear(1, 0b101u, 3) == 3);   // skips item 2, not owned
		CHECK(NextHeadwear(3, 0b101u, 3) == 0);   // back to bare-headed
		CHECK(NextHeadwear(0, 0u, 3) == 0);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
