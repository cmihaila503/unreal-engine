// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/UI/PaperMap paper_map_rules_test.cpp -o pm && ./pm
#include "PaperMapRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarMap;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B, float E = 1e-4f) { return std::fabs(A - B) < E; }

int main()
{
	const FBounds B{ { -300000.f, -250000.f }, { 300000.f, 250000.f } }; // 6 x 5 km
	const FViewLimits L;

	Run("world to paper and back", [&]
	{
		const FV2 C = WorldToUV({ 0.f, 0.f }, B);
		CHECK(Near(C.X, 0.5f) && Near(C.Y, 0.5f));
		const FV2 Corner = WorldToUV({ 300000.f, -250000.f }, B);
		CHECK(Near(Corner.X, 1.f) && Near(Corner.Y, 0.f));
		const FV2 W = UVToWorld(WorldToUV({ 12345.f, -6789.f }, B), B);
		CHECK(Near(W.X, 12345.f, 0.1f) && Near(W.Y, -6789.f, 0.1f));
		CHECK(OnPaper(C) && !OnPaper(WorldToUV({ 400000.f, 0.f }, B)));
		CHECK(Near(WorldToUV({ 5.f, 5.f }, FBounds{}).X, 0.f)); // degenerate bounds don't divide by zero
	});

	Run("the view never shows past the paper", [&]
	{
		FView V; V.Zoom = 2.f; V.Center = { 0.05f, 0.95f };
		const FView C = Clamp(V, 1.f, L);
		CHECK(Near(C.Center.X, 0.25f) && Near(C.Center.Y, 0.75f));
		FView Wide; Wide.Zoom = 1.f; Wide.Center = { 0.1f, 0.1f };
		const FView W = Clamp(Wide, 16.f / 9.f, L);
		CHECK(Near(W.Center.X, 0.5f) && Near(W.Center.Y, 0.5f)); // whole paper visible: centred
		FView Far; Far.Zoom = 50.f;
		CHECK(Near(Clamp(Far, 1.f, L).Zoom, 6.f));
	});

	Run("zooming keeps the point under the cursor", [&]
	{
		FView V; V.Zoom = 2.f; V.Center = { 0.5f, 0.5f };
		const FV2 Cursor{ 0.75f, 0.5f };
		const FV2 Before = { V.Center.X + (Cursor.X - 0.5f) / V.Zoom, 0.5f };
		const FView N = ZoomAt(V, 2.f, Cursor, 1.f, L);
		const FV2 After = { N.Center.X + (Cursor.X - 0.5f) / N.Zoom, 0.5f };
		CHECK(Near(N.Zoom, 4.f));
		CHECK(Near(Before.X, After.X));
		const FView Out = ZoomAt(N, 0.01f, Cursor, 1.f, L);
		CHECK(Near(Out.Zoom, 1.f) && Near(Out.Center.X, 0.5f));
	});

	Run("paper to screen", [&]
	{
		FView V; V.Zoom = 2.f; V.Center = { 0.5f, 0.5f };
		const FV2 S = UVToViewport({ 0.75f, 0.5f }, V, 1.f);
		CHECK(Near(S.X, 1.f) && Near(S.Y, 0.5f));
		const FV2 Mid = UVToViewport({ 0.5f, 0.5f }, V, 16.f / 9.f);
		CHECK(Near(Mid.X, 0.5f) && Near(Mid.Y, 0.5f));
	});

	Run("what's marked", [&]
	{
		CHECK(ShowMarker(EMarker::Player, { false, false }));
		CHECK(!ShowMarker(EMarker::Safehouse, { false, false }));
		CHECK(ShowMarker(EMarker::Safehouse, { true, false }));
		CHECK(!ShowMarker(EMarker::JobDrop, { true, false }));
		CHECK(ShowMarker(EMarker::JobDrop, { false, true }));
	});

	Run("opens on him", [&]
	{
		const FView V = OpenAt({ 0.9f, 0.1f }, 1.f, L);
		CHECK(Near(V.Zoom, 2.f) && Near(V.Center.X, 0.75f) && Near(V.Center.Y, 0.25f));
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
