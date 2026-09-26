// Unit tests for MurdarHeatModel.h — plain C++17, no engine.
//   g++ -std=c++17 -Wall -Wextra -I../Source/Murdar_GameDev/AI/Heat heat_model_test.cpp -o heat_test && ./heat_test
// The same header compiles inside the UE module; these tests pin its rules down before it gets there.

#include "MurdarHeatModel.h"

#include <cstdio>
#include <functional>

using namespace MurdarHeat;

static int Failures = 0;
static int Checks = 0;

#define CHECK(cond) do { ++Checks; if (!(cond)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK(std::abs(double(a) - double(b)) <= (eps))

static void Run(const char* Name, const std::function<void()>& Fn)
{
	const int Before = Failures;
	Fn();
	std::printf("%s %s\n", Failures == Before ? "ok  " : "FAIL", Name);
}

// Advance the model in fixed steps (like the 1 Hz memory tick, but finer).
static void Advance(FModel& M, double& Now, double Seconds, bool bSeen = false, bool bSearch = false, double Step = 0.1)
{
	const int N = int(Seconds / Step + 0.5);
	for (int i = 0; i < N; ++i)
	{
		Now += Step;
		M.Tick(Now, float(Step), bSeen, bSearch);
	}
}

int main()
{
	const FConfig C; // defaults = the values documented in Docs/HEAT_SYSTEM.md
	enum { Speeding = 1, Shots = 2, Murder = 3 };

	Run("crime adds its heat and raises the wanted level", [&]
	{
		FModel M(C);
		M.AddCrime(Shots, 35.f, 0.0, "shots");
		CHECK_NEAR(M.GetHeat(), 35.f, 1e-4);
		CHECK(M.GetWanted() == EWanted::Stop);
		M.AddCrime(Speeding, 6.f, 1.0, "speeding");
		CHECK(M.GetWanted() == EWanted::Pursuit);
	});

	Run("no decay while seen, none during the delay, then the base rate", [&]
	{
		FModel M(C);
		double Now = 0.0;
		M.AddCrime(Speeding, 30.f, Now, "x");
		Advance(M, Now, 20.0, /*seen*/ true);
		CHECK_NEAR(M.GetHeat(), 30.f, 1e-4);            // seen: frozen, and the delay restarts at the last sighting
		Advance(M, Now, C.DecayDelaySeconds - 0.5);
		CHECK_NEAR(M.GetHeat(), 30.f, 1e-4);            // still inside the delay
		Advance(M, Now, 10.5);                           // 10 s of real decay (speeding: severity 30/80)
		const float Severity01 = 30.f / C.MaxCrimeHeat;
		const float Rate = C.DecayPerSecond * (1.f + (C.SevereCrimeDecayScale - 1.f) * Severity01);
		CHECK_NEAR(M.GetHeat(), 30.f - Rate * 10.f, 0.05);
	});

	Run("a murder decays much slower than a speeding ticket", [&]
	{
		FModel A(C), B(C);
		double Ta = 0.0, Tb = 0.0;
		A.SetHeat(80.f, 0.0, "cheat");
		A.AddCrime(Speeding, 0.f, 0.0, "light");          // only sets 'recent crime' severity ~0
		B.SetHeat(0.f, 0.0, "cheat");
		B.AddCrime(Murder, 80.f, 0.0, "murder");
		Advance(A, Ta, 70.0);
		Advance(B, Tb, 70.0);
		CHECK(B.GetHeat() > A.GetHeat() + 20.f);
		// Before the change: 80 heat gone in ~133 s + delay. After a murder it must still be Lethal-ish after 70 s.
		CHECK(B.GetHeat() > 60.f);
	});

	Run("severity stops slowing decay after the memory window", [&]
	{
		FModel M(C);
		double Now = 0.0;
		M.AddCrime(Murder, 80.f, Now, "murder");
		Advance(M, Now, C.SeverityMemorySeconds + 1.0);
		const float H1 = M.GetHeat();
		Advance(M, Now, 10.0);
		CHECK_NEAR(H1 - M.GetHeat(), std::min(H1, C.DecayPerSecond * 10.f), 0.05);
	});

	Run("search slows decay", [&]
	{
		FModel A(C), B(C);
		double Ta = 0.0, Tb = 0.0;
		A.AddCrime(Speeding, 40.f, 0.0, "x");
		B.AddCrime(Speeding, 40.f, 0.0, "x");
		Advance(A, Ta, 40.0, false, false);
		Advance(B, Tb, 40.0, false, true);
		CHECK(B.GetHeat() > A.GetHeat());
	});

	Run("repeated speeding gives diminishing heat, then recovers after the window", [&]
	{
		FModel M(C);
		const float G1 = M.AddCrime(Speeding, 6.f, 0.0, "s");
		const float G2 = M.AddCrime(Speeding, 6.f, 1.0, "s");
		const float G3 = M.AddCrime(Speeding, 6.f, 2.0, "s");
		CHECK_NEAR(G1, 6.f, 1e-4);
		CHECK_NEAR(G2, 3.f, 1e-4);
		CHECK_NEAR(G3, 1.5f, 1e-4);
		for (int i = 0; i < 10; ++i) M.AddCrime(Speeding, 6.f, 3.0 + i, "s");
		const float Gn = M.AddCrime(Speeding, 6.f, 14.0, "s");
		CHECK_NEAR(Gn, 6.f * C.RepeatFloor, 1e-4);        // floor, never zero
		const float Gother = M.AddCrime(Shots, 35.f, 15.0, "shots");
		CHECK(Gother > 0.f);                               // other crime types are unaffected
		const float Glater = M.AddCrime(Speeding, 6.f, 15.0 + C.RepeatWindowSeconds + 1.0, "s");
		CHECK_NEAR(Glater, 6.f, 1e-4);
	});

	Run("wanted level hysteresis: no flapping around 40", [&]
	{
		FModel M(C);
		double Now = 0.0;
		M.SetHeat(41.f, Now, "cheat");                     // SetHeat is not police activity: decay starts at once
		CHECK(M.GetWanted() == EWanted::Pursuit);
		Advance(M, Now, 2.5);                              // 41 - 0.6 × 2.5 = 39.5, just under 40
		CHECK(M.GetHeat() < 40.f);
		CHECK(M.GetWanted() == EWanted::Pursuit);          // kept: inside the hysteresis band
		M.AddCrime(Speeding, 1.f, Now, "nudge");          // back above 40 and below again: still no change
		CHECK(M.GetWanted() == EWanted::Pursuit);
		Advance(M, Now, 60.0);
		CHECK(M.GetHeat() < C.PursuitHeat - C.WantedHysteresisHeat);
		CHECK(M.GetWanted() == EWanted::Stop);
	});

	Run("a large drop passes several levels at once", [&]
	{
		FModel M(C);
		M.SetHeat(90.f, 0.0, "cheat");
		CHECK(M.GetWanted() == EWanted::Lethal);
		M.SetHeat(0.f, 1.0, "bribe");                      // SetHeat snaps
		CHECK(M.GetWanted() == EWanted::None);
	});

	Run("heat is clamped to 0..100 and history is bounded", [&]
	{
		FModel M(C);
		for (int i = 0; i < 40; ++i) M.AddCrime(100 + i, 80.f, double(i), "big");
		CHECK_NEAR(M.GetHeat(), 100.f, 1e-4);
		CHECK(int(M.GetHistory().size()) == C.HistorySize);
	});

	Run("report queue: first report adds heat, later witnesses corroborate", [&]
	{
		FReportQueue Q(C);
		const uint64_t I = Q.FindOrCreateIncident(Shots, {0, 0, 0}, 0.0);
		CHECK(Q.FindOrCreateIncident(Shots, {500, 0, 0}, 2.0) == I);     // same shooting, other witness
		CHECK(Q.FindOrCreateIncident(Shots, {5000, 0, 0}, 2.0) != I);    // too far: another incident
		CHECK(Q.FindOrCreateIncident(Murder, {0, 0, 0}, 1.0) != I);      // other crime type
		Q.Queue(/*witness*/ 11, I, 35.f, 30.0);
		Q.Queue(12, I, 35.f, 20.0);
		Q.Queue(12, I, 35.f, 25.0);                                       // duplicate ignored
		CHECK(Q.NumPending() == 2);
		CHECK(Q.Collect(10.0).empty());
		auto D = Q.Collect(40.0);
		CHECK(D.size() == 2);
		CHECK(D[0].WitnessId == 12 && D[0].CorroborationIndex == 0);    // earliest due is the first report
		CHECK(D[1].WitnessId == 11 && D[1].CorroborationIndex == 1);
		CHECK(D[0].CrimeTime == 0.0);                                     // police get the old fix, not "now"
	});

	Run("report queue: killed witness never reports; expedite brings a report forward", [&]
	{
		FReportQueue Q(C);
		const uint64_t I = Q.FindOrCreateIncident(Murder, {0, 0, 0}, 0.0);
		Q.Queue(1, I, 80.f, 60.0);
		Q.Queue(2, I, 80.f, 60.0);
		Q.CancelWitness(1);
		CHECK(!Q.HasPending(1));
		Q.Expedite(2, 5.0);
		auto D = Q.Collect(6.0);
		CHECK(D.size() == 1 && D[0].WitnessId == 2 && D[0].CorroborationIndex == 0);
	});

	Run("report queue: police saw it -> civilian reports only corroborate", [&]
	{
		FReportQueue Q(C);
		const uint64_t I = Q.FindOrCreateIncident(Shots, {0, 0, 0}, 0.0);
		Q.MarkIncidentKnown(I);
		Q.Queue(7, I, 35.f, 10.0);
		auto D = Q.Collect(10.0);
		CHECK(D.size() == 1 && D[0].CorroborationIndex == 1);
	});

	Run("report queue: flush before save delivers everything", [&]
	{
		FReportQueue Q(C);
		const uint64_t I = Q.FindOrCreateIncident(Shots, {0, 0, 0}, 0.0);
		Q.Queue(1, I, 35.f, 999.0);
		auto D = Q.FlushAll(1.0);
		CHECK(D.size() == 1 && Q.NumPending() == 0);
	});

	Run("report queue: old incidents are forgotten once nothing is pending", [&]
	{
		FReportQueue Q(C);
		const uint64_t I = Q.FindOrCreateIncident(Shots, {0, 0, 0}, 0.0);
		Q.Queue(1, I, 35.f, 1.0);
		Q.Collect(100.0);
		CHECK(Q.NumIncidents() == 0);
	});

	Run("end to end: two witnesses of a murder, one silenced", [&]
	{
		FModel M(C);
		FReportQueue Q(C);
		double Now = 0.0;
		const uint64_t I = Q.FindOrCreateIncident(Murder, {0, 0, 0}, Now);
		Q.Queue(1, I, 80.f, 40.0);
		Q.Queue(2, I, 80.f, 55.0);
		Advance(M, Now, 30.0);
		CHECK(M.GetHeat() == 0.f);                          // nobody has reported yet: he can still get away
		Q.CancelWitness(1);                                  // silenced
		for (; Now < 60.0; )
		{
			Advance(M, Now, 1.0);
			for (const FDelivery& D : Q.Collect(Now))
			{
				if (D.CorroborationIndex == 0) M.AddCrime(D.CrimeType, D.BaseHeat, Now, "report");
				else M.AddCorroboration(D.BaseHeat, D.CorroborationIndex, Now, "corroboration");
			}
		}
		CHECK(M.GetWanted() == EWanted::Lethal);            // the second witness got through at 55 s
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
