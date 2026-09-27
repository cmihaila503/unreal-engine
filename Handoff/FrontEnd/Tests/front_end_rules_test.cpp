// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/UI/FrontEnd front_end_rules_test.cpp -o fe && ./fe
#include "FrontEndRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarFrontEnd;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	Run("continue picks the newest save", [&]
	{
		std::vector<FSlot> S(4);
		CHECK(ContinueSlot(S) == -1 && !AnySave(S));
		S[1].bExists = true; S[1].SavedAt = 100;
		S[3].bExists = true; S[3].SavedAt = 250;
		S[2].SavedAt = 999; // not existing: ignored
		CHECK(ContinueSlot(S) == 3 && AnySave(S));
	});

	Run("the game clock line", [&]
	{
		CHECK(Clock(0, 480.f) == "Ziua 1, 08:00");
		CHECK(Clock(2, 1265.f) == "Ziua 3, 21:05");
		CHECK(Clock(0, 1500.f) == "Ziua 1, 01:00"); // wraps
		CHECK(Clock(5, -1.f).empty());
	});

	Run("slot labels", [&]
	{
		FSlot E; CHECK(Label(E, "Slot 1") == "Slot 1 \xE2\x80\x94 gol");
		FSlot F; F.bExists = true; F.Chapter = "Capitolul doi"; F.Day = 2; F.Minutes = 1265.f;
		CHECK(Label(F, "Slot 2") == "Slot 2 \xE2\x80\x94 Capitolul doi \xC2\xB7 Ziua 3, 21:05");
		FSlot G; G.bExists = true; CHECK(Label(G, "Auto") == "Auto \xE2\x80\x94 ?");
		CHECK(NeedsOverwriteConfirm(F) && !NeedsOverwriteConfirm(E));
	});

	Run("tips don't repeat", [&]
	{
		const std::vector<int> Recent = { 4, 1, 2 };
		for (float r : { 0.f, 0.3f, 0.6f, 0.99f })
		{
			const int t = NextTip(6, Recent, r);
			CHECK(t != 1 && t != 2 && t != 4 && t >= 0 && t < 6);
		}
		CHECK(NextTip(2, { 0 }, 0.f) == 1);   // two tips: alternate
		CHECK(NextTip(1, { 0 }, 0.5f) == 0);  // one tip: it's all there is
		CHECK(NextTip(0, {}, 0.5f) == -1);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
