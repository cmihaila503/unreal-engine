#include "Vehicle/Weight/CarWeightComponent.h"
#include "Vehicle/Weight/CarWeightSettings.h"

#include "Vehicle/MurdarVehicle.h"
#include "Vehicle/VehicleDefinition.h"
#include "VehicleDriveAssemblyComponent.h"
#include "VehicleWheelComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"

static TAutoConsoleVariable<int32> CVarCarWeightDebug(TEXT("Murdar.CarWeight.Debug"), 0, TEXT("1 = log airborne / landing / impact feel for the player's car"), ECVF_Cheat);

UCarWeightComponent::UCarWeightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics; // like UpdateDownforce: forces go in before the step
}

AMurdarVehicle* UCarWeightComponent::Vehicle() const { return Cast<AMurdarVehicle>(GetOwner()); }
UPrimitiveComponent* UCarWeightComponent::Body() const { const AActor* A = GetOwner(); return A ? Cast<UPrimitiveComponent>(A->GetRootComponent()) : nullptr; }

void UCarWeightComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyBodyContact();
}

void UCarWeightComponent::ApplyBodyContact()
{
	const UCarWeightSettings* S = UCarWeightSettings::Get();
	UPrimitiveComponent* B = Body();
	if (!S || !S->bBodyContact || !B) { return; }
	if (UPhysicalMaterial* PM = S->BodyMaterial.LoadSynchronous())
	{
		// The chassis only: the wheels' grip is authored on the road materials and read by KinetiForge's own traces
		// (VehicleSurfaceResponse.h) - a body override does not touch it. ADAPT: confirm the wheels are not shapes on
		// this body (if they are, grip changes - then put the override on the body's collision shapes only).
		B->SetPhysMaterialOverride(PM);
	}
	// ADAPT: FBodyInstance::SetMaxDepenetrationVelocity pushes it to the live body in UE5; if this version lacks it,
	// set bOverrideMaxDepenetrationVelocity + MaxDepenetrationVelocity before the physics state is created (constructor).
	B->BodyInstance.SetMaxDepenetrationVelocity(S->MaxDepenetrationVelocity);
}

int32 UCarWeightComponent::WheelsDown() const
{
	const AMurdarVehicle* V = Vehicle();
	UVehicleDriveAssemblyComponent* Drive = V ? V->GetDriveAssembly() : nullptr;
	if (!Drive) { return 4; } // unknown: treat as grounded (never add gravity blind)
	int32 N = 0;
	for (UVehicleWheelComponent* W : Drive->GetWheels())
	{
		if (W && W->GetIsWheelOnGround()) { ++N; }
	}
	return N;
}

void UCarWeightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UPrimitiveComponent* B = Body();
	const UCarWeightSettings* S = UCarWeightSettings::Get();
	if (!B || !S || !B->IsSimulatingPhysics() || !B->IsAnyRigidBodyAwake()) { return; }

	const MurdarWeight::FAirTuning T = S->Air();
	const bool bWasAirborne = Air.bAirborne;
	const bool bLanded = Air.Update(WheelsDown(), DeltaTime, T);
	if (CVarCarWeightDebug.GetValueOnGameThread() != 0 && Vehicle() && Vehicle()->IsOccupied())
	{
		if (!bWasAirborne && Air.bAirborne) { UE_LOG(LogTemp, Log, TEXT("CarWeight: airborne")); }
		if (bLanded) { UE_LOG(LogTemp, Log, TEXT("CarWeight: landed after %.2f s"), Air.LastAirTime); }
	}

	// 1. Gravity while flying (acceleration change: mass-independent, like real gravity).
	if (S->bAirGravity)
	{
		const float Extra = MurdarWeight::ExtraGravity(Air, T);
		if (Extra > 0.f) { B->AddForce(FVector(0.f, 0.f, -Extra), NAME_None, /*bAccelChange*/ true); }
	}

	// 2. Pitch / roll in the air: damp the spin, lean back towards wheels-down. Vector form, no Euler signs to get wrong:
	//    the tilt is the rotation that would bring the car's up onto the world's up; yaw (world Z) is never touched.
	if (S->bAirStabilize && Air.bAirborne)
	{
		const FVector Up = B->GetUpVector();
		const FVector Omega = B->GetPhysicsAngularVelocityInRadians();
		const FVector OmegaH(Omega.X, Omega.Y, 0.f);
		const FVector Axis = FVector::CrossProduct(Up, FVector::UpVector); // rotating about this brings Up to world up
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, FVector::UpVector), -1.f, 1.f));
		const FVector Tilt = -Axis.GetSafeNormal() * Angle; // rules convention: acceleration = -k * tilt
		// World X and Y components through the same per-axis rule (Pitch slot = X, Roll slot = Y).
		const MurdarWeight::FAxes A = MurdarWeight::AirStabilize({ OmegaH.X, OmegaH.Y }, { Tilt.X, Tilt.Y }, Air, T);
		const FVector AngAccel(A.Pitch, A.Roll, 0.f);
		if (!AngAccel.IsNearlyZero()) { B->AddTorqueInRadians(AngAccel, NAME_None, /*bAccelChange*/ true); }
	}

	// 3. Landing: no pogo.
	if (S->bLandingSettle)
	{
		const float Settle = MurdarWeight::LandingSettle(B->GetPhysicsLinearVelocity().Z, Air, T);
		if (Settle != 0.f) { B->AddForce(FVector(0.f, 0.f, Settle), NAME_None, true); }
	}
}

void UCarWeightComponent::OnImpact(float HorizontalCms, float VerticalCms, const FVector& Where)
{
	AMurdarVehicle* V = Vehicle();
	const UCarWeightSettings* S = UCarWeightSettings::Get();
	UWorld* World = GetWorld();
	if (!V || !S || !World || !V->Definition) { return; }
	const UVehicleDefinition& D = *V->Definition;
	const float Now = World->GetRealTimeSeconds();
	const MurdarWeight::FImpactFeel F = MurdarWeight::ImpactFeel(HorizontalCms, VerticalCms, V->IsOccupied(), Now - LastHitStopTime, S->Impact(D.CrashMinDeltaV, D.CrashFullDeltaV));
	if (F.Severity <= 0.f) { return; }

	if (F.Shake > 0.f && D.CrashShakeClass)
	{
		if (APlayerController* PC = Cast<APlayerController>(V->GetController()); PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraShake(D.CrashShakeClass, F.Shake);
		}
	}
	if (F.ThumpVolume > 0.f)
	{
		if (USoundBase* Thump = S->ThumpSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(World, Thump, Where, F.ThumpVolume, FMath::Lerp(1.05f, 0.8f, F.Severity));
		}
	}
	if (S->bHitStop && F.HitStopSeconds > 0.f)
	{
		LastHitStopTime = Now;
		HitStop(F.HitStopSeconds, S->HitStopDilation);
	}
	if (CVarCarWeightDebug.GetValueOnGameThread() != 0 && V->IsOccupied())
	{
		UE_LOG(LogTemp, Log, TEXT("CarWeight: impact dv %.0f/%.0f -> severity %.2f shake %.2f hitstop %.3f s"), HorizontalCms, VerticalCms, F.Severity, F.Shake, F.HitStopSeconds);
	}
}

void UCarWeightComponent::HitStop(float Seconds, float Dilation)
{
	UWorld* World = GetWorld();
	if (!World || bHitStopActive) { return; }
	// Someone else owns time (a slow-motion cheat, a cutscene, the pause menu): leave it alone.
	if (!FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.f) || UGameplayStatics::IsGamePaused(World)) { return; }
	bHitStopActive = true;
	UGameplayStatics::SetGlobalTimeDilation(World, Dilation);
	// Restored on the core ticker: real time, not the dilated game clock (a game timer would wait 20x longer).
	TWeakObjectPtr<UCarWeightComponent> Weak(this);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, WeakWorld](float)
	{
		if (UWorld* W = WeakWorld.Get()) { UGameplayStatics::SetGlobalTimeDilation(W, 1.f); }
		if (UCarWeightComponent* C = Weak.Get()) { C->bHitStopActive = false; }
		return false; // once
	}), Seconds);
}

FString UCarWeightComponent::Describe() const
{
	return FString::Printf(TEXT("weight: %s air %.2f s, since landing %.2f s, wheels down %d"), Air.bAirborne ? TEXT("AIRBORNE") : TEXT("grounded"),
		Air.AirTime, Air.SinceLanding, WheelsDown());
}
