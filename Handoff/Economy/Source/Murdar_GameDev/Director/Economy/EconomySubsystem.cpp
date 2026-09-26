#include "Director/Economy/EconomySubsystem.h"

#include "Director/Economy/EconomyRules.h"
#include "Director/Economy/EconomySettings.h"
#include "Director/Economy/MoneyPickup.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Character/MurdarCharacter.h"
#include "Character/MurdarHUD.h"
#include "Weapons/WeaponComponent.h" // ADAPT path

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	FGameplayTag MoneyTag() { static const FGameplayTag T = Tag(TEXT("Stat.Money")); return T; }
	constexpr const TCHAR* StartedFact = TEXT("Fact.Economy.Started");
}

UEconomySubsystem* UEconomySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UEconomySubsystem>() : nullptr;
}

bool UEconomySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEconomySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// New game: the starting cash, once. A loaded save brings its own Stat.Money and the fact with it.
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this); State && !State->HasFact(Tag(StartedFact)))
	{
		State->SetFact(Tag(StartedFact));
		State->SetValue(MoneyTag(), GetDefault<UEconomySettings>()->StartingCash);
	}

	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UEconomySubsystem> Weak(this);
		// The police accepted money (ReceiveBribe -> RecordBribe): now it leaves the wallet. A refused offer never gets here.
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Police_Bribed, [Weak](const FGameEvent& E)
		{
			if (Weak.IsValid()) { Weak->Take(E.Magnitude, TEXT("bribe")); }
		}));
		// Shops: a dialogue choice publishes Event.Economy.Buy.<Item>.
		BusHandles.Add(Bus->Subscribe(Tag(TEXT("Event.Economy.Buy")), [Weak](const FGameEvent& E)
		{
			if (Weak.IsValid()) { Weak->Buy(E.Tag); }
		}));
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Actor_Died, [Weak](const FGameEvent& E)
		{
			if (Weak.IsValid()) { Weak->OnDied(E); }
		}));
	}
}

void UEconomySubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		for (const int32 H : BusHandles) { Bus->Unsubscribe(H); }
	}
	BusHandles.Reset();
	Super::Deinitialize();
}

float UEconomySubsystem::GetCash() const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State ? State->GetValue(MoneyTag()) : 0.f;
}

FText UEconomySubsystem::GetCashText() const
{
	return FText::FromString(UTF8_TO_TCHAR(MurdarEconomy::FormatLei(GetCash()).c_str()));
}

int32 UEconomySubsystem::Day() const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State ? FMath::Max(0, FMath::FloorToInt(State->GetValue(Tag(TEXT("Stat.Day"))))) : 0; // Handoff/TimeOfDay
}

void UEconomySubsystem::SetCash(float NewCash, float Delta, FName Reason)
{
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	if (!State || FMath::IsNearlyZero(Delta)) { return; }
	State->SetValue(MoneyTag(), FMath::Max(0.f, NewCash));
	UE_LOG(LogTemp, Log, TEXT("Economy: %+.0f (%s) -> %.0f"), Delta, *Reason.ToString(), NewCash);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Publish(FGameEvent(Tag(TEXT("Event.Economy.Changed")), nullptr, FVector::ZeroVector, Delta));
	}
}

void UEconomySubsystem::Earn(float Amount, FName Reason)
{
	if (Amount > 0.f) { SetCash(GetCash() + Amount, Amount, Reason); }
}

bool UEconomySubsystem::Spend(float Amount, FName Reason)
{
	const MurdarEconomy::FPayResult R = MurdarEconomy::TryPay(GetCash(), Amount);
	if (R.bPaid) { SetCash(R.Cash, -Amount, Reason); }
	return R.bPaid;
}

float UEconomySubsystem::Take(float Amount, FName Reason)
{
	const float Before = GetCash();
	const MurdarEconomy::FPayResult R = MurdarEconomy::Take(Before, Amount);
	if (R.Shortfall > 0.f)
	{
		// Only a cheat (MurdarBribe) or an unguarded dialogue choice pays money he doesn't have.
		UE_LOG(LogTemp, Warning, TEXT("Economy: %s of %.0f with only %.0f in the pocket"), *Reason.ToString(), Amount, Before);
	}
	SetCash(R.Cash, R.Cash - Before, Reason);
	return Before - R.Cash;
}

float UEconomySubsystem::LoseFraction(float Fraction, float MinLoss, FName Reason)
{
	const float L = MurdarEconomy::Loss(GetCash(), Fraction, MinLoss);
	return L > 0.f ? Take(L, Reason) : 0.f;
}

float UEconomySubsystem::GetPrice(FGameplayTag Item) const
{
	const UEconomySettings* S = GetDefault<UEconomySettings>();
	const FEconomyItem* I = S->Items.Find(Item);
	return I ? MurdarEconomy::Inflated(I->BasePrice, Day(), S->DailyInflation) : -1.f;
}

bool UEconomySubsystem::Buy(FGameplayTag Item)
{
	const UEconomySettings* S = GetDefault<UEconomySettings>();
	const FEconomyItem* I = S->Items.Find(Item);
	if (!I)
	{
		UE_LOG(LogTemp, Error, TEXT("Economy: nothing called %s in Project Settings > Murdar Economy > Items"), *Item.ToString());
		return false;
	}
	const float Price = GetPrice(Item);

	// Full pockets: don't take money for rounds he can't carry.
	AMurdarCharacter* Player = Cast<AMurdarCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UWeaponComponent* Weapons = Player ? Player->GetWeaponComponent() : nullptr;
	if (I->bGivesAmmo && Weapons && Weapons->GetAmmo(I->AmmoType) >= Weapons->GetAmmoCapacity(I->AmmoType))
	{
		Say(S->AmmoFullLine);
		return false;
	}

	if (!Spend(Price, *Item.ToString()))
	{
		Say(S->CantAffordLine);
		if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
		{
			Bus->Publish(FGameEvent(Tag(TEXT("Event.Economy.CantAfford")), nullptr, FVector::ZeroVector, Price, Item));
		}
		return false;
	}

	if (I->bGivesAmmo && Weapons) { Weapons->AddAmmo(I->AmmoType, I->AmmoAmount); }
	if (!I->Weapon.IsNull() && Weapons)
	{
		if (UWeaponDefinition* Def = I->Weapon.LoadSynchronous()) { Weapons->GiveWeapon(Def, false, true); }
	}
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this))
	{
		for (const FGameplayTag& F : I->FactsOnBuy) { State->SetFact(F); }
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Publish(FGameEvent(Tag(TEXT("Event.Economy.Bought")), nullptr, FVector::ZeroVector, Price, Item));
	}
	return true;
}

void UEconomySubsystem::OnDied(const FGameEvent& E)
{
	const UEconomySettings* S = GetDefault<UEconomySettings>();
	const AActor* Dead = E.Source.Get();
	const APawn* DeadPawn = Cast<APawn>(Dead);
	if (!Dead || S->CashDropClasses.Num() == 0 || (DeadPawn && DeadPawn->IsPlayerControlled()))
	{
		return;
	}
	bool bListed = false;
	for (const TSoftClassPtr<APawn>& C : S->CashDropClasses)
	{
		if (UClass* Class = C.LoadSynchronous(); Class && Dead->IsA(Class)) { bListed = true; break; }
	}
	if (!bListed || FMath::FRand() > S->CashDropChance) { return; }

	UClass* PickupClass = S->PickupClass.IsNull() ? nullptr : S->PickupClass.LoadSynchronous();
	// Next to the body, on the ground (the body's origin is at the hips).
	const FVector At = Dead->GetActorLocation() + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), -60.f);
	const FTransform Where(FRotator(0.f, FMath::FRand() * 360.f, 0.f), At);
	// Deferred: the amount must be set before BeginPlay, or a player standing on the spot picks up the default.
	AMoneyPickup* Wallet = GetWorld()->SpawnActorDeferred<AMoneyPickup>(PickupClass ? PickupClass : AMoneyPickup::StaticClass(), Where,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Wallet)
	{
		Wallet->Amount = MurdarEconomy::PocketCash(S->CashDropMin, S->CashDropMax, FMath::FRand());
		Wallet->LifeSeconds = 180.f; // somebody else takes it
		Wallet->FinishSpawning(Where);
	}
}

void UEconomySubsystem::Say(const FText& Line) const
{
	if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->ShowSubtitle(Line); }
}

FString UEconomySubsystem::Describe() const
{
	const UEconomySettings* S = GetDefault<UEconomySettings>();
	FString Out = FString::Printf(TEXT("Economy: %s, day %d, inflation %.2f%%/day\n"), *GetCashText().ToString(), Day(), S->DailyInflation * 100.f);
	for (const TPair<FGameplayTag, FEconomyItem>& P : S->Items)
	{
		Out += FString::Printf(TEXT("  %-40s %-20s %.0f lei\n"), *P.Key.ToString(), *P.Value.Name.ToString(), GetPrice(P.Key));
	}
	return Out;
}
