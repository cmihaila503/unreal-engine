#include "AI/Civilians/CivilianPopulationSubsystem.h"

#include "AI/Civilians/CivilianPopulationSettings.h"
#include "AI/Civilians/CivilianProfile.h"
#include "AI/MurdarAISettings.h"     // ADAPT: path of UMurdarAILibrary / UMurdarAISettings (RECON §3.4: AI/MurdarAISettings.cpp:42)
#include "Director/ZoneVolume.h"     // ADAPT: path of AZoneVolume

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCivDebug(
		TEXT("Murdar.Civ.Debug"), 0,
		TEXT("1 = draw the civilian spawn/despawn rings, each civilian, and the population stats on screen."),
		ECVF_Cheat);

	constexpr float CmPerHectareSide = 10000.f; // 100 m, unit conversion (1 ha = 100 m × 100 m)
	constexpr int32 DebugStatsKey = 0x0C1F;     // stable on-screen message slot

	// ADAPT: how a zone volume exposes its box and its Zone.* tag. AZoneVolume is "a named box" (PROJECT_OVERVIEW
	// §4.1); if it is an AVolume use EncompassesPoint instead of the bounds test.
	bool ZoneContains(const AZoneVolume* Zone, const FVector& Point)
	{
		return Zone->GetComponentsBoundingBox(/*bNonColliding*/ true).IsInsideOrOn(Point);
	}
	FGameplayTag ZoneTagOf(const AZoneVolume* Zone)
	{
		return Zone->ZoneTag; // ADAPT: member name
	}
}

bool UCivilianPopulationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCivilianPopulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	if (S->RandomSeed != 0)
	{
		Rng.Initialize(S->RandomSeed);
	}
	else
	{
		Rng.GenerateNewSeed();
	}

	ResolveArchetypes();

	// Random phase so the update doesn't line up with the other 2 Hz timers (director, etc.).
	InWorld.GetTimerManager().SetTimer(UpdateTimer, this, &UCivilianPopulationSubsystem::Update,
		S->UpdateIntervalSeconds, /*bLoop*/ true, Rng.FRandRange(0.f, S->UpdateIntervalSeconds));
}

void UCivilianPopulationSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	Active.Reset();
	Pool.Reset();
	Super::Deinitialize();
}

void UCivilianPopulationSubsystem::ResolveArchetypes()
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();

	auto Resolve = [this](const TArray<FCivilianArchetypeWeight>& In, TArray<FResolvedArchetype>& Out)
	{
		Out.Reset();
		for (const FCivilianArchetypeWeight& A : In)
		{
			// Synchronous: a handful of tiny data assets, once, at world start.
			if (UCivilianProfile* P = A.Profile.LoadSynchronous(); P && A.Weight > 0.f)
			{
				LoadedProfiles.AddUnique(P);
				Out.Add({P, A.Weight});
			}
		}
	};

	Resolve(S->DefaultArchetypes, DefaultArchetypes);
	ZoneArchetypes.SetNum(S->ZoneDensities.Num());
	for (int32 i = 0; i < S->ZoneDensities.Num(); ++i)
	{
		Resolve(S->ZoneDensities[i].Archetypes, ZoneArchetypes[i]);
	}
}

void UCivilianPopulationSubsystem::SetDensityScale(float Scale)
{
	DensityScale = FMath::Max(0.f, Scale);
}

void UCivilianPopulationSubsystem::SetMaxOverride(int32 Max)
{
	MaxOverride = Max;
}

TArray<APawn*> UCivilianPopulationSubsystem::GetActiveCivilians() const
{
	TArray<APawn*> Out;
	Out.Reserve(Active.Num());
	for (const FSlot& Slot : Active)
	{
		if (APawn* P = Slot.Pawn.Get())
		{
			Out.Add(P);
		}
	}
	return Out;
}

UCivilianPopulationSubsystem::FSlot* UCivilianPopulationSubsystem::FindSlot(const APawn* Pawn)
{
	return Active.FindByPredicate([Pawn](const FSlot& S) { return S.Pawn.Get() == Pawn; });
}

const UCivilianPopulationSubsystem::FSlot* UCivilianPopulationSubsystem::FindSlot(const APawn* Pawn) const
{
	return Active.FindByPredicate([Pawn](const FSlot& S) { return S.Pawn.Get() == Pawn; });
}

void UCivilianPopulationSubsystem::SetPinned(APawn* Civilian, bool bPinned)
{
	if (FSlot* Slot = FindSlot(Civilian))
	{
		Slot->bPinned = bPinned;
	}
}

void UCivilianPopulationSubsystem::NotifyCivilianDied(APawn* Civilian)
{
	// Removed from the population, never pooled: the corpse belongs to the existing death/corpse code.
	Active.RemoveAll([Civilian](const FSlot& S) { return S.Pawn.Get() == Civilian; });
}

UCivilianProfile* UCivilianPopulationSubsystem::GetProfileOf(const APawn* Civilian) const
{
	const FSlot* Slot = FindSlot(Civilian);
	return Slot ? Slot->Profile.Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------

void UCivilianPopulationSubsystem::Update()
{
	UWorld* World = GetWorld();
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	if (!World || !S->bEnabled)
	{
		return;
	}

	// Drop bodies destroyed by something else (level streaming, a cheat, the corpse timer).
	Active.RemoveAll([](const FSlot& Slot) { return !IsValid(Slot.Pawn.Get()); });
	Pool.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !IsValid(P.Get()); });

	FViewer Viewer;
	if (!GetViewer(Viewer))
	{
		return; // no player pawn (loading, dead between respawns): hold the population as it is
	}

	const double Now = World->GetTimeSeconds();
	DespawnPass(Viewer, Now);
	SpawnPass(Viewer, Now);

	Stats.Active = Active.Num();
	Stats.Pooled = Pool.Num();
	Stats.Pinned = Active.FilterByPredicate([](const FSlot& Slot) { return Slot.bPinned; }).Num();

	if (CVarCivDebug.GetValueOnGameThread() != 0)
	{
		DebugDraw(Viewer);
	}
}

bool UCivilianPopulationSubsystem::GetViewer(FViewer& Out) const
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr; // on foot or the car he drives
	if (!PlayerPawn)
	{
		return false;
	}

	FRotator ViewRotation;
	PC->GetPlayerViewPoint(Out.ViewLocation, ViewRotation);
	Out.ViewDirection = ViewRotation.Vector();
	Out.PawnLocation = PlayerPawn->GetActorLocation();
	Out.IgnoreActor = PlayerPawn;

	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();

	// Spawn ring leads the player along his velocity (the car's, when driving).
	FVector LookAhead = PlayerPawn->GetVelocity() * S->SpawnLookAheadSeconds;
	LookAhead.Z = 0.f;
	Out.SpawnCentre = Out.PawnLocation + LookAhead.GetClampedToMaxSize(S->MaxSpawnLookAheadCm);

	// The frustum's corners are farther off-axis than its horizontal half-FOV, so the "in view" cone uses the
	// half-diagonal (from the viewport aspect) plus the margin for camera swing.
	const float HFovDeg = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : S->FallbackFovDeg;
	int32 VpW = 0, VpH = 0;
	PC->GetViewportSize(VpW, VpH);
	const float Aspect = (VpW > 0 && VpH > 0) ? float(VpW) / float(VpH) : 16.f / 9.f; // 16:9 when headless
	const float TanH = FMath::Tan(FMath::DegreesToRadians(0.5f * HFovDeg));
	const float TanV = TanH / Aspect;
	const float HalfDiagDeg = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(TanH * TanH + TanV * TanV)));
	Out.CosViewCone = FMath::Cos(FMath::DegreesToRadians(FMath::Min(HalfDiagDeg + S->ViewConeMarginDeg, 179.f)));
	Out.PixelsPerUnitAtUnitDistance = VpH > 0 ? float(VpH) / (2.f * TanV) : 0.f;
	return true;
}

bool UCivilianPopulationSubsystem::IsVisible(const FViewer& Viewer, const FVector& FeetLocation, const AActor* Candidate) const
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();

	// Too small to notice: a whole person under ImperceptiblePixelHeight on screen.
	const float DistToFeet = FVector::Dist(FeetLocation, Viewer.ViewLocation);
	if (Viewer.PixelsPerUnitAtUnitDistance > 0.f && DistToFeet > KINDA_SMALL_NUMBER
		&& S->HeadHeightCm * Viewer.PixelsPerUnitAtUnitDistance / DistToFeet < S->ImperceptiblePixelHeight)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CivilianVisibility), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(Viewer.IgnoreActor);
	if (Candidate)
	{
		Params.AddIgnoredActor(Candidate);
	}

	for (const float Height : {S->HeadHeightCm, S->ChestHeightCm})
	{
		const FVector Point = FeetLocation + FVector(0.f, 0.f, Height);
		const FVector ToPoint = Point - Viewer.ViewLocation;
		const float Dist = ToPoint.Size();
		if (Dist < KINDA_SMALL_NUMBER)
		{
			return true;
		}
		// A round cone through the frustum's corners: covers the whole screen, a bit more above and below.
		if (FVector::DotProduct(ToPoint / Dist, Viewer.ViewDirection) < Viewer.CosViewCone)
		{
			continue; // outside the view cone
		}
		// Static geometry only: cars and people move away and would reveal the spawn a moment later.
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Viewer.ViewLocation, Point,
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return true; // in the cone and nothing static in between
		}
	}
	return false;
}

float UCivilianPopulationSubsystem::DespawnDistance() const
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	return FMath::Max(S->DespawnDistanceCm, S->SpawnRingMaxCm + S->MaxSpawnLookAheadCm + S->MinSeparationCm);
}

void UCivilianPopulationSubsystem::DespawnPass(const FViewer& Viewer, double Now)
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	const float DespawnSq = FMath::Square(DespawnDistance());

	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		FSlot& Slot = Active[i];
		APawn* Pawn = Slot.Pawn.Get();
		const FVector Loc = Pawn->GetActorLocation();

		if (FVector::DistSquared2D(Loc, Viewer.PawnLocation) <= DespawnSq)
		{
			// Inside the despawn radius nobody leaves: no need to know whether they're seen.
			Slot.LastSeenTime = Now;
			continue;
		}
		// Feet location = capsule bottom; for Phase 1 the actor location minus half height is close enough.
		const FVector Feet = Loc - FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight());
		if (IsVisible(Viewer, Feet, Pawn))
		{
			Slot.LastSeenTime = Now;
			continue;
		}
		if (Slot.bPinned || Now - Slot.LastSeenTime < S->DespawnUnseenSeconds)
		{
			continue;
		}
		ReturnToPool(Pawn);
		Active.RemoveAtSwap(i);
		++Stats.Despawned;
	}
}

int32 UCivilianPopulationSubsystem::ComputeTarget(const FVector& PlayerLocation, const TArray<FResolvedArchetype>*& OutArchetypes) const
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();

	float PerHectare = S->DefaultPeoplePerHectare;
	OutArchetypes = &DefaultArchetypes;

	// The zone the player stands in decides. Few zone volumes per level; a linear scan at 2 Hz is fine.
	for (TActorIterator<AZoneVolume> It(GetWorld()); It; ++It)
	{
		if (!ZoneContains(*It, PlayerLocation))
		{
			continue;
		}
		const FGameplayTag Tag = ZoneTagOf(*It);
		const int32 Index = S->ZoneDensities.IndexOfByPredicate([&Tag](const FCivilianZoneDensity& Z) { return Z.Zone == Tag; });
		if (Index != INDEX_NONE)
		{
			PerHectare = S->ZoneDensities[Index].PeoplePerHectare;
			if (ZoneArchetypes.IsValidIndex(Index) && ZoneArchetypes[Index].Num() > 0)
			{
				OutArchetypes = &ZoneArchetypes[Index];
			}
			break;
		}
	}

	const float DiscHectares = PI * FMath::Square(S->SpawnRingMaxCm / CmPerHectareSide);
	const int32 Cap = MaxOverride >= 0 ? MaxOverride : S->MaxCivilians;
	return FMath::Clamp(FMath::RoundToInt(PerHectare * DiscHectares * DensityScale), 0, Cap);
}

bool UCivilianPopulationSubsystem::FindSpawnPoint(const FViewer& Viewer, FVector& OutLocation)
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		return false;
	}

	FNavAgentProperties Agent;
	Agent.AgentRadius = S->NavAgentRadiusCm;
	Agent.AgentHeight = S->NavAgentHeightCm;
	const ANavigationData* NavData = Nav->GetNavDataForProps(Agent);
	if (!NavData)
	{
		return false;
	}

	const float MinR = S->SpawnRingMinCm;
	const float MaxR = FMath::Max(S->SpawnRingMaxCm, MinR);
	const FVector Extent(S->ProjectExtentXYCm, S->ProjectExtentXYCm, S->ProjectExtentZCm);
	const float SeparationSq = FMath::Square(S->MinSeparationCm);

	// Uniform over the ring's area (sqrt of a uniform between the squared radii), not clumped at the inner edge.
	const float Angle = Rng.FRandRange(0.f, 2.f * PI);
	const float Radius = FMath::Sqrt(Rng.FRandRange(MinR * MinR, MaxR * MaxR));
	const FVector Candidate = Viewer.SpawnCentre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

	// The ring leads the player; never spawn closer to him than the inner radius.
	if (FVector::DistSquared2D(Candidate, Viewer.PawnLocation) < FMath::Square(MinR))
	{
		++Stats.RejectedCrowded;
		return false;
	}

	FNavLocation OnNav;
	if (!Nav->ProjectPointToNavigation(Candidate, OnNav, Extent, NavData))
	{
		++Stats.RejectedNav;
		return false;
	}
	for (const FSlot& Slot : Active)
	{
		if (FVector::DistSquared(Slot.Pawn->GetActorLocation(), OnNav.Location) < SeparationSq)
		{
			++Stats.RejectedCrowded;
			return false;
		}
	}
	if (IsVisible(Viewer, OnNav.Location, nullptr))
	{
		++Stats.RejectedVisible;
		return false;
	}

	OutLocation = OnNav.Location;
	return true;
}

void UCivilianPopulationSubsystem::SpawnPass(const FViewer& Viewer, double Now)
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();

	const TArray<FResolvedArchetype>* Archetypes = nullptr;
	Stats.Target = ComputeTarget(Viewer.PawnLocation, Archetypes);

	int32 Missing = Stats.Target - Active.Num();
	int32 Spawned = 0;
	for (int32 Attempt = 0; Attempt < S->SpawnAttemptsPerUpdate && Missing > 0 && Spawned < S->MaxSpawnsPerUpdate; ++Attempt)
	{
		FVector Location;
		if (!FindSpawnPoint(Viewer, Location))
		{
			continue;
		}
		// Face a random direction; Phase 2's Wander turns them onto the sidewalk's direction.
		const FRotator Rotation(0.f, Rng.FRandRange(-180.f, 180.f), 0.f);
		UCivilianProfile* Profile = Archetypes ? PickProfile(*Archetypes) : nullptr;

		if (APawn* Pawn = AcquirePawn(Location, Rotation, Profile))
		{
			FSlot& Slot = Active.AddDefaulted_GetRef();
			Slot.Pawn = Pawn;
			Slot.Profile = Profile;
			Slot.LastSeenTime = Now;
			--Missing;
			++Spawned;
		}
		else
		{
			++Stats.SpawnFailed;
		}
	}
}

UCivilianProfile* UCivilianPopulationSubsystem::PickProfile(const TArray<FResolvedArchetype>& Archetypes)
{
	float Total = 0.f;
	for (const FResolvedArchetype& A : Archetypes)
	{
		Total += A.Weight;
	}
	if (Total <= 0.f)
	{
		return nullptr;
	}
	float Roll = Rng.FRandRange(0.f, Total);
	for (const FResolvedArchetype& A : Archetypes)
	{
		Roll -= A.Weight;
		if (Roll <= 0.f)
		{
			return A.Profile;
		}
	}
	return Archetypes.Last().Profile;
}

APawn* UCivilianPopulationSubsystem::AcquirePawn(const FVector& FeetLocation, const FRotator& Rotation, UCivilianProfile* Profile)
{
	// Reuse a pooled body first. Phase 1 assumes one body class; per-archetype bodies (PawnClassOverride) need a
	// pool per class — add it when a second class exists.
	while (Pool.Num() > 0)
	{
		APawn* Pawn = Pool.Pop(EAllowShrinking::No).Get();
		if (!IsValid(Pawn))
		{
			continue;
		}
		const FVector ActorLocation = FeetLocation + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight());
		Pawn->SetActorLocationAndRotation(ActorLocation, Rotation, /*bSweep*/ false, nullptr, ETeleportType::ResetPhysics);
		SetBodyActive(Pawn, true);
		++Stats.ReusedFromPool;
		return Pawn;
	}

	// ADAPT: the real SpawnNPC signature. It must spawn an unarmed civilian (Civilian faction, AMurdarNPCAIController
	// possessing it) at the given location. If it spawns at a location+half height itself, pass FeetLocation;
	// otherwise add the capsule half height as above. Body class: Profile->PawnClassOverride, then
	// DefaultPawnClass, then UMurdarAISettings::NPCPawnClass (SpawnNPC's own default).
	APawn* Pawn = UMurdarAILibrary::SpawnNPC(GetWorld(), EMurdarFaction::Civilian, FeetLocation, Rotation);
	if (Pawn)
	{
		++Stats.SpawnedNew;
	}
	return Pawn;
}

void UCivilianPopulationSubsystem::SetBodyActive(APawn* Pawn, bool bActive) const
{
	Pawn->SetActorHiddenInGame(!bActive);
	Pawn->SetActorEnableCollision(bActive);
	Pawn->SetActorTickEnabled(bActive);

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			if (bActive)
			{
				Move->SetMovementMode(MOVE_Walking);
			}
			else
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
			}
			Move->SetComponentTickEnabled(bActive);
		}
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->SetComponentTickEnabled(bActive); // no animation update (no motion-matching search) while pooled
		}
	}

	// ADAPT: pause/resume the brain. AMurdarNPCAIController runs a 5 Hz Think timer and perception; both must stop
	// while pooled and restart (state reset to Idle, memory of the previous life cleared) on reuse. The in-car driver
	// is already made "dormant" by AMurdarVehicle::Enter (PROJECT_OVERVIEW §4.7) — reuse that path for the pawn half
	// and add e.g. AMurdarNPCAIController::SetDormant(bool) for the brain half if it doesn't exist.
}

void UCivilianPopulationSubsystem::ReturnToPool(APawn* Pawn)
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	if (Pool.Num() >= S->MaxPooled)
	{
		Pawn->Destroy(); // ADAPT: if the controller isn't destroyed with its pawn, destroy it too
		return;
	}
	SetBodyActive(Pawn, false);
	Pool.Add(Pawn);
}

void UCivilianPopulationSubsystem::DebugDraw(const FViewer& Viewer) const
{
	const UCivilianPopulationSettings* S = GetDefault<UCivilianPopulationSettings>();
	UWorld* World = GetWorld();
	const float Life = S->UpdateIntervalSeconds;
	const FVector C = Viewer.PawnLocation;
	const int32 Segments = 64;

	const FVector R = Viewer.SpawnCentre;
	DrawDebugCircle(World, R, S->SpawnRingMinCm, Segments, FColor::Green, false, Life, 0, 0.f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugCircle(World, R, S->SpawnRingMaxCm, Segments, FColor::Green, false, Life, 0, 0.f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugCircle(World, C, DespawnDistance(), Segments, FColor::Red, false, Life, 0, 0.f, FVector::ForwardVector, FVector::RightVector, false);

	for (const FSlot& Slot : Active)
	{
		if (const APawn* P = Slot.Pawn.Get())
		{
			DrawDebugPoint(World, P->GetActorLocation() + FVector(0.f, 0.f, 120.f), 12.f,
				Slot.bPinned ? FColor::Orange : FColor::Cyan, false, Life);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(DebugStatsKey, Life, FColor::Cyan, FString::Printf(
			TEXT("CIV active %d/%d (pinned %d) pool %d | new %d reuse %d despawn %d | rej vis %d nav %d crowd %d fail %d | dens x%.2f"),
			Stats.Active, Stats.Target, Stats.Pinned, Stats.Pooled, Stats.SpawnedNew, Stats.ReusedFromPool, Stats.Despawned,
			Stats.RejectedVisible, Stats.RejectedNav, Stats.RejectedCrowded, Stats.SpawnFailed, DensityScale));
	}
}
