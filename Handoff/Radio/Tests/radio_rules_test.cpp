// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Radio radio_rules_test.cpp -o rt && ./rt
#include "RadioRules.h"

#include <cstdio>
#include <functional>
#include <set>

using namespace MurdarRadio;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }
static bool Near(float A, float B, float E = 1e-3f) { return std::fabs(A - B) < E; }

static FStationContent Station()
{
	FStationContent C;
	C.Songs = { 200.f, 180.f, 240.f, 210.f, 190.f, 220.f };
	C.SongLicensed = { false, true, false, false, true, false };
	C.Djs = { 15.f, 12.f };
	C.Ads = { 30.f };
	C.Seed = 7;
	return C;
}

int main()
{
	Run("the program: every song once, links interleaved", [&]
	{
		const std::vector<FItem> P = BuildProgram(Station(), false);
		std::set<int> Songs; int Dj = 0, Ad = 0, News = 0;
		for (const FItem& I : P)
		{
			if (I.Kind == EKind::Song) { Songs.insert(I.Index); }
			Dj += I.Kind == EKind::Dj; Ad += I.Kind == EKind::Ad; News += I.Kind == EKind::News;
		}
		CHECK(Songs.size() == 6);
		CHECK(News == 1);           // after the 6th song
		CHECK(Ad == 1);             // after the 3rd (the 6th is news)
		CHECK(Dj == 2);             // after the 2nd and 4th
		CHECK(P.front().Kind == EKind::Song);
	});

	Run("same seed, same program; another seed, another order", [&]
	{
		const std::vector<FItem> A = BuildProgram(Station(), false), B = BuildProgram(Station(), false);
		bool bSame = A.size() == B.size();
		for (size_t i = 0; bSame && i < A.size(); ++i) { bSame = A[i].Kind == B[i].Kind && A[i].Index == B[i].Index; }
		CHECK(bSame);
		FStationContent Other = Station(); Other.Seed = 99;
		const std::vector<FItem> C = BuildProgram(Other, false);
		bool bDiffers = false;
		for (size_t i = 0; i < C.size() && i < A.size(); ++i) { bDiffers |= C[i].Index != A[i].Index; }
		CHECK(bDiffers);
	});

	Run("streamer mode drops the licensed songs only", [&]
	{
		const std::vector<FItem> P = BuildProgram(Station(), true);
		int Songs = 0; bool bLicensed = false;
		for (const FItem& I : P) { if (I.Kind == EKind::Song) { ++Songs; bLicensed |= (I.Index == 1 || I.Index == 4); } }
		CHECK(Songs == 4 && !bLicensed);
	});

	Run("tuning in lands mid-item, by the clock", [&]
	{
		const std::vector<FItem> P = BuildProgram(Station(), false);
		const FPosition A = At(P, 0.0, 0.f);
		CHECK(A.Item == 0 && Near(A.Offset, 0.f));
		const FPosition B = At(P, double(P[0].Duration) + 5.0, 0.f);
		CHECK(B.Item == 1 && Near(B.Offset, 5.f));
		const FPosition C = At(P, double(Length(P)) * 3.0 + 1.0, 0.f); // loops
		CHECK(C.Item == 0 && Near(C.Offset, 1.f, 0.05f));
		const FPosition D = At(P, 0.0, P[0].Duration + 2.f);          // another station's phase
		CHECK(D.Item == 1 && Near(D.Offset, 2.f));
		CHECK(At({}, 10.0, 0.f).Item == -1);
		CHECK(At(P, -5.0, 0.f).Item >= 0);
	});

	Run("a talk station (no songs) still plays", [&]
	{
		FStationContent Talk; Talk.Djs = { 60.f };
		const std::vector<FItem> P = BuildProgram(Talk, false);
		CHECK(P.size() == 1 && P[0].Kind == EKind::Dj);
	});

	Run("the dial wraps through off", [&]
	{
		CHECK(Dial(-1, 3, +1) == 0);
		CHECK(Dial(2, 3, +1) == -1);
		CHECK(Dial(-1, 3, -1) == 2);
		CHECK(Dial(0, 3, -1) == -1);
		CHECK(Dial(0, 0, +1) == -1);
	});

	Run("the news desk", [&]
	{
		FNewsDesk D;
		CHECK(D.Take(0.0).empty());
		D.Add("News.CarTheft", 1, 0.0, 600.f);
		D.Add("News.Chase", 3, 0.0, 300.f);
		D.Add("News.Chase", 2, 100.0, 300.f); // refresh, keeps the higher priority
		CHECK(D.Count() == 2);
		CHECK(D.Take(10.0) == "News.Chase");
		CHECK(D.Take(10.0) == "News.CarTheft");
		D.Add("News.Arrest", 5, 0.0, 60.f);
		CHECK(D.Take(61.0).empty());           // stale
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
