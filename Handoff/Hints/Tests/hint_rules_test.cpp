// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Hints hint_rules_test.cpp -o hi && ./hi
#include "HintRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarHints;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	const FTuning T;

	Run("said once, at its moment", [&]
	{
		FScheduler S;
		S.Offer("Hint.Lights", false, 0.0);
		CHECK(S.Poll(0.0, false, T) == "Hint.Lights");
		CHECK(S.Poll(1.0, false, T).empty());
		S.Offer("Hint.Lights", true, 50.0); // shown already (the fact)
		CHECK(S.Poll(60.0, false, T).empty());
	});

	Run("never two close together", [&]
	{
		FScheduler S;
		S.Offer("A", false, 0.0); S.Offer("B", false, 0.0);
		CHECK(S.Poll(0.0, false, T) == "A");
		CHECK(S.Poll(10.0, false, T).empty());
		CHECK(S.Poll(19.0, false, T).empty());
		S.Offer("B", false, 19.0); // offered again: no duplicate
		CHECK(S.Waiting() == 1);
	});

	Run("waits for calm, then lapses", [&]
	{
		FScheduler S;
		S.Offer("Hint.Pager", false, 100.0);
		CHECK(S.Poll(105.0, true, T).empty());    // a chase
		CHECK(S.Poll(115.0, false, T) == "Hint.Pager");
		S.Offer("Hint.Rain", false, 200.0);
		CHECK(S.Poll(205.0, true, T).empty());
		CHECK(S.Poll(221.0, false, T).empty());   // 21 s later: lapsed
		CHECK(S.Waiting() == 0);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
