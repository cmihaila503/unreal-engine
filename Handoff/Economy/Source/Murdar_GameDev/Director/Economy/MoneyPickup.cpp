#include "Director/Economy/MoneyPickup.h"

#include "Director/Economy/EconomySubsystem.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Character/MurdarCharacter.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AMoneyPickup::AMoneyPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(40.f, 40.f, 40.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetRelativeScale3D(FVector(0.15f, 0.08f, 0.02f)); // placeholder "wallet" until an art asset exists
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded()) { Mesh->SetStaticMesh(CubeFinder.Object); }
}

void AMoneyPickup::BeginPlay()
{
	Super::BeginPlay();
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	if (TakenFact.IsValid() && State && State->HasFact(TakenFact))
	{
		Destroy(); // taken in an earlier session
		return;
	}
	if (LifeSeconds > 0.f) { SetLifeSpan(LifeSeconds); }
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMoneyPickup::OnTriggerBeginOverlap);
}

void AMoneyPickup::OnTriggerBeginOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	// The player on foot only: a wallet you drive over stays on the road (as for AAmmoPickup).
	const AMurdarCharacter* Character = Cast<AMurdarCharacter>(Other);
	if (!Character || !Character->IsPlayerControlled())
	{
		return;
	}
	if (UEconomySubsystem* Economy = UEconomySubsystem::Get(this))
	{
		Economy->Earn(Amount, TEXT("pickup"));
	}
	if (TakenFact.IsValid())
	{
		if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this)) { State->SetFact(TakenFact); }
	}
	if (PickupSound) { UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation()); }
	Destroy();
}
