// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Safehouse safehouse_rules_test.cpp -o sh && ./sh
#include "SafehouseRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarSafehouse;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

int main()
{
	const FTuning T;

	Run("sleep only when it's quiet", [&]
	{
		CHECK(CanSleep(0, 1e9f, false, T) == EBlock::None);
		CHECK(CanSleep(1, 1e9f, false, T) == EBlock::Wanted);
		CHECK(CanSleep(0, 5.f, false, T) == EBlock::Combat);
		CHECK(CanSleep(0, 1e9f, true, T) == EBlock::Mission);
		CHECK(CanSleep(2, 5.f, true, T) == EBlock::Wanted);
	});

	Run("how long he sleeps", [&]
	{
		CHECK(Near(SleepHours(23.f, T), 9.f));    // to 8 am
		CHECK(Near(SleepHours(2.f, T), 6.f));
		CHECK(Near(SleepHours(6.f, T), 3.f));     // 2 h to morning, but a sleep is at least 3
		CHECK(Near(SleepHours(20.f, T), 12.f));
		CHECK(Near(SleepHours(14.f, T), 4.f));    // a nap
		CHECK(Near(SleepHours(8.f, T), 4.f));     // morning already: a nap
		CHECK(Near(SleepHours(-1.f, T), 9.f));    // wraps
		CHECK(Near(SleepHours(47.f, T), 9.f));
	});

	Run("the stash", [&]
	{
		FMoney M{ 500.f, 100.f };
		FMoney D = Deposit(M);
		CHECK(Near(D.Cash, 0.f) && Near(D.Stash, 600.f));
		FMoney P = Deposit(M, 200.f);
		CHECK(Near(P.Cash, 300.f) && Near(P.Stash, 300.f));
		FMoney W = Withdraw(D, 250.f);
		CHECK(Near(W.Cash, 250.f) && Near(W.Stash, 350.f));
		FMoney A = Withdraw(M);
		CHECK(Near(A.Cash, 600.f) && Near(A.Stash, 0.f));
		FMoney N = Deposit(M, -50.f);
		CHECK(Near(N.Cash, 500.f) && Near(N.Stash, 100.f));
		CHECK(Near(Deposit(M).Cash + Deposit(M).Stash, 600.f)); // nothing created or lost
	});

	Run("hiding cools the heat faster once unseen for a while", [&]
	{
		CHECK(HideExtraDecay(false, 60.f, 0.6f, T) == 0.f);
		CHECK(HideExtraDecay(true, 5.f, 0.6f, T) == 0.f);
		CHECK(Near(HideExtraDecay(true, 12.f, 0.6f, T), 1.2f)); // x3 total
	});

	Run("wardrobe cycles", [&]
	{
		CHECK(NextOutfit(0, 3) == 1);
		CHECK(NextOutfit(2, 3) == 0);
		CHECK(NextOutfit(5, 0) == 0);
		CHECK(NextOutfit(-1, 3) == 0);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
