// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Vehicle/Theft theft_rules_test.cpp -o tt && ./tt
#include "TheftRules.h"

#include <cmath>
#include <cstdio>
#include <functional>

using namespace MurdarTheft;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

int main()
{
	const FTuning T;

	Run("more cars are locked at night", [&]
	{
		CHECK(RollLocked(0.5f, false, T));
		CHECK(!RollLocked(0.6f, false, T));
		CHECK(RollLocked(0.6f, true, T));
		CHECK(!RollLocked(0.9f, true, T));
	});

	Run("carjack: only a traffic car, slow, within reach, never police", [&]
	{
		FCarjackCheck C; C.bAIDriven = true; C.bHasTrafficDriver = true; C.SpeedKph = 5.f; C.DistanceCm = 200.f;
		CHECK(CanCarjack(C, T));
		FCarjackCheck Fast = C; Fast.SpeedKph = 30.f; CHECK(!CanCarjack(Fast, T));
		FCarjackCheck Far = C; Far.DistanceCm = 400.f; CHECK(!CanCarjack(Far, T));
		FCarjackCheck Cop = C; Cop.bPolice = true; CHECK(!CanCarjack(Cop, T));
		FCarjackCheck Parked = C; Parked.bAIDriven = false; CHECK(!CanCarjack(Parked, T));
		FCarjackCheck NoDriver = C; NoDriver.bHasTrafficDriver = false; CHECK(!CanCarjack(NoDriver, T));
	});

	Run("break-in: window, then wires, then the car", [&]
	{
		FBreakIn B; B.Start(10.f);
		CHECK(B.Update(10.5f, 100.f, T) == FBreakIn::EState::Window);
		CHECK(!B.ConsumeWindowBroken(10.5f, T));
		CHECK(B.Update(11.3f, 100.f, T) == FBreakIn::EState::Wires);
		CHECK(B.ConsumeWindowBroken(11.3f, T));
		CHECK(!B.ConsumeWindowBroken(11.4f, T));   // once
		CHECK(Near(B.Progress01(11.85f, T), 0.5f));
		CHECK(B.Update(13.75f, 100.f, T) == FBreakIn::EState::Done);
		CHECK(B.Progress01(20.f, T) == 1.f);
	});

	Run("break-in: walking away cancels, and stays cancelled", [&]
	{
		FBreakIn B; B.Start(0.f);
		CHECK(B.Update(1.f, 300.f, T) == FBreakIn::EState::Cancelled);
		CHECK(B.Update(9.f, 50.f, T) == FBreakIn::EState::Cancelled);
		CHECK(B.Progress01(9.f, T) == 0.f);
		CHECK(!B.ConsumeWindowBroken(9.f, T));
		FBreakIn Idle; CHECK(Idle.Update(5.f, 0.f, T) == FBreakIn::EState::Idle);
	});

	Run("who saw it: an alarm is heard further", [&]
	{
		CHECK(!Witnessed(-1.f, true, T));
		CHECK(Witnessed(1500.f, false, T));
		CHECK(!Witnessed(3000.f, false, T));
		CHECK(Witnessed(3000.f, true, T));
		CHECK(!Witnessed(6000.f, true, T));
	});

	Run("report delays: victim fast, witness slower, owner much later", [&]
	{
		CHECK(Near(ReportDelay(EMethod::Carjack, false, 0.f, T), 15.f));
		CHECK(Near(ReportDelay(EMethod::Carjack, false, 1.f, T), 35.f));
		CHECK(Near(ReportDelay(EMethod::BreakIn, true, 0.5f, T), 52.5f));
		CHECK(Near(ReportDelay(EMethod::Unlocked, false, 0.f, T), 240.f));
		CHECK(ReportDelay(EMethod::BreakIn, false, 2.f, T) == T.ReportOwnerMax); // clamped
	});

	Run("the paper trail: reported once, recognised once, respray ends it", [&]
	{
		FStolenCar S; S.ReportAt = 30.f;
		CHECK(!S.UpdateReport(29.f));
		CHECK(!S.UpdateRecognition(true, true));   // not reported yet: nothing to recognise
		CHECK(S.UpdateReport(30.f));
		CHECK(!S.UpdateReport(31.f));
		CHECK(!S.UpdateRecognition(true, false));  // he isn't in it
		CHECK(!S.UpdateRecognition(false, true));  // nobody sees him
		CHECK(S.UpdateRecognition(true, true));
		CHECK(!S.UpdateRecognition(true, true));   // once
		FStolenCar C; C.ReportAt = 10.f; C.bCleaned = true;
		CHECK(!C.UpdateReport(99.f));
	});

	Run("the crime is enough for a stop, not a chase", [&]
	{
		CHECK(T.CrimeSeverity >= 12.f && T.CrimeSeverity < 40.f); // HeatStop / HeatPursuit in UFactionMemorySubsystem
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
