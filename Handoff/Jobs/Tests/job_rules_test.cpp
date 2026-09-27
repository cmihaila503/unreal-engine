// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Jobs -I../../Missions/Source/Murdar_GameDev/Director/Missions job_rules_test.cpp -o jt && ./jt
#include "JobRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarJobs;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B, float E = 1e-2f) { return std::fabs(A - B) < E; }

static FPoint P(float Xm, float Ym, bool bBorder = false) { FPoint p; p.X = Xm * 100.f; p.Y = Ym * 100.f; p.bBorder = bBorder; return p; }

int main()
{
	const FTuning T;
	const FPoint Player = P(0, 0);
	const std::vector<FPoint> Pickups = { P(100, 0), P(800, 0), P(3000, 0) };        // 100 m (too close), 800 m, 3 km (too far)
	const std::vector<FPoint> Drops = { P(800, 1500), P(800, 50), P(800, 2500, true) }; // 1.5 km, 50 m, 2.5 km (border)

	Run("points are picked inside the distance band", [&]
	{
		CHECK(PickPoint(Pickups, Player, T.PickupMinCm, T.PickupMaxCm, 0.f) == 1);
		CHECK(PickPoint(Pickups, Player, T.PickupMinCm, T.PickupMaxCm, 0.99f) == 1);
		CHECK(PickPoint({ P(10, 0), P(5000, 0) }, Player, T.PickupMinCm, T.PickupMaxCm, 0.5f) == 0); // none inside: nearest to the band
		CHECK(PickPoint({}, Player, 0.f, 1.f, 0.5f) == -1);
		CHECK(PickPoint(Drops, Pickups[1], T.DropMinCm, T.DropMaxCm, 0.5f, -1, 1) == 2);            // border only
		CHECK(PickPoint(Drops, Pickups[1], T.DropMinCm, T.DropMaxCm, 0.5f, -1, 0) == 0);            // no border
	});

	Run("a delivery: pickup, drop, pay by distance, a clock", [&]
	{
		const FJob J = Generate(EKind::Delivery, Pickups, Drops, Player, 0.f, 0.f, 1.f, T);
		CHECK(J.bValid && J.Pickup == 1 && J.Drop == 0 && !J.bNoPolice);
		CHECK(Near(J.Pay, 280.f));                      // 100 + 120 * 1.5 km
		CHECK(J.TimeLimit > 60.f && J.TimeLimit < 400.f);
	});

	Run("smuggling goes to the border, pays more, no police allowed", [&]
	{
		const FJob J = Generate(EKind::Smuggling, Pickups, Drops, Player, 0.f, 0.f, 1.f, T);
		CHECK(J.bValid && Drops[J.Drop].bBorder && J.bNoPolice);
		CHECK(Near(J.Pay, 1000.f));                     // (100 + 120 * 2.5) * 2.5
		const MurdarMission::FMissionSpec S = ToSpec(J, Pickups, Drops);
		CHECK(S.FailOnEvents.size() == 1 && S.FailOnEvents[0] == "Event.Police.PursuitStarted");
	});

	Run("a contact who pays better", [&]
	{
		const FJob J = Generate(EKind::Delivery, Pickups, Drops, Player, 0.f, 0.f, 1.5f, T);
		CHECK(Near(J.Pay, 420.f));
	});

	Run("no points, no job", [&]
	{
		CHECK(!Generate(EKind::Delivery, {}, Drops, Player, 0.f, 0.f, 1.f, T).bValid);
		CHECK(!Generate(EKind::Delivery, Pickups, {}, Player, 0.f, 0.f, 1.f, T).bValid);
		CHECK(!Generate(EKind::Collection, {}, Drops, Player, 0.f, 0.f, 1.f, T).bValid);
	});

	Run("the spec runs on the mission runtime", [&]
	{
		const FJob J = Generate(EKind::Delivery, Pickups, Drops, Player, 0.f, 0.f, 1.f, T);
		MurdarMission::FMissionRuntime R(ToSpec(J, Pickups, Drops));
		auto No = [](const std::string&) { return false; };
		R.Start(0.0);
		CHECK(R.GetObjective() == 0);
		R.Tick(10.0, { 80000.f, 0.f, 0.f }, No);           // at the pickup
		CHECK(R.GetObjective() == 1);
		R.Tick(20.0, { 80000.f, 150000.f, 0.f }, No);      // at the drop
		CHECK(R.GetState() == MurdarMission::EState::Succeeded);
	});

	Run("car delivery needs him in the right car at the drop", [&]
	{
		FJob J = Generate(EKind::CarDelivery, Pickups, Drops, Player, 0.f, 0.f, 1.f, T);
		CHECK(J.bValid && J.TimeLimit == 0.f);
		bool bInCar = false;
		auto Facts = [&bInCar](const std::string& F) { return F == "Job.InTargetCar" && bInCar; };
		MurdarMission::FMissionRuntime R(ToSpec(J, Pickups, Drops));
		R.Start(0.0);
		const MurdarMission::FVec AtDrop{ Drops[J.Drop].X, Drops[J.Drop].Y, 0.f };
		R.Tick(1.0, AtDrop, Facts);
		CHECK(R.GetObjective() == 0);                       // on foot at the drop: nothing
		bInCar = true;
		R.Tick(2.0, { 0.f, 0.f, 0.f }, Facts);
		CHECK(R.GetObjective() == 1);
		R.Tick(3.0, AtDrop, Facts);
		CHECK(R.GetState() == MurdarMission::EState::Succeeded);
		CHECK(Near(CarDeliveryPay(600.f, 0.f), 600.f) && Near(CarDeliveryPay(600.f, 0.5f), 360.f) && Near(CarDeliveryPay(600.f, 2.f), 120.f));
	});

	Run("collection completes on the collected event", [&]
	{
		const FJob J = Generate(EKind::Collection, Pickups, Drops, Player, 0.f, 0.f, 1.f, T);
		MurdarMission::FMissionRuntime R(ToSpec(J, Pickups, Drops));
		auto No = [](const std::string&) { return false; };
		R.Start(0.0);
		R.OnEvent("Event.Job.Collected", "");
		R.Tick(1.0, { 0.f, 0.f, 0.f }, No);
		CHECK(R.GetState() == MurdarMission::EState::Succeeded);
		CHECK(DebtorPays(false, 0.5f, T) && !DebtorPays(false, 0.7f, T) && DebtorPays(true, 0.99f, T));
	});

	Run("the pager: only when he's free, at random intervals", [&]
	{
		CHECK(CanPage(false, false, 0));
		CHECK(!CanPage(true, false, 0) && !CanPage(false, true, 0) && !CanPage(false, false, 1));
		CHECK(Near(NextPageIn(0.f, T), 240.f) && Near(NextPageIn(1.f, T), 600.f));
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
