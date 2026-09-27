#include "Vehicle/Garage/MurdarGarage.h"

#include "Vehicle/Garage/GarageRules.h"
#include "Vehicle/Garage/GarageSettings.h"
#include "Vehicle/Garage/GarageSubsystem.h"
#include "Vehicle/Theft/VehicleTheftSubsystem.h"      // Handoff/VehicleTheft
#include "Vehicle/MurdarVehicle.h"
#include "Vehicle/VehicleDefinition.h"
#include "Vehicle/VehicleSubsystem.h"
#include "AI/FactionMemorySubsystem.h"
#include "Character/MurdarHUD.h"
#include "Director/Economy/EconomySubsystem.h"       // Handoff/Economy
#include "Director/GameEventSubsystem.h"
#include "Director/Interaction/InteractableComponent.h" // Handoff/Interaction
#include "Director/WorldState/WorldStateTypes.h"     // Handoff/WorldState

#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MurdarGarage"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }

	FText RecordName(const FStoredCarRecord& R)
	{
		const UVehicleDefinition* D = Cast<UVehicleDefinition>(R.Definition.TryLoad());
		return D && !D->DisplayName.IsEmpty() ? D->DisplayName : LOCTEXT("Car", "mașina");
	}

	void Say(const UObject* Ctx, const FText& Line) { if (AMurdarHUD* HUD = AMurdarHUD::Get(Ctx)) { HUD->ShowSubtitle(Line); } }
}

AMurdarGarage::AMurdarGarage()
{
	PrimaryActorTick.bCanEverTick = false;
	Bay = CreateDefaultSubobject<UBoxComponent>(TEXT("Bay"));
	Bay->SetBoxExtent(FVector(350.f, 200.f, 150.f));
	Bay->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bay->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bay->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bay->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	Bay->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	RootComponent = Bay;
	SpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(Bay);
	SpawnPoint->SetRelativeLocation(FVector(700.f, 0.f, 0.f));
	Counter = CreateDefaultSubobject<UInteractableComponent>(TEXT("Counter"));
}

void AMurdarGarage::BeginPlay()
{
	Super::BeginPlay();
	Counter->OnInteracted.AddUniqueDynamic(this, &AMurdarGarage::OnCounterUsed);
	Counter->LocalPoint = FVector(0.f, -300.f, 0.f); // ADAPT: where the counter / door is on the Blueprint
	RefreshCounter();
	GetWorldTimerManager().SetTimer(TickTimer, this, &AMurdarGarage::Tick4Hz, 0.25f, true, FMath::FRandRange(0.f, 0.25f));
}

void AMurdarGarage::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}

TArray<int32> AMurdarGarage::RecordIndices(bool bImpound) const
{
	TArray<int32> Out;
	const UGarageSubsystem* Sub = UGarageSubsystem::Get(this);
	const FMurdarWorldState* W = Sub ? Sub->Records() : nullptr;
	if (!W) { return Out; }
	const TArray<FStoredCarRecord>& List = bImpound ? W->Impounded : W->StoredCars;
	for (int32 i = 0; i < List.Num(); ++i) { if (List[i].GarageId == GarageId) { Out.Add(i); } }
	return Out;
}

void AMurdarGarage::RefreshCounter()
{
	// The counter's prompt says what it would do now; nothing to hand over = no prompt.
	if (Kind == EGarageKind::Respray) { Counter->bEnabled = false; return; }
	const TArray<int32> Idx = RecordIndices(Kind == EGarageKind::Impound);
	Counter->bEnabled = Idx.Num() > 0;
	if (!Counter->bEnabled) { return; }
	const UGarageSubsystem* Sub = UGarageSubsystem::Get(this);
	const FMurdarWorldState* W = Sub->Records();
	if (Kind == EGarageKind::Storage)
	{
		NextToRetrieve %= Idx.Num();
		Counter->Verb = FText::Format(LOCTEXT("TakeOut", "Scoate {0}"), RecordName(W->StoredCars[Idx[NextToRetrieve]]));
	}
	else
	{
		const FStoredCarRecord& R = W->Impounded[Idx[0]];
		const float Fee = MurdarGarage::ImpoundFee(Sub->CurrentDay() - R.Day, GetDefault<UGarageSettings>()->ToPrices());
		Counter->Verb = FText::Format(LOCTEXT("Pay", "Plătește și ia {0} ({1} lei)"), RecordName(R), FText::AsNumber(FMath::RoundToInt(Fee)));
	}
}

AMurdarVehicle* AMurdarGarage::SpawnFromRecord(const FStoredCarRecord& R) const
{
	UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(this);
	UClass* Class = R.VehicleClass.TryLoadClass<AMurdarVehicle>();
	if (!Vehicles || !Class) { return nullptr; }
	// The spot must be clear, or two cars end up inside each other.
	for (const TWeakObjectPtr<AMurdarVehicle>& V : Vehicles->GetVehicles())
	{
		if (V.IsValid() && FVector::Dist(V->GetActorLocation(), SpawnPoint->GetComponentLocation()) < 400.f) { return nullptr; }
	}
	AMurdarVehicle* Car = Vehicles->SpawnVehicle(Class, Cast<UVehicleDefinition>(R.Definition.TryLoad()), SpawnPoint->GetComponentTransform());
	if (Car)
	{
		if (R.Paint.A > 0.f) { Car->SetPaintColor(R.Paint); }
		Car->SetDamage01(R.Damage01);
		Car->Tags.AddUnique(TEXT("Owned"));
	}
	return Car;
}

void AMurdarGarage::OnCounterUsed(UInteractableComponent*, APawn*)
{
	UGarageSubsystem* Sub = UGarageSubsystem::Get(this);
	FMurdarWorldState* W = Sub ? Sub->Records() : nullptr;
	if (!W) { return; }
	const bool bImpound = Kind == EGarageKind::Impound;
	const TArray<int32> Idx = RecordIndices(bImpound);
	if (Idx.Num() == 0) { return; }
	TArray<FStoredCarRecord>& List = bImpound ? W->Impounded : W->StoredCars;
	const int32 Pick = bImpound ? Idx[0] : Idx[NextToRetrieve % Idx.Num()];
	const FStoredCarRecord R = List[Pick];
	// Impound: afford it, then the car comes out, then the money goes (never money for a car that couldn't come out).
	float Fee = 0.f;
	UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
	if (bImpound)
	{
		Fee = MurdarGarage::ImpoundFee(Sub->CurrentDay() - R.Day, GetDefault<UGarageSettings>()->ToPrices());
		if (!Economy || !Economy->CanAfford(Fee)) { Say(this, GetDefault<UGarageSettings>()->ResprayNoMoney); return; }
	}
	if (!SpawnFromRecord(R))
	{
		Say(this, LOCTEXT("Blocked", "Mută mașina din fața porții."));
		return;
	}
	if (bImpound) { Economy->Spend(Fee, TEXT("impound")); }
	List.RemoveAt(Pick);
	++NextToRetrieve;
	RefreshCounter();
}

void AMurdarGarage::Tick4Hz()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	AMurdarVehicle* Car = Cast<AMurdarVehicle>(Player);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Kind == EGarageKind::Respray)
	{
		const bool bInBay = Car && Bay->IsOverlappingActor(Car);
		if (!bInBay && !bJobRunning) { StoppedSince = -1.f; bJobDoneThisVisit = false; }
		if (bInBay) { TickRespray(Car, Now); }
	}
	else if (Kind == EGarageKind::Storage)
	{
		TickStorage(Player);
	}
	RefreshCounter();
}

void AMurdarGarage::TickRespray(AMurdarVehicle* Car, float Now)
{
	if (bJobRunning || bJobDoneThisVisit) { return; }
	const UGarageSettings* S = GetDefault<UGarageSettings>();
	if (Car->GetSpeedKph() > 3.f) { StoppedSince = -1.f; return; }
	if (StoppedSince < 0.f) { StoppedSince = Now; }
	if (Now - StoppedSince < S->SettleSeconds) { return; }

	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	const UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
	const bool bWanted = Mem && Mem->GetWantedLevel() != EWantedLevel::None;
	MurdarGarage::FResprayCheck C;
	C.SpeedKph = Car->GetSpeedKph();
	C.SinceSeenByPolice = Mem ? Mem->TimeSinceLastSighting() : 1e9f;
	C.Cash = Economy ? Economy->GetCash() : 0.f;
	C.Price = MurdarGarage::ResprayPrice(Car->GetDamage(), bWanted, S->ToPrices());
	bJobDoneThisVisit = true; // one answer per visit: drive out and back in to try again
	switch (MurdarGarage::CanRespray(C))
	{
	case MurdarGarage::EResprayResult::Seen:    Say(this, S->RespraySeen); return;
	case MurdarGarage::EResprayResult::NoMoney: Say(this, S->ResprayNoMoney); return;
	case MurdarGarage::EResprayResult::Moving:  bJobDoneThisVisit = false; return;
	case MurdarGarage::EResprayResult::Ok:      break;
	}
	// The door comes down: screen to black, the car is held, the job takes a couple of seconds.
	bJobRunning = true;
	JobCar = Car;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		Car->DisableInput(PC);
		if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.4f, FLinearColor::Black, false, true); }
	}
	GetWorldTimerManager().SetTimer(JobTimer, this, &AMurdarGarage::FinishRespray, S->JobSeconds, false);
}

void AMurdarGarage::FinishRespray()
{
	bJobRunning = false;
	AMurdarVehicle* Car = JobCar.Get();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.6f, FLinearColor::Black, false, false); }
	if (!Car) { return; }
	if (PC) { Car->EnableInput(PC); }

	const UGarageSettings* S = GetDefault<UGarageSettings>();
	UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	const bool bWanted = Mem && Mem->GetWantedLevel() != EWantedLevel::None;
	const float Price = MurdarGarage::ResprayPrice(Car->GetDamage(), bWanted, S->ToPrices());
	UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
	if (!Economy || !Economy->Spend(Price, TEXT("respray"))) { Say(this, S->ResprayNoMoney); return; }

	std::vector<MurdarGarage::FColor3> Palette;
	for (const FLinearColor& C : S->Palette) { Palette.push_back(UGarageSettings::ToC3(C)); }
	const int32 Pick = MurdarGarage::PickNewColour(UGarageSettings::ToC3(Car->GetPaintColor()), Palette, FMath::FRand());
	if (Pick >= 0) { Car->SetPaintColor(S->Palette[Pick]); }
	Car->SetDamage01(0.f);

	if (UVehicleTheftSubsystem* Theft = UVehicleTheftSubsystem::Get(this)) { Theft->MarkCleaned(Car); }
	if (Mem) { Mem->SetHeat(MurdarGarage::HeatAfterRespray(Mem->GetHeat(), Mem->HeatStop, Mem->HeatPursuit)); }
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Publish(FGameEvent(Tag(TEXT("Event.Garage.Resprayed")), Car, Car->GetActorLocation(), Price));
	}
	Say(this, S->ResprayDone);
}

void AMurdarGarage::TickStorage(APawn* Player)
{
	// A car he left in the bay, and he has walked out of it: it's kept.
	UGarageSubsystem* Sub = UGarageSubsystem::Get(this);
	AMurdarVehicle* Car = Sub ? Sub->GetLastCar() : nullptr;
	FMurdarWorldState* W = Sub ? Sub->Records() : nullptr;
	if (!Car || !W || !Player || Player == Car || Car->IsOccupied() || !Bay->IsOverlappingActor(Car) || Bay->IsOverlappingActor(Player)) { return; }

	std::vector<bool> Used(Slots, false);
	const int32 Here = RecordIndices(false).Num();
	for (int32 i = 0; i < Here && i < Slots; ++i) { Used[i] = true; }
	if (MurdarGarage::FreeSlot(Used) < 0)
	{
		if (LeftInBay.Get() != Car) { Say(this, GetDefault<UGarageSettings>()->GarageFull); LeftInBay = Car; }
		return;
	}
	FStoredCarRecord& R = W->StoredCars.AddDefaulted_GetRef();
	R.GarageId = GarageId;
	R.VehicleClass = FSoftClassPath(Car->GetClass());
	R.Definition = FSoftObjectPath(Car->Definition.Get());
	R.Paint = Car->GetPaintColor();
	R.Damage01 = Car->GetDamage();
	UE_LOG(LogTemp, Log, TEXT("Garage %s: stored %s"), *GarageId.ToString(), *Car->GetName());
	Car->Destroy();
}

#undef LOCTEXT_NAMESPACE
