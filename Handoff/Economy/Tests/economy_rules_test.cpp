// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Economy economy_rules_test.cpp -o et && ./et
#include "EconomyRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarEconomy;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

int main()
{
	Run("prices look like prices", [&]
	{
		CHECK(NicePrice(0.f) == 0.f);
		CHECK(NicePrice(0.3f) == 1.f);
		CHECK(NicePrice(17.4f) == 17.f);
		CHECK(NicePrice(23.f) == 25.f);
		CHECK(NicePrice(437.f) == 440.f);
		CHECK(NicePrice(1234.f) == 1250.f);
		CHECK(NicePrice(5555.f) == 5600.f);
		CHECK(NicePrice(12345.f) == 12500.f);
	});

	Run("inflation compounds per day, off when the rate is 0", [&]
	{
		CHECK(Inflated(200.f, 0, 0.01f) == 200.f);
		CHECK(Inflated(200.f, 30, 0.f) == 200.f);
		CHECK(Inflated(200.f, 30, 0.01f) == 270.f);   // 200 * 1.01^30 = 269.6
		CHECK(Inflated(200.f, 365, 0.005f) == 1250.f); // a game year at 0.5 %/day: ~6.2x (the 90s)
		CHECK(Inflated(0.f, 100, 0.01f) == 0.f);
	});

	Run("buying is all or nothing", [&]
	{
		FPayResult R = TryPay(250.f, 200.f);
		CHECK(R.bPaid && Near(R.Cash, 50.f) && R.Shortfall == 0.f);
		R = TryPay(150.f, 200.f);
		CHECK(!R.bPaid && Near(R.Cash, 150.f) && Near(R.Shortfall, 50.f));
		R = TryPay(200.f, 200.f); CHECK(R.bPaid && R.Cash == 0.f);
		R = TryPay(10.f, 0.f); CHECK(R.bPaid && R.Cash == 10.f);
	});

	Run("money taken never leaves the wallet negative", [&]
	{
		FPayResult R = Take(500.f, 200.f); CHECK(R.bPaid && Near(R.Cash, 300.f));
		R = Take(100.f, 200.f); CHECK(!R.bPaid && R.Cash == 0.f && Near(R.Shortfall, 100.f));
	});

	Run("losses: a fraction, a floor, never more than there is", [&]
	{
		CHECK(Loss(1000.f, 0.5f) == 500.f);
		CHECK(Loss(999.f, 0.5f) == 499.f);
		CHECK(Loss(100.f, 0.1f, 50.f) == 50.f);
		CHECK(Loss(30.f, 0.1f, 50.f) == 30.f);
		CHECK(Loss(0.f, 0.5f, 50.f) == 0.f);
		CHECK(Loss(100.f, 3.f) == 100.f);
	});

	Run("pocket cash is in the range and round", [&]
	{
		CHECK(PocketCash(20.f, 120.f, 0.f) == 20.f);
		CHECK(PocketCash(20.f, 120.f, 1.f) == 120.f);
		CHECK(PocketCash(20.f, 120.f, 0.37f) == 55.f); // 57 -> 55
		CHECK(PocketCash(120.f, 20.f, 0.f) == 20.f);
	});

	Run("lei are written the Romanian way", [&]
	{
		CHECK(FormatLei(0.f) == "0 lei");
		CHECK(FormatLei(999.f) == "999 lei");
		CHECK(FormatLei(1250.f) == "1.250 lei");
		CHECK(FormatLei(1234567.f) == "1.234.567 lei");
		CHECK(FormatLei(-200.f) == "-200 lei");
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
