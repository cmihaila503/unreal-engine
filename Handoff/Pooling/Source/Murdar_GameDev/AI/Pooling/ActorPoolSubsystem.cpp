#include "AI/Pooling/ActorPoolSubsystem.h"

#include "AI/MurdarAISettings.h"        // UMurdarAILibrary::SpawnNPC
#include "AI/MurdarNPCAIController.h"
#include "Character/MurdarCharacter.h"
#include "Health/HealthComponent.h"
#include "Director/MurdarTags.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"

namespace
{
	/** Parked out of the way: under the map at the spot, so nothing (nav, overlaps) finds them. */
	constexpr float ParkDepthCm = 20000.f;
}

UActorPoolSubsystem* UActorPoolSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UActorPoolSubsystem>() : nullptr;
}

bool UActorPoolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UActorPoolSubsystem::Deinitialize()
{
	Parked.Reset(); // the world destroys them
	Super::Deinitialize();
}

void UActorPoolSubsystem::Park(AMurdarCharacter* P)
{
	if (AMurdarNPCAIController* Brain = Cast<AMurdarNPCAIController>(P->GetController()))
	{
		Brain->StopMovement();
		Brain->ResetForPool(); // README §Patches 2: goals, targets, ambient mode, hold-fire flags
	}
	if (UCharacterMovementComponent* Move = P->GetCharacterMovement()) { Move->StopMovementImmediately(); Move->DisableMovement(); Move->SetComponentTickEnabled(false); }
	P->SetActorEnableCollision(false);
	P->SetActorHiddenInGame(true);
	P->SetActorTickEnabled(false);
	if (USkeletalMeshComponent* Mesh = P->GetMesh()) { Mesh->bPauseAnims = true; Mesh->SetComponentTickEnabled(false); }
	P->SetActorLocation(P->GetActorLocation() - FVector(0.f, 0.f, ParkDepthCm), false, nullptr, ETeleportType::TeleportPhysics);
	Parked.Add({ P, GetWorld()->GetTimeSeconds() });
}

void UActorPoolSubsystem::Wake(AMurdarCharacter* P, const FTransform& Where)
{
	P->SetActorLocationAndRotation(Where.GetLocation(), Where.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (UHealthComponent* H = P->GetHealthComponent()) { H->Revive(); }
	if (USkeletalMeshComponent* Mesh = P->GetMesh()) { Mesh->bPauseAnims = false; Mesh->SetComponentTickEnabled(true); }
	if (UCharacterMovementComponent* Move = P->GetCharacterMovement()) { Move->SetComponentTickEnabled(true); Move->SetMovementMode(MOVE_Walking); }
	P->SetActorTickEnabled(true);
	P->SetActorEnableCollision(true);
	P->SetActorHiddenInGame(false);
}

AMurdarCharacter* UActorPoolSubsystem::AcquirePedestrian(const FTransform& Where, FGameplayTag Sector)
{
	Parked.RemoveAll([](const FParked& S) { return !S.Person.IsValid(); });
	if (MurdarPool::Acquire(Parked.Num()) == MurdarPool::EAcquire::Reuse)
	{
		std::vector<MurdarPool::FSlot> Slots;
		for (int32 i = 0; i < Parked.Num(); ++i) { Slots.push_back({ i, Parked[i].ParkedAt }); }
		const int32 I = MurdarPool::PickReuse(Slots);
		AMurdarCharacter* P = Parked[I].Person.Get();
		Parked.RemoveAtSwap(I);
		Wake(P, Where);
		++Reused;
		return P;
	}
	++Spawned;
	return UMurdarAILibrary::SpawnNPC(GetWorld(), ENPCFaction::Civilian, Where, nullptr, Sector);
}

void UActorPoolSubsystem::ReleasePedestrian(AMurdarCharacter* P)
{
	if (!P) { return; }
	// A body is a body: the dead, the knocked down and the ragdolled go the old way.
	const UHealthComponent* H = P->GetHealthComponent();
	const bool bReusable = (!H || !H->IsDead()) && !P->IsRagdoll() && Cast<AMurdarNPCAIController>(P->GetController());
	if (!bReusable || MurdarPool::Release(Parked.Num(), Tuning) == MurdarPool::ERelease::Destroy)
	{
		if (AController* C = P->GetController()) { C->Destroy(); }
		P->Destroy();
		++Destroyed;
		return;
	}
	Park(P);
}

void UActorPoolSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Warm up in quiet frames only (under 20 ms): never add a hitch to fix hitches.
	const bool bQuiet = FApp::GetDeltaTime() < 0.02;
	const int32 N = MurdarPool::Prewarm(Parked.Num(), bQuiet, Tuning);
	for (int32 i = 0; i < N; ++i)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector Base = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
		if (AMurdarCharacter* P = UMurdarAILibrary::SpawnNPC(GetWorld(), ENPCFaction::Civilian, FTransform(Base - FVector(0.f, 0.f, ParkDepthCm)), nullptr, MurdarTags::Sector_Sandbox))
		{
			++Spawned;
			Park(P);
		}
	}
}

FString UActorPoolSubsystem::Describe() const
{
	return FString::Printf(TEXT("Pool: %d parked | reused %d, spawned %d, destroyed %d"), Parked.Num(), Reused, Spawned, Destroyed);
}
