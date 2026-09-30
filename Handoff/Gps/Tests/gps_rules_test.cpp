// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/UI/Gps gps_rules_test.cpp -o gps && ./gps
#include "GpsRules.h"

#include <cstdio>

using namespace MurdarGps;

static int GChecks = 0, GFailed = 0;
#define CHECK(c) do { ++GChecks; if (!(c)) { ++GFailed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool Near(float A, float B, float Eps = 1e-2f) { return std::fabs(A - B) <= Eps; }

static void Pins()
{
	FPins P;
	CHECK(!P.Get(EPin::Mission).bSet && !P.Get(EPin::Waypoint).bSet);
	P.Set(EPin::Mission, { 1000.f, 0.f });
	CHECK(P.ToggleWaypoint({ 5000.f, 5000.f }, 3000.f));
	// both at the same time
	CHECK(P.Get(EPin::Mission).bSet && P.Get(EPin::Waypoint).bSet);
	// pressing near it removes it; the mission pin stays
	CHECK(!P.ToggleWaypoint({ 6000.f, 5500.f }, 3000.f));
	CHECK(!P.Get(EPin::Waypoint).bSet && P.Get(EPin::Mission).bSet);
	// far away moves it
	P.ToggleWaypoint({ 0.f, 0.f }, 3000.f);
	CHECK(P.ToggleWaypoint({ 20000.f, 0.f }, 3000.f) && Near(P.Get(EPin::Waypoint).At.X, 20000.f));
	CHECK(WaypointReached(P, { 19000.f, 0.f }, 2500.f));
	CHECK(!WaypointReached(P, { 10000.f, 0.f }, 2500.f));
	P.Clear(EPin::Mission);
	CHECK(!P.Get(EPin::Mission).bSet);
}

static void Projection()
{
	const std::vector<FV2> L = { { 0.f, 0.f }, { 1000.f, 0.f }, { 1000.f, 1000.f } };
	FProjection A = Project(L, { 500.f, 100.f });
	CHECK(A.Segment == 0 && Near(A.T, 0.5f) && Near(A.OffCm, 100.f) && Near(A.RemainingCm, 1500.f));
	FProjection B = Project(L, { 1100.f, 600.f });
	CHECK(B.Segment == 1 && Near(B.OffCm, 100.f) && Near(B.RemainingCm, 400.f));
	// searching forward from segment 1 never goes back to 0
	FProjection C = Project(L, { 10.f, 0.f }, 1);
	CHECK(C.Segment == 1);
	CHECK(Project({}, { 1.f, 1.f }).OffCm == 0.f);
}

static void Following()
{
	FRouteTuning T;
	FRoute R;
	CHECK(R.Update({ 0.f, 0.f }, { 5000.f, 0.f }, 0.f, T));   // no route yet: plan
	R.Adopt({ { 0.f, 0.f }, { 5000.f, 0.f }, { 5000.f, 5000.f } }, { 5000.f, 5000.f }, 0.f);
	CHECK(R.bValid && Near(R.RemainingCm, 10000.f));
	CHECK(!R.Update({ 1000.f, 50.f }, { 5000.f, 5000.f }, 1.f, T));   // on it
	CHECK(Near(R.RemainingCm, 9000.f, 1.f));
	// a wide turn: off for 1 s only -> no re-plan
	CHECK(!R.Update({ 1500.f, 4000.f }, { 5000.f, 5000.f }, 5.f, T));
	CHECK(!R.Update({ 2100.f, 200.f }, { 5000.f, 5000.f }, 5.5f, T));
	// really off: 1.5 s -> re-plan
	CHECK(!R.Update({ 1500.f, 4000.f }, { 5000.f, 5000.f }, 6.f, T));
	CHECK(R.Update({ 1500.f, 4200.f }, { 5000.f, 5000.f }, 7.6f, T));
	// the target moved far: re-plan (but not twice within 2 s of a plan)
	R.Adopt({ { 0.f, 0.f }, { 5000.f, 0.f } }, { 5000.f, 0.f }, 10.f);
	CHECK(!R.Update({ 0.f, 0.f }, { 20000.f, 0.f }, 11.f, T));
	CHECK(R.Update({ 0.f, 0.f }, { 20000.f, 0.f }, 12.1f, T));
	// periodic
	R.Adopt({ { 0.f, 0.f }, { 5000.f, 0.f } }, { 5000.f, 0.f }, 20.f);
	CHECK(!R.Update({ 100.f, 0.f }, { 5000.f, 0.f }, 39.f, T));
	CHECK(R.Update({ 100.f, 0.f }, { 5000.f, 0.f }, 40.1f, T));
	// progress never goes backwards
	R.Adopt({ { 0.f, 0.f }, { 1000.f, 0.f }, { 2000.f, 0.f }, { 3000.f, 0.f } }, { 3000.f, 0.f }, 50.f);
	R.Update({ 2500.f, 0.f }, { 3000.f, 0.f }, 51.f, T);
	const int P = R.Progress;
	R.Update({ 500.f, 0.f }, { 3000.f, 0.f }, 52.f, T);
	CHECK(R.Progress == P && P == 2);
}

static void Minimap()
{
	FMinimapTuning T;
	CHECK(Near(TargetRadius(0.f, T), T.RadiusAtRestCm) && Near(TargetRadius(200.f, T), T.RadiusAtSpeedCm));
	CHECK(TargetRadius(55.f, T) > T.RadiusAtRestCm && TargetRadius(55.f, T) < T.RadiusAtSpeedCm);
	const float S = SmoothRadius(9000.f, 25000.f, 0.1f, T);
	CHECK(S > 9000.f && S < 25000.f);
	CHECK(Near(SmoothRadius(9000.f, 25000.f, 100.f, T), 25000.f, 1.f));

	// heading +X (yaw 0): a point ahead is straight up, a point at +Y (right) is to the right
	FV2 Ahead = ToMinimap({ 5000.f, 0.f }, { 0.f, 0.f }, 0.f, 10000.f);
	CHECK(Near(Ahead.X, 0.f) && Near(Ahead.Y, -0.5f));
	FV2 Right = ToMinimap({ 0.f, 5000.f }, { 0.f, 0.f }, 0.f, 10000.f);
	CHECK(Near(Right.X, 0.5f) && Near(Right.Y, 0.f));
	// heading +Y (yaw 90): +Y is ahead now, +X is to the left
	FV2 Ahead90 = ToMinimap({ 0.f, 5000.f }, { 0.f, 0.f }, 90.f, 10000.f);
	CHECK(Near(Ahead90.X, 0.f) && Near(Ahead90.Y, -0.5f));
	FV2 Left90 = ToMinimap({ 5000.f, 0.f }, { 0.f, 0.f }, 90.f, 10000.f);
	CHECK(Near(Left90.X, -0.5f) && Near(Left90.Y, 0.f));

	FEdge In = ClampToEdge({ 0.3f, -0.4f });
	CHECK(!In.bOnEdge && Near(In.At.X, 0.3f));
	FEdge Out = ClampToEdge({ 2.f, -1.f });
	CHECK(Out.bOnEdge && Near(Out.At.X, 0.92f) && Near(Out.At.Y, -0.46f));
}

static void Visible()
{
	FRoute R;
	std::vector<FV2> Long;
	for (int i = 0; i <= 50; ++i) { Long.push_back({ i * 1000.f, 0.f }); }
	R.Adopt(Long, Long.back(), 0.f);
	R.Progress = 10;
	std::vector<FV2> V = VisibleRoute(R, { 10500.f, 0.f }, 10000.f);
	CHECK(V.size() >= 2 && Near(V[0].X, 10500.f));        // starts at the player
	CHECK(Near(V[1].X, 11000.f));                           // then the next point ahead
	CHECK(V.back().X > 10500.f + 15000.f);                  // stops at the first point outside 1.5 r
	CHECK(V.size() < 25);
	FRoute Empty;
	CHECK(VisibleRoute(Empty, { 0.f, 0.f }, 10000.f).empty());
}

int main()
{
	Pins();
	Projection();
	Following();
	Minimap();
	Visible();
	std::printf("%d checks, %d failed\n", GChecks, GFailed);
	return GFailed == 0 ? 0 : 1;
}
