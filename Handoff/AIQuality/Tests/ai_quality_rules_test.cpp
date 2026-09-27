// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/AI/Quality ai_quality_rules_test.cpp -o aq && ./aq
#include "AIQualityRules.h"

#include <cstdio>
#include <functional>

using namespace MurdarAIQ;

static int GChecks = 0, GFailed = 0;
#define CHECK(c) do { ++GChecks; if (!(c)) { ++GFailed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static void WatchdogLevels()
{
	FWatchTuning T;
	FWatchdog W;
	bool bUp = false;
	CHECK(W.Update(0.0, 0.f, 0.f, true, false, T, bUp) == 0); // first call anchors
	CHECK(W.Update(3.9, 10.f, 0.f, true, false, T, bUp) == 0);
	CHECK(W.Update(4.1, 10.f, 0.f, true, false, T, bUp) == 1 && bUp);
	CHECK(W.Update(4.3, 10.f, 0.f, true, false, T, bUp) == 1 && !bUp);   // no second escalation at the same level
	CHECK(W.Update(12.2, 10.f, 0.f, true, false, T, bUp) == 2 && bUp);
	CHECK(W.Update(30.2, 10.f, 0.f, true, false, T, bUp) == 3 && bUp);
	CHECK(W.Escalations == 3);
	// progress resets everything
	CHECK(W.Update(31.0, 400.f, 0.f, true, false, T, bUp) == 0);
	CHECK(W.StalledFor(31.0) == 0.0);
}

static void WatchdogWaits()
{
	FWatchTuning T;
	FWatchdog W;
	bool bUp = false;
	W.Update(0.0, 0.f, 0.f, true, false, T, bUp);
	// not wanting to move is never stuck
	CHECK(W.Update(50.0, 0.f, 0.f, false, false, T, bUp) == 0);
	// a red light for 60 s: fine
	CHECK(W.Update(60.0, 0.f, 0.f, true, true, T, bUp) == 0);
	CHECK(W.Update(120.0, 0.f, 0.f, true, true, T, bUp) == 0);
	// ...a red light that never changes (a broken signal, a deadlock that looks legitimate): stuck after 90 s + levels
	CHECK(W.Update(154.5, 0.f, 0.f, true, true, T, bUp) == 1);
	CHECK(W.Update(180.5, 0.f, 0.f, true, true, T, bUp) == 3);
	// a stall that starts while stopping and moves under the threshold stays a stall
	FWatchdog W2;
	W2.Update(0.0, 0.f, 0.f, true, false, T, bUp);
	CHECK(W2.Update(5.0, 100.f, 0.f, true, false, T, bUp) == 1); // 1 m of creeping is not progress
}

static void Junctions()
{
	FJunctionOther Going; Going.bCommitted = true; Going.SpeedCms = 600.f;
	CHECK(CommittedBlocksEntry(Going));
	FJunctionOther NotCommitted;
	CHECK(!CommittedBlocksEntry(NotCommitted));
	FJunctionOther Parked = Going; Parked.SpeedCms = 10.f; Parked.StillSeconds = 3.5f;
	CHECK(!CommittedBlocksEntry(Parked));                   // stopped in the box: no longer reserves it
	FJunctionOther Pausing = Going; Pausing.SpeedCms = 10.f; Pausing.StillSeconds = 1.f;
	CHECK(CommittedBlocksEntry(Pausing));                   // a brief stop still counts
	FJunctionOther Stranded = Going; Stranded.bObstacle = true;
	CHECK(!CommittedBlocksEntry(Stranded));
	FJunctionOther Stuck = Going; Stuck.WatchLevel = 2;
	CHECK(!CommittedBlocksEntry(Stuck));

	CHECK(SirenHoldsJunction(3000.f, 1500.f, 1200.f, 7000.f));  // coming fast
	CHECK(!SirenHoldsJunction(3000.f, 0.f, 0.f, 7000.f));       // parked at a scene
	CHECK(!SirenHoldsJunction(3000.f, 1500.f, -900.f, 7000.f)); // driving away
	CHECK(!SirenHoldsJunction(9000.f, 1500.f, 1200.f, 7000.f)); // far

	CHECK(ForceJunction(2, false, false));
	CHECK(!ForceJunction(1, false, false));
	CHECK(!ForceJunction(2, true, false));
	CHECK(!ForceJunction(3, false, true));
}

static void Obstacles()
{
	FObstacleTuning T;
	CHECK(ObstacleAction(EObstacleKind::Queue, 60.f, false, 0, T) == EObstacleAction::Follow);   // a queue is a queue
	CHECK(ObstacleAction(EObstacleKind::Queue, 5.f, false, 2, T) == EObstacleAction::DriveRound); // ...unless its head is stuck
	CHECK(ObstacleAction(EObstacleKind::Car, 4.5f, false, 0, T) == EObstacleAction::DriveRound);
	CHECK(ObstacleAction(EObstacleKind::Car, 4.5f, true, 0, T) == EObstacleAction::Honk);       // near a junction: longer
	CHECK(ObstacleAction(EObstacleKind::Car, 10.5f, true, 0, T) == EObstacleAction::DriveRound);
	CHECK(ObstacleAction(EObstacleKind::Person, 1.f, false, 0, T) == EObstacleAction::Follow);
	CHECK(ObstacleAction(EObstacleKind::Person, 2.5f, false, 0, T) == EObstacleAction::Honk);
	CHECK(ObstacleAction(EObstacleKind::Person, 5.5f, false, 0, T) == EObstacleAction::DriveRound);
	CHECK(ObstacleAction(EObstacleKind::Player, 2.f, false, 0, T) == EObstacleAction::Honk);
	CHECK(ObstacleAction(EObstacleKind::Player, 7.f, false, 0, T) == EObstacleAction::Honk);
	CHECK(ObstacleAction(EObstacleKind::Player, 8.5f, false, 0, T) == EObstacleAction::DriveRound);
	CHECK(ObstacleAction(EObstacleKind::Body, 2.f, false, 0, T) == EObstacleAction::DriveRound);

	// right of the lane, reversing: wheel right (+) swings the nose left, back towards the lane
	CHECK(ReverseSteer(200.f, 0.f) > 0.f);
	CHECK(ReverseSteer(-200.f, 0.f) < 0.f);
	CHECK(ReverseSteer(0.f, 0.f) == 0.f);
	CHECK(ReverseSteer(1000.f, -100.f) <= 1.f);
	CHECK(IsFlipped(90.f, 0.f) && IsFlipped(0.f, -80.f) && !IsFlipped(20.f, 10.f));
}

static void OnFoot()
{
	CHECK(FootFix(0) == EFootFix::None);
	CHECK(FootFix(1) == EFootFix::Sidestep);
	CHECK(FootFix(2) == EFootFix::ForcePath);
	CHECK(FootFix(3) == EFootFix::NewGoal);
	CHECK(SidestepSide(0) == 1 && SidestepSide(1) == -1 && SidestepSide(2) == 1);

	// oncoming, dead ahead: both go right, half the gap each
	CHECK(PassOffset(300.f, 0.f, -1.f, true) > 40.f && PassOffset(300.f, 0.f, -1.f, true) < 50.f);
	// the player does not yield: we take the whole gap
	CHECK(PassOffset(300.f, 0.f, -1.f, false) > 85.f);
	// already clear to the side
	CHECK(PassOffset(300.f, 120.f, -1.f, true) == 0.f);
	// standing slightly to our right: pass on its left (shift left)
	CHECK(PassOffset(200.f, 40.f, 0.f, false) < 0.f);
	// walking the same way ahead of us: nothing here (normal path)
	CHECK(PassOffset(200.f, 0.f, 1.f, false) == 0.f);
	// behind us / far
	CHECK(PassOffset(-100.f, 0.f, -1.f, false) == 0.f && PassOffset(900.f, 0.f, -1.f, false) == 0.f);
}

static FSpot Spot(bool bReach, float Path, float Straight, bool bHidden, float ThreatDist, float Away = 1.f)
{
	FSpot S;
	S.bOnNavmesh = true; S.bReachable = bReach; S.PathCm = Path; S.StraightCm = Straight; S.bHidden = bHidden;
	S.ThreatDistCm = ThreatDist; S.AwayDot = Away;
	return S;
}

static void Spots()
{
	// round the block is not sensible
	CHECK(PathSensible(Spot(true, 800.f, 600.f, false, 0.f)));
	CHECK(!PathSensible(Spot(true, 3000.f, 600.f, false, 0.f)));
	CHECK(!PathSensible(Spot(false, 600.f, 600.f, false, 0.f)));
	FSpot OffNav = Spot(true, 600.f, 600.f, false, 0.f); OffNav.bOnNavmesh = false;
	CHECK(!PathSensible(OffNav));   // the old hide ring put people inside walls

	// flee: towards him is rejected, the road costs
	CHECK(ScoreFlee(Spot(true, 1000.f, 1000.f, false, 2000.f, -0.5f)) < 0.f);
	FSpot Road = Spot(true, 1000.f, 1000.f, false, 2000.f, 0.9f); Road.bOnRoad = true;
	FSpot Pavement = Spot(true, 1000.f, 1000.f, false, 2000.f, 0.9f);
	CHECK(ScoreFlee(Pavement) > ScoreFlee(Road));

	// hide: hidden beats far
	std::vector<FSpot> H = { Spot(true, 600.f, 600.f, false, 3000.f), Spot(true, 600.f, 600.f, true, 1200.f) };
	CHECK(PickBest(H, ScoreHide) == 1);

	// cover: must be hidden; peeking wins; range matters; don't stack on a mate
	FCoverTuning T;
	auto Cover = [&](const FSpot& S) { return ScoreCover(S, T); };
	std::vector<FSpot> C = { Spot(true, 400.f, 400.f, false, 1100.f), Spot(true, 400.f, 400.f, true, 1100.f), Spot(true, 400.f, 400.f, true, 1100.f) };
	C[2].bCanPeek = true;
	CHECK(PickBest(C, Cover) == 2);
	C[2].AllyDistCm = 100.f;
	CHECK(PickBest(C, Cover) == 1);
	std::vector<FSpot> Far = { Spot(true, 2000.f, 1500.f, true, 1100.f) };
	CHECK(PickBest(Far, Cover) == -1);  // too far to walk to
	std::vector<FSpot> None;
	CHECK(PickBest(None, Cover) == -1);
}

static void CoverCycle()
{
	FCoverCycle Cyc;
	Cyc.Start(0.0);
	CHECK(Cyc.Update(0.5, false, false, false, 1.f, 0.6f, 12.f) == ECoverPhase::Moving);
	CHECK(Cyc.Update(1.0, true, false, false, 1.f, 0.6f, 12.f) == ECoverPhase::Hidden);
	CHECK(Cyc.Update(1.5, true, false, false, 1.f, 0.6f, 12.f) == ECoverPhase::Hidden);
	CHECK(Cyc.Update(2.1, true, false, false, 1.f, 0.6f, 12.f) == ECoverPhase::Peek);
	CHECK(Cyc.Update(2.8, true, false, false, 1.f, 0.6f, 12.f) == ECoverPhase::Hidden);
	// reloading keeps us down
	CHECK(Cyc.Update(5.0, true, false, true, 1.f, 0.6f, 12.f) == ECoverPhase::Hidden);
	// flanked: leave
	CHECK(Cyc.Update(5.2, true, true, false, 1.f, 0.6f, 12.f) == ECoverPhase::Leave);
	FCoverCycle Long;
	Long.Start(0.0);
	Long.Update(0.0, true, false, true, 1.f, 0.6f, 12.f);
	CHECK(Long.Update(12.5, true, false, true, 1.f, 0.6f, 12.f) == ECoverPhase::Leave);  // the fight moves
}

static void Recycling()
{
	FRecycleInput I; I.WatchLevel = 3; I.DistCm = 5000.f;
	CHECK(Recycle(I) == ERecycle::Remove);
	FRecycleInput Seen = I; Seen.bVisible = true;
	CHECK(Recycle(Seen) == ERecycle::Keep);
	FRecycleInput Close = I; Close.DistCm = 800.f;
	CHECK(Recycle(Close) == ERecycle::Keep);
	FRecycleInput Chase = I; Chase.bInChase = true;
	CHECK(Recycle(Chase) == ERecycle::Keep);
	FRecycleInput Mine = I; Mine.bPlayerOwned = true;
	CHECK(Recycle(Mine) == ERecycle::Keep);
	FRecycleInput Mission = I; Mission.bMission = true;
	CHECK(Recycle(Mission) == ERecycle::Keep);
	FRecycleInput Fine; Fine.DistCm = 5000.f;
	CHECK(Recycle(Fine) == ERecycle::Keep);
	FRecycleInput Stranded; Stranded.bStranded = true; Stranded.DistCm = 5000.f;
	CHECK(Recycle(Stranded) == ERecycle::Remove);
}

static void Hash()
{
	FSpatialHash2D H(2000.f);
	H.Insert(0, 0.f, 0.f);
	H.Insert(1, 1500.f, 0.f);
	H.Insert(2, -2500.f, 100.f);    // a negative cell
	H.Insert(3, 10000.f, 10000.f);
	std::vector<int> Out;
	H.Query(0.f, 0.f, 1600.f, Out);
	CHECK(Out.size() == 2);
	H.Query(-2400.f, 0.f, 300.f, Out);
	CHECK(Out.size() == 1 && Out[0] == 2);
	H.Query(0.f, 0.f, 20000.f, Out);
	CHECK(Out.size() == 4);
	H.Query(50000.f, 50000.f, 100.f, Out);
	CHECK(Out.empty());
	// rebuild: buckets kept, contents gone
	const size_t Buckets = H.NumBuckets();
	H.Clear();
	CHECK(H.Num() == 0 && H.NumBuckets() == Buckets);
	H.Query(0.f, 0.f, 20000.f, Out);
	CHECK(Out.empty());
	// brute-force agreement on a grid of points
	FSpatialHash2D G(700.f);
	std::vector<std::pair<float, float>> P;
	for (int i = 0; i < 400; ++i) { P.push_back({ static_cast<float>((i * 7919) % 20000 - 10000), static_cast<float>((i * 104729) % 20000 - 10000) }); G.Insert(i, P.back().first, P.back().second); }
	bool bAgree = true;
	for (int q = 0; q < 50 && bAgree; ++q)
	{
		const float Qx = static_cast<float>(q * 397 % 20000 - 10000), Qy = static_cast<float>(q * 911 % 20000 - 10000);
		G.Query(Qx, Qy, 2500.f, Out);
		size_t Brute = 0;
		for (const auto& Pt : P) { const float Dx = Pt.first - Qx, Dy = Pt.second - Qy; if (Dx * Dx + Dy * Dy <= 2500.f * 2500.f) { ++Brute; } }
		bAgree = Brute == Out.size();
	}
	CHECK(bAgree);
}

static void Soak()
{
	CHECK(IsGridlock({ 2, 2, 3 }));
	CHECK(!IsGridlock({ 2, 1, 3 }));
	CHECK(!IsGridlock({}));
	FSoakStats S;
	S.Seconds = 600.0;
	S.Add(EAgent::Traffic, 3);
	S.Add(EAgent::Traffic, 3);
	S.Add(EAgent::Pedestrian, 1);
	S.Add(EAgent::Pedestrian, 0);   // ignored
	CHECK(S.Level3Per10Min(EAgent::Traffic) == 2.0);
	CHECK(S.Passes());
	S.Add(EAgent::Traffic, 3);
	CHECK(!S.Passes());
	FSoakStats G; G.Seconds = 600.0; G.Gridlocks = 1;
	CHECK(!G.Passes());
	CHECK(S.Summary().find("traffic L1 0 L2 0 L3 3") != std::string::npos);
	CHECK(S.Summary().find("FAIL") != std::string::npos);
	FSoakStats Empty;
	CHECK(Empty.Level3Per10Min(EAgent::Police) == 0.0 && Empty.Passes());
}

int main()
{
	WatchdogLevels();
	WatchdogWaits();
	Junctions();
	Obstacles();
	OnFoot();
	Spots();
	CoverCycle();
	Recycling();
	Hash();
	Soak();
	std::printf("%d checks, %d failed\n", GChecks, GFailed);
	return GFailed == 0 ? 0 : 1;
}
