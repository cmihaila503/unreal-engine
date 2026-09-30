// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Vehicle/Weight car_weight_rules_test.cpp -o cw && ./cw
#include "CarWeightRules.h"

#include <cstdio>

using namespace MurdarWeight;

static int GChecks = 0, GFailed = 0;
#define CHECK(c) do { ++GChecks; if (!(c)) { ++GFailed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool Near(float A, float B, float Eps = 1e-3f) { return std::fabs(A - B) <= Eps; }

static void Airborne()
{
	FAirTuning T;
	FAirState S;
	const float Dt = 1.f / 60.f;
	CHECK(!S.Update(4, Dt, T) && !S.bAirborne);
	// a bump: wheels off for 3 ticks (50 ms) is not a jump
	S.Update(0, Dt, T); S.Update(0, Dt, T); S.Update(0, Dt, T);
	CHECK(!S.bAirborne);
	CHECK(!S.Update(4, Dt, T));   // back down without "landing"
	// a jump: 1 s off
	for (int i = 0; i < 60; ++i) { S.Update(0, Dt, T); }
	CHECK(S.bAirborne && S.AirTime > 0.85f && S.AirTime < 0.95f);
	CHECK(S.Update(2, Dt, T));    // lands
	CHECK(!S.bAirborne && S.SinceLanding == 0.f && S.LastAirTime > 0.85f);
	S.Update(2, Dt, T);
	CHECK(Near(S.SinceLanding, Dt));
}

static void Gravity()
{
	FAirTuning T;
	FAirState S;
	CHECK(ExtraGravity(S, T) == 0.f);                 // on the ground: nothing
	S.bAirborne = true; S.AirTime = 0.f;
	CHECK(ExtraGravity(S, T) == 0.f);                 // just left: ramps from 0
	S.AirTime = 0.125f;
	CHECK(Near(ExtraGravity(S, T), 0.8f * G * 0.5f, 0.5f));
	S.AirTime = 2.f;
	CHECK(Near(ExtraGravity(S, T), 0.8f * G, 0.5f));  // 1.8 g total
	FAirTuning Real = T; Real.GravityMultiplier = 1.f;
	CHECK(ExtraGravity(S, Real) == 0.f);
	// a 1 m drop takes ~0.45 s at 1 g, ~0.36 s at 1.8 g: jumps end sooner, like a heavy car
	const float H = 100.f;
	const float TReal = std::sqrt(2.f * H / G), THeavy = std::sqrt(2.f * H / (1.8f * G));
	CHECK(THeavy < TReal * 0.8f);
}

static void Stabilize()
{
	FAirTuning T;
	FAirState Ground;
	FAxes None = AirStabilize({ 1.f, 1.f }, { 0.2f, 0.2f }, Ground, T);
	CHECK(None.Pitch == 0.f && None.Roll == 0.f);       // never on the ground
	FAirState Air; Air.bAirborne = true;
	FAxes A = AirStabilize({ 2.f, -1.f }, { 0.f, 0.f }, Air, T);
	CHECK(A.Pitch < 0.f && A.Roll > 0.f);               // opposes the spin
	FAxes Lvl = AirStabilize({ 0.f, 0.f }, { 0.3f, -0.3f }, Air, T);
	CHECK(Lvl.Pitch < 0.f && Lvl.Roll > 0.f);           // nudges level
	FAxes Over = AirStabilize({ 0.f, 0.f }, { 1.2f, 0.f }, Air, T); // ~69 deg: rolling over for real
	CHECK(Over.Pitch == 0.f);
}

static void Settle()
{
	FAirTuning T;
	FAirState S; S.SinceLanding = 0.f;
	CHECK(LandingSettle(300.f, S, T) < 0.f);            // bouncing up: damped
	CHECK(LandingSettle(-300.f, S, T) == 0.f);          // compressing: leave the suspension alone
	S.SinceLanding = T.SettleSeconds * 0.5f;
	CHECK(Near(LandingSettle(300.f, S, T), -T.SettleDamping * 300.f * 0.5f, 0.5f)); // fades
	S.SinceLanding = T.SettleSeconds + 0.01f;
	CHECK(LandingSettle(300.f, S, T) == 0.f);
}

static void Impact()
{
	FImpactTuning T;
	FImpactFeel Scrape = ImpactFeel(100.f, 0.f, true, 10.f, T);
	CHECK(Scrape.Severity == 0.f && Scrape.Shake == 0.f && Scrape.HitStopSeconds == 0.f);
	FImpactFeel Light = ImpactFeel(400.f, 0.f, true, 10.f, T);
	CHECK(Light.Severity > 0.1f && Light.Severity < 0.35f && Light.HitStopSeconds == 0.f && Light.Shake > T.ShakeMin);
	FImpactFeel Hard = ImpactFeel(1500.f, 0.f, true, 10.f, T);
	CHECK(Near(Hard.Severity, 1.f) && Near(Hard.HitStopSeconds, T.HitStopMaxSeconds) && Near(Hard.Shake, T.ShakeMax));
	FImpactFeel Again = ImpactFeel(1500.f, 0.f, true, 0.3f, T);
	CHECK(Again.HitStopSeconds == 0.f && Again.Shake > 0.f);    // cooldown: no second stop
	FImpactFeel Ai = ImpactFeel(1500.f, 0.f, false, 10.f, T);
	CHECK(Near(Ai.Severity, 1.f) && Ai.Shake == 0.f && Ai.HitStopSeconds == 0.f);
	FImpactFeel Landing = ImpactFeel(0.f, 1000.f, true, 10.f, T);
	FImpactFeel Wall = ImpactFeel(1000.f, 0.f, true, 10.f, T);
	CHECK(Landing.Severity < Wall.Severity);                    // a landing counts less than a wall
	CHECK(Hard.ThumpVolume > Light.ThumpVolume && Scrape.ThumpVolume == 0.f);
}

static void Scale()
{
	FScaleCheck Ok = CheckScale(440.f, Dacia1300Length.Cm);
	CHECK(Ok.Verdict == EScale::Ok);
	FScaleCheck Small = CheckScale(330.f, Dacia1300Length.Cm);
	CHECK(Small.Verdict == EScale::TooSmall && Small.Factor > 1.3f && Small.Factor < 1.33f);
	FScaleCheck WideLane = CheckScale(560.f, Lane.Cm);
	CHECK(WideLane.Verdict == EScale::TooBig && WideLane.Factor < 0.62f);
	CHECK(CheckScale(0.f, 100.f).Verdict == EScale::Ok);  // nothing measured: no verdict
	CHECK(Diagnose(Small, WideLane).find("car mesh") != std::string::npos);
	CHECK(Diagnose(Ok, WideLane).find("narrow the lanes") != std::string::npos);
	CHECK(Diagnose(Ok, CheckScale(340.f, Lane.Cm)).find("camera") != std::string::npos);
	CHECK(CameraLooksSmall(92.f, 700.f));                 // today's defaults
	FCameraTarget C;
	CHECK(!CameraLooksSmall(C.FovAtSpeed, C.DistanceAtSpeedCm));
}

int main()
{
	Airborne();
	Gravity();
	Stabilize();
	Settle();
	Impact();
	Scale();
	std::printf("%d checks, %d failed\n", GChecks, GFailed);
	return GFailed == 0 ? 0 : 1;
}
