#include "AI/Quality/AIWatchdogSubsystem.h"
#include "AI/Quality/AIQualitySettings.h"

#include "AI/MurdarPoliceAIController.h"
#include "Character/MurdarCharacter.h"
#include "Vehicle/MurdarVehicle.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMurdarAIQ, Log, All);

static TAutoConsoleVariable<int32> CVarStuckDraw(TEXT("Murdar.AI.StuckDraw"), 0, TEXT("1 = mark agents at watchdog level 1+ (yellow 1, orange 2, red 3)"), ECVF_Cheat);

namespace
{
	UWorld* FirstGameWorld()
	{
		if (!GEngine) { return nullptr; }
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if ((C.WorldType == EWorldType::PIE || C.WorldType == EWorldType::Game) && C.World()) { return C.World(); }
		}
		return nullptr;
	}

	FAutoConsoleCommand CmdSoak(TEXT("Murdar.AI.Soak"), TEXT("Murdar.AI.Soak <seconds=600> [tour]: record every AI stuck event, write Saved/Logs/AISoak_<time>.csv, print PASS/FAIL. 'stop' ends it early."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UAIWatchdogSubsystem* W = UAIWatchdogSubsystem::Get(FirstGameWorld());
			if (!W) { UE_LOG(LogMurdarAIQ, Warning, TEXT("Murdar.AI.Soak: no game world (start PIE first)")); return; }
			if (Args.Num() > 0 && Args[0].Equals(TEXT("stop"), ESearchCase::IgnoreCase)) { W->StopSoak(); return; }
			const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 600.f;
			const bool bTour = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("tour"), ESearchCase::IgnoreCase); });
			W->StartSoak(Seconds > 0.f ? Seconds : 600.f, bTour);
		}));

	FAutoConsoleCommand CmdStuck(TEXT("Murdar.AI.Stuck"), TEXT("List the AI agents at watchdog level 1+ right now."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UAIWatchdogSubsystem* W = UAIWatchdogSubsystem::Get(FirstGameWorld())) { UE_LOG(LogMurdarAIQ, Display, TEXT("%s"), *W->Describe()); }
		}));
}

UAIWatchdogSubsystem* UAIWatchdogSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UAIWatchdogSubsystem>() : nullptr;
}

bool UAIWatchdogSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAIWatchdogSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(TickTimer, FTimerDelegate::CreateUObject(this, &UAIWatchdogSubsystem::Tick1Hz), 1.f, true);
}

void UAIWatchdogSubsystem::Deinitialize()
{
	if (bSoaking) { FinishSoak(); }
	if (UWorld* W = GetWorld()) { W->GetTimerManager().ClearTimer(TickTimer); }
	Super::Deinitialize();
}

void UAIWatchdogSubsystem::Report(AActor* Agent, MurdarAIQ::EAgent Kind, int32 Level, bool bEscalated, bool bStranded, const FString& Why, int32 JunctionZone)
{
	if (!Agent) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (Level <= 0 && !bStranded)
	{
		Agents.Remove(Agent);
		return;
	}
	FEntry& E = Agents.FindOrAdd(Agent);
	if (E.Level != Level) { E.Since = Now; }
	E.Kind = Kind;
	E.Level = Level;
	E.bStranded = bStranded;
	E.JunctionZone = JunctionZone;
	E.Why = Why;
	if (bEscalated)
	{
		UE_LOG(LogMurdarAIQ, Log, TEXT("%s %s: level %d (%s)"), ANSI_TO_TCHAR(MurdarAIQ::AgentName(Kind)), *Agent->GetName(), Level, *Why);
		if (bSoaking)
		{
			Stats.Add(Kind, Level);
			Events.Add(FEvent{ Now - SoakStart, Kind, Level, Agent->GetName(), Agent->GetActorLocation(), Why });
		}
	}
}

void UAIWatchdogSubsystem::Forget(AActor* Agent) { Agents.Remove(Agent); }

int32 UAIWatchdogSubsystem::LevelOf(const AActor* Agent) const
{
	const FEntry* E = Agents.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Agent)));
	return E ? E->Level : 0;
}

void UAIWatchdogSubsystem::Tick1Hz()
{
	for (auto It = Agents.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()) { It.RemoveCurrent(); }
	}
	GridlockPass();
	RecyclePass();
	if (bSoaking)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (bTour && Now >= NextTour) { TourStep(); NextTour = Now + 60.f; }
		if (Now >= SoakEnd) { FinishSoak(); }
	}
	if (CVarStuckDraw.GetValueOnGameThread() != 0) { Draw(); }
}

void UAIWatchdogSubsystem::GridlockPass()
{
	TMap<int32, std::vector<int>> ByZone;
	TMap<int32, FVector> Where;
	for (const TPair<TWeakObjectPtr<AActor>, FEntry>& KV : Agents)
	{
		if (KV.Value.Kind != MurdarAIQ::EAgent::Traffic || KV.Value.JunctionZone == INDEX_NONE || !KV.Key.IsValid()) { continue; }
		ByZone.FindOrAdd(KV.Value.JunctionZone).push_back(KV.Value.Level);
		Where.FindOrAdd(KV.Value.JunctionZone) = KV.Key->GetActorLocation();
	}
	TSet<int32> Now;
	for (const TPair<int32, std::vector<int>>& KV : ByZone)
	{
		if (!MurdarAIQ::IsGridlock(KV.Value)) { continue; }
		Now.Add(KV.Key);
		if (KnownGridlocks.Contains(KV.Key)) { continue; }
		const FVector W = Where.FindRef(KV.Key);
		UE_LOG(LogMurdarAIQ, Warning, TEXT("GRIDLOCK at junction zone %d (%.0f, %.0f, %.0f): %d cars stuck"), KV.Key, W.X, W.Y, W.Z, static_cast<int32>(KV.Value.size()));
		if (bSoaking)
		{
			++Stats.Gridlocks;
			Events.Add(FEvent{ GetWorld()->GetTimeSeconds() - SoakStart, MurdarAIQ::EAgent::Traffic, 5, FString::Printf(TEXT("zone %d"), KV.Key), W, TEXT("gridlock") });
		}
	}
	KnownGridlocks = MoveTemp(Now);
}

bool UAIWatchdogSubsystem::IsVisibleToPlayer(const AActor* A) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !A) { return false; }
	const FVector P = A->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	FVector2D Screen;
	if (!PC->ProjectWorldLocationToScreen(P, Screen)) { return false; }
	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);
	if (Screen.X < -50.f || Screen.Y < -50.f || Screen.X > W + 50.f || Screen.Y > H + 50.f) { return false; }
	// On screen - but behind a building is out of sight.
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AIQVisible), false, PC->GetPawn());
	Q.AddIgnoredActor(A);
	return !GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, P, ECC_Visibility, Q);
}

bool UAIWatchdogSubsystem::IsInChase(const AActor* A) const
{
	const APawn* P = Cast<APawn>(A);
	const AMurdarPoliceAIController* Cop = P ? Cast<AMurdarPoliceAIController>(P->GetController()) : nullptr;
	if (!Cop) { return false; }
	const EPoliceState S = Cop->GetState();
	return !(S == EPoliceState::Patrol || S == EPoliceState::Disabled || S == EPoliceState::StandDown);
}

bool UAIWatchdogSubsystem::IsPlayerOwned(const AActor* A) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (A == Player) { return true; }
	if (const AMurdarVehicle* V = Cast<AMurdarVehicle>(A); V && V->IsOccupied()) { return true; } // the player sits in it (AI driving does not set Driver - the population's despawn relies on the same)
	return A->Tags.Contains(TEXT("Owned"));
}

void UAIWatchdogSubsystem::RecyclePass()
{
	const UAIQualitySettings* S = UAIQualitySettings::Get();
	if (!S || !S->bRecycleStuck) { return; }
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) { return; }
	TArray<AActor*> ToRemove;
	for (const TPair<TWeakObjectPtr<AActor>, FEntry>& KV : Agents)
	{
		AActor* A = KV.Key.Get();
		if (!A) { continue; }
		MurdarAIQ::FRecycleInput I;
		I.WatchLevel = KV.Value.Level;
		I.bStranded = KV.Value.bStranded;
		I.DistCm = FVector::Dist2D(A->GetActorLocation(), Player->GetActorLocation());
		I.bInChase = IsInChase(A);
		I.bPlayerOwned = IsPlayerOwned(A);
		I.bMission = A->Tags.Contains(TEXT("Mission")) || A->Tags.Contains(TEXT("Scripted")); // ADAPT: the tags missions / street events put on their actors
		if (!I.bStranded && I.WatchLevel < 3) { continue; } // not given up: nothing to decide (and no trace)
		I.bVisible = IsVisibleToPlayer(A);
		if (MurdarAIQ::Recycle(I, S->RecycleMinDistanceCm) == MurdarAIQ::ERecycle::Remove) { ToRemove.Add(A); }
	}
	for (AActor* A : ToRemove)
	{
		const FEntry E = Agents.FindRef(A);
		UE_LOG(LogMurdarAIQ, Log, TEXT("recycled %s %s (level %d%s: %s)"), ANSI_TO_TCHAR(MurdarAIQ::AgentName(E.Kind)), *A->GetName(), E.Level, E.bStranded ? TEXT(", stranded") : TEXT(""), *E.Why);
		if (bSoaking)
		{
			++Stats.Recycled[static_cast<int>(E.Kind)];
			Events.Add(FEvent{ GetWorld()->GetTimeSeconds() - SoakStart, E.Kind, 4, A->GetName(), A->GetActorLocation(), E.Why });
		}
		Agents.Remove(A);
		// A foot NPC reports through its controller's pawn; a car through the car. Destroy the controller too, as the
		// population's own despawn does - and the population drops the dead weak pointer on its next pass.
		if (APawn* P = Cast<APawn>(A))
		{
			if (AController* C = P->GetController()) { C->Destroy(); }
		}
		A->Destroy();
	}
}

void UAIWatchdogSubsystem::StartSoak(float Seconds, bool bInTour)
{
	Stats = MurdarAIQ::FSoakStats();
	Events.Reset();
	bSoaking = true;
	bTour = bInTour;
	SoakStart = GetWorld()->GetTimeSeconds();
	SoakEnd = SoakStart + Seconds;
	NextTour = SoakStart + 60.f;
	UE_LOG(LogMurdarAIQ, Display, TEXT("AI soak started: %.0f s%s"), Seconds, bTour ? TEXT(", touring") : TEXT(""));
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("AI soak: %.0f s"), Seconds)); }
}

void UAIWatchdogSubsystem::StopSoak()
{
	if (bSoaking) { FinishSoak(); }
}

void UAIWatchdogSubsystem::TourStep()
{
	// Somewhere else on the navmesh, 80-200 m away: the population spawns and despawns round the new spot, and the
	// soak sees the streets, not one corner. On foot only (a car would be teleported into traffic).
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	APawn* P = PC ? PC->GetPawn() : nullptr;
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!P || !Nav || Cast<AMurdarVehicle>(P)) { return; }
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FNavLocation To;
		if (!Nav->GetRandomReachablePointInRadius(P->GetActorLocation(), 20000.f, To)) { continue; }
		if (FVector::Dist2D(To.Location, P->GetActorLocation()) < 8000.f) { continue; }
		P->TeleportTo(To.Location + FVector(0.f, 0.f, 100.f), P->GetActorRotation());
		UE_LOG(LogMurdarAIQ, Log, TEXT("soak tour: player to (%.0f, %.0f, %.0f)"), To.Location.X, To.Location.Y, To.Location.Z);
		return;
	}
}

void UAIWatchdogSubsystem::FinishSoak()
{
	bSoaking = false;
	Stats.Seconds = GetWorld() ? GetWorld()->GetTimeSeconds() - SoakStart : 0.0;
	FString Csv = TEXT("time_s,kind,level,who,x,y,z,why\n");
	for (const FEvent& E : Events)
	{
		Csv += FString::Printf(TEXT("%.1f,%s,%d,%s,%.0f,%.0f,%.0f,\"%s\"\n"), E.Time, ANSI_TO_TCHAR(MurdarAIQ::AgentName(E.Kind)), E.Level, *E.Who,
			E.Where.X, E.Where.Y, E.Where.Z, *E.Why.Replace(TEXT("\""), TEXT("'")));
	}
	const FString Summary = UTF8_TO_TCHAR(Stats.Summary().c_str());
	Csv += TEXT("# ") + Summary + TEXT("\n");
	const FString Path = FPaths::ProjectLogDir() / FString::Printf(TEXT("AISoak_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FFileHelper::SaveStringToFile(Csv, *Path);
	UE_LOG(LogMurdarAIQ, Display, TEXT("%s -> %s"), *Summary, *Path);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, Stats.Passes() ? FColor::Green : FColor::Red, Summary); }
}

FString UAIWatchdogSubsystem::Describe() const
{
	FString Out = FString::Printf(TEXT("AI watchdog: %d agent(s) at level 1+%s"), Agents.Num(), bSoaking ? TEXT(" | SOAKING") : TEXT(""));
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (const TPair<TWeakObjectPtr<AActor>, FEntry>& KV : Agents)
	{
		if (!KV.Key.IsValid()) { continue; }
		Out += FString::Printf(TEXT("\n  %-10s %s L%d %.0fs%s%s: %s"), ANSI_TO_TCHAR(MurdarAIQ::AgentName(KV.Value.Kind)), *KV.Key->GetName(), KV.Value.Level, Now - KV.Value.Since,
			KV.Value.bStranded ? TEXT(" stranded") : TEXT(""), KV.Value.JunctionZone != INDEX_NONE ? *FString::Printf(TEXT(" junction %d"), KV.Value.JunctionZone) : TEXT(""), *KV.Value.Why);
	}
	return Out;
}

void UAIWatchdogSubsystem::Draw() const
{
	for (const TPair<TWeakObjectPtr<AActor>, FEntry>& KV : Agents)
	{
		const AActor* A = KV.Key.Get();
		if (!A) { continue; }
		const FColor C = KV.Value.Level >= 3 || KV.Value.bStranded ? FColor::Red : KV.Value.Level == 2 ? FColor::Orange : FColor::Yellow;
		DrawDebugSphere(GetWorld(), A->GetActorLocation() + FVector(0.f, 0.f, 250.f), 60.f, 8, C, false, 1.05f, 0, 4.f);
		DrawDebugString(GetWorld(), A->GetActorLocation() + FVector(0.f, 0.f, 320.f), FString::Printf(TEXT("L%d %s"), KV.Value.Level, *KV.Value.Why), nullptr, C, 1.05f, true);
	}
}
