// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/UI/Menus menu_rules_test.cpp -o mnt && ./mnt
#include "MenuRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarMenu;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-4f; }

int main()
{
	Run("focus skips disabled rows and wraps", [&]
	{
		const std::vector<bool> E = { true, false, true, true }; // Continue, [Save disabled], Load, Quit
		CHECK(FirstFocus(E) == 0);
		CHECK(MoveFocus(E, 0, +1) == 2);
		CHECK(MoveFocus(E, 3, +1) == 0);
		CHECK(MoveFocus(E, 0, -1) == 3);
		CHECK(MoveFocus(E, 2, -1) == 0);
	});

	Run("focus edge cases", [&]
	{
		CHECK(MoveFocus({}, 0, 1) == -1);
		CHECK(MoveFocus({ false, false }, 0, 1) == -1);
		CHECK(MoveFocus({ false, true, false }, 1, 1) == 1);  // the only enabled row keeps focus
		CHECK(FirstFocus({ false, false, true }) == 2);
		CHECK(MoveFocus({ true, true }, 7, -1) == 1);          // out of range: from the end going up
	});

	Run("choices step without wrapping", [&]
	{
		CHECK(StepChoice(2, 4, +1) == 3);
		CHECK(StepChoice(3, 4, +1) == 3);
		CHECK(StepChoice(0, 4, -1) == 0);
		CHECK(StepChoice(5, 0, 1) == 0);
	});

	Run("sliders snap to the step and clamp", [&]
	{
		float V = 0.f;
		for (int i = 0; i < 3; ++i) { V = StepSlider(V, 0.f, 1.f, 0.1f, +1); }
		CHECK(Near(V, 0.3f));
		CHECK(Near(StepSlider(0.97f, 0.f, 1.f, 0.05f, +1), 1.f));
		CHECK(Near(StepSlider(0.02f, 0.f, 1.f, 0.05f, -1), 0.f));
		CHECK(Near(StepSlider(1.f, 0.2f, 3.f, 0.1f, +1), 1.1f));
		CHECK(Near(StepSlider(0.23f, 0.2f, 3.f, 0.1f, -1), 0.2f));
		CHECK(Percent(0.8f) == "80 %" && Percent(1.4f) == "100 %");
	});

	Run("saving only when nothing is going on", [&]
	{
		FSaveContext C;
		CHECK(CanSave(C) == ESaveBlock::None);
		C.bDrivingFast = true; CHECK(CanSave(C) == ESaveBlock::Driving);
		C.bTalking = true; CHECK(CanSave(C) == ESaveBlock::Talking);
		C.bMissionActive = true; CHECK(CanSave(C) == ESaveBlock::Mission);
		C.SinceShot = 5.f; CHECK(CanSave(C) == ESaveBlock::Combat);
		C.Wanted = 1; CHECK(CanSave(C) == ESaveBlock::Wanted);
		FSaveContext Late; Late.SinceShot = 21.f; CHECK(CanSave(Late) == ESaveBlock::None);
	});

	Run("resolutions: unique, big enough, sorted, current kept", [&]
	{
		const std::vector<FRes> All = { { 1920, 1080 }, { 800, 600 }, { 1280, 720 }, { 1920, 1080 }, { 2560, 1440 }, { 0, 0 } };
		std::vector<FRes> O = OfferedResolutions(All, { 1920, 1080 });
		CHECK((O.size() == 3 && O[0] == FRes{ 1280, 720 } && O[2] == FRes{ 2560, 1440 }));
		O = OfferedResolutions(All, { 1024, 640 }); // an odd current mode stays selectable
		CHECK(IndexOf(O, { 1024, 640 }) == 0 && O.size() == 4);
		CHECK(IndexOf(O, { 3840, 2160 }) == -1);
		CHECK(Format({ 1920, 1080 }) == "1920 x 1080");
	});

	Run("the settings page knows when something changed", [&]
	{
		FSettings A, B;
		CHECK(A == B);
		B.Music = 0.8f + 1e-6f; CHECK(A == B);               // float noise is not a change
		B.Music = 0.75f; CHECK(A != B);
		CHECK(!NeedsDisplayConfirm(A, B));
		B.Resolution = { 1280, 720 }; CHECK(NeedsDisplayConfirm(A, B));
		FSettings C; C.WindowMode = 2; CHECK(NeedsDisplayConfirm(A, C));
		CHECK(FrameLimitValue(0) == 0 && FrameLimitValue(2) == 60 && FrameLimitValue(9) == 144);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
