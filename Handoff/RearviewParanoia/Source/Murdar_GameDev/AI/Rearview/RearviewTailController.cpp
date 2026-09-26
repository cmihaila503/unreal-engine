#include "AI/Rearview/RearviewTailController.h"

#include "AI/Rearview/RearviewSettings.h"
#include "AI/Rearview/RearviewSubsystem.h"
#include "AI/FactionMemorySubsystem.h"
#include "AI/PopulationSubsystem.h"
#include "AI/VehiclePursuitComponent.h"
#include "Director/GameEventSubsystem.h"
#include "Vehicle/MurdarVehicle.h"

#include "CollisionQueryParams.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	constexpr float ThinkSeconds = 0.25f; // 4 Hz, the police brain's rate
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
}

ARearviewTailController::ARearviewTailController()
{
	PrimaryActorTick.bCanEverTick = false; // timer brain; the pursuit component drives per frame
}

void ARearviewTailController::Setup(MurdarRearview::ETailRole InRole, float InSkill01, AMurdarVehicle* InTarget)
{
	Role = InRole;
	Skill01 = InSkill01;
	Target = InTarget;
	Exposure = MakeUnique<MurdarRearview::FTailExposure>(GetDefault<URearviewSettings>()->ToExposure());
	Rng.GenerateNewSeed();
}

UVehiclePursuitComponent* ARearviewTailController::Pursuit() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<UVehiclePursuitComponent>() : nullptr;
}

AMurdarVehicle* ARearviewTailController::Car() const
{
	return Cast<AMurdarVehicle>(GetPawn());
}

void ARearviewTailController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AMurdarVehicle* V = Cast<AMurdarVehicle>(InPawn);
	if (!V)
	{
		return;
	}
	// Same pattern as AMurdarPoliceAIController::OnPossess: the driver is a component the controller adds.
	if (!V->FindComponentByClass<UVehiclePursuitComponent>())
	{
		UVehiclePursuitComponent* Comp = NewObject<UVehiclePursuitComponent>(V, TEXT("Pursuit"));
		Comp->RegisterComponent();
	}
	V->BeginAIDriving();
	V->SetHeadlights(true); // at night everybody drives lit; a follower with no lights would be a giveaway

	// Remember the headlights so high beams can be restored exactly.
	TArray<USpotLightComponent*> Spots;
	V->GetComponents<USpotLightComponent>(Spots);
	for (USpotLightComponent* L : Spots)
	{
		Beams.Add({L, L->Intensity, L->AttenuationRadius});
	}

	if (!Exposure) { Exposure = MakeUnique<MurdarRearview::FTailExposure>(GetDefault<URearviewSettings>()->ToExposure()); }
	const float Now = GetWorld()->GetTimeSeconds();
	StartTime = Now;
	LastSeenTime = Now; // spawned behind him with him in front: count it as a sighting
	if (AMurdarVehicle* T = Target.Get())
	{
		LastSeenLocation = T->GetActorLocation();
		LastSeenVelocity = T->GetVelocity();
	}
	Enter(ETailState::Tailing, TEXT("started"));
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ARearviewTailController::Think, ThinkSeconds, true, Rng.FRandRange(0.f, ThinkSeconds));
}

void ARearviewTailController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	if (UVehiclePursuitComponent* P = Pursuit()) { P->SetMode(EPursuitMode::Idle); }
	Super::OnUnPossess();
}

void ARearviewTailController::EndPlay(const EEndPlayReason::Type Reason_)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (!bFinished)
	{
		if (URearviewSubsystem* Sub = URearviewSubsystem::Get(this)) { Sub->NotifyTailEnded(this); }
	}
	Super::EndPlay(Reason_);
}

bool ARearviewTailController::CanSeePlayer(float& OutDistCm) const
{
	const AMurdarVehicle* Me = Car();
	const AMurdarVehicle* T = Target.Get();
	if (!Me || !T)
	{
		OutDistCm = TNumericLimits<float>::Max();
		return false;
	}
	OutDistCm = FVector::Dist(Me->GetActorLocation(), T->GetActorLocation());
	// Line of sight at roof height, static world only (a car between us hides him for a moment; the grace covers it).
	FCollisionQueryParams Q(SCENE_QUERY_STAT(RearviewSight), false);
	Q.AddIgnoredActor(Me);
	Q.AddIgnoredActor(T);
	FHitResult Hit;
	const FVector Up(0.f, 0.f, 120.f); // eye height in a car ~1.2 m; not a tuning value
	const bool bLOS = !GetWorld()->LineTraceSingleByObjectType(Hit, Me->GetActorLocation() + Up, T->GetActorLocation() + Up,
		FCollisionObjectQueryParams(ECC_WorldStatic), Q);
	const URearviewSubsystem* Sub = URearviewSubsystem::Get(this);
	return MurdarRearview::CanSeeTarget(OutDistCm, bLOS, Sub && Sub->IsDark(), T->AreHeadlightsOn(), bHighBeams,
		GetDefault<URearviewSettings>()->ToVisibility());
}

bool ARearviewTailController::IsWithinReact() const
{
	const AMurdarVehicle* Me = Car();
	const AMurdarVehicle* T = Target.Get();
	return Me && T && FVector::Dist(Me->GetActorLocation(), T->GetActorLocation()) <= GetDefault<URearviewSettings>()->ReactRangeCm;
}

void ARearviewTailController::SetHighBeams(bool bOn)
{
	if (bOn == bHighBeams)
	{
		return;
	}
	bHighBeams = bOn;
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	for (const FBeam& B : Beams)
	{
		if (USpotLightComponent* L = B.Light.Get())
		{
			L->SetIntensity(bOn ? B.Intensity * S->HighBeamIntensityScale : B.Intensity);
			L->SetAttenuationRadius(bOn ? B.Radius * S->HighBeamReachScale : B.Radius);
		}
	}
}

void ARearviewTailController::Enter(ETailState Next, const TCHAR* Why)
{
	State = Next;
	StateSince = GetWorld()->GetTimeSeconds();
	Reason = Why;
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	UVehiclePursuitComponent* P = Pursuit();
	AMurdarVehicle* T = Target.Get();
	if (!P)
	{
		return;
	}
	const float Follow = Role == MurdarRearview::ETailRole::Gang ? S->GangFollowCm : S->UndercoverFollowCm;
	switch (Next)
	{
	case ETailState::Tailing:
		SetHighBeams(false);
		P->SetTargetEstimate(false, FVector::ZeroVector, FVector::ZeroVector);
		P->SetTarget(T);
		P->FollowDistance = Follow;
		P->SetMode(EPursuitMode::Follow);
		break;
	case ETailState::BackingOff:
		P->FollowDistance = Follow * S->UndercoverBackOffScale; // Follow opens a gap by coasting: no brake lights flashing
		P->SetMode(EPursuitMode::Follow);
		break;
	case ETailState::Alongside:
	case ETailState::Aggressive:
		P->SetTarget(T);
		P->SetMode(EPursuitMode::PullAlongside);
		break;
	case ETailState::Lost:
	{
		SetHighBeams(true);
		const float Age = FMath::Min(GetWorld()->GetTimeSeconds() - LastSeenTime, S->SearchPredictSeconds);
		const FVector Guess = LastSeenLocation + LastSeenVelocity * Age;
		P->SetTargetEstimate(true, Guess, LastSeenVelocity); // what the wheel may know: the estimate, not the man
		P->DriveTo(Guess, S->SearchKph);
		break;
	}
	case ETailState::BreakOff:
		SetHighBeams(false);
		P->SetTarget(nullptr);
		P->DriveTo(BreakOffGoal, P->MaxSpeedKph * 0.5f);
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("Rearview tail %s -> %s: %s"), *GetName(), *UEnum::GetValueAsString(Next), Why);
}

void ARearviewTailController::Think()
{
	AMurdarVehicle* Me = Car();
	AMurdarVehicle* T = Target.Get();
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Me)
	{
		Finish();
		return;
	}
	if (State != ETailState::BreakOff)
	{
		// He left the car, the car is gone, or it has gone on long enough: a tail follows a car, and not forever.
		if (!T || !T->GetDriver() || Now - StartTime > S->TailMaxSeconds)
		{
			BreakOff(!T || !T->GetDriver() ? TEXT("target out of the car") : TEXT("tail ran its time"));
			return;
		}
	}

	float Dist = 0.f;
	const bool bSeen = State != ETailState::BreakOff && CanSeePlayer(Dist);
	if (bSeen)
	{
		LastSeenTime = Now;
		LastSeenLocation = T->GetActorLocation();
		LastSeenVelocity = T->GetVelocity();
	}
	Exposure->Tick(ThinkSeconds);

	// Undercover with the police already interested: the radio hears where he is (the marked cars act on it).
	if (bSeen && Role == MurdarRearview::ETailRole::Undercover && Now - LastReportTime > S->UndercoverReportSeconds)
	{
		UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(this);
		if (Mem && Mem->GetWantedLevel() >= EWantedLevel::Stop)
		{
			Mem->ReportSighting(Me, T, ENPCFaction::Police);
			LastReportTime = Now;
		}
	}

	if (State != ETailState::BreakOff && State != ETailState::Aggressive && Exposure->IsBlown(Skill01))
	{
		if (Role == MurdarRearview::ETailRole::Undercover)
		{
			if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
			{
				FGameEvent E; E.Tag = Tag(TEXT("Event.Rearview.TailBlown")); E.Source = Me; E.Location = Me->GetActorLocation(); Bus->Publish(E);
			}
			BreakOff(TEXT("made: a pro lets go"));
			return;
		}
		Enter(ETailState::Aggressive, TEXT("made: stops pretending"));
		return;
	}

	switch (State)
	{
	case ETailState::Tailing:
		if (!bSeen && Now - LastSeenTime > S->LoseGraceSeconds)
		{
			Enter(ETailState::Lost, TEXT("lost him: high beams, flat out"));
		}
		break;
	case ETailState::BackingOff:
		if (Now - StateSince > S->BrakeReactSeconds) { Enter(ETailState::Tailing, TEXT("settled back in")); }
		break;
	case ETailState::Alongside:
		if (Now - StateSince > S->GangAlongsideSeconds) { Enter(ETailState::Tailing, TEXT("dropped back behind")); }
		break;
	case ETailState::Lost:
		if (bSeen)
		{
			Exposure->Add(S->ExposureRushedAfterDark); // he saw us come flying with the high beams on
			Enter(ETailState::Tailing, TEXT("found him again"));
		}
		else if (Now - StateSince > S->SearchSeconds)
		{
			BreakOff(TEXT("lost him for good"));
		}
		break;
	case ETailState::Aggressive:
		if (UVehiclePursuitComponent* P = Pursuit())
		{
			if (Now - StateSince > S->GangAlongsideSeconds && P->GetMode() != EPursuitMode::Ram)
			{
				P->RamTarget();
				if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
				{
					FGameEvent E; E.Tag = Tag(TEXT("Event.Rearview.GangAttack")); E.Source = Me; E.Location = Me->GetActorLocation(); Bus->Publish(E);
				}
			}
		}
		if (!bSeen && Now - LastSeenTime > S->SearchSeconds) { BreakOff(TEXT("he got away")); }
		break;
	case ETailState::BreakOff:
	{
		// Gone once he can't see us and we're far enough (never vanish in front of him).
		const UPopulationSubsystem* Pop = UPopulationSubsystem::Get(this);
		const APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
		const bool bFar = !PlayerPawn || FVector::Dist(PlayerPawn->GetActorLocation(), Me->GetActorLocation()) > S->DespawnDistanceCm;
		if (bFar && Pop && !Pop->IsVisibleToPlayer(Me->GetActorLocation()))
		{
			Finish();
		}
		break;
	}
	}
}

void ARearviewTailController::OnPlayerBrakeCheck()
{
	if (!IsWithinReact() || State == ETailState::Lost || State == ETailState::BreakOff || State == ETailState::Aggressive)
	{
		return;
	}
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	Exposure->Add(S->ExposureHeldOnBrakeCheck);
	if (Role == MurdarRearview::ETailRole::Gang)
	{
		Enter(ETailState::Alongside, TEXT("brake check: comes up level with him"));
	}
	else
	{
		Enter(ETailState::BackingOff, TEXT("brake check: drops back, no horn"));
	}
}

void ARearviewTailController::OnPlayerInspectionTurn()
{
	if (!IsWithinReact() || State == ETailState::Lost || State == ETailState::BreakOff)
	{
		return;
	}
	if (MurdarRearview::TakesTheBait(Role, Skill01, Rng.FRand()))
	{
		// Follow keeps following — he'll see us take the same side street.
		Exposure->Add(GetDefault<URearviewSettings>()->ExposureFollowedTurn);
		return;
	}
	BreakOff(TEXT("inspection turn: a pro drives on"));
}

void ARearviewTailController::BreakOff(const TCHAR* Why)
{
	const AMurdarVehicle* Me = Car();
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	BreakOffGoal = Me ? Me->GetActorLocation() + Me->GetActorForwardVector() * S->BreakOffDriveCm : FVector::ZeroVector;
	Enter(ETailState::BreakOff, Why);
	if (URearviewSubsystem* Sub = URearviewSubsystem::Get(this))
	{
		Sub->NotifyTailEnded(this); // the cooldown starts now; the car is still on the road until it can vanish
	}
	bFinished = true;
}

void ARearviewTailController::Finish()
{
	if (!bFinished)
	{
		if (URearviewSubsystem* Sub = URearviewSubsystem::Get(this)) { Sub->NotifyTailEnded(this); }
		bFinished = true;
	}
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	APawn* P = GetPawn();
	UnPossess();
	if (P) { P->Destroy(); }
	Destroy();
}

FString ARearviewTailController::Describe() const
{
	float Dist = 0.f;
	const bool bSeen = CanSeePlayer(Dist);
	return FString::Printf(TEXT("%s %s skill %.2f exposure %.2f | %s %.0f m%s | %s"),
		Role == MurdarRearview::ETailRole::Gang ? TEXT("GANG") : TEXT("UNDERCOVER"), *UEnum::GetValueAsString(State),
		Skill01, Exposure ? Exposure->Get() : 0.f, bSeen ? TEXT("sees him") : TEXT("blind"), Dist / 100.f,
		bHighBeams ? TEXT(" [high beams]") : TEXT(""), *Reason);
}
