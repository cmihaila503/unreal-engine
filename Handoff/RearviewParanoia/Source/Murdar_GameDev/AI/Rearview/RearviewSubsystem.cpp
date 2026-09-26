#include "AI/Rearview/RearviewSubsystem.h"

#include "AI/Rearview/RearviewSettings.h"
#include "AI/Rearview/RearviewTailController.h"
#include "AI/Rearview/MirrorViewComponent.h"
#include "AI/FactionMemorySubsystem.h"
#include "AI/MurdarRoadNavigation.h"
#include "AI/MurdarTrafficAIController.h"
#include "AI/PopulationSubsystem.h"
#include "AI/TrafficDriverComponent.h"
#include "Director/GameEventSubsystem.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Vehicle/MurdarVehicle.h"
#include "Vehicle/VehicleDefinition.h"
#include "Vehicle/VehicleEffectsComponent.h"
#include "Vehicle/VehicleSignalsComponent.h"
#include "Vehicle/VehicleSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr float UpdateSeconds = 0.1f;   // 10 Hz: the detectors are tuned and unit-tested on this rate
	constexpr int32 DarknessEveryN = 10;    // darkness once a second
	constexpr int32 DirectorEveryN = 10;    // tail director once a second
	constexpr float BrakePedalOn = 0.05f;   // pedal travel that lights the brake lamps (a switch, not a tuning value)

	// Tags live in Config/DefaultGameplayTags.ini (README §Patches 1); requested by name, no MurdarTags edit.
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false); }
}

URearviewSubsystem* URearviewSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URearviewSubsystem>() : nullptr;
}

bool URearviewSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URearviewSubsystem::ResetDetectors()
{
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	BrakeCheck = MakeUnique<MurdarRearview::FBrakeCheckDetector>(S->ToBrakeCheck());
	InspectionTurn = MakeUnique<MurdarRearview::FInspectionTurnDetector>(S->ToInspectionTurn());
	Maneuvers = MakeUnique<MurdarRearview::FManeuverTracker>(S->ToManeuver());
}

void URearviewSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ResetDetectors();
	Rng.GenerateNewSeed();
	UpdateDarkness();
	InWorld.GetTimerManager().SetTimer(UpdateTimer, this, &URearviewSubsystem::Update, UpdateSeconds, /*bLoop*/ true);
}

void URearviewSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Tails.Reset();
	Super::Deinitialize();
}

AMurdarVehicle* URearviewSubsystem::PlayerCar() const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AMurdarVehicle* Car = PC ? Cast<AMurdarVehicle>(PC->GetPawn()) : nullptr;
	return Car && !Car->IsAIDriven() ? Car : nullptr;
}

void URearviewSubsystem::Publish(FGameplayTag EventTag, const AActor* Source, float Magnitude) const
{
	UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this);
	if (!Bus || !EventTag.IsValid())
	{
		return;
	}
	FGameEvent E;
	E.Tag = EventTag;
	E.Source = Source;
	E.Location = Source ? Source->GetActorLocation() : FVector::ZeroVector;
	E.Magnitude = Magnitude;
	Bus->Publish(E);
}

void URearviewSubsystem::UpdateDarkness()
{
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	if (NightOverride >= 0)
	{
		bDark = NightOverride == 1;
		return;
	}
	bool bNight = S->bForceNight;
	if (!bNight && S->bNightFromSun && GetWorld())
	{
		// ADAPT/verify: the level's sun is the first directional light. Pointing down (pitch < 0) = above the horizon.
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			if (const UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
			{
				bNight = L->GetComponentRotation().Pitch > S->SunDownPitchDeg;
			}
			break;
		}
	}
	bDark = bNight;
}

void URearviewSubsystem::UpdatePlayerBrakeLights(AMurdarVehicle* Car)
{
	// The player's car had no brake lamps (only traffic drove them). Pedal = lamps; the handbrake lights nothing — the
	// old trick for slowing down dark, and now it works (CanSeeTarget: brake lights give a dark car away).
	UVehicleSignalsComponent* Sig = Car->FindComponentByClass<UVehicleSignalsComponent>();
	if (!Sig)
	{
		Sig = NewObject<UVehicleSignalsComponent>(Car, TEXT("Signals")); // same as UTrafficDriverComponent does
		Sig->RegisterComponent();
	}
	bPlayerBraking = Car->GetBrakeInput() > BrakePedalOn; // README §Patches 2: GetBrakeInput() on AMurdarVehicle
	Sig->SetBraking(bPlayerBraking);
}

void URearviewSubsystem::Update()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	++UpdateCount;
	if (UpdateCount % DarknessEveryN == 0)
	{
		UpdateDarkness();
	}
	Tails.RemoveAll([](const TWeakObjectPtr<ARearviewTailController>& T) { return !T.IsValid(); });

	AMurdarVehicle* Car = PlayerCar();
	if (Car != LastPlayerCar.Get())
	{
		// New car (or on foot): yaw and speed history belong to the car he left.
		ResetDetectors();
		LastPlayerCar = Car;
		bPlayerBraking = false;
		if (Car)
		{
			EnsureMirror(Car);
			bLastLights = Car->AreHeadlightsOn();
		}
	}
	if (!Car)
	{
		return;
	}
	UpdatePlayerBrakeLights(Car);

	// The tests only mean something at night (the design: empty roads, dark). By day nobody reads them.
	if (bDark)
	{
		const float Kph = Car->GetSpeedKph();
		const float Yaw = Car->GetActorRotation().Yaw;
		if (BrakeCheck->Sample(Now, Kph))
		{
			OnBrakeCheck(Car);
		}
		// The player has no indicator input yet; once he has, the car's signals component carries it (README §Manual).
		const UVehicleSignalsComponent* Signals = Car->FindComponentByClass<UVehicleSignalsComponent>();
		const bool bIndicating = Signals && Signals->GetTurn() != 0;
		if (const int32 Side = InspectionTurn->Sample(Now, Yaw, Kph, bIndicating))
		{
			OnInspectionTurn(Car, Side);
		}
		const MurdarRearview::EManeuver M = Maneuvers->Sample(Now, Yaw, Kph);
		if (M != MurdarRearview::EManeuver::None)
		{
			OnManeuver(Car, M);
		}
		const bool bLights = Car->AreHeadlightsOn();
		if (bLastLights && !bLights)
		{
			Publish(Tag(TEXT("Event.Rearview.LightsOut")), Car); // followers see it through CanSeeTarget; the bus is for sound/tension
		}
		bLastLights = bLights;
	}

	if (UpdateCount % DirectorEveryN == 0)
	{
		MaybeStartTail(Now);
	}
}

void URearviewSubsystem::OnBrakeCheck(AMurdarVehicle* Car)
{
	++BrakeChecks;
	Publish(Tag(TEXT("Event.Rearview.BrakeCheck")), Car);
	CivilianReactsToBrakeCheck(Car);
	for (const TWeakObjectPtr<ARearviewTailController>& T : Tails)
	{
		if (ARearviewTailController* Tail = T.Get()) { Tail->OnPlayerBrakeCheck(); }
	}
}

void URearviewSubsystem::OnInspectionTurn(AMurdarVehicle* Car, int32 Side)
{
	++InspectionTurns;
	Publish(Tag(TEXT("Event.Rearview.InspectionTurn")), Car, float(Side));
	for (const TWeakObjectPtr<ARearviewTailController>& T : Tails)
	{
		if (ARearviewTailController* Tail = T.Get()) { Tail->OnPlayerInspectionTurn(); }
	}
}

void URearviewSubsystem::OnManeuver(AMurdarVehicle* Car, MurdarRearview::EManeuver Maneuver)
{
	const bool bLoop = Maneuver == MurdarRearview::EManeuver::AroundTheBlock;
	++(bLoop ? Loops : UTurns);
	Publish(Tag(bLoop ? TEXT("Event.Rearview.AroundTheBlock") : TEXT("Event.Rearview.UTurn")), Car);
	for (const TWeakObjectPtr<ARearviewTailController>& T : Tails)
	{
		if (ARearviewTailController* Tail = T.Get()) { Tail->OnPlayerManeuver(Maneuver); }
	}
}

void URearviewSubsystem::CivilianReactsToBrakeCheck(AMurdarVehicle* Car)
{
	// The nearest ordinary car behind him, pointing his way. Close: a civilian doesn't wonder, he honks. Further back:
	// two blinks of the headlights. A follower does neither — and that silence is the tell.
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	const FVector P = Car->GetActorLocation();
	const FVector F = Car->GetActorForwardVector();
	AMurdarVehicle* Best = nullptr;
	float BestDist = FMath::Max(S->CivilianHonkRangeCm, S->CivilianFlashRangeCm);
	for (TActorIterator<AMurdarVehicle> It(GetWorld()); It; ++It)
	{
		AMurdarVehicle* Other = *It;
		if (Other == Car || !Other->FindComponentByClass<UTrafficDriverComponent>())
		{
			continue; // followers and police have no traffic driver: they never honk
		}
		const FVector D = Other->GetActorLocation() - P;
		const float Dist = D.Size2D();
		if (Dist < BestDist && FVector::DotProduct(D, F) < 0.f && FVector::DotProduct(Other->GetActorForwardVector(), F) > 0.7f)
		{
			Best = Other;
			BestDist = Dist;
		}
	}
	if (!Best)
	{
		return;
	}
	if (BestDist <= S->CivilianHonkRangeCm)
	{
		if (UVehicleEffectsComponent* Fx = Best->FindComponentByClass<UVehicleEffectsComponent>())
		{
			Fx->SetHorn(true);
			FTimerHandle Off;
			TWeakObjectPtr<UVehicleEffectsComponent> WeakFx(Fx);
			GetWorld()->GetTimerManager().SetTimer(Off, FTimerDelegate::CreateLambda([WeakFx]()
			{
				if (UVehicleEffectsComponent* F2 = WeakFx.Get()) { F2->SetHorn(false); }
			}), S->CivilianHonkSeconds, false);
		}
	}
	else if (Best->AreHeadlightsOn())
	{
		FlashHeadlights(Best, 2);
	}
}

void URearviewSubsystem::FlashHeadlights(AMurdarVehicle* Car, int32 BlinksLeft)
{
	// Off, on, off, on: each half-blink one timer step. Weak pointer: the car may be gone before the blink ends.
	if (!Car || BlinksLeft <= 0)
	{
		return;
	}
	const float Half = GetDefault<URearviewSettings>()->CivilianFlashBlinkSeconds;
	Car->SetHeadlights(false);
	TWeakObjectPtr<AMurdarVehicle> Weak(Car);
	TWeakObjectPtr<URearviewSubsystem> Self(this);
	FTimerHandle On;
	GetWorld()->GetTimerManager().SetTimer(On, FTimerDelegate::CreateLambda([Weak, Self, BlinksLeft, Half]()
	{
		AMurdarVehicle* C = Weak.Get();
		URearviewSubsystem* Sub = Self.Get();
		if (!C || !Sub) { return; }
		C->SetHeadlights(true);
		if (BlinksLeft > 1)
		{
			FTimerHandle Next;
			Sub->GetWorld()->GetTimerManager().SetTimer(Next, FTimerDelegate::CreateLambda([Weak, Self, BlinksLeft]()
			{
				if (Self.IsValid() && Weak.IsValid()) { Self->FlashHeadlights(Weak.Get(), BlinksLeft - 1); }
			}), Half, false);
		}
	}), Half, false);
}

void URearviewSubsystem::EnsureMirror(AMurdarVehicle* Car)
{
	if (!Car->FindComponentByClass<UMirrorViewComponent>())
	{
		UMirrorViewComponent* M = NewObject<UMirrorViewComponent>(Car, TEXT("RearviewMirror"));
		M->RegisterComponent();
	}
}

void URearviewSubsystem::MaybeStartTail(float Now)
{
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	AMurdarVehicle* Car = PlayerCar();
	if (!bDark || !Car || Tails.Num() > 0 || Now - LastTailEndTime < S->TailCooldownSeconds || Car->GetSpeedKph() < S->TailMinPlayerKph)
	{
		return;
	}
	// A chase in progress is the marked cars' business, not a tail's.
	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(this);
	const EWantedLevel Wanted = Mem ? Mem->GetWantedLevel() : EWantedLevel::None;
	if (Wanted >= EWantedLevel::Pursuit)
	{
		return;
	}
	// Chance per director tick (1 s) from the per-minute chance.
	const float PerTick = 1.f - FMath::Pow(1.f - S->TailChancePerMinute, 1.f / 60.f);
	if (Rng.FRand() >= PerTick)
	{
		return;
	}
	const bool bSuspected = Wanted == EWantedLevel::Stop || (Mem && Mem->GetKnownVehicle().IsValid());
	const float WUndercover = S->UndercoverBaseWeight + (bSuspected ? S->UndercoverSuspicionWeight : 0.f);
	const UNarrativeStateSubsystem* Story = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UNarrativeStateSubsystem>() : nullptr;
	const bool bGangHunting = S->GangHuntingFact.IsValid() && Story && Story->HasFact(S->GangHuntingFact);
	const float WGang = S->GangBaseWeight + (bGangHunting ? S->GangFactWeight : 0.f);
	const float WDecoy = S->CivilianDecoyWeight;
	const float Total = WUndercover + WGang + WDecoy;
	if (Total <= 0.f)
	{
		return;
	}
	const float Pick = Rng.FRand() * Total;
	StartTail(Pick < WUndercover ? MurdarRearview::ETailRole::Undercover
		: Pick < WUndercover + WGang ? MurdarRearview::ETailRole::Gang
		: MurdarRearview::ETailRole::Civilian);
}

bool URearviewSubsystem::FindSpotBehind(const AMurdarVehicle* Car, FTransform& Out) const
{
	// Behind him on his own lane, facing his way, out of his view, nothing parked on it.
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	UWorld* World = GetWorld();
	const UPopulationSubsystem* Pop = UPopulationSubsystem::Get(this);
	if (!World || !Pop)
	{
		return false;
	}
	const FVector Fwd = Car->GetActorForwardVector().GetSafeNormal2D();
	const FVector Guess = Car->GetActorLocation() - Fwd * S->SpawnBehindCm;
	FZoneGraphLaneLocation Lane;
	if (!MurdarRoad::NearestLane(World, Guess, S->SpawnLaneSearchCm, Fwd, Lane)
		|| FVector::DotProduct(Lane.Direction.GetSafeNormal2D(), Fwd) < 0.5f
		|| Pop->IsVisibleToPlayer(Lane.Position)
		|| !UPopulationSubsystem::IsClearOfBodies(World, Lane.Position, 800.f, 200.f)) // ADAPT: the population's own clearance numbers if exposed
	{
		return false;
	}
	Out = FTransform(Lane.Direction.Rotation(), Lane.Position + FVector(0.f, 0.f, 60.f)); // = UPopulationSubsystem CarSpawnLiftCm (60) — ADAPT: expose it and use it
	return true;
}

bool URearviewSubsystem::SpawnDecoy(const FTransform& Where)
{
	// An ordinary car, spawned exactly as UPopulationSubsystem::TrySpawnTrafficCar does, then handed to the population
	// (Adopt) so it is tidied away like any other traffic. It goes wherever its own route takes it — sometimes his way.
	UWorld* World = GetWorld();
	UPopulationSubsystem* Pop = UPopulationSubsystem::Get(this);
	UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(this);
	const TArray<UVehicleDefinition*> Defs = Pop ? Pop->TrafficDefinitions() : TArray<UVehicleDefinition*>();
	if (!World || !Vehicles || Defs.Num() == 0)
	{
		return false;
	}
	AMurdarVehicle* Car = Vehicles->SpawnVehicle(Pop->CarClass(), Defs[Rng.RandHelper(Defs.Num())], Where);
	if (!Car)
	{
		return false;
	}
	Car->AIControllerClass = AMurdarTrafficAIController::StaticClass();
	Car->AutoPossessAI = EAutoPossessAI::Disabled;
	AMurdarTrafficAIController* Brain = World->SpawnActor<AMurdarTrafficAIController>(AMurdarTrafficAIController::StaticClass(), Car->GetActorTransform());
	if (!Brain)
	{
		Car->Destroy();
		return false;
	}
	Brain->Possess(Car);
	Car->SetHeadlights(true);
	Pop->Adopt(EPopulationKind::Traffic, Car);
	return true;
}

bool URearviewSubsystem::StartTail(MurdarRearview::ETailRole Role)
{
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	UWorld* World = GetWorld();
	AMurdarVehicle* Car = PlayerCar();
	UPopulationSubsystem* Pop = UPopulationSubsystem::Get(this);
	UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(this);
	FTransform Xf;
	if (!World || !Car || !Pop || !Vehicles || !FindSpotBehind(Car, Xf))
	{
		return false;
	}

	if (Role == MurdarRearview::ETailRole::Civilian)
	{
		if (!SpawnDecoy(Xf))
		{
			return false;
		}
		++DecoysStarted;
		LastTailEndTime = World->GetTimeSeconds(); // a decoy uses up the cooldown too: the mirror stays quiet a while
		UE_LOG(LogTemp, Log, TEXT("Rearview: decoy civilian behind the player"));
		return true;
	}

	// The car: what the settings name for the role, else anything the traffic drives (a follower looks like anybody).
	UVehicleDefinition* Def = (Role == MurdarRearview::ETailRole::Undercover ? S->UndercoverCar : S->GangCar).LoadSynchronous();
	if (!Def)
	{
		const TArray<UVehicleDefinition*> Defs = Pop->TrafficDefinitions();
		Def = Defs.Num() > 0 ? Defs[Rng.RandHelper(Defs.Num())] : nullptr;
	}
	AMurdarVehicle* TailCar = Def ? Vehicles->SpawnVehicle(Pop->CarClass(), Def, Xf) : nullptr;
	if (!TailCar)
	{
		return false;
	}
	TailCar->AutoPossessAI = EAutoPossessAI::Disabled;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARearviewTailController* Ctrl = World->SpawnActor<ARearviewTailController>(ARearviewTailController::StaticClass(), Xf, P);
	if (!Ctrl)
	{
		TailCar->Destroy();
		return false;
	}
	const float Skill = Role == MurdarRearview::ETailRole::Undercover
		? Rng.FRandRange(S->UndercoverSkillMin, S->UndercoverSkillMax)
		: Rng.FRandRange(0.f, S->GangSkillMax);
	Ctrl->Setup(Role, Skill, Car);
	Ctrl->Possess(TailCar);
	Tails.Add(Ctrl);
	++TailsStarted;
	Publish(Tag(TEXT("Event.Rearview.TailStarted")), TailCar, float(int32(Role)));
	UE_LOG(LogTemp, Log, TEXT("Rearview: %s tail started (skill %.2f) %.0f m behind"),
		Role == MurdarRearview::ETailRole::Undercover ? TEXT("undercover") : TEXT("gang"), Skill, S->SpawnBehindCm / 100.f);
	return true;
}

void URearviewSubsystem::NotifyTailEnded(ARearviewTailController* Tail)
{
	Tails.RemoveAll([Tail](const TWeakObjectPtr<ARearviewTailController>& T) { return !T.IsValid() || T.Get() == Tail; });
	LastTailEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

FString URearviewSubsystem::Describe() const
{
	FString Out = FString::Printf(TEXT("rearview: %s | brake checks %d, sudden turns %d, U-turns %d, loops %d | tails %d, decoys %d, active %d"),
		bDark ? TEXT("night") : TEXT("day"), BrakeChecks, InspectionTurns, UTurns, Loops, TailsStarted, DecoysStarted, Tails.Num());
	for (const TWeakObjectPtr<ARearviewTailController>& T : Tails)
	{
		if (const ARearviewTailController* Tail = T.Get()) { Out += TEXT("\n  ") + Tail->Describe(); }
	}
	return Out;
}
