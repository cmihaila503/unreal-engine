#include "Director/WorldState/WorldStateSubsystem.h"

#include "Director/WorldState/WorldStateComponent.h"
#include "Director/WorldState/WorldStateRules.h"
#include "Director/WorldState/WorldStateTypes.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Character/MurdarCharacter.h"
#include "Vehicle/MurdarVehicle.h"      // ADAPT path
#include "Vehicle/VehicleSubsystem.h"   // ADAPT path
#include "Vehicle/VehicleDefinition.h"  // ADAPT path

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	std::string ToStd(FName N) { return std::string(TCHAR_TO_UTF8(*N.ToString())); }

	MurdarWorld::FRecord ToRule(const FWorldActorRecord& R)
	{
		MurdarWorld::FRecord X;
		X.Map = ToStd(R.Map); X.Id = ToStd(R.Id);
		X.bDestroyed = R.bDestroyed; X.bHasTransform = R.bHasTransform; X.bHasValue = R.bHasValue;
		return X;
	}
}

UWorldStateSubsystem* UWorldStateSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWorldStateSubsystem>() : nullptr;
}

bool UWorldStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

FMurdarWorldState* UWorldStateSubsystem::Data() const
{
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State ? &State->MutableWorld() : nullptr; // README §Patches 1
}

FName UWorldStateSubsystem::MapName() const
{
	// Without the PIE prefix, so a save from PIE applies in a packaged build and back.
	return FName(*UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString*/ true));
}

bool UWorldStateSubsystem::IsStoryMode() const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const FGameplayTag Story = FGameplayTag::RequestGameplayTag(FName(TEXT("Fact.Consequences.Checkpoint")), false);
	return State && Story.IsValid() && State->HasFact(Story);
}

void UWorldStateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this))
	{
		BeforeSaveHandle = State->OnBeforeSave.AddUObject(this, &UWorldStateSubsystem::Capture);
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UWorldStateSubsystem> Weak(this);
		// Magnitude 1 = loaded from a file (0 = ResetAll / new game).
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_State_Loaded, [Weak](const FGameEvent& E)
		{
			if (!Weak.IsValid()) { return; }
			if (FMurdarWorldState* W = Weak->Data()) { W->bPendingApply = E.Magnitude > 0.5f; }
		}));
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Chapter_Entered, [Weak](const FGameEvent&)
		{
			// Next tick: the director has just placed him and spawned the chapter car in the same call.
			if (Weak.IsValid()) { Weak->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(Weak.Get(), &UWorldStateSubsystem::Apply)); }
		}));
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Player_EnteredVehicle, [Weak](const FGameEvent& E)
		{
			if (Weak.IsValid()) { Weak->PlayerCar = const_cast<AMurdarVehicle*>(Cast<AMurdarVehicle>(E.Source.Get())); }
		}));
	}
}

void UWorldStateSubsystem::Deinitialize()
{
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this)) { State->OnBeforeSave.Remove(BeforeSaveHandle); }
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		for (const int32 H : BusHandles) { Bus->Unsubscribe(H); }
	}
	BusHandles.Reset();
	Registered.Reset();
	Super::Deinitialize();
}

void UWorldStateSubsystem::Register(UWorldStateComponent* Component) { if (Component) { Registered.AddUnique(Component); } }
void UWorldStateSubsystem::Unregister(UWorldStateComponent* Component) { Registered.RemoveSingleSwap(Component); }

void UWorldStateSubsystem::NotifyDestroyed(UWorldStateComponent* Component)
{
	if (FMurdarWorldState* W = Data())
	{
		FWorldActorRecord& R = W->Upsert(MapName(), Component->GetStableId());
		R.bDestroyed = true;
		R.bHasTransform = R.bHasValue = false;
	}
}

void UWorldStateSubsystem::Capture()
{
	FMurdarWorldState* W = Data();
	if (!W) { return; }
	const FName Map = MapName();
	W->Map = Map;

	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AMurdarVehicle* DrivenNow = Cast<AMurdarVehicle>(const_cast<APawn*>(Pawn));
	if (DrivenNow) { PlayerCar = DrivenNow; }
	W->bHasPlayer = Pawn != nullptr;
	W->bPlayerInCar = DrivenNow != nullptr;
	if (Pawn) { W->Player = FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Pawn->GetActorLocation()); }

	W->Car = FPlayerCarRecord();
	if (const AMurdarVehicle* Car = PlayerCar.Get())
	{
		W->Car.bValid = true;
		W->Car.VehicleClass = FSoftClassPath(Car->GetClass());
		W->Car.Definition = FSoftObjectPath(Car->Definition.Get());
		W->Car.Transform = Car->GetActorTransform();
		W->Car.Damage01 = Car->GetDamage();
		W->Car.Paint = Car->GetPaintColor(); // Handoff/Garage patch 1
	}

	for (const TWeakObjectPtr<UWorldStateComponent>& Weak : Registered)
	{
		const UWorldStateComponent* C = Weak.Get();
		if (!C || !C->GetOwner()) { continue; }
		FWorldActorRecord& R = W->Upsert(Map, C->GetStableId());
		if (R.bDestroyed) { continue; } // sticky
		if (C->bSaveTransform) { R.bHasTransform = true; R.Transform = C->GetOwner()->GetActorTransform(); }
		if (C->bSaveValue) { R.bHasValue = true; R.Value = C->Value; }
	}
}

AMurdarVehicle* UWorldStateSubsystem::RestoreCar(const FMurdarWorldState& W)
{
	const float KillZ = GetWorld()->GetWorldSettings() ? GetWorld()->GetWorldSettings()->KillZ : -1e6f;
	if (!W.Car.bValid || !MurdarWorld::IsPlaceable(W.Car.Transform.GetLocation().Z, KillZ)) { return nullptr; }
	UClass* Class = W.Car.VehicleClass.TryLoadClass<AMurdarVehicle>();
	UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(GetWorld());
	if (!Class || !Vehicles) { return nullptr; }

	// The chapter may have spawned one of this class at the checkpoint: move that one instead of making a second.
	AMurdarVehicle* Car = nullptr;
	for (const TWeakObjectPtr<AMurdarVehicle>& V : Vehicles->GetVehicles())
	{
		if (V.IsValid() && V->GetClass() == Class && !V->GetDriver()) { Car = V.Get(); break; }
	}
	if (Car)
	{
		Car->TeleportTo(W.Car.Transform.GetLocation(), W.Car.Transform.Rotator(), false, true);
	}
	else
	{
		Car = Vehicles->SpawnVehicle(Class, Cast<UVehicleDefinition>(W.Car.Definition.TryLoad()), W.Car.Transform);
	}
	if (Car) { Car->SetDamage01(W.Car.Damage01); } // README §Patches 2
	if (Car && W.Car.Paint.A > 0.f) { Car->SetPaintColor(W.Car.Paint); } // Handoff/Garage patch 1
	PlayerCar = Car;
	return Car;
}

void UWorldStateSubsystem::Apply()
{
	FMurdarWorldState* W = Data();
	const FName Map = MapName();
	if (!W || !MurdarWorld::ShouldApply(W->bPendingApply, ToStd(W->Map), ToStd(Map)))
	{
		if (W) { W->bPendingApply = false; }
		return;
	}
	W->bPendingApply = false;

	// Placed actors first (a crate back where it was before the car lands next to it).
	int32 Destroyed = 0, Restored = 0;
	for (int32 i = Registered.Num() - 1; i >= 0; --i)
	{
		UWorldStateComponent* C = Registered[i].Get();
		const FWorldActorRecord* R = C ? W->Find(Map, C->GetStableId()) : nullptr;
		const MurdarWorld::FRecord Rule = R ? ToRule(*R) : MurdarWorld::FRecord();
		switch (MurdarWorld::DecideActor(R ? &Rule : nullptr))
		{
		case MurdarWorld::EActorAction::Destroy:
			C->GetOwner()->Destroy();
			++Destroyed;
			break;
		case MurdarWorld::EActorAction::Restore:
			if (R->bHasTransform && C->bSaveTransform) { C->GetOwner()->SetActorTransform(R->Transform, false, nullptr, ETeleportType::TeleportPhysics); }
			if (R->bHasValue && C->bSaveValue) { C->Value = R->Value; C->OnRestored.Broadcast(R->Value); }
			++Restored;
			break;
		case MurdarWorld::EActorAction::Leave:
			break;
		}
	}

	const bool bStory = IsStoryMode();
	AMurdarVehicle* Car = bStory ? nullptr : RestoreCar(*W); // a story chapter keeps its own car setup
	AMurdarCharacter* Man = Cast<AMurdarCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const float KillZ = GetWorld()->GetWorldSettings() ? GetWorld()->GetWorldSettings()->KillZ : -1e6f;
	const bool bPlaceable = MurdarWorld::IsPlaceable(W->Player.GetLocation().Z, KillZ);
	switch (MurdarWorld::DecidePlayer(bStory, W->bHasPlayer && bPlaceable, W->bPlayerInCar, Car != nullptr))
	{
	case MurdarWorld::EPlayerRestore::InCar:
		if (Man && Car) { Car->Enter(Man); }
		break;
	case MurdarWorld::EPlayerRestore::OnFoot:
		if (Man)
		{
			Man->TeleportTo(W->Player.GetLocation(), W->Player.Rotator(), false, true);
			if (AController* PC = Man->GetController()) { PC->SetControlRotation(W->Player.Rotator()); }
		}
		break;
	case MurdarWorld::EPlayerRestore::Checkpoint:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("WorldState applied on %s: %d destroyed, %d restored, car %s, story %d"),
		*Map.ToString(), Destroyed, Restored, Car ? *Car->GetName() : TEXT("-"), bStory ? 1 : 0);
}

FString UWorldStateSubsystem::Describe() const
{
	const FMurdarWorldState* W = Data();
	if (!W) { return TEXT("WorldState: no narrative state"); }
	int32 Here = 0;
	for (const FWorldActorRecord& R : W->Actors) { Here += R.Map == MapName() ? 1 : 0; }
	return FString::Printf(TEXT("WorldState: saved map %s, %d records here (%d total), %d registered, car %s, player %s, pending %d"),
		*W->Map.ToString(), Here, W->Actors.Num(), Registered.Num(), W->Car.bValid ? *W->Car.VehicleClass.ToString() : TEXT("-"),
		W->bHasPlayer ? (W->bPlayerInCar ? TEXT("in car") : TEXT("on foot")) : TEXT("-"), W->bPendingApply ? 1 : 0);
}
