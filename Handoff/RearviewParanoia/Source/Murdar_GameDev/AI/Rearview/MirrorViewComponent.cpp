#include "AI/Rearview/MirrorViewComponent.h"

#include "AI/Rearview/RearviewSettings.h"
#include "Vehicle/MurdarVehicle.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

void UMirrorViewComponent::OnRegister()
{
	Super::OnRegister();
	EnsureCamera();
}

void UMirrorViewComponent::EnsureCamera()
{
	AActor* Owner = GetOwner();
	if (MirrorCamera || !Owner || !Owner->GetRootComponent())
	{
		return;
	}
	const URearviewSettings* S = GetDefault<URearviewSettings>();
	MirrorCamera = NewObject<UCameraComponent>(Owner, TEXT("RearviewMirrorCamera"));
	MirrorCamera->SetupAttachment(Owner->GetRootComponent());
	// Just behind the body, looking back: from inside the cabin the camera would look through the car's own mesh
	// (review 26 Sep). The body's length comes from its bounds; the mask material frames the view as the mirror.
	const AMurdarVehicle* Car = Cast<AMurdarVehicle>(Owner);
	const float HalfLength = Car ? Car->GetBodyHalfExtents().X : 0.f;
	MirrorCamera->SetRelativeLocation(FVector(-(HalfLength + S->MirrorBehindBodyCm), 0.f, S->MirrorHeightCm));
	MirrorCamera->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	MirrorCamera->SetFieldOfView(S->MirrorFovDeg);
	MirrorCamera->bUsePawnControlRotation = false;
	MirrorCamera->PostProcessBlendWeight = 1.f;
	MirrorCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
	MirrorCamera->PostProcessSettings.VignetteIntensity = S->MirrorVignette;
	if (UMaterialInterface* Mask = S->MirrorMaskMaterial.LoadSynchronous())
	{
		// The mirror's frame and the left-right flip a real mirror has (a camera doesn't) — manual asset.
		MirrorCamera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Mask));
	}
	MirrorCamera->SetAutoActivate(false);
	MirrorCamera->RegisterComponent();
	MirrorCamera->Deactivate();
}

void UMirrorViewComponent::SetMirrorView(bool bOn)
{
	AActor* Owner = GetOwner();
	if (bOn == bOn_ || !Owner)
	{
		return;
	}
	EnsureCamera();
	if (!MirrorCamera)
	{
		return;
	}
	bOn_ = bOn;
	const URearviewSettings* S = GetDefault<URearviewSettings>();

	if (bOn)
	{
		// The view target's active camera is the one that renders: switch the car's own off, ours on.
		// ADAPT/verify: AMurdarVehicle's feel layer (Tick) must not re-activate its camera while the mirror is up.
		Hidden.Reset();
		TArray<UCameraComponent*> Cams;
		Owner->GetComponents<UCameraComponent>(Cams);
		for (UCameraComponent* C : Cams)
		{
			if (C != MirrorCamera && C->IsActive())
			{
				Hidden.Add(C);
				C->Deactivate();
			}
		}
		MirrorCamera->Activate();

		// Engine down, to hear what is behind. ADAPT: AMurdarVehicle's EngineAudio is protected; this finds it by name —
		// a public getter on the car would be cleaner.
		TArray<UAudioComponent*> Audio;
		Owner->GetComponents<UAudioComponent>(Audio);
		for (UAudioComponent* A : Audio)
		{
			if (A->GetFName() == TEXT("EngineAudio"))
			{
				Engine = A;
				EngineVolumeBefore = A->VolumeMultiplier;
				A->SetVolumeMultiplier(EngineVolumeBefore * S->MirrorEngineVolume);
				break;
			}
		}
	}
	else
	{
		MirrorCamera->Deactivate();
		for (const TWeakObjectPtr<UCameraComponent>& C : Hidden)
		{
			if (UCameraComponent* Cam = C.Get()) { Cam->Activate(); }
		}
		Hidden.Reset();
		if (UAudioComponent* A = Engine.Get()) { A->SetVolumeMultiplier(EngineVolumeBefore); }
		Engine.Reset();
	}
}

void UMirrorViewComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	SetMirrorView(false);
	Super::EndPlay(Reason);
}
