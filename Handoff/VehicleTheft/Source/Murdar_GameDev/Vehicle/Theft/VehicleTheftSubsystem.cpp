#include "Vehicle/Theft/VehicleTheftSubsystem.h"

#include "Vehicle/Theft/TheftSettings.h"
#include "Vehicle/MurdarVehicle.h"
#include "Vehicle/VehicleDefinition.h"
#include "Vehicle/VehicleSignalsComponent.h"
#include "Vehicle/VehicleSubsystem.h"
#include "AI/FactionMemorySubsystem.h"
#include "AI/MurdarAISettings.h"          // UMurdarAILibrary::SpawnNPC
#include "AI/MurdarNPCAIController.h"
#include "AI/MurdarPoliceAIController.h"
#include "AI/PedestrianComponent.h"
#include "AI/TrafficDriverComponent.h"
#include "Character/MurdarCharacter.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MurdarTheft"

namespace
{
	constexpr float TickSeconds = 0.1f;
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }

	FText CarName(const AMurdarVehicle* Car)
	{
		return Car && Car->Definition && !Car->Definition->DisplayName.IsEmpty() ? Car->Definition->DisplayName : LOCTEXT("Car", "mașină");
	}

	FVector DoorOf(const AMurdarVehicle* Car)
	{
		return Car->GetActorTransform().TransformPosition(Car->Definition ? Car->Definition->ExitOffset : FVector(0.f, -170.f, 0.f));
	}
}

UVehicleTheftSubsystem* UVehicleTheftSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UVehicleTheftSubsystem>() : nullptr;
}

bool UVehicleTheftSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVehicleTheftSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		// Any car he sits in is his from then on (the chapter car, one he stole, one the story gave him).
		TWeakObjectPtr<UVehicleTheftSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event_Player_EnteredVehicle, [Weak](const FGameEvent& E)
		{
			if (Weak.IsValid()) { Weak->Owned.Add(const_cast<AMurdarVehicle*>(Cast<AMurdarVehicle>(E.Source.Get()))); }
		});
	}
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UVehicleTheftSubsystem::Tick, TickSeconds, true);
}

void UVehicleTheftSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	StopAlarm();
	Super::Deinitialize();
}

bool UVehicleTheftSubsystem::IsNight() const
{
	// From the saved clock (Handoff/TimeOfDay); without it, never night.
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const float Minutes = State ? State->GetValue(Tag(TEXT("Stat.TimeOfDay")), 720.f) : 720.f;
	return Minutes < 6.f * 60.f || Minutes >= 20.f * 60.f;
}

bool UVehicleTheftSubsystem::IsLocked(AMurdarVehicle* Car)
{
	// "Owned" tag: the chapter car and cars placed for him (README §Patches 4) are never locked.
	if (!Car || Owned.Contains(Car) || Car->IsAIDriven() || Car->ActorHasTag(TEXT("Owned"))) { return false; }
	if (const bool* L = Locks.Find(Car)) { return *L; }
	const bool bLocked = MurdarTheft::RollLocked(FMath::FRand(), IsNight(), GetDefault<UTheftSettings>()->ToTuning());
	Locks.Add(Car, bLocked);
	return bLocked;
}

bool UVehicleTheftSubsystem::IsReportedStolen(const AMurdarVehicle* Car) const
{
	const MurdarTheft::FStolenCar* S = Stolen.Find(const_cast<AMurdarVehicle*>(Car));
	return S && S->bReported && !S->bCleaned;
}

void UVehicleTheftSubsystem::MarkCleaned(AMurdarVehicle* Car)
{
	if (MurdarTheft::FStolenCar* S = Stolen.Find(Car)) { S->bCleaned = true; }
}

UVehicleTheftSubsystem::FTarget UVehicleTheftSubsystem::FindTarget(const AMurdarCharacter* Man) const
{
	FTarget Out;
	const UVehicleSubsystem* Vehicles = Man ? UVehicleSubsystem::Get(Man) : nullptr;
	if (!Vehicles) { return Out; }
	const MurdarTheft::FTuning T = GetDefault<UTheftSettings>()->ToTuning();
	const FVector At = Man->GetActorLocation();
	float Best = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AMurdarVehicle>& W : Vehicles->GetVehicles())
	{
		AMurdarVehicle* Car = W.Get();
		if (!Car || !Car->Definition || Car->IsOccupied()) { continue; }
		const float D = Car->GetDistanceTo(Man);
		if (!Car->IsAIDriven())
		{
			if (D <= Car->Definition->EnterRadius && D < Best) { Best = D; Out.Car = Car; Out.bCarjack = false; }
			continue;
		}
		MurdarTheft::FCarjackCheck C;
		C.bAIDriven = true;
		C.bPolice = Cast<AMurdarPoliceAIController>(Car->GetController()) != nullptr;
		const UTrafficDriverComponent* Driver = Car->FindComponentByClass<UTrafficDriverComponent>();
		C.bHasTrafficDriver = Driver && Driver->IsDriving();
		C.SpeedKph = Car->GetSpeedKph();
		C.DistanceCm = FVector::Dist(DoorOf(Car), At);
		if (MurdarTheft::CanCarjack(C, T) && D < Best) { Best = D; Out.Car = Car; Out.bCarjack = true; }
	}
	return Out;
}

FText UVehicleTheftSubsystem::GetPrompt(const AMurdarCharacter* Man) const
{
	if (IsBreakingIn())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const int32 Pct = FMath::RoundToInt(100.f * BreakIn.Progress01(Now, GetDefault<UTheftSettings>()->ToTuning()));
		return BreakIn.Get() == MurdarTheft::FBreakIn::EState::Window
			? FText::Format(LOCTEXT("Window", "Spargi geamul... {0}%"), Pct)
			: FText::Format(LOCTEXT("Wires", "Pornești fără cheie... {0}%"), Pct);
	}
	const FTarget T = FindTarget(Man);
	if (!T.Car) { return FText::GetEmpty(); }
	if (T.bCarjack) { return FText::Format(LOCTEXT("Carjack", "Scoate-l din {0}"), CarName(T.Car)); }
	// IsLocked may roll the lock the first time: fine to do from the HUD (the answer is then fixed).
	if (const_cast<UVehicleTheftSubsystem*>(this)->IsLocked(T.Car)) { return FText::Format(LOCTEXT("BreakIn", "Sparge geamul la {0}"), CarName(T.Car)); }
	return FText::Format(LOCTEXT("Enter", "Urcă în {0}"), CarName(T.Car));
}

bool UVehicleTheftSubsystem::TryTakeCar(AMurdarCharacter* Man)
{
	if (!Man || IsBreakingIn()) { return IsBreakingIn(); }
	const FTarget T = FindTarget(Man);
	if (!T.Car) { return false; }
	if (T.bCarjack) { StartCarjack(Man, T.Car); return true; }
	if (IsLocked(T.Car)) { StartBreakIn(Man, T.Car); return true; }
	const bool bOwned = Owned.Contains(T.Car) || T.Car->ActorHasTag(TEXT("Owned"));
	if (!T.Car->Enter(Man)) { return false; }
	// An unlocked car that isn't his is still somebody's.
	if (!bOwned) { FinishTheft(Man, T.Car, MurdarTheft::EMethod::Unlocked, false); }
	return true;
}

void UVehicleTheftSubsystem::StartBreakIn(AMurdarCharacter* Man, AMurdarVehicle* Car)
{
	BreakIn = MurdarTheft::FBreakIn();
	BreakIn.Start(GetWorld()->GetTimeSeconds());
	BreakInCar = Car;
	BreakInMan = Man;
	bBreakInAlarm = false;
}

void UVehicleTheftSubsystem::StartCarjack(AMurdarCharacter* Man, AMurdarVehicle* Car)
{
	// The car stops for good, its AI lets go, and the driver is on the pavement.
	if (UTrafficDriverComponent* Driver = Car->FindComponentByClass<UTrafficDriverComponent>()) { Driver->StopDriving(); }
	if (AController* C = Car->GetController()) { C->UnPossess(); C->Destroy(); }
	Car->EndAIDriving();

	const FVector Door = DoorOf(Car) + FVector(0.f, 0.f, 100.f);
	const FRotator Facing = (Man->GetActorLocation() - Door).GetSafeNormal2D().Rotation();
	if (AMurdarCharacter* Victim = UMurdarAILibrary::SpawnNPC(this, ENPCFaction::Civilian, FTransform(Facing, Door + Car->GetActorRightVector() * -120.f)))
	{
		// He shouts at the thief, then goes (ADAPT: BeginConfront with a null car must just end the confront).
		if (AMurdarNPCAIController* Brain = Cast<AMurdarNPCAIController>(Victim->GetController()))
		{
			Brain->GetOrCreateAmbient()->BeginConfront(Man, Door, nullptr);
		}
	}
	Publish(TEXT("Event.Vehicle.Carjacked"), Car);

	TWeakObjectPtr<UVehicleTheftSubsystem> Weak(this);
	TWeakObjectPtr<AMurdarCharacter> WeakMan(Man);
	TWeakObjectPtr<AMurdarVehicle> WeakCar(Car);
	FTimerHandle H;
	GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Weak, WeakMan, WeakCar]
	{
		if (!Weak.IsValid() || !WeakMan.IsValid() || !WeakCar.IsValid()) { return; }
		if (WeakCar->Enter(WeakMan.Get())) { Weak->FinishTheft(WeakMan.Get(), WeakCar.Get(), MurdarTheft::EMethod::Carjack, false); }
	}), FMath::Max(GetDefault<UTheftSettings>()->CarjackEnterDelay, 0.01f), false);
}

float UVehicleTheftSubsystem::NearestWitnessCm(const FVector& Where) const
{
	// People only (AI characters with a controller): a car's driver behind glass counts as its car.
	float Best = -1.f;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerControlled() || !It->GetController()) { continue; }
		const float D = FVector::Dist(It->GetActorLocation(), Where);
		if (Best < 0.f || D < Best) { Best = D; }
	}
	return Best;
}

void UVehicleTheftSubsystem::FinishTheft(AMurdarCharacter* Man, AMurdarVehicle* Car, MurdarTheft::EMethod Method, bool bAlarm)
{
	const MurdarTheft::FTuning T = GetDefault<UTheftSettings>()->ToTuning();
	const bool bSeen = Method == MurdarTheft::EMethod::Carjack || MurdarTheft::Witnessed(NearestWitnessCm(Car->GetActorLocation()), bAlarm, T);
	MurdarTheft::FStolenCar S;
	S.ReportAt = GetWorld()->GetTimeSeconds() + MurdarTheft::ReportDelay(Method, bSeen, FMath::FRand(), T);
	Stolen.Add(Car, S);
	Locks.Add(Car, false); // the window is gone / the door is open
	Publish(TEXT("Event.Vehicle.Stolen"), Car);
	UE_LOG(LogTemp, Log, TEXT("Theft: %s by %s (%s, %s) -> report in %.0f s"), *Car->GetName(), *GetNameSafe(Man),
		Method == MurdarTheft::EMethod::Carjack ? TEXT("carjack") : Method == MurdarTheft::EMethod::BreakIn ? TEXT("break-in") : TEXT("unlocked"),
		bSeen ? TEXT("seen") : TEXT("unseen"), S.ReportAt - GetWorld()->GetTimeSeconds());
}

void UVehicleTheftSubsystem::StartAlarm(AMurdarVehicle* Car)
{
	StopAlarm();
	AlarmCar = Car;
	AlarmEndsAt = GetWorld()->GetTimeSeconds() + GetDefault<UTheftSettings>()->AlarmSeconds;
	if (UVehicleSignalsComponent* Signals = Car->FindComponentByClass<UVehicleSignalsComponent>()) { Signals->SetHazards(true); }
	if (USoundBase* Sound = GetDefault<UTheftSettings>()->AlarmSound.LoadSynchronous())
	{
		Alarm = UGameplayStatics::SpawnSoundAttached(Sound, Car->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true);
	}
	Publish(TEXT("Event.Vehicle.Alarm"), Car);
}

void UVehicleTheftSubsystem::StopAlarm()
{
	if (Alarm) { Alarm->Stop(); Alarm = nullptr; }
	if (AMurdarVehicle* Car = AlarmCar.Get())
	{
		if (UVehicleSignalsComponent* Signals = Car->FindComponentByClass<UVehicleSignalsComponent>()) { Signals->SetHazards(false); }
	}
	AlarmCar.Reset();
}

void UVehicleTheftSubsystem::Tick()
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	const MurdarTheft::FTuning T = GetDefault<UTheftSettings>()->ToTuning();

	// Break-in in progress.
	if (IsBreakingIn())
	{
		AMurdarCharacter* Man = BreakInMan.Get();
		AMurdarVehicle* Car = BreakInCar.Get();
		if (!Man || !Car || Car->IsOccupied())
		{
			BreakIn = MurdarTheft::FBreakIn();
		}
		else
		{
			const MurdarTheft::FBreakIn::EState S = BreakIn.Update(Now, FVector::Dist(Man->GetActorLocation(), DoorOf(Car)), T);
			if (BreakIn.ConsumeWindowBroken(Now, T))
			{
				if (USoundBase* Glass = GetDefault<UTheftSettings>()->GlassSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, Glass, DoorOf(Car)); }
				Publish(TEXT("Event.Vehicle.WindowBroken"), Car);
				bBreakInAlarm = FMath::FRand() < T.AlarmChance;
				if (bBreakInAlarm) { StartAlarm(Car); }
			}
			if (S == MurdarTheft::FBreakIn::EState::Done && Car->Enter(Man))
			{
				FinishTheft(Man, Car, MurdarTheft::EMethod::BreakIn, bBreakInAlarm);
				BreakIn = MurdarTheft::FBreakIn();
			}
		}
	}

	if (AlarmCar.IsValid() && Now >= AlarmEndsAt) { StopAlarm(); }

	// Reports, and a patrol recognising a reported car.
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(World);
	const bool bSeenNow = Mem && Mem->TimeSinceLastSighting() < 1.f;
	for (auto It = Stolen.CreateIterator(); It; ++It)
	{
		AMurdarVehicle* Car = It->Key.Get();
		if (!Car) { It.RemoveCurrent(); continue; }
		if (It->Value.UpdateReport(Now))
		{
			Publish(TEXT("Event.Vehicle.ReportedStolen"), Car);
			UE_LOG(LogTemp, Log, TEXT("Theft: %s reported stolen"), *Car->GetName());
		}
		if (It->Value.UpdateRecognition(bSeenNow, PlayerPawn == Car) && Mem)
		{
			Mem->ReportCrime(Tag(TEXT("Crime.CarTheft")), T.CrimeSeverity, Car->GetActorLocation(), Mem->GetThreat().LastWitness.Get());
		}
	}
}

void UVehicleTheftSubsystem::Publish(const TCHAR* TagName, const AActor* Source) const
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Publish(FGameEvent(Tag(TagName), Source, Source ? Source->GetActorLocation() : FVector::ZeroVector));
	}
}

FString UVehicleTheftSubsystem::Describe() const
{
	FString Out = FString::Printf(TEXT("Theft: %d locks known, %d owned, %d stolen%s\n"), Locks.Num(), Owned.Num(), Stolen.Num(), IsBreakingIn() ? TEXT(", BREAKING IN") : TEXT(""));
	const float Now = GetWorld()->GetTimeSeconds();
	for (const TPair<TWeakObjectPtr<AMurdarVehicle>, MurdarTheft::FStolenCar>& P : Stolen)
	{
		Out += FString::Printf(TEXT("  %-28s %s%s%s\n"), *GetNameSafe(P.Key.Get()),
			P.Value.bCleaned ? TEXT("cleaned") : P.Value.bReported ? TEXT("REPORTED") : *FString::Printf(TEXT("report in %.0f s"), P.Value.ReportAt - Now),
			P.Value.bRecognised ? TEXT(", recognised") : TEXT(""), TEXT(""));
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
