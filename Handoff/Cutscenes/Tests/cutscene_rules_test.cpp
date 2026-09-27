// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Cutscenes cutscene_rules_test.cpp -o cs && ./cs
#include "CutsceneRules.h"

#include <cmath>
#include <cstdio>
#include <functional>

using namespace MurdarCutscene;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	Run("play, wait or drop", [&]
	{
		CHECK(Decide(FGate{}) == EDecision::Play);
		FGate Chase; Chase.Wanted = 2; CHECK(Decide(Chase) == EDecision::Wait);
		FGate Stop; Stop.Wanted = 1; CHECK(Decide(Stop) == EDecision::Play);        // a traffic stop doesn't hold a scene
		FGate Talk; Talk.bInDialogue = true; CHECK(Decide(Talk) == EDecision::Wait);
		FGate Done; Done.bOnce = true; Done.bAlreadyPlayed = true; CHECK(Decide(Done) == EDecision::Drop);
		FGate Again; Again.bAlreadyPlayed = true; CHECK(Decide(Again) == EDecision::Play); // not once-only
		FGate NoFacts; NoFacts.bFactsOk = false; NoFacts.Wanted = 3; CHECK(Decide(NoFacts) == EDecision::Drop);
	});

	Run("the queue: bounded, no duplicates, stale ones dropped", [&]
	{
		FQueue Q(2, 60.0);
		CHECK(Q.Push("A", 0.0) && !Q.Push("A", 1.0) && Q.Push("B", 50.0) && !Q.Push("C", 51.0));
		CHECK(Q.Pop(70.0) == "B");   // A went stale (70 s > 60)
		CHECK(Q.Pop(70.0).empty() && Q.Size() == 0);
	});

	Run("skip is a hold, and fires once", [&]
	{
		FSkipHold S(1.f);
		CHECK(!S.Update(true, 0.5f));
		CHECK(std::fabs(S.Progress01() - 0.5f) < 1e-4f);
		CHECK(!S.Update(false, 0.1f));          // let go: starts over
		CHECK(S.Progress01() == 0.f);
		CHECK(!S.Update(true, 0.6f) && S.Update(true, 0.5f));
		CHECK(!S.Update(true, 1.f));            // once per hold
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
