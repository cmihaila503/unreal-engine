// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Weather weather_rules_test.cpp -o wt && ./wt
#include "WeatherRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarWeather;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B, float E = 1e-4f) { return std::fabs(A - B) < E; }

int main()
{
	const FMatrix M = DefaultMatrix();
	const FTuning T;

	Run("the chain follows the weights", [&]
	{
		CHECK(Next(EState::Clear, 0.f, true, M) == EState::Clear);
		CHECK(Next(EState::Clear, 0.85f, true, M) == EState::Cloudy);
		CHECK(Next(EState::Storm, 0.f, true, M) == EState::Cloudy);   // storms never jump to clear
		CHECK(Next(EState::Storm, 0.5f, true, M) == EState::Rain);
		CHECK(Next(EState::Fog, 0.99f, false, M) != EState::Fog);      // fog can't persist by day
		FMatrix Z{}; CHECK(Next(EState::Rain, 0.5f, true, Z) == EState::Rain); // all-zero row: stay
	});

	Run("over many hours it's mostly fine, and every state happens", [&]
	{
		int Count[N] = { 0 };
		EState S = EState::Clear;
		unsigned Seed = 12345u;
		for (int h = 0; h < 20000; ++h)
		{
			Seed = Seed * 1664525u + 1013904223u;
			S = Next(S, float(Seed >> 8) / float(1u << 24), (h % 24) < 7, M);
			++Count[int(S)];
		}
		CHECK(Count[0] > Count[2] && Count[0] > Count[3]);
		for (int i = 0; i < N; ++i) { CHECK(Count[i] > 0); }
	});

	Run("states blend, never snap", [&]
	{
		FLook L = LookOf(EState::Clear);
		L = Blend(L, LookOf(EState::Storm), 10.f, T.BlendRate);
		CHECK(L.Rain > 0.f && L.Rain < 0.2f);
		for (int i = 0; i < 200; ++i) { L = Blend(L, LookOf(EState::Storm), 1.f, T.BlendRate); }
		CHECK(Near(L.Rain, 1.f) && Near(L.Cloud, 1.f));
		const FLook Back = Blend(L, LookOf(EState::Clear), 1e6f, T.BlendRate);
		CHECK(Near(Back.Rain, 0.f));
	});

	Run("roads get wet and dry, slower at night", [&]
	{
		float W = 0.f;
		W = UpdateWetness(W, 1.f, false, 60.f, T); CHECK(Near(W, 0.5f));
		W = UpdateWetness(W, 1.f, false, 600.f, T); CHECK(Near(W, 1.f));
		const float Day = UpdateWetness(1.f, 0.f, false, 300.f, T), Night = UpdateWetness(1.f, 0.f, true, 300.f, T);
		CHECK(Near(Day, 0.5f) && Night > Day);
		CHECK(UpdateWetness(0.f, 0.f, false, 100.f, T) == 0.f);
	});

	Run("grip, people, gloom", [&]
	{
		CHECK(Near(GripScale(0.f, T), 1.f) && Near(GripScale(1.f, T), 0.75f) && Near(GripScale(0.5f, T), 0.875f));
		CHECK(Near(PeopleScale(LookOf(EState::Clear), T), 1.f));
		CHECK(Near(PeopleScale(LookOf(EState::Storm), T), 0.35f));
		CHECK(PeopleScale(LookOf(EState::Fog), T) < 1.f && PeopleScale(LookOf(EState::Fog), T) > 0.35f);
		CHECK(Gloom(LookOf(EState::Storm)) > Gloom(LookOf(EState::Cloudy)) && Gloom(LookOf(EState::Clear)) < 0.1f);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
