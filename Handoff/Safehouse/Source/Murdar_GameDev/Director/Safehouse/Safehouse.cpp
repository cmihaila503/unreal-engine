#include "Director/Safehouse/Safehouse.h"

#include "Director/Safehouse/SafehouseRules.h"
#include "Director/ChapterDirector.h"
#include "Director/Economy/EconomySubsystem.h"         // Handoff/Economy
#include "Director/GameEventSubsystem.h"
#include "Director/Interaction/InteractableComponent.h" // Handoff/Interaction
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Director/TimeOfDay/TimeOfDaySubsystem.h"      // Handoff/TimeOfDay
#include "AI/FactionMemorySubsystem.h"
#include "Character/MurdarCharacter.h"
#include "Character/MurdarHUD.h"
#include "Health/HealthComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MurdarSafehouse"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	void Say(const UObject* Ctx, const FText& T) { if (AMurdarHUD* HUD = AMurdarHUD::Get(Ctx)) { HUD->ShowSubtitle(T); } }
	constexpr int32 OutfitCount = 3; // ADAPT: Handoff/Disguise's outfit list length
}

ASafehouse::ASafehouse()
{
	PrimaryActorTick.bCanEverTick = false;
	Interior = CreateDefaultSubobject<UBoxComponent>(TEXT("Interior"));
	Interior->SetBoxExtent(FVector(500.f, 400.f, 150.f));
	Interior->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Interior->SetCollisionResponseToAllChannels(ECR_Ignore);
	Interior->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Interior;
	Bed = CreateDefaultSubobject<UInteractableComponent>(TEXT("Bed"));
	Stash = CreateDefaultSubobject<UInteractableComponent>(TEXT("Stash"));
	Wardrobe = CreateDefaultSubobject<UInteractableComponent>(TEXT("Wardrobe"));
	// ADAPT in the Blueprint: move each LocalPoint onto the furniture.
	Bed->LocalPoint = FVector(200.f, 0.f, 0.f);
	Stash->LocalPoint = FVector(0.f, 200.f, 0.f);
	Wardrobe->LocalPoint = FVector(-200.f, 0.f, 0.f);
	Wardrobe->Verb = LOCTEXT("Wardrobe", "Schimbă-te");
	Bed->Verb = LOCTEXT("Sleep", "Dormi");            // Refresh() rewrites these every second
	Stash->Verb = LOCTEXT("Hide", "Ascunde banii");
}

void ASafehouse::BeginPlay()
{
	Super::BeginPlay();
	Bed->OnInteracted.AddUniqueDynamic(this, &ASafehouse::OnBed);
	Stash->OnInteracted.AddUniqueDynamic(this, &ASafehouse::OnStash);
	Wardrobe->OnInteracted.AddUniqueDynamic(this, &ASafehouse::OnWardrobe);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<ASafehouse> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	GetWorldTimerManager().SetTimer(TickTimer, this, &ASafehouse::Tick1Hz, 1.f, true);
	Refresh();
}

void ASafehouse::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}

void ASafehouse::OnBusEvent(const FGameEvent& E)
{
	if (E.Tag == MurdarTags::Event_Combat_Shot) { LastShotTime = GetWorld()->GetTimeSeconds(); }
	else if (E.Tag == Tag(TEXT("Event.Mission.Started"))) { ++ActiveMissions; }
	else if (E.Tag == Tag(TEXT("Event.Mission.Succeeded")) || E.Tag == Tag(TEXT("Event.Mission.Failed"))) { ActiveMissions = FMath::Max(0, ActiveMissions - 1); }
}

bool ASafehouse::IsOwned() const
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return !OwnedFact.IsValid() || (State && State->HasFact(OwnedFact));
}

bool ASafehouse::IsPlayerInside() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC && PC->GetPawn() && Interior->IsOverlappingActor(PC->GetPawn());
}

void ASafehouse::Refresh()
{
	const bool bOwned = IsOwned();
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const float Stashed = State ? State->GetValue(Tag(TEXT("Stat.MoneyStash"))) : 0.f;
	const float Cash = State ? State->GetValue(Tag(TEXT("Stat.Money"))) : 0.f;
	Bed->Verb = bOwned ? LOCTEXT("Sleep", "Dormi")
		: FText::Format(LOCTEXT("Buy", "Cumpără {0} ({1} lei)"), DisplayName, FText::AsNumber(FMath::RoundToInt(Price)));
	Stash->bEnabled = bOwned && (Cash > 0.f || Stashed > 0.f);
	// Put away what's on you; with empty pockets, take it back.
	Stash->Verb = Cash > 0.f ? LOCTEXT("Hide", "Ascunde banii") : LOCTEXT("Take", "Ia banii ascunși");
	Wardrobe->bEnabled = bOwned;
}

void ASafehouse::Tick1Hz()
{
	Refresh();
	// Hiding: inside, unseen for a while — the heat goes down faster.
	UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	if (!Mem || !IsOwned() || Mem->GetHeat() <= 0.f) { return; }
	const float Extra = MurdarSafehouse::HideExtraDecay(IsPlayerInside(), Mem->TimeSinceLastSighting(), Mem->HeatDecayPerSecond, MurdarSafehouse::FTuning());
	if (Extra > 0.f) { Mem->SetHeat(FMath::Max(0.f, Mem->GetHeat() - Extra)); }
}

void ASafehouse::OnBed(UInteractableComponent*, APawn*)
{
	if (bSleeping) { return; }
	if (!IsOwned())
	{
		UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
		if (Economy && Economy->Spend(Price, TEXT("safehouse")))
		{
			if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this)) { State->SetFact(OwnedFact); }
			Say(this, FText::Format(LOCTEXT("Bought", "{0} e a ta."), DisplayName));
		}
		else { Say(this, LOCTEXT("NoMoney", "N-ai destui bani.")); }
		Refresh();
		return;
	}

	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	const MurdarSafehouse::FTuning T;
	switch (MurdarSafehouse::CanSleep(Mem ? int(Mem->GetWantedLevel()) : 0, GetWorld()->GetTimeSeconds() - LastShotTime, ActiveMissions > 0, T))
	{
	case MurdarSafehouse::EBlock::Wanted:  Say(this, LOCTEXT("Wanted", "Nu dormi cu poliția pe urme.")); return;
	case MurdarSafehouse::EBlock::Combat:  Say(this, LOCTEXT("Combat", "Nu acum.")); return;
	case MurdarSafehouse::EBlock::Mission: Say(this, LOCTEXT("Mission", "Ai o treabă de terminat.")); return;
	case MurdarSafehouse::EBlock::None:    break;
	}
	const UTimeOfDaySubsystem* Time = UTimeOfDaySubsystem::Get(this);
	const float Hours = MurdarSafehouse::SleepHours(Time ? Time->GetHour() : 12.f, T);
	bSleeping = true;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		if (PC->GetPawn()) { PC->GetPawn()->DisableInput(PC); }
		if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 1.f, FLinearColor::Black, false, true); }
	}
	GetWorldTimerManager().SetTimer(SleepTimer, FTimerDelegate::CreateUObject(this, &ASafehouse::WakeUp, Hours), 1.5f, false);
}

void ASafehouse::WakeUp(float Hours)
{
	bSleeping = false;
	if (UTimeOfDaySubsystem* Time = UTimeOfDaySubsystem::Get(this)) { Time->SkipHours(Hours); }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (AMurdarCharacter* Man = PC ? Cast<AMurdarCharacter>(PC->GetPawn()) : nullptr)
	{
		if (UHealthComponent* H = Man->GetHealthComponent()) { H->Heal(H->MaxHealth); }
		Man->EnableInput(PC);
	}
	// Sleeping saves (as MurdarSave / the pause menu do).
	if (UChapterDirector* Director = UChapterDirector::Get(GetWorld())) { Director->CaptureLoadout(); }
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this)) { State->Save(TEXT("manual")); }
	if (PC && PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.5f, FLinearColor::Black, false, false); }
	if (const UTimeOfDaySubsystem* Time = UTimeOfDaySubsystem::Get(this))
	{
		const int32 H = FMath::FloorToInt(Time->GetHour()), M = FMath::FloorToInt(FMath::Fmod(Time->GetHour(), 1.f) * 60.f);
		Say(this, FText::FromString(FString::Printf(TEXT("%02d:%02d"), H, M)));
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Publish(FGameEvent(Tag(TEXT("Event.Safehouse.Slept")), this, GetActorLocation(), Hours)); }
}

void ASafehouse::OnStash(UInteractableComponent*, APawn*)
{
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
	if (!State || !Economy) { return; }
	const FGameplayTag StashTag = Tag(TEXT("Stat.MoneyStash"));
	MurdarSafehouse::FMoney M{ Economy->GetCash(), State->GetValue(StashTag) };
	const MurdarSafehouse::FMoney After = M.Cash > 0.f ? MurdarSafehouse::Deposit(M) : MurdarSafehouse::Withdraw(M);
	// Through the economy, so Event.Economy.Changed is published for the pocket side.
	if (After.Cash < M.Cash) { Economy->Take(M.Cash - After.Cash, TEXT("stash")); }
	else if (After.Cash > M.Cash) { Economy->Earn(After.Cash - M.Cash, TEXT("stash")); }
	State->SetValue(StashTag, After.Stash);
	Say(this, FText::Format(LOCTEXT("Stashed", "Ascunși: {0} lei."), FText::AsNumber(FMath::RoundToInt(After.Stash))));
	Refresh();
}

void ASafehouse::OnWardrobe(UInteractableComponent*, APawn*)
{
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	if (!State) { return; }
	const FGameplayTag OutfitTag = Tag(TEXT("Stat.Outfit"));
	const int32 Next = MurdarSafehouse::NextOutfit(FMath::RoundToInt(State->GetValue(OutfitTag)), OutfitCount);
	State->SetValue(OutfitTag, float(Next));
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Publish(FGameEvent(Tag(TEXT("Event.Player.OutfitChanged")), this, GetActorLocation(), float(Next))); }
}

#undef LOCTEXT_NAMESPACE
