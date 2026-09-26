// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Consequences consequence_rules_test.cpp -o ct && ./ct
#include "ConsequenceRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarConsequence;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

int main()
{
	Run("where he wakes up", [&]
	{
		FContext C; C.bHasHospital = C.bHasStation = true;
		CHECK(Decide(C) == EOutcome::Hospital);
		C.bArrest = true; CHECK(Decide(C) == EOutcome::Station);
		C.bStoryMode = true; CHECK(Decide(C) == EOutcome::Checkpoint);
		FContext Bare; CHECK(Decide(Bare) == EOutcome::Checkpoint);     // a map without spawn points: as today
		Bare.bArrest = true; CHECK(Decide(Bare) == EOutcome::Checkpoint);
		FContext OnlyH; OnlyH.bHasHospital = true; OnlyH.bArrest = true; CHECK(Decide(OnlyH) == EOutcome::Checkpoint);
	});

	Run("hospital: a bill he can pay, hours pass", [&]
	{
		FTuning T;
		FHospital H = HospitalStay(3000.f, T); CHECK(Near(H.Bill, 300.f) && Near(H.Hours, 6.f));
		H = HospitalStay(500.f, T); CHECK(Near(H.Bill, 100.f));   // the minimum
		H = HospitalStay(60.f, T); CHECK(Near(H.Bill, 60.f));     // all he has
		H = HospitalStay(0.f, T); CHECK(H.Bill == 0.f && H.Hours > 0.f);
	});

	Run("station: a quiet stop costs a night, a lethal chase much more", [&]
	{
		FTuning T;
		FSentence S = StationSentence(10.f, 0, 5000.f, T);
		CHECK(Near(S.CellHours, 10.f) && Near(S.Fine, 150.f) && S.bConfiscate);
		S = StationSentence(90.f, 0, 5000.f, T);
		CHECK(Near(S.CellHours, 26.f) && Near(S.Fine, 550.f));
	});

	Run("station: every earlier arrest makes it longer and dearer, up to a cap", [&]
	{
		FTuning T;
		const FSentence First = StationSentence(50.f, 0, 1e6f, T);
		const FSentence Third = StationSentence(50.f, 2, 1e6f, T);
		CHECK(Third.CellHours > First.CellHours && Near(Third.CellHours, 27.f));
		CHECK(Near(Third.Fine, 525.f));
		CHECK(Near(StationSentence(100.f, 20, 1e6f, T).CellHours, 72.f));
	});

	Run("station: the fine takes what there is; out-of-range heat is clamped", [&]
	{
		FTuning T;
		FSentence S = StationSentence(50.f, 0, 120.f, T);
		CHECK(Near(S.Fine, 120.f) && Near(S.FineOwed, 350.f));
		CHECK(StationSentence(50.f, 0, 0.f, T).Fine == 0.f);
		CHECK(Near(StationSentence(500.f, 0, 1e6f, T).CellHours, StationSentence(100.f, 0, 1e6f, T).CellHours));
		CHECK(Near(StationSentence(-5.f, -3, 1e6f, T).CellHours, 8.f));
	});

	Run("station: confiscation from a heat threshold", [&]
	{
		FTuning T; T.ConfiscateFromHeat = 40.f;
		CHECK(!StationSentence(39.f, 0, 0.f, T).bConfiscate);
		CHECK(StationSentence(40.f, 0, 0.f, T).bConfiscate);
		T.ConfiscateFromHeat = 101.f; CHECK(!StationSentence(100.f, 0, 0.f, T).bConfiscate);
	});

	Run("health comes back by itself only part of the way", [&]
	{
		FRegen R;
		CHECK(Regenerate(30.f, 100.f, 3.f, 1.f, R) == 30.f);       // hit 3 s ago: nothing yet
		CHECK(Near(Regenerate(30.f, 100.f, 7.f, 1.f, R), 32.f));
		CHECK(Near(Regenerate(59.f, 100.f, 7.f, 1.f, R), 60.f));  // stops at the cap
		CHECK(Regenerate(80.f, 100.f, 60.f, 1.f, R) == 80.f);      // above the cap: never lowered
		CHECK(Regenerate(0.f, 100.f, 60.f, 1.f, R) == 0.f);        // dead stays dead
	});

	Run("nearest spawn point", [&]
	{
		CHECK(PickNearest({}) == -1);
		CHECK(PickNearest({ 9.f, 4.f, 16.f }) == 1);
		CHECK(PickNearest({ 4.f, 4.f }) == 0);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
