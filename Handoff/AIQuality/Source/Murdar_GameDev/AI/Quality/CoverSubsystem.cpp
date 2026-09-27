#include "AI/Quality/CoverSubsystem.h"
#include "AI/Quality/AIQualitySettings.h"
#include "AI/MurdarNavigation.h"
#include "AI/MurdarRoadNavigation.h"

#include "NavigationSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr float ChestZ = 40.f;   // above the capsule centre (the actor location)
	constexpr float HeadZ = 75.f;
	constexpr float PeekSideCm = 90.f;

	float PathLength(const TArray<FVector>& Pts)
	{
		float L = 0.f;
		for (int32 i = 0; i + 1 < Pts.Num(); ++i) { L += FVector::Dist(Pts[i], Pts[i + 1]); }
		return L;
	}
}

UCoverSubsystem* UCoverSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UCoverSubsystem>() : nullptr;
}

bool UCoverSubsystem::TakeBudget()
{
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	const int32 Second = FMath::FloorToInt(GetWorld()->GetTimeSeconds());
	if (Second != BudgetSecond) { BudgetSecond = Second; BudgetUsed = 0; }
	if (BudgetUsed >= (S ? S->CoverSearchesPerSecond : 6)) { return false; }
	++BudgetUsed;
	return true;
}

float UCoverSubsystem::NearestClaim(const FVector& P, const AActor* Except) const
{
	float Best = 1e9f;
	for (const TPair<TWeakObjectPtr<const AActor>, FVector>& KV : Claims)
	{
		if (!KV.Key.IsValid() || KV.Key.Get() == Except) { continue; }
		Best = FMath::Min(Best, FVector::Dist2D(P, KV.Value));
	}
	return Best;
}

bool UCoverSubsystem::OnRoad(const FVector& P) const
{
	// On a traffic lane = within half its width of the nearest lane's centre line. PreferredDir zero: any direction
	// (ADAPT: check NearestLane treats a zero PreferredDir as "no preference").
	FZoneGraphLaneLocation Loc;
	if (!MurdarRoad::NearestLane(GetWorld(), P, 600.f, FVector::ZeroVector, Loc)) { return false; }
	const float Half = FMath::Max(MurdarRoad::LaneWidth(GetWorld(), Loc), 300.f) * 0.5f;
	return FVector::Dist2D(Loc.Position, P) < Half;
}

bool UCoverSubsystem::IsExposed(const APawn* Me, const AActor* Threat) const
{
	if (!Me || !Threat) { return false; }
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CoverExposed), false, Me);
	Q.AddIgnoredActor(Threat);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Threat->GetActorLocation() + FVector(0.f, 0.f, HeadZ), Me->GetActorLocation() + FVector(0.f, 0.f, ChestZ), ECC_Visibility, Q);
}

bool UCoverSubsystem::FindSpot(ESpotPurpose Purpose, const APawn* Me, const AActor* Threat, float PreferredRangeCm, FVector& OutSpot, FVector& OutPeek, bool& bOutNone)
{
	bOutNone = false;
	if (!Me || !Threat) { bOutNone = true; return false; }
	if (!TakeBudget()) { return false; }
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!S || !Nav) { bOutNone = true; return false; }

	const FVector MyLoc = Me->GetActorLocation();
	const FVector ThreatEyes = Threat->GetActorLocation() + FVector(0.f, 0.f, HeadZ);
	const FVector Away = (MyLoc - Threat->GetActorLocation()).GetSafeNormal2D();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CoverFind), false, Me);
	Q.AddIgnoredActor(Threat);

	TArray<float> Rings = S->CoverRingsCm;
	if (Purpose != ESpotPurpose::Cover) { Rings = { 800.f, 1400.f, 2000.f }; } // hiding and running go farther
	const int32 N = FMath::Max(S->CoverSamplesPerRing, 6);

	std::vector<MurdarAIQ::FSpot> Spots;
	TArray<FVector> Points, Peeks;
	for (float R : Rings)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float A = (i + (R > 800.f ? 0.5f : 0.f)) * 2.f * PI / N;
			const FVector Raw = MyLoc + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R;
			FNavLocation OnNav;
			MurdarAIQ::FSpot Sp;
			Sp.StraightCm = FVector::Dist2D(MyLoc, Raw);
			// ADAPT: the Human agent's nav data - ProjectPointToNavigation picks the default agent unless given the
			// agent properties (MurdarNav keeps HumanRadius/HumanHeight: pass an FNavAgentProperties built from them).
			Sp.bOnNavmesh = Nav->ProjectPointToNavigation(Raw, OnNav, FVector(150.f, 150.f, 300.f));
			if (!Sp.bOnNavmesh) { continue; }
			const FVector P = OnNav.Location + FVector(0.f, 0.f, 90.f); // capsule centre above the navmesh
			const FVector ToP = (P - MyLoc).GetSafeNormal2D();
			Sp.AwayDot = FVector::DotProduct(ToP, Away);
			if (Purpose != ESpotPurpose::Cover && Sp.AwayDot < 0.f) { continue; } // never towards him to hide or run
			Sp.ThreatDistCm = FVector::Dist2D(P, Threat->GetActorLocation());
			FHitResult Hit;
			Sp.bHidden = GetWorld()->LineTraceSingleByChannel(Hit, ThreatEyes, P + FVector(0.f, 0.f, ChestZ), ECC_Visibility, Q);
			if (Purpose == ESpotPurpose::Cover && !Sp.bHidden) { continue; }
			// Peek: a step either side (perpendicular to the line to him) that sees his head.
			FVector Peek = P;
			if (Purpose == ESpotPurpose::Cover)
			{
				const FVector ToThreat = (Threat->GetActorLocation() - P).GetSafeNormal2D();
				const FVector Side(-ToThreat.Y, ToThreat.X, 0.f);
				for (float Sign : { 1.f, -1.f })
				{
					const FVector Try = P + Side * Sign * PeekSideCm;
					if (!GetWorld()->LineTraceSingleByChannel(Hit, ThreatEyes, Try + FVector(0.f, 0.f, ChestZ), ECC_Visibility, Q))
					{
						Sp.bCanPeek = true;
						Peek = Try;
						break;
					}
				}
			}
			Sp.bOnRoad = OnRoad(OnNav.Location);
			Sp.AllyDistCm = NearestClaim(P, Me);
			Spots.push_back(Sp);
			Points.Add(P);
			Peeks.Add(Peek);
		}
	}
	// Paths only for the best few by the cheap score (a path costs more than all the traces).
	auto Cheap = [&](const MurdarAIQ::FSpot& Sp)
	{
		MurdarAIQ::FSpot Guess = Sp;
		Guess.bReachable = true;
		Guess.PathCm = Sp.StraightCm;
		MurdarAIQ::FCoverTuning T;
		T.PreferredRangeCm = PreferredRangeCm; T.AllySpacingCm = S->AllySpacingCm; T.MaxPathCm = S->MaxCoverPathCm;
		return Purpose == ESpotPurpose::Cover ? MurdarAIQ::ScoreCover(Guess, T) : Purpose == ESpotPurpose::Hide ? MurdarAIQ::ScoreHide(Guess) : MurdarAIQ::ScoreFlee(Guess);
	};
	TArray<int32> Order;
	for (int32 i = 0; i < static_cast<int32>(Spots.size()); ++i) { if (Cheap(Spots[i]) >= 0.f) { Order.Add(i); } }
	Order.Sort([&](int32 A, int32 B) { return Cheap(Spots[A]) > Cheap(Spots[B]); });
	if (Order.Num() > 4) { Order.SetNum(4); }
	std::vector<MurdarAIQ::FSpot> Final;
	TArray<int32> FinalIdx;
	for (int32 i : Order)
	{
		TArray<FVector> Path;
		MurdarAIQ::FSpot Sp = Spots[i];
		Sp.bReachable = MurdarNav::FindPath(GetWorld(), MyLoc, Points[i], MurdarNav::HumanRadius, MurdarNav::HumanHeight, Path, Me) && Path.Num() >= 2
			&& FVector::Dist2D(Path.Last(), Points[i]) < 150.f; // a partial path is not "reachable"
		Sp.PathCm = Sp.bReachable ? PathLength(Path) : 0.f;
		Final.push_back(Sp);
		FinalIdx.Add(i);
	}
	MurdarAIQ::FCoverTuning T;
	T.PreferredRangeCm = PreferredRangeCm; T.AllySpacingCm = S->AllySpacingCm; T.MaxPathCm = S->MaxCoverPathCm;
	int32 Best = -1;
	switch (Purpose)
	{
	case ESpotPurpose::Cover: Best = MurdarAIQ::PickBest(Final, [&](const MurdarAIQ::FSpot& Sp) { return MurdarAIQ::ScoreCover(Sp, T); }); break;
	case ESpotPurpose::Hide: Best = MurdarAIQ::PickBest(Final, MurdarAIQ::ScoreHide); break;
	case ESpotPurpose::Flee: Best = MurdarAIQ::PickBest(Final, MurdarAIQ::ScoreFlee); break;
	}
	if (Best < 0) { bOutNone = true; return false; }
	OutSpot = Points[FinalIdx[Best]];
	OutPeek = Peeks[FinalIdx[Best]];
	return true;
}

void UCoverSubsystem::Claim(const AActor* Claimer, const FVector& Spot)
{
	for (auto It = Claims.CreateIterator(); It; ++It) { if (!It->Key.IsValid()) { It.RemoveCurrent(); } }
	Claims.Add(Claimer, Spot);
}

void UCoverSubsystem::Release(const AActor* Claimer) { Claims.Remove(Claimer); }
