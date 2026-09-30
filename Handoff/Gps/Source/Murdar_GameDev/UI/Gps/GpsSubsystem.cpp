#include "UI/Gps/GpsSubsystem.h"
#include "UI/Gps/GpsSettings.h"
#include "UI/Gps/SMurdarMinimap.h"

#include "AI/MurdarNavigation.h"
#include "AI/MurdarRoadNavigation.h"
#include "Vehicle/MurdarVehicle.h"
#if __has_include("Director/Jobs/JobSubsystem.h")
#include "Director/Jobs/JobSubsystem.h"
#define MURDAR_GPS_HAS_JOBS 1
#else
#define MURDAR_GPS_HAS_JOBS 0
#endif

#include "Director/GameEventSubsystem.h"
#include "GameplayTagContainer.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWeakWidget.h"

namespace
{
	MurdarGps::FV2 V2(const FVector& V) { return { static_cast<float>(V.X), static_cast<float>(V.Y) }; }
	int32 Idx(EGpsPin K) { return static_cast<int32>(K); }
	constexpr float ThinkInterval = 0.25f;

	UWorld* FirstGameWorld()
	{
		if (!GEngine) { return nullptr; }
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if ((C.WorldType == EWorldType::PIE || C.WorldType == EWorldType::Game) && C.World()) { return C.World(); }
		}
		return nullptr;
	}

	FAutoConsoleCommand CmdGps(TEXT("Murdar.Gps"), TEXT("Murdar.Gps [clear] - pins, routes and remaining distance; 'clear' removes your waypoint."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UGpsSubsystem* G = UGpsSubsystem::Get(FirstGameWorld());
			if (!G) { return; }
			if (Args.Num() > 0 && Args[0] == TEXT("clear")) { G->ClearWaypoint(); }
			UE_LOG(LogTemp, Display, TEXT("%s"), *G->Describe());
		}));
}

UGpsSubsystem* UGpsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGpsSubsystem>() : nullptr;
}

bool UGpsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGpsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	NextThink = 0.f;
	// Story missions: an objective with a place pins it; the end of the mission clears it. Tags looked up by name
	// (no compile dependency on the Missions handoff). ADAPT: the tag names the mission runtime really publishes, and
	// that it puts the objective's place in FGameEvent::Location.
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UGpsSubsystem> Weak(this);
		const FGameplayTag Objective = FGameplayTag::RequestGameplayTag(TEXT("Event.Mission.Objective"), false);
		const FGameplayTag Ended[] = { FGameplayTag::RequestGameplayTag(TEXT("Event.Mission.Succeeded"), false), FGameplayTag::RequestGameplayTag(TEXT("Event.Mission.Failed"), false) };
		if (Objective.IsValid())
		{
			BusHandles.Add(Bus->Subscribe(Objective, [Weak](const FGameEvent& E)
			{
				if (Weak.IsValid() && !E.Location.IsZero()) { Weak->SetMissionPin(E.Location, FText::FromString(E.Payload)); }
			}));
		}
		for (const FGameplayTag& T : Ended)
		{
			if (!T.IsValid()) { continue; }
			BusHandles.Add(Bus->Subscribe(T, [Weak](const FGameEvent&) { if (Weak.IsValid() && Weak->bScriptedMission) { Weak->ClearMissionPin(); } }));
		}
	}
}

void UGpsSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		for (auto& H : BusHandles) { Bus->Unsubscribe(H); }
	}
	BusHandles.Reset();
	RemoveMinimap();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------------
// Pins

void UGpsSubsystem::SetMissionPin(FVector Where, FText Label)
{
	Pins.Set(MurdarGps::EPin::Mission, V2(Where));
	Labels[Idx(EGpsPin::Mission)] = Label;
	PinZ[Idx(EGpsPin::Mission)] = Where.Z;
	bScriptedMission = true;
	bMissionFromJob = false;
	NextThink = 0.f; // route it now
}

void UGpsSubsystem::ClearMissionPin()
{
	Pins.Clear(MurdarGps::EPin::Mission);
	Routes[Idx(EGpsPin::Mission)].Clear();
	bScriptedMission = false;
	bMissionFromJob = false;
}

bool UGpsSubsystem::ToggleWaypoint(FVector Where)
{
	const UGpsSettings* S = UGpsSettings::Get();
	// From the map there is no height: find the ground under the point (the lanes and the navmesh are 3D).
	if (Where.Z == 0.0 && GetWorld())
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(Where.X, Where.Y, 500000.0), FVector(Where.X, Where.Y, -500000.0), ECC_Visibility)) { Where.Z = Hit.ImpactPoint.Z; }
	}
	const bool bSet = Pins.ToggleWaypoint(V2(Where), S ? S->RemoveRadiusCm : 3000.f);
	Routes[Idx(EGpsPin::Waypoint)].Clear();
	PinZ[Idx(EGpsPin::Waypoint)] = Where.Z;
	Labels[Idx(EGpsPin::Waypoint)] = NSLOCTEXT("Murdar", "GpsWaypoint", "Punctul tău");
	NextThink = 0.f;
	return bSet;
}

void UGpsSubsystem::ClearWaypoint()
{
	Pins.Clear(MurdarGps::EPin::Waypoint);
	Routes[Idx(EGpsPin::Waypoint)].Clear();
}

bool UGpsSubsystem::GetPin(EGpsPin Kind, FVector& OutWhere, FText& OutLabel) const
{
	const MurdarGps::FPin& P = Pins.Get(static_cast<MurdarGps::EPin>(Kind));
	if (!P.bSet) { return false; }
	OutWhere = FVector(P.At.X, P.At.Y, PinZ[Idx(Kind)]);
	OutLabel = Labels[Idx(Kind)];
	return true;
}

TArray<FVector> UGpsSubsystem::GetRoute(EGpsPin Kind) const
{
	TArray<FVector> Out;
	for (const MurdarGps::FV2& P : Routes[Idx(Kind)].Pts) { Out.Add(FVector(P.X, P.Y, 0.f)); }
	return Out;
}

float UGpsSubsystem::GetRemainingCm(EGpsPin Kind) const { return Routes[Idx(Kind)].bValid ? Routes[Idx(Kind)].RemainingCm : 0.f; }

bool UGpsSubsystem::GetPlayer(FVector& OutLoc, float& OutYaw, float& OutKph) const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!P) { return false; }
	OutLoc = P->GetActorLocation();
	OutYaw = P->GetActorRotation().Yaw;
	OutKph = P->GetVelocity().Size2D() * 0.036f;
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tick

void UGpsSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) { return; }
	EnsureMinimap();
	const float Now = World->GetTimeSeconds();
	if (Now >= NextThink)
	{
		NextThink = Now + ThinkInterval;
		Think(Now);
	}
}

void UGpsSubsystem::PullJobPin()
{
#if MURDAR_GPS_HAS_JOBS
	if (bScriptedMission) { return; } // a story mission outranks a job
	UJobSubsystem* Jobs = GetWorld()->GetSubsystem<UJobSubsystem>();
	FVector Where;
	FText Label;
	if (Jobs && Jobs->GetTargetLocation(Where, Label))
	{
		Pins.Set(MurdarGps::EPin::Mission, V2(Where));
		Labels[Idx(EGpsPin::Mission)] = Label;
		PinZ[Idx(EGpsPin::Mission)] = Where.Z;
		bMissionFromJob = true;
	}
	else if (bMissionFromJob)
	{
		ClearMissionPin();
	}
#endif
}

void UGpsSubsystem::Think(float Now)
{
	PullJobPin();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const UGpsSettings* S = UGpsSettings::Get();
	if (!Pawn || !S) { return; }
	const FVector Loc = Pawn->GetActorLocation();
	const MurdarGps::FV2 Me = V2(Loc);
	const bool bDriving = Pawn->IsA<AMurdarVehicle>();

	if (MurdarGps::WaypointReached(Pins, Me, S->ArriveCm))
	{
		ClearWaypoint();
		UE_LOG(LogTemp, Log, TEXT("GPS: waypoint reached"));
	}

	const MurdarGps::FRouteTuning T = S->Routing();
	bool bPlannedThisThink = false;
	for (int32 k = 0; k < MurdarGps::NumRouted; ++k)
	{
		const int32 K = (k + PlanTurn) % MurdarGps::NumRouted;
		const MurdarGps::FPin& Pin = Pins.Get(static_cast<MurdarGps::EPin>(K));
		MurdarGps::FRoute& R = Routes[K];
		if (!Pin.bSet) { R.Clear(); continue; }
		const bool bDue = R.Update(Me, Pin.At, Now, T);
		if (bDue && !bPlannedThisThink)
		{
			Plan(static_cast<EGpsPin>(K), Loc, Pawn->GetActorForwardVector(), bDriving, Now);
			bPlannedThisThink = true;
		}
	}
	PlanTurn = (PlanTurn + 1) % MurdarGps::NumRouted;
}

void UGpsSubsystem::Plan(EGpsPin Kind, const FVector& From, const FVector& FromDir, bool bDriving, float Now)
{
	const UGpsSettings* S = UGpsSettings::Get();
	const MurdarGps::FPin& Pin = Pins.Get(static_cast<MurdarGps::EPin>(Kind));
	const FVector To(Pin.At.X, Pin.At.Y, PinZ[Idx(Kind)]);
	TArray<FVector> Pts;
	bool bOk = false;
	// Driving (or far on foot - you will take a car): along the lanes, the way traffic may go.
	const bool bRoads = bDriving || FVector::Dist2D(From, To) > 60000.f;
	if (bRoads && MurdarRoad::HasRoads(GetWorld()))
	{
		bOk = MurdarRoad::RouteAlongLanes(GetWorld(), From, FromDir, To, S ? S->LaneSnapCm : 4000.f, Pts);
		if (bOk)
		{
			// The lanes end at the lane point nearest the pin: finish the line to the pin itself (a courtyard, a field).
			Pts.Insert(From, 0);
			Pts.Add(To);
		}
	}
	if (!bOk && !bDriving)
	{
		bOk = MurdarNav::FindPath(GetWorld(), From, To, MurdarNav::HumanRadius, MurdarNav::HumanHeight, Pts, nullptr) && Pts.Num() >= 2;
	}
	if (!bOk)
	{
		Pts = { From, To }; // off the roads and the navmesh: as the crow flies
	}
	std::vector<MurdarGps::FV2> P;
	P.reserve(Pts.Num());
	for (const FVector& V : Pts) { P.push_back(V2(V)); }
	Routes[Idx(Kind)].Adopt(P, Pin.At, Now);
}

// ---------------------------------------------------------------------------------------------------------------------
// Minimap on screen

void UGpsSubsystem::EnsureMinimap()
{
	const UGpsSettings* S = UGpsSettings::Get();
	UWorld* World = GetWorld();
	UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
	if (!S || !S->bMinimap || !VC)
	{
		RemoveMinimap();
		return;
	}
	if (bMinimapAdded) { return; }
	Minimap = SNew(SMurdarMinimap).Gps(this);
	MinimapHost = SNew(SWeakWidget).PossiblyNullContent(Minimap);
	// Under the menus (they sit at higher Z orders), over the game.
	VC->AddViewportWidgetContent(MinimapHost.ToSharedRef(), 5);
	bMinimapAdded = true;
}

void UGpsSubsystem::RemoveMinimap()
{
	if (!bMinimapAdded) { return; }
	UWorld* World = GetWorld();
	UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
	if (VC && MinimapHost.IsValid()) { VC->RemoveViewportWidgetContent(MinimapHost.ToSharedRef()); }
	MinimapHost.Reset();
	Minimap.Reset();
	bMinimapAdded = false;
}

FString UGpsSubsystem::Describe() const
{
	FString Out = TEXT("GPS:");
	const TCHAR* Names[] = { TEXT("mission"), TEXT("waypoint") };
	for (int32 k = 0; k < MurdarGps::NumRouted; ++k)
	{
		const MurdarGps::FPin& P = Pins.Get(static_cast<MurdarGps::EPin>(k));
		const MurdarGps::FRoute& R = Routes[k];
		Out += FString::Printf(TEXT("\n  %-8s %s"), Names[k], P.bSet
			? *FString::Printf(TEXT("(%.0f, %.0f) '%s' route %d pts, %.0f m left"), P.At.X, P.At.Y, *Labels[k].ToString(), static_cast<int32>(R.Pts.size()), R.RemainingCm / 100.f)
			: TEXT("-"));
	}
	return Out;
}
