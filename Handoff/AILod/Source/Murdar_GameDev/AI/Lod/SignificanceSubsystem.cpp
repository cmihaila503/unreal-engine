#include "AI/Lod/SignificanceSubsystem.h"

#include "AI/MurdarNPCAIController.h"
#include "AI/MurdarPoliceAIController.h"
#include "AI/PedestrianComponent.h"
#include "AI/TrafficDriverComponent.h"
#include "Vehicle/MurdarVehicle.h"
#include "Vehicle/VehicleSignalsComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace
{
	constexpr float PassSeconds = 0.25f;
	TAutoConsoleVariable<int32> CVarLod(TEXT("murdar.AILod"), 1, TEXT("AI significance LOD: 1 on, 0 everyone at full rate"));
}

USignificanceSubsystem* USignificanceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<USignificanceSubsystem>() : nullptr;
}

bool USignificanceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USignificanceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(PassTimer, this, &USignificanceSubsystem::Pass, PassSeconds, true, PassSeconds);
}

void USignificanceSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	States.Reset();
	Super::Deinitialize();
}

void USignificanceSubsystem::SetEnabled(bool bOn)
{
	bEnabled = bOn;
	if (!bOn)
	{
		for (TPair<TWeakObjectPtr<APawn>, MurdarLod::FTierState>& P : States)
		{
			if (APawn* Pawn = P.Key.Get()) { ApplyTier(Pawn, MurdarLod::ETier::Full); }
		}
		States.Reset();
	}
}

bool USignificanceSubsystem::IsEngaged(const APawn* Pawn) const
{
	if (Pawn->ActorHasTag(TEXT("Mission"))) { return true; }
	if (const AMurdarPoliceAIController* Police = Cast<AMurdarPoliceAIController>(Pawn->GetController()))
	{
		const EPoliceState S = Police->GetState();
		return S != EPoliceState::Patrol && S != EPoliceState::StandDown && S != EPoliceState::Disabled;
	}
	// The NPC brain ticks only while it has a goal or a target (its own design): that is "busy".
	if (const AMurdarNPCAIController* Npc = Cast<AMurdarNPCAIController>(Pawn->GetController()))
	{
		return Npc->IsActorTickEnabled();
	}
	return false;
}

void USignificanceSubsystem::Pass()
{
	if (bEnabled != (CVarLod.GetValueOnGameThread() != 0)) { SetEnabled(CVarLod.GetValueOnGameThread() != 0); }

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Player) { return; }
	UpdateStreaming(Player);
	if (!bEnabled) { return; }

	FVector ViewLoc; FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector ViewDir = ViewRot.Vector();
	const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(0.5f * (PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 90.f) + 10.f));

	TArray<APawn*> Pawns;
	std::vector<MurdarLod::FAgent> Agents;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (Pawn == Player || Pawn->IsPlayerControlled() || !Pawn->GetController()) { continue; } // parked, empty cars: nothing ticks anyway
		const FVector To = Pawn->GetActorLocation() - ViewLoc;
		const float Dist = To.Size();
		MurdarLod::FAgent A;
		A.DistanceCm = float(FVector::Dist(Pawn->GetActorLocation(), Player->GetActorLocation()));
		A.bInView = Dist < 1.f || FVector::DotProduct(To / Dist, ViewDir) >= CosHalfFov;
		A.bEngaged = IsEngaged(Pawn);
		Pawns.Add(Pawn);
		Agents.push_back(A);
	}

	const MurdarLod::FBudget Budget;
	const std::vector<MurdarLod::ETier> Wanted = MurdarLod::AssignTiers(Agents, Budget);
	FMemory::Memzero(Counts);
	for (int32 i = 0; i < Pawns.Num(); ++i)
	{
		const bool bNew = !States.Contains(Pawns[i]); // new entries start at Full and are set explicitly once
		MurdarLod::FTierState& S = States.FindOrAdd(Pawns[i]);
		if (S.Update(Wanted[i], Budget.DemotePasses) || bNew) { ApplyTier(Pawns[i], S.Current); }
		++Counts[int32(S.Current)];
	}
	// Forget the gone.
	for (auto It = States.CreateIterator(); It; ++It) { if (!It->Key.IsValid()) { It.RemoveCurrent(); } }
}

void USignificanceSubsystem::ApplyTier(APawn* Pawn, MurdarLod::ETier Tier) const
{
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		const MurdarLod::FIntervals I = MurdarLod::Intervals(Tier, MurdarLod::EKind::Pedestrian);
		if (UPedestrianComponent* Ped = Character->FindComponentByClass<UPedestrianComponent>()) { Ped->SetComponentTickInterval(I.Brain); }
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement()) { Move->SetComponentTickInterval(I.Movement); }
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->VisibilityBasedAnimTickOption = I.bAnimOnlyWhenRendered
				? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered
				: EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
		return;
	}
	if (AMurdarVehicle* Car = Cast<AMurdarVehicle>(Pawn))
	{
		const MurdarLod::FIntervals I = MurdarLod::Intervals(Tier, MurdarLod::EKind::Car);
		if (UVehicleSignalsComponent* Signals = Car->FindComponentByClass<UVehicleSignalsComponent>()) { Signals->SetComponentTickInterval(I.Signals); }
		if (UTrafficDriverComponent* Driver = Car->FindComponentByClass<UTrafficDriverComponent>())
		{
			// Physics stays per frame; only the driver's decisions slow down, and only far and out of sight.
			Driver->SetComponentTickInterval(I.bDriverReduced ? 0.1f : 0.f);
		}
	}
}

void USignificanceSubsystem::UpdateStreaming(const APawn* Player)
{
	// Only if the player's car carries a streaming source (README §Streaming); otherwise World Partition uses the
	// player controller's default source and nothing here changes.
	const AMurdarVehicle* Car = Cast<AMurdarVehicle>(Player);
	UWorldPartitionStreamingSourceComponent* Source = Car ? Car->FindComponentByClass<UWorldPartitionStreamingSourceComponent>() : nullptr;
	if (!Source) { return; }
	const MurdarLod::FStreamingTuning T;
	if (StreamingRadius <= 0.f) { StreamingRadius = T.BaseRadiusCm; }
	const float Next = MurdarLod::StreamingRadius(StreamingRadius, Car->GetSpeedKph(), T);
	if (Next != StreamingRadius)
	{
		StreamingRadius = Next;
		// ADAPT: UE 5.8 API for a source's radius — shapes on the component (Shapes[0].Radius with bUseGridLoadingRange
		// off), or the target grid's loading range. Verify against the engine headers before building.
		if (Source->Shapes.Num() > 0) { Source->Shapes[0].bUseGridLoadingRange = false; Source->Shapes[0].Radius = StreamingRadius; }
	}
}

FString USignificanceSubsystem::Describe() const
{
	return FString::Printf(TEXT("AI LOD %s: full %d, reduced %d, minimal %d, dormant %d (tracked %d); streaming radius %.0f m"),
		bEnabled ? TEXT("on") : TEXT("OFF"), Counts[0], Counts[1], Counts[2], Counts[3], States.Num(), StreamingRadius / 100.f);
}
