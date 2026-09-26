// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Music music_rules_test.cpp -o mt && ./mt
#include "MusicRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarMusic;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

int main()
{
	const FTuning T;

	Run("what the moment asks for", [&]
	{
		FInputs I;
		CHECK(TargetMood(I, T) == EMood::Silence);
		I.Stress01 = 0.6f; CHECK(TargetMood(I, T) == EMood::Unease);
		I.Stress01 = 0.f; I.Wanted = 1; CHECK(TargetMood(I, T) == EMood::Unease);     // being pulled over
		I.Wanted = 0; I.bTailed = true; CHECK(TargetMood(I, T) == EMood::Suspense);
		I.SincePursuitEnded = 5.f; CHECK(TargetMood(I, T) == EMood::Aftermath);      // the breath after beats the tail
		I.Wanted = 2; CHECK(TargetMood(I, T) == EMood::Pursuit);
		I.SinceShot = 3.f; CHECK(TargetMood(I, T) == EMood::Combat);
		FInputs L; L.Wanted = 3; CHECK(TargetMood(L, T) == EMood::Combat);
		FInputs Old; Old.SinceShot = 12.f; CHECK(TargetMood(Old, T) == EMood::Silence); // the window closed
	});

	Run("intensity per mood, nudged by stress", [&]
	{
		CHECK(Intensity(EMood::Silence, 1.f) == 0.f);
		CHECK(Near(Intensity(EMood::Suspense, 0.5f), 0.4f));
		CHECK(Near(Intensity(EMood::Suspense, 1.f), 0.45f));
		CHECK(Intensity(EMood::Combat, 1.f) <= 1.f);
		CHECK(Intensity(EMood::Pursuit, 0.f) > Intensity(EMood::Suspense, 1.f));
	});

	Run("going up is immediate", [&]
	{
		FMoodMachine M;
		CHECK(M.Update(EMood::Suspense, 1.f, T) && M.Get() == EMood::Suspense);
		CHECK(M.Update(EMood::Combat, 1.1f, T) && M.Get() == EMood::Combat);
		CHECK(!M.Update(EMood::Combat, 2.f, T));
	});

	Run("coming down waits for the delay and the dwell", [&]
	{
		FMoodMachine M;
		M.Update(EMood::Pursuit, 0.f, T);
		CHECK(!M.Update(EMood::Silence, 3.f, T));    // lower since 3
		CHECK(!M.Update(EMood::Silence, 8.9f, T));   // 5.9 s lower
		CHECK(M.Update(EMood::Silence, 9.f, T) && M.Get() == EMood::Silence);
	});

	Run("a short pause in the chase doesn't drop the score", [&]
	{
		FMoodMachine M;
		M.Update(EMood::Pursuit, 0.f, T);
		M.Update(EMood::Aftermath, 10.f, T);          // lost them behind a block...
		CHECK(!M.Update(EMood::Pursuit, 14.f, T));    // ...found again: still pursuit, no change
		CHECK(!M.Update(EMood::Aftermath, 15.f, T));  // the lower timer restarted
		CHECK(!M.Update(EMood::Aftermath, 20.9f, T));
		CHECK(M.Update(EMood::Aftermath, 21.f, T));
	});

	Run("dwell: a mood just entered isn't left at once", [&]
	{
		FMoodMachine M;
		M.Update(EMood::Suspense, 0.f, T);
		M.Update(EMood::Silence, 0.5f, T);
		CHECK(!M.Update(EMood::Silence, 7.f, T));     // lower for 6.5 s but played only 7 s < 8
		CHECK(M.Update(EMood::Silence, 8.f, T));
	});

	Run("the target changing while lower restarts the wait", [&]
	{
		FMoodMachine M;
		M.Update(EMood::Combat, 0.f, T);
		M.Update(EMood::Aftermath, 10.f, T);
		CHECK(!M.Update(EMood::Silence, 14.f, T));    // a different lower target: timer from 14
		CHECK(!M.Update(EMood::Silence, 19.f, T));
		CHECK(M.Update(EMood::Silence, 20.f, T) && M.Get() == EMood::Silence);
	});

	Run("fast rise, slow release", [&]
	{
		CHECK(Near(Approach(0.f, 1.f, 0.5f, T), 0.4f));
		CHECK(Near(Approach(0.9f, 1.f, 1.f, T), 1.f));
		CHECK(Near(Approach(1.f, 0.f, 1.f, T), 0.88f));
		CHECK(Near(Approach(0.05f, 0.f, 1.f, T), 0.f));
	});

	Run("layers fade in over their range", [&]
	{
		CHECK(LayerGain(0.2f, 0.3f, 0.6f) == 0.f);
		CHECK(Near(LayerGain(0.45f, 0.3f, 0.6f), 0.5f));
		CHECK(LayerGain(0.9f, 0.3f, 0.6f) == 1.f);
		CHECK(LayerGain(0.5f, 0.5f, 0.5f) == 1.f && LayerGain(0.49f, 0.5f, 0.5f) == 0.f);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
