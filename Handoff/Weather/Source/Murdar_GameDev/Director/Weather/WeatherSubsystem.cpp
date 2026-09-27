#include "Director/Weather/WeatherSubsystem.h"

#include "Director/Weather/WeatherSettings.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Director/TimeOfDay/TimeOfDaySubsystem.h" // Handoff/TimeOfDay

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "NiagaraComponent.h"            // ADAPT: "Niagara" in Build.cs
#include "NiagaraFunctionLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
}

UWeatherSubsystem* UWeatherSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWeatherSubsystem>() : nullptr;
}

bool UWeatherSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWeatherSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!GetDefault<UWeatherSettings>()->bEnabled) { return; }
	for (TActorIterator<AExponentialHeightFog> It(&InWorld); It; ++It) { Fog = *It; FogBaseDensity = It->GetComponent()->FogDensity; break; }
	for (const TSoftObjectPtr<UPhysicalMaterial>& P : GetDefault<UWeatherSettings>()->WetAffected)
	{
		if (UPhysicalMaterial* M = P.LoadSynchronous()) { OriginalFriction.Add(M, M->Friction); }
	}
	LoadFromState();
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UWeatherSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event_State_Loaded, [Weak](const FGameEvent&) { if (Weak.IsValid()) { Weak->LoadFromState(); } });
	}
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UWeatherSubsystem::Tick1Hz, 1.f, true);
}

void UWeatherSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Restore(); // the physical materials are assets: in the editor, PIE must not leave them changed
	Super::Deinitialize();
}

void UWeatherSubsystem::Restore()
{
	for (const TPair<TWeakObjectPtr<UPhysicalMaterial>, float>& P : OriginalFriction)
	{
		if (UPhysicalMaterial* M = P.Key.Get()) { M->Friction = P.Value; }
	}
	if (AExponentialHeightFog* F = Fog.Get(); F && FogBaseDensity >= 0.f) { F->GetComponent()->SetFogDensity(FogBaseDensity); }
}

float UWeatherSubsystem::CurrentHour() const
{
	const UTimeOfDaySubsystem* Time = UTimeOfDaySubsystem::Get(this);
	return Time ? Time->GetHour() : 12.f;
}

float UWeatherSubsystem::GetPeopleScale() const
{
	return MurdarWeather::PeopleScale(Look, GetDefault<UWeatherSettings>()->ToTuning());
}

void UWeatherSubsystem::SetState(MurdarWeather::EState NewState, bool bInstant)
{
	State = NewState;
	if (bInstant) { Look = MurdarWeather::LookOf(State); }
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Publish(FGameEvent(Tag(TEXT("Event.Weather.Changed")), nullptr, FVector::ZeroVector, float(int32(State)))); }
	Apply();
	SaveToState();
}

void UWeatherSubsystem::Tick1Hz()
{
	const MurdarWeather::FTuning T = GetDefault<UWeatherSettings>()->ToTuning();
	const float Hour = CurrentHour();
	const int32 H = FMath::FloorToInt(Hour);
	if (LastHour < 0) { LastHour = H; }
	if (H != LastHour)
	{
		LastHour = H;
		const bool bFogPossible = Hour < 8.f || Hour >= 21.f;
		const MurdarWeather::EState NewState = MurdarWeather::Next(State, FMath::FRand(), bFogPossible, MurdarWeather::DefaultMatrix());
		if (NewState != State) { SetState(NewState, false); }
	}
	const MurdarWeather::FLook Before = Look;
	Look = MurdarWeather::Blend(Look, MurdarWeather::LookOf(State), 1.f, T.BlendRate);
	Wetness = MurdarWeather::UpdateWetness(Wetness, Look.Rain, Hour < 6.f || Hour >= 20.f, 1.f, T);
	// Thunder now and then in a storm.
	if (State == MurdarWeather::EState::Storm && Look.Rain > 0.8f && FMath::FRand() < 0.03f)
	{
		if (USoundBase* S = GetDefault<UWeatherSettings>()->Thunder.LoadSynchronous()) { UGameplayStatics::PlaySound2D(this, S, FMath::FRandRange(0.6f, 1.f)); }
	}
	Apply();
	if (FMath::Abs(Before.Rain - Look.Rain) > 0.f || GetWorld()->GetTimeSeconds() < 2.f) { SaveToState(); }
}

void UWeatherSubsystem::Apply()
{
	const UWeatherSettings* S = GetDefault<UWeatherSettings>();
	const MurdarWeather::FTuning T = S->ToTuning();

	const float Grip = MurdarWeather::GripScale(Wetness, T);
	for (const TPair<TWeakObjectPtr<UPhysicalMaterial>, float>& P : OriginalFriction)
	{
		// ADAPT: KinetiForge reads Friction from the hit's physical material each raycast (CacheImpactFriction). If
		// Chaos caches it instead, also call FPhysicsInterface::UpdateMaterial / PhysMat->UpdateEngineMaterial().
		if (UPhysicalMaterial* M = P.Key.Get()) { M->Friction = P.Value * Grip; }
	}

	if (UMaterialParameterCollection* MPC = S->Parameters.LoadSynchronous())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Rain"), Look.Rain);
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Wetness"), Wetness);
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Cloud"), Look.Cloud);
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Fog"), Look.Fog);
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Wind"), Look.Wind);
	}

	if (AExponentialHeightFog* F = Fog.Get(); F && FogBaseDensity >= 0.f)
	{
		F->GetComponent()->SetFogDensity(FogBaseDensity * (1.f + (S->FogMultiplier - 1.f) * Look.Fog));
	}

	// Rain around the camera; its loop sound follows the intensity.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	USceneComponent* Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetRootComponent() : nullptr;
	if (Look.Rain > 0.01f && Cam)
	{
		if (!Rain)
		{
			if (UNiagaraSystem* Sys = S->RainSystem.LoadSynchronous())
			{
				Rain = UNiagaraFunctionLibrary::SpawnSystemAttached(Sys, Cam, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
			}
		}
		if (!RainAudio)
		{
			if (USoundBase* Loop = S->RainLoop.LoadSynchronous()) { RainAudio = UGameplayStatics::SpawnSound2D(this, Loop, 0.01f, 1.f, 0.f, nullptr, false, false); }
		}
	}
	if (Rain) { Rain->SetVariableFloat(TEXT("Intensity"), Look.Rain); Rain->SetVisibility(Look.Rain > 0.01f); }
	if (RainAudio) { RainAudio->AdjustVolume(1.f, FMath::Max(0.001f, Look.Rain)); }
}

void UWeatherSubsystem::SaveToState() const
{
	if (UNarrativeStateSubsystem* St = UNarrativeStateSubsystem::Get(this))
	{
		St->SetValue(Tag(TEXT("Stat.Weather")), float(int32(State)));
		St->SetValue(Tag(TEXT("Stat.Wetness")), Wetness);
	}
}

void UWeatherSubsystem::LoadFromState()
{
	if (const UNarrativeStateSubsystem* St = UNarrativeStateSubsystem::Get(this))
	{
		State = MurdarWeather::EState(FMath::Clamp(FMath::RoundToInt(St->GetValue(Tag(TEXT("Stat.Weather")), 0.f)), 0, MurdarWeather::N - 1));
		Wetness = St->GetValue(Tag(TEXT("Stat.Wetness")), 0.f);
	}
	Look = MurdarWeather::LookOf(State);
	LastHour = -1;
	Apply();
}

FString UWeatherSubsystem::Describe() const
{
	static const TCHAR* Names[] = { TEXT("clear"), TEXT("cloudy"), TEXT("rain"), TEXT("storm"), TEXT("fog") };
	return FString::Printf(TEXT("Weather: %s | rain %.2f cloud %.2f fog %.2f | wet %.2f grip x%.2f people x%.2f gloom %.2f"),
		Names[int32(State)], Look.Rain, Look.Cloud, Look.Fog, Wetness, MurdarWeather::GripScale(Wetness, GetDefault<UWeatherSettings>()->ToTuning()),
		GetPeopleScale(), GetGloom());
}
