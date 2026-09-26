// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/AI/Lod significance_rules_test.cpp -o st && ./st
#include "SignificanceRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarLod;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

static FAgent Ag(float D, bool bView, bool bEngaged = false, bool bPinned = false)
{
	FAgent A; A.DistanceCm = D; A.bInView = bView; A.bEngaged = bEngaged; A.bPinned = bPinned; return A;
}

int main()
{
	const FBudget B;

	Run("score: near and in view first; engaged above all", [&]
	{
		CHECK(Score(Ag(1000.f, true), B) > Score(Ag(3000.f, true), B));
		CHECK(Score(Ag(1000.f, true), B) > Score(Ag(1000.f, false), B));
		CHECK(Score(Ag(20000.f, false, true), B) > Score(Ag(0.f, true), B));
		CHECK(Score(Ag(20000.f, false, false, true), B) >= 1000.f);
	});

	Run("tiers by distance", [&]
	{
		const std::vector<ETier> T = AssignTiers({ Ag(1000.f, true), Ag(10000.f, true), Ag(20000.f, true), Ag(40000.f, false), Ag(40000.f, true) }, B);
		CHECK(T[0] == ETier::Full);
		CHECK(T[1] == ETier::Reduced);
		CHECK(T[2] == ETier::Minimal);
		CHECK(T[3] == ETier::Dormant);
		CHECK(T[4] == ETier::Minimal); // far but in view: never dormant (it might be watched through a long street)
	});

	Run("the full-rate budget goes to the most deserving", [&]
	{
		FBudget Small = B; Small.FullMax = 2;
		const std::vector<ETier> T = AssignTiers({ Ag(3000.f, false), Ag(500.f, true), Ag(2000.f, true), Ag(1000.f, true) }, Small);
		CHECK(T[1] == ETier::Full && T[3] == ETier::Full);
		CHECK(T[2] == ETier::Reduced && T[0] == ETier::Reduced);
	});

	Run("engaged and pinned are full rate outside the budget", [&]
	{
		FBudget Tiny = B; Tiny.FullMax = 1;
		const std::vector<ETier> T = AssignTiers({ Ag(500.f, true), Ag(25000.f, false, true), Ag(50000.f, false, false, true), Ag(600.f, true) }, Tiny);
		CHECK(T[1] == ETier::Full && T[2] == ETier::Full);
		CHECK(T[0] == ETier::Full);      // the budget of one still goes to the nearest ordinary agent
		CHECK(T[3] == ETier::Reduced);
	});

	Run("promotion now, demotion after a few passes", [&]
	{
		FTierState S; S.Current = ETier::Minimal;
		CHECK(S.Update(ETier::Full, 3) && S.Current == ETier::Full);
		CHECK(!S.Update(ETier::Reduced, 3));
		CHECK(!S.Update(ETier::Reduced, 3));
		CHECK(S.Update(ETier::Reduced, 3) && S.Current == ETier::Reduced);
		CHECK(!S.Update(ETier::Minimal, 3));
		CHECK(!S.Update(ETier::Reduced, 3));   // back to the current: the count restarts
		CHECK(!S.Update(ETier::Minimal, 3));
		CHECK(!S.Update(ETier::Minimal, 3));
		CHECK(S.Update(ETier::Minimal, 3));
	});

	Run("intervals: slower with each tier; cars' physics never throttled", [&]
	{
		CHECK(Intervals(ETier::Full, EKind::Pedestrian).Brain < Intervals(ETier::Reduced, EKind::Pedestrian).Brain);
		CHECK(Intervals(ETier::Reduced, EKind::Pedestrian).Brain < Intervals(ETier::Minimal, EKind::Pedestrian).Brain);
		CHECK(!Intervals(ETier::Full, EKind::Pedestrian).bAnimOnlyWhenRendered);
		CHECK(Intervals(ETier::Reduced, EKind::Pedestrian).bAnimOnlyWhenRendered);
		CHECK(Intervals(ETier::Dormant, EKind::Car).Movement == 0.f);
		CHECK(!Intervals(ETier::Minimal, EKind::Car).bDriverReduced && Intervals(ETier::Dormant, EKind::Car).bDriverReduced);
	});

	Run("streaming radius grows with speed, in steps, without flapping", [&]
	{
		const FStreamingTuning T;
		float R = T.BaseRadiusCm;
		R = StreamingRadius(R, 10.f, T);  CHECK(R == T.BaseRadiusCm);   // target 27 500: inside the dead band
		CHECK(StreamingRadius(R, 20.f, T) == 30000.f);                   // target 30 000: one step up
		R = StreamingRadius(R, 70.f, T);  CHECK(R == 45000.f);          // target 42 500 -> step 45 000
		CHECK(StreamingRadius(R, 75.f, T) == 45000.f);                   // small change: stays
		CHECK(StreamingRadius(R, 200.f, T) == T.MaxRadiusCm);
		CHECK(StreamingRadius(T.MaxRadiusCm, 0.f, T) == T.BaseRadiusCm);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
