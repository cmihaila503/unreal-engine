// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/UI/KeyRemap key_remap_rules_test.cpp -o kr && ./kr
#include "KeyRemapRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarKeys;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

static std::vector<FBinding> Defaults()
{
	return {
		{ "Interact",  ESlot::KeyboardMouse, "E",        { "OnFoot" } },
		{ "Reload",    ESlot::KeyboardMouse, "R",        { "OnFoot" } },
		{ "Handbrake", ESlot::KeyboardMouse, "SpaceBar", { "Vehicle" } },
		{ "Jump",      ESlot::KeyboardMouse, "SpaceBar", { "OnFoot" } },
		{ "Horn",      ESlot::KeyboardMouse, "H",        { "Vehicle" } },
		{ "Interact",  ESlot::Gamepad,       "Gamepad_FaceButton_Left", { "OnFoot" } },
	};
}

int main()
{
	const std::vector<std::string> Reserved = { "Escape", "Gamepad_Special_Right" };

	Run("the defaults have no clashes (same key in different contexts is fine)", [&]
	{
		CHECK(CountClashes(Defaults()) == 0);
	});

	Run("a free key just binds", [&]
	{
		auto B = Defaults();
		CHECK(Rebind(B, 0, "F", Reserved) == EResult::Ok && B[0].Key == "F");
	});

	Run("a clash in the same context swaps", [&]
	{
		auto B = Defaults();
		CHECK(Rebind(B, 0, "R", Reserved) == EResult::Swapped);
		CHECK(B[0].Key == "R" && B[1].Key == "E");
		CHECK(CountClashes(B) == 0);
	});

	Run("a key used only in another context doesn't clash", [&]
	{
		auto B = Defaults();
		CHECK(Rebind(B, 1, "H", Reserved) == EResult::Ok);  // Horn is Vehicle-only
		CHECK(B[4].Key == "H");
	});

	Run("reserved keys, wrong slot, same key", [&]
	{
		auto B = Defaults();
		CHECK(Rebind(B, 0, "Escape", Reserved) == EResult::Reserved);
		CHECK(Rebind(B, 0, "Gamepad_FaceButton_Bottom", Reserved) == EResult::WrongSlot);
		CHECK(Rebind(B, 5, "Q", Reserved) == EResult::WrongSlot);
		CHECK(Rebind(B, 0, "E", Reserved) == EResult::Same);
		CHECK(Rebind(B, 99, "E", Reserved) == EResult::WrongSlot);
		CHECK(B[0].Key == "E");
	});

	Run("counting clashes in a broken set", [&]
	{
		auto B = Defaults();
		B[1].Key = "E"; // two on-foot actions on E
		CHECK(CountClashes(B) == 1);
		CHECK(Conflicts(B, 0, "E").size() == 1 && Conflicts(B, 0, "E")[0] == 1);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
