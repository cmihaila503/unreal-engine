// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Vehicle/Garage garage_rules_test.cpp -o gt && ./gt
#include "GarageRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarGarage;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-3f; }

static const FColor3 White{ 0.9f, 0.9f, 0.88f }, OffWhite{ 0.85f, 0.85f, 0.8f }, Red{ 0.6f, 0.05f, 0.05f },
	DarkRed{ 0.5f, 0.06f, 0.06f }, Blue{ 0.1f, 0.2f, 0.55f }, Beige{ 0.8f, 0.72f, 0.55f };

int main()
{
	Run("colour distance: same-looking colours are close", [&]
	{
		CHECK(ColorDistance(White, White) == 0.f);
		CHECK(ColorDistance(White, OffWhite) < 0.35f);
		CHECK(ColorDistance(Red, DarkRed) < 0.35f);
		CHECK(ColorDistance(White, Red) > 0.6f);
		CHECK(Near(ColorDistance(Red, Blue), ColorDistance(Blue, Red)));
	});

	Run("the description: model and colour", [&]
	{
		CHECK(MatchesDescription(true, Red, DarkRed));
		CHECK(!MatchesDescription(true, Red, Blue));      // resprayed
		CHECK(!MatchesDescription(false, Red, Red));      // another model
	});

	Run("a new colour is clearly different", [&]
	{
		const std::vector<FColor3> Palette = { OffWhite, Red, Blue, Beige };
		for (float r : { 0.f, 0.3f, 0.6f, 0.99f })
		{
			const int i = PickNewColour(White, Palette, r);
			CHECK(i >= 0 && ColorDistance(White, Palette[i]) >= 0.6f);
		}
		CHECK(PickNewColour(White, { OffWhite }, 0.5f) == -1);
		CHECK(PickNewColour(White, {}, 0.5f) == -1);
	});

	Run("prices: paint + repair, dearer when hot", [&]
	{
		const FPrices P;
		CHECK(Near(ResprayPrice(0.f, false, P), 150.f));
		CHECK(Near(ResprayPrice(0.5f, false, P), 350.f));
		CHECK(Near(ResprayPrice(0.5f, true, P), 700.f));
		CHECK(Near(ResprayPrice(3.f, false, P), 550.f));
		CHECK(Near(ImpoundFee(0, P), 200.f) && Near(ImpoundFee(3, P), 350.f) && Near(ImpoundFee(-2, P), 200.f));
	});

	Run("when the man will paint it", [&]
	{
		FResprayCheck C; C.Cash = 1000.f; C.Price = 150.f;
		CHECK(CanRespray(C) == EResprayResult::Ok);
		FResprayCheck Seen = C; Seen.SinceSeenByPolice = 2.f; CHECK(CanRespray(Seen) == EResprayResult::Seen);
		FResprayCheck Moving = C; Moving.SpeedKph = 20.f; CHECK(CanRespray(Moving) == EResprayResult::Moving);
		FResprayCheck Poor = C; Poor.Cash = 100.f; CHECK(CanRespray(Poor) == EResprayResult::NoMoney);
	});

	Run("a chase cools to under a stop; a stop stays", [&]
	{
		CHECK(Near(HeatAfterRespray(60.f, 12.f, 40.f), 11.f));
		CHECK(Near(HeatAfterRespray(90.f, 12.f, 40.f), 11.f));
		CHECK(Near(HeatAfterRespray(20.f, 12.f, 40.f), 20.f));
		CHECK(Near(HeatAfterRespray(0.f, 12.f, 40.f), 0.f));
	});

	Run("storage slots", [&]
	{
		CHECK(FreeSlot({ true, false, true }) == 1);
		CHECK(FreeSlot({ true, true }) == -1);
		CHECK(FreeSlot({}) == -1);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
