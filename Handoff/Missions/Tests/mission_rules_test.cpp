// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Missions mission_rules_test.cpp -o ms && ./ms
#include "MissionRules.h"

#include <cstdio>
#include <set>

using namespace MurdarMission;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

static bool Has(const std::vector<FStep>& S, EStepKind K) { for (const FStep& X : S) { if (X.Kind == K) return true; } return false; }

// "Du mașina la garaj": 1) get in the car (event), 2) drive it into Zone.Garage without heat (fact + forbidden via fail fact)
static FMissionSpec CarJob()
{
	FMissionSpec M;
	M.Id = "Mission.Test.CarJob";
	FObjectiveSpec A; A.Id = "get_in"; A.CompleteOnEvent = "Event.Player.EnteredVehicle";
	FObjectiveSpec B; B.Id = "garage"; B.RequiredFacts = {"Zone.Garage"}; B.TimeLimitSeconds = 300.f;
	M.Objectives = {A, B};
	M.FailOnEvents = {"Event.Actor.Died.Player", "Event.Police.Arrested"};
	M.FailIfFacts = {"Fact.Car.Destroyed"};
	return M;
}

int main()
{
	Run("tag match follows the hierarchy", [&]
	{
		CHECK(TagMatches("Event.Trigger", "Event.Trigger"));
		CHECK(TagMatches("Event.Trigger.Sandbox", "Event.Trigger"));
		CHECK(!TagMatches("Event.TriggerX", "Event.Trigger"));
		CHECK(!TagMatches("Event", "Event.Trigger"));
		CHECK(!TagMatches("Event.Trigger", ""));
	});

	Run("objectives run in order and the mission succeeds", [&]
	{
		FMissionRuntime R(CarJob());
		std::set<std::string> Facts;
		auto Q = [&](const std::string& F) { return Facts.count(F) > 0; };
		auto S = R.Start(0.0);
		CHECK(Has(S, EStepKind::ObjectiveStarted) && R.GetObjective() == 0);
		CHECK(R.Tick(1.0, {}, Q).empty());                   // no event yet
		Facts.insert("Zone.Garage");
		CHECK(R.Tick(1.5, {}, Q).empty());                   // being in the garage early doesn't skip objective 1
		R.OnEvent("Event.Player.EnteredVehicle", "");
		S = R.Tick(2.1, {}, Q);
		CHECK(Has(S, EStepKind::ObjectiveCompleted) && Has(S, EStepKind::ObjectiveStarted) && R.GetObjective() == 1);
		S = R.Tick(2.35, {}, Q);                              // already in the zone: objective 2 completes on the next poll
		CHECK(Has(S, EStepKind::Succeeded) && R.GetState() == EState::Succeeded);
	});

	Run("an event before its objective is active is not remembered", [&]
	{
		FMissionSpec M;
		FObjectiveSpec A; A.Id = "wait"; A.RequiredFacts = {"Fact.Ready"};
		FObjectiveSpec B; B.Id = "signal"; B.CompleteOnEvent = "Event.Signal";
		M.Objectives = {A, B};
		FMissionRuntime R(M);
		bool bReady = false;
		auto Q = [&](const std::string&) { return bReady; };
		R.Start(0.0);
		R.OnEvent("Event.Signal", "");                  // too early
		bReady = true;
		R.Tick(1.1, {}, Q);                                  // objective A done, B starts
		CHECK(R.GetObjective() == 1);
		CHECK(R.Tick(1.3, {}, Q).empty());                   // B still waits for a NEW signal
		R.OnEvent("Event.Signal.Loud", "");              // a child tag counts
		CHECK(Has(R.Tick(1.5, {}, Q), EStepKind::Succeeded));
	});

	Run("payload filter: only the named trigger completes it", [&]
	{
		FMissionSpec M;
		FObjectiveSpec A; A.Id = "t"; A.CompleteOnEvent = "Event.Trigger"; A.CompleteOnPayload = "Trigger.Garage";
		M.Objectives = {A};
		FMissionRuntime R(M);
		R.Start(0.0);
		R.OnEvent("Event.Trigger", "Trigger.Other");
		CHECK(R.Tick(1.1, {}, nullptr).empty());
		R.OnEvent("Event.Trigger", "Trigger.Garage");
		CHECK(Has(R.Tick(1.3, {}, nullptr), EStepKind::Succeeded));
	});

	Run("fail on event, even when the objective would complete in the same tick", [&]
	{
		FMissionRuntime R(CarJob());
		R.Start(0.0);
		R.OnEvent("Event.Player.EnteredVehicle", "");
		const auto S = R.OnEvent("Event.Police.Arrested", "");
		CHECK(Has(S, EStepKind::Failed) && R.GetState() == EState::Failed);
		CHECK(R.Tick(1.1, {}, nullptr).empty());             // nothing after the end
	});

	Run("fail on fact and on objective time limit", [&]
	{
		FMissionRuntime A(CarJob());
		A.Start(0.0);
		CHECK(Has(A.Tick(1.0, {}, [](const std::string& F) { return F == "Fact.Car.Destroyed"; }), EStepKind::Failed));

		FMissionRuntime B(CarJob());
		B.Start(0.0);
		B.OnEvent("Event.Player.EnteredVehicle", "");
		B.Tick(1.1, {}, [](const std::string&) { return false; });   // objective 2 starts at 1.1
		CHECK(B.Tick(200.0, {}, [](const std::string&) { return false; }).empty());
		const auto S = B.Tick(302.0, {}, [](const std::string&) { return false; });
		CHECK(Has(S, EStepKind::Failed));
		CHECK(S[0].Reason.find("garage") != std::string::npos);
	});

	Run("reach a point within a radius", [&]
	{
		FMissionSpec M;
		FObjectiveSpec A; A.Id = "go"; A.bReach = true; A.ReachLocation = {1000.f, 0.f, 0.f}; A.ReachRadiusCm = 300.f;
		M.Objectives = {A};
		FMissionRuntime R(M);
		R.Start(0.0);
		CHECK(R.Tick(1.0, {0.f, 0.f, 0.f}, nullptr).empty());
		CHECK(Has(R.Tick(2.0, {800.f, 100.f, 0.f}, nullptr), EStepKind::Succeeded));
	});

	Run("mission time limit and abort", [&]
	{
		FMissionSpec M = CarJob();
		M.TimeLimitSeconds = 60.f;
		FMissionRuntime R(M);
		R.Start(0.0);
		CHECK(Has(R.Tick(61.0, {}, nullptr), EStepKind::Failed));
		FMissionRuntime R2(CarJob());
		R2.Start(0.0);
		CHECK(Has(R2.Abort("chapter changed"), EStepKind::Failed));
		CHECK(R2.Abort("again").empty());
		CHECK(R2.Start(5.0).size() == 1 && R2.GetState() == EState::Running && R2.GetObjective() == 0); // retry restarts
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
