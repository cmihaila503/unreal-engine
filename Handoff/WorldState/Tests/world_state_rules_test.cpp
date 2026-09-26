// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/WorldState world_state_rules_test.cpp -o wt && ./wt
#include "WorldStateRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarWorld;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

int main()
{
	Run("applied once, after a load, on the same map", [&]
	{
		CHECK(ShouldApply(true, "Freeroam", "Freeroam"));
		CHECK(!ShouldApply(false, "Freeroam", "Freeroam"));   // a chapter jump, not a load
		CHECK(!ShouldApply(true, "Freeroam", "Sandbox"));     // saved elsewhere
		CHECK(!ShouldApply(true, "", ""));                    // an old save without world state
	});

	Run("where the player comes back", [&]
	{
		CHECK(DecidePlayer(true, true, true, true) == EPlayerRestore::Checkpoint);
		CHECK(DecidePlayer(false, false, false, false) == EPlayerRestore::Checkpoint);
		CHECK(DecidePlayer(false, true, false, true) == EPlayerRestore::OnFoot);
		CHECK(DecidePlayer(false, true, true, true) == EPlayerRestore::InCar);
		CHECK(DecidePlayer(false, true, true, false) == EPlayerRestore::OnFoot); // saved in the car, but no car came back
	});

	Run("positions under the world are not restored", [&]
	{
		CHECK(IsPlaceable(0.f, -10000.f));
		CHECK(!IsPlaceable(-9950.f, -10000.f));
	});

	Run("records: destroyed is sticky, captures merge", [&]
	{
		FRecords R;
		R.Capture("Freeroam", "Door_3", false, true);
		R.Capture("Freeroam", "Door_3", true, false);
		CHECK(R.Count("Freeroam") == 1);
		const FRecord* D = R.Find("Freeroam", "Door_3");
		CHECK(D && D->bHasTransform && D->bHasValue);
		CHECK(DecideActor(D) == EActorAction::Restore);

		R.MarkDestroyed("Freeroam", "Lamp_12");
		R.Capture("Freeroam", "Lamp_12", true, true);   // a stale capture of a destroyed actor
		const FRecord* L = R.Find("Freeroam", "Lamp_12");
		CHECK(L && L->bDestroyed && !L->bHasTransform);
		CHECK(DecideActor(L) == EActorAction::Destroy);
	});

	Run("records are per map; unknown actors are left alone", [&]
	{
		FRecords R;
		R.MarkDestroyed("Sandbox", "Lamp_12");
		CHECK(R.Find("Freeroam", "Lamp_12") == nullptr);
		CHECK(DecideActor(R.Find("Freeroam", "Lamp_12")) == EActorAction::Leave);
		CHECK(R.Count("Sandbox") == 1 && R.Count("Freeroam") == 0);
		R.Capture("Freeroam", "Crate", false, false);
		CHECK(DecideActor(R.Find("Freeroam", "Crate")) == EActorAction::Leave); // nothing to restore
		CHECK(R.Get().size() == 2);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
