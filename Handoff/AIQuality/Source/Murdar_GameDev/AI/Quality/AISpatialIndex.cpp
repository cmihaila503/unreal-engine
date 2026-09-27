#include "AI/Quality/AISpatialIndex.h"
#include "AI/Quality/AIQualitySettings.h"

#include "Vehicle/MurdarVehicle.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UAISpatialIndex* UAISpatialIndex::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UAISpatialIndex>() : nullptr;
}

bool UAISpatialIndex::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAISpatialIndex::Tick(float DeltaTime)
{
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	if (!S || !S->bSpatialIndex) { return; }
	SinceRebuild += DeltaTime;
	if (SinceRebuild >= S->RebuildSeconds) { Rebuild(); }
}

void UAISpatialIndex::Rebuild()
{
	SinceRebuild = 0.f;
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	if (S && !FMath::IsNearlyEqual(S->CellCm, CellCm))
	{
		CellCm = S->CellCm;
		VehicleGrid = MurdarAIQ::FSpatialHash2D(CellCm);
		CharacterGrid = MurdarAIQ::FSpatialHash2D(CellCm);
	}
	VehicleGrid.Clear();
	CharacterGrid.Clear();
	Vehicles.Reset();
	Characters.Reset();
	UWorld* World = GetWorld();
	if (!World) { return; }
	for (TActorIterator<AMurdarVehicle> It(World); It; ++It)
	{
		const FVector L = It->GetActorLocation();
		VehicleGrid.Insert(Vehicles.Num(), L.X, L.Y);
		Vehicles.Add(*It);
	}
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (It->IsHidden() || It->GetAttachParentActor()) { continue; } // parked by the pool / sitting in a car
		const FVector L = It->GetActorLocation();
		CharacterGrid.Insert(Characters.Num(), L.X, L.Y);
		Characters.Add(*It);
	}
}

void UAISpatialIndex::QueryVehicles(const FVector& Where, float RadiusCm, TArray<AMurdarVehicle*>& Out) const
{
	Out.Reset();
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	const float Slack = MaxSpeedCms * (S ? S->RebuildSeconds : 0.1f);
	VehicleGrid.Query(Where.X, Where.Y, RadiusCm + Slack, Scratch);
	for (int Id : Scratch)
	{
		if (AMurdarVehicle* V = Vehicles.IsValidIndex(Id) ? Vehicles[Id].Get() : nullptr) { Out.Add(V); }
	}
	VisitsSaved += FMath::Max(0, Vehicles.Num() - static_cast<int32>(Scratch.size()));
}

void UAISpatialIndex::QueryCharacters(const FVector& Where, float RadiusCm, TArray<ACharacter*>& Out) const
{
	Out.Reset();
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	const float Slack = 900.f * (S ? S->RebuildSeconds : 0.1f); // a sprint
	CharacterGrid.Query(Where.X, Where.Y, RadiusCm + Slack, Scratch);
	for (int Id : Scratch)
	{
		ACharacter* C = Characters.IsValidIndex(Id) ? Characters[Id].Get() : nullptr;
		// Hidden / seated since the rebuild: the old scans skipped them too.
		if (C && !C->IsHidden() && !C->GetAttachParentActor()) { Out.Add(C); }
	}
	VisitsSaved += FMath::Max(0, Characters.Num() - static_cast<int32>(Scratch.size()));
}
