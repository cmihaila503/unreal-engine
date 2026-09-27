#include "Director/Disguise/DisguiseSubsystem.h"

#include "AI/FactionMemorySubsystem.h"
#include "Character/MurdarCharacter.h"
#include "Character/MurdarHUD.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace { FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); } }

UDisguiseSubsystem* UDisguiseSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UDisguiseSubsystem>() : nullptr;
}

bool UDisguiseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

MurdarDisguise::FTuning UDisguiseSubsystem::Tuning() const
{
	MurdarDisguise::FTuning T;
	T.CloseDay = GetDefault<UDisguiseSettings>()->CloseDay;
	T.CloseNight = GetDefault<UDisguiseSettings>()->CloseNight;
	return T;
}

void UDisguiseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UDisguiseSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	ApplyLook();
}

void UDisguiseSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	Super::Deinitialize();
}

MurdarDisguise::FLookOfHim UDisguiseSubsystem::GetLook() const
{
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	MurdarDisguise::FLookOfHim L;
	if (S)
	{
		L.Outfit = FMath::RoundToInt(S->GetValue(Tag(TEXT("Stat.Outfit"))));
		L.Headwear = FMath::RoundToInt(S->GetValue(Tag(TEXT("Stat.Headwear"))));
	}
	return L;
}

bool UDisguiseSubsystem::IsRecognisable(const AActor* Witness, const APawn* Player) const
{
	if (!Witness || !Player) { return true; }
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	const float Minutes = S ? S->GetValue(Tag(TEXT("Stat.TimeOfDay")), 720.f) : 720.f;
	return MurdarDisguise::Recognise(Description, GetLook(), float(FVector::Dist(Witness->GetActorLocation(), Player->GetActorLocation())),
		Minutes < 360.f || Minutes >= 1200.f, Tuning());
}

void UDisguiseSubsystem::NoteSighting() { Description = MurdarDisguise::Describe(GetLook()); }

void UDisguiseSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (E.Tag == Tag(TEXT("Event.Player.OutfitChanged"))) { Changed(); return; }
	if (E.Tag == Tag(TEXT("Event.Economy.Bought")))
	{
		// A hat bought: owned (bit per headwear item), and put on.
		const TArray<FHeadwearLook>& H = GetDefault<UDisguiseSettings>()->Headwear;
		UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
		for (int32 i = 0; S && i < H.Num(); ++i)
		{
			if (H[i].ShopItem.IsValid() && H[i].ShopItem == E.Payload)
			{
				const uint32 Mask = uint32(S->GetValue(Tag(TEXT("Stat.HeadwearOwned")))) | (1u << i);
				S->SetValue(Tag(TEXT("Stat.HeadwearOwned")), float(Mask));
				S->SetValue(Tag(TEXT("Stat.Headwear")), float(i + 1));
				Changed();
			}
		}
	}
}

void UDisguiseSubsystem::ToggleHeadwear()
{
	UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	if (!S) { return; }
	const uint32 Mask = uint32(S->GetValue(Tag(TEXT("Stat.HeadwearOwned"))));
	const int32 Next = MurdarDisguise::NextHeadwear(GetLook().Headwear, Mask, GetDefault<UDisguiseSettings>()->Headwear.Num());
	if (Next == GetLook().Headwear) { return; }
	S->SetValue(Tag(TEXT("Stat.Headwear")), float(Next));
	Changed();
}

void UDisguiseSubsystem::Changed()
{
	ApplyLook();
	// Out of their sight, the description stops fitting at a distance, and a chase cools to a stop.
	UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	if (Mem && MurdarDisguise::ChangeCounts(Mem->TimeSinceLastSighting(), Tuning()) && Description.bValid && !(Description.Look == GetLook()))
	{
		Mem->SetHeat(MurdarDisguise::HeatAfterChange(Mem->GetHeat(), Mem->HeatStop, Mem->HeatPursuit));
	}
}

void UDisguiseSubsystem::ApplyLook()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AMurdarCharacter* Man = PC ? Cast<AMurdarCharacter>(PC->GetPawn()) : nullptr;
	if (!Man) { return; }
	const UDisguiseSettings* S = GetDefault<UDisguiseSettings>();
	const MurdarDisguise::FLookOfHim L = GetLook();
	if (S->Outfits.IsValidIndex(L.Outfit))
	{
		if (USkeletalMesh* Mesh = S->Outfits[L.Outfit].Mesh.LoadSynchronous()) { Man->GetMesh()->SetSkeletalMeshAsset(Mesh); }
	}
	if (!Hat || Hat->GetOwner() != Man)
	{
		Hat = NewObject<UStaticMeshComponent>(Man, TEXT("Headwear"));
		Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hat->RegisterComponent();
	}
	const FHeadwearLook* H = S->Headwear.IsValidIndex(L.Headwear - 1) ? &S->Headwear[L.Headwear - 1] : nullptr;
	Hat->SetStaticMesh(H ? H->Mesh.LoadSynchronous() : nullptr);
	if (H) { Hat->AttachToComponent(Man->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, H->Socket); }
}

void UDisguiseSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !Cast<AMurdarCharacter>(PC->GetPawn())) { return; } // on foot only
	for (const FKey& K : GetDefault<UDisguiseSettings>()->HeadwearKeys) { if (PC->WasInputKeyJustPressed(K)) { ToggleHeadwear(); return; } }
}

FString UDisguiseSubsystem::Describe() const
{
	const MurdarDisguise::FLookOfHim L = GetLook();
	return FString::Printf(TEXT("Disguise: wearing outfit %d headwear %d | described: %s"), L.Outfit, L.Headwear,
		Description.bValid ? *FString::Printf(TEXT("outfit %d headwear %d%s"), Description.Look.Outfit, Description.Look.Headwear, Description.Look == L ? TEXT(" (matches)") : TEXT(" (DIFFERENT)")) : TEXT("nothing"));
}
