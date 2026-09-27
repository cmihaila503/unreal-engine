// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/AI/Pooling pool_rules_test.cpp -o pl && ./pl
#include "PoolRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarPool;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	const FTuning T;

	Run("reuse when there is one, spawn otherwise", [&]
	{
		CHECK(Acquire(3) == EAcquire::Reuse && Acquire(0) == EAcquire::Spawn);
	});

	Run("park up to the cap, destroy beyond", [&]
	{
		CHECK(Release(0, T) == ERelease::Park && Release(15, T) == ERelease::Park && Release(16, T) == ERelease::Destroy);
		CHECK(Trim(20, T) == 4 && Trim(10, T) == 0);
	});

	Run("warming is slow and only in quiet frames", [&]
	{
		CHECK(Prewarm(0, true, T) == 1);
		CHECK(Prewarm(0, false, T) == 0);
		CHECK(Prewarm(8, true, T) == 0);
		FTuning Fast = T; Fast.PrewarmPerTick = 5;
		CHECK(Prewarm(6, true, Fast) == 2);
	});

	Run("reuse the newest, trim the oldest", [&]
	{
		const std::vector<FSlot> S = { { 1, 10.0 }, { 2, 30.0 }, { 3, 20.0 } };
		CHECK(PickReuse(S) == 1 && PickTrim(S) == 0);
		CHECK(PickReuse({}) == -1 && PickTrim({}) == -1);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
