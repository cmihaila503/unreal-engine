#include "Vehicle/Garage/GarageSubsystem.h"

#include "Vehicle/Garage/GarageSettings.h"
#include "Vehicle/Garage/MurdarGarage.h"
#include "Vehicle/MurdarVehicle.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Director/WorldState/WorldStateTypes.h" // Handoff/WorldState

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

namespace { FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); } }

UGarageSubsystem* UGarageSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGarageSubsystem>() : nullptr;
}

bool UGarageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGarageSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UGarageSubsystem> Weak(this);
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Player_EnteredVehicle, [Weak](const FGameEvent& E)
		{
			if (Weak.IsValid()) { Weak->LastCar = const_cast<AMurdarVehicle*>(Cast<AMurdarVehicle>(E.Source.Get())); }
		}));
		// Where he was taken: by Event.Consequence.Station he is already at the station.
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Police_Arrested, [Weak](const FGameEvent&)
		{
			const APlayerController* PC = Weak.IsValid() ? Weak->GetWorld()->GetFirstPlayerController() : nullptr;
			if (PC && PC->GetPawn()) { Weak->ArrestLocation = PC->GetPawn()->GetActorLocation(); Weak->bHasArrestLocation = true; }
		}));
		BusHandles.Add(Bus->Subscribe(Tag(TEXT("Event.Consequence.Station")), [Weak](const FGameEvent&)
		{
			if (Weak.IsValid()) { Weak->Impound(); }
		}));
	}
}

void UGarageSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { for (const int32 H : BusHandles) { Bus->Unsubscribe(H); } }
	BusHandles.Reset();
	Super::Deinitialize();
}

FMurdarWorldState* UGarageSubsystem::Records() const
{
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State ? &State->MutableWorld() : nullptr;
}

int32 UGarageSubsystem::CurrentDay() const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State ? FMath::FloorToInt(State->GetValue(Tag(TEXT("Stat.Day")))) : 0;
}

void UGarageSubsystem::RandomPaint(AMurdarVehicle* Car)
{
	const TArray<FLinearColor>& Palette = GetDefault<UGarageSettings>()->Palette;
	if (Car && Palette.Num() > 0 && Car->GetPaintColor().A <= 0.f)
	{
		Car->SetPaintColor(Palette[FMath::RandHelper(Palette.Num())]);
	}
}

void UGarageSubsystem::Impound()
{
	// The car he was taken from goes to the lot — if the map has one; otherwise it stays where it was, as today.
	AMurdarVehicle* Car = LastCar.Get();
	FMurdarWorldState* W = Records();
	const AMurdarGarage* Lot = nullptr;
	for (TActorIterator<AMurdarGarage> It(GetWorld()); It; ++It) { if (It->Kind == EGarageKind::Impound) { Lot = *It; break; } }
	if (!Car || !W || !Lot || Car->IsOccupied()) { return; }
	// Only a car he was arrested in or next to (not one parked across town).
	const bool bNear = bHasArrestLocation && FVector::Dist(ArrestLocation, Car->GetActorLocation()) <= 3000.f;
	bHasArrestLocation = false;
	if (!bNear) { return; }

	FStoredCarRecord& R = W->Impounded.AddDefaulted_GetRef();
	R.GarageId = Lot->GarageId;
	R.VehicleClass = FSoftClassPath(Car->GetClass());
	R.Definition = FSoftObjectPath(Car->Definition.Get());
	R.Paint = Car->GetPaintColor();
	R.Damage01 = Car->GetDamage();
	R.Day = CurrentDay();
	UE_LOG(LogTemp, Log, TEXT("Garage: %s impounded at %s"), *Car->GetName(), *Lot->GarageId.ToString());
	LastCar.Reset();
	Car->Destroy();
}

FString UGarageSubsystem::Describe() const
{
	const FMurdarWorldState* W = Records();
	FString Out = FString::Printf(TEXT("Garages: last car %s\n"), *GetNameSafe(LastCar.Get()));
	if (!W) { return Out; }
	for (const FStoredCarRecord& R : W->StoredCars) { Out += FString::Printf(TEXT("  stored  %-16s %s dmg %.2f\n"), *R.GarageId.ToString(), *R.Definition.GetAssetName(), R.Damage01); }
	for (const FStoredCarRecord& R : W->Impounded) { Out += FString::Printf(TEXT("  impound %-16s %s since day %d\n"), *R.GarageId.ToString(), *R.Definition.GetAssetName(), R.Day); }
	return Out;
}
