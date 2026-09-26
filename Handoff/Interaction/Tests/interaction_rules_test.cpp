// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Interaction interaction_rules_test.cpp -o it && ./it
#include "InteractionRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarInteract;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

static FCandidate C(int Id, float Dist, float Angle, float Prio = 1.f, bool bAvail = true)
{
	FCandidate X; X.Id = Id; X.DistanceCm = Dist; X.AngleDeg = Angle; X.Priority = Prio; X.bAvailable = bAvail; return X;
}

int main()
{
	Run("out of range, out of angle or unavailable: not in play", [&]
	{
		CHECK(Score(C(1, 200.f, 0.f)) < 0.f);
		CHECK(Score(C(1, 100.f, 70.f)) < 0.f);
		CHECK(Score(C(1, 50.f, 0.f, 1.f, false)) < 0.f);
		CHECK(Score(C(1, 100.f, 30.f)) >= 0.f);
		CHECK(PickBest({C(1, 500.f, 0.f)}, -1) == -1);
	});

	Run("you use what you look at: facing beats being a bit closer", [&]
	{
		// door ahead at 1.2 m vs. a bench at 0.6 m but 50 degrees off
		CHECK(PickBest({C(1, 120.f, 5.f), C(2, 60.f, 50.f)}, -1) == 0);
	});

	Run("priority lifts the phone over the bench next to it", [&]
	{
		CHECK(PickBest({C(1, 80.f, 10.f, 1.f), C(2, 90.f, 12.f, 2.f)}, -1) == 1);
	});

	Run("stickiness: two doors side by side don't flicker", [&]
	{
		// current focus 1 is marginally worse than 2: keep 1
		CHECK(PickBest({C(1, 100.f, 12.f), C(2, 100.f, 10.f)}, 1) == 0);
		// 2 is now clearly better: switch
		CHECK(PickBest({C(1, 140.f, 40.f), C(2, 60.f, 2.f)}, 1) == 1);
		// current left range: pick the other
		CHECK(PickBest({C(1, 400.f, 0.f), C(2, 100.f, 10.f)}, 1) == 1);
	});

	Run("facing angle in the ground plane", [&]
	{
		CHECK(std::fabs(FacingAngleDeg(1, 0, 5, 0)) < 1e-3f);
		CHECK(std::fabs(FacingAngleDeg(1, 0, 0, 5) - 90.f) < 1e-2f);
		CHECK(std::fabs(FacingAngleDeg(1, 0, -3, 0) - 180.f) < 1e-2f);
		CHECK(FacingAngleDeg(1, 0, 0, 0) == 0.f);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
