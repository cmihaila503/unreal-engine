#include "Director/Consequences/ConsequenceSubsystem.h"

#include "Director/Consequences/ConsequenceRules.h"
#include "Director/Consequences/ConsequenceSettings.h"
#include "Director/Economy/EconomySubsystem.h"      // Handoff/Economy
#include "Director/TimeOfDay/TimeOfDaySubsystem.h"  // Handoff/TimeOfDay
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "AI/FactionMemorySubsystem.h"              // ADAPT path
#include "Character/MurdarCharacter.h"
#include "Character/MurdarHUD.h"
#include "Health/HealthComponent.h"                 // ADAPT path
#include "Weapons/WeaponComponent.h"                // ADAPT path

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	constexpr float RecoverSeconds = 0.25f;

	FText Lei(float Amount)
	{
		return FText::Format(NSLOCTEXT("MurdarConsequence", "Lei", "{0} lei"), FText::AsNumber(FMath::RoundToInt(Amount)));
	}
}

UConsequenceSubsystem* UConsequenceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UConsequenceSubsystem>() : nullptr;
}

bool UConsequenceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UConsequenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UConsequenceSubsystem> Weak(this);
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Actor_Damaged, [Weak](const FGameEvent& E)
		{
			const APawn* Pawn = Cast<APawn>(E.Source.Get());
			if (Weak.IsValid() && Pawn && Pawn->IsPlayerControlled() && E.Magnitude > 0.f)
			{
				Weak->LastPlayerDamageTime = Weak->GetWorld()->GetTimeSeconds();
			}
		}));
		BusHandles.Add(Bus->Subscribe(MurdarTags::Event_Police_Arrested, [Weak](const FGameEvent&)
		{
			if (!Weak.IsValid()) { return; }
			const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(Weak->GetWorld());
			Weak->HeatAtArrest = Mem ? Mem->GetHeat() : 0.f;
		}));
		BusHandles.Add(Bus->Subscribe(Tag(TEXT("Event.Economy.Bought")), [Weak](const FGameEvent& E)
		{
			if (!Weak.IsValid() || !GetDefault<UConsequenceSettings>()->FullHealItems.HasTagExact(E.Payload)) { return; }
			const AMurdarCharacter* Player = Cast<AMurdarCharacter>(UGameplayStatics::GetPlayerPawn(Weak.Get(), 0));
			if (UHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr)
			{
				Health->Heal(Health->MaxHealth); // the doctor
			}
		}));
	}
	InWorld.GetTimerManager().SetTimer(RecoverTimer, this, &UConsequenceSubsystem::Recover, RecoverSeconds, true);
}

void UConsequenceSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		for (const int32 H : BusHandles) { Bus->Unsubscribe(H); }
	}
	BusHandles.Reset();
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Super::Deinitialize();
}

bool UConsequenceSubsystem::IsStoryMode() const
{
	// A chapter that must rewind (a scripted sequence, the prologue) sets this fact in its FactsOnEnter.
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State && State->HasFact(Tag(TEXT("Fact.Consequences.Checkpoint")));
}

APlayerStart* UConsequenceSubsystem::FindNearest(FName StartTag, const FVector& From) const
{
	TArray<APlayerStart*> Starts;
	std::vector<float> DistSq;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == StartTag || It->ActorHasTag(StartTag))
		{
			Starts.Add(*It);
			DistSq.push_back(float(FVector::DistSquared(From, It->GetActorLocation())));
		}
	}
	const int I = MurdarConsequence::PickNearest(DistSq);
	return I >= 0 ? Starts[I] : nullptr;
}

void UConsequenceSubsystem::WakeAt(AMurdarCharacter* Player, const APlayerStart* Start) const
{
	// Same as UChapterDirector::PlacePlayerAt: teleport, face the way the start faces.
	Player->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation(), false, true);
	if (AController* C = Player->GetController()) { C->SetControlRotation(Start->GetActorRotation()); }
}

void UConsequenceSubsystem::ClearHeat() const
{
	// Whatever he did is paid for. Units still chasing lose the reason to; the memory of crimes stays (witnesses).
	if (UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld())) { Mem->SetHeat(0.f); }
}

bool UConsequenceSubsystem::HandlePlayerDeath(AMurdarCharacter* Player)
{
	const UConsequenceSettings* S = GetDefault<UConsequenceSettings>();
	APlayerStart* Hospital = Player ? FindNearest(S->HospitalTag, Player->GetActorLocation()) : nullptr;
	MurdarConsequence::FContext Ctx;
	Ctx.bStoryMode = IsStoryMode();
	Ctx.bHasHospital = Hospital != nullptr;
	if (MurdarConsequence::Decide(Ctx) != MurdarConsequence::EOutcome::Hospital) { return false; }

	UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
	const MurdarConsequence::FHospital Stay = MurdarConsequence::HospitalStay(Economy ? Economy->GetCash() : 0.f, S->ToTuning());
	if (Economy && Stay.Bill > 0.f) { Economy->Take(Stay.Bill, TEXT("hospital")); }
	WakeAt(Player, Hospital);
	ClearHeat();
	if (UTimeOfDaySubsystem* Time = UTimeOfDaySubsystem::Get(this)) { Time->SkipHours(Stay.Hours); }

	FFormatNamedArguments Args;
	Args.Add(TEXT("Bill"), Lei(Stay.Bill));
	if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->ClearSubtitles(); HUD->ShowSubtitle(FText::Format(S->HospitalLine, Args), 5.f); }
	Publish(TEXT("Event.Consequence.Hospital"), Stay.Bill);
	UE_LOG(LogTemp, Log, TEXT("Consequence: hospital %s, %.0f h, bill %.0f"), *Hospital->GetName(), Stay.Hours, Stay.Bill);
	return true;
}

bool UConsequenceSubsystem::HandlePlayerArrest(AMurdarCharacter* Player)
{
	const UConsequenceSettings* S = GetDefault<UConsequenceSettings>();
	APlayerStart* Station = Player ? FindNearest(S->StationTag, Player->GetActorLocation()) : nullptr;
	MurdarConsequence::FContext Ctx;
	Ctx.bArrest = true;
	Ctx.bStoryMode = IsStoryMode();
	Ctx.bHasStation = Station != nullptr;
	if (MurdarConsequence::Decide(Ctx) != MurdarConsequence::EOutcome::Station) { return false; }

	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const FGameplayTag ArrestsTag = Tag(TEXT("Stat.Arrests"));
	const int32 Prior = State ? FMath::RoundToInt(State->GetValue(ArrestsTag)) : 0;
	UEconomySubsystem* Economy = UEconomySubsystem::Get(this);
	const MurdarConsequence::FSentence Sentence = MurdarConsequence::StationSentence(HeatAtArrest, Prior, Economy ? Economy->GetCash() : 0.f, S->ToTuning());

	if (Economy && Sentence.Fine > 0.f) { Economy->Take(Sentence.Fine, TEXT("fine")); }
	if (Sentence.bConfiscate) { Confiscate(Player); }
	if (State) { State->AddValue(ArrestsTag, 1.f); }
	WakeAt(Player, Station);
	ClearHeat();
	if (UTimeOfDaySubsystem* Time = UTimeOfDaySubsystem::Get(this)) { Time->SkipHours(Sentence.CellHours); }

	FFormatNamedArguments Args;
	Args.Add(TEXT("Hours"), FText::AsNumber(FMath::RoundToInt(Sentence.CellHours)));
	Args.Add(TEXT("Fine"), Lei(Sentence.Fine));
	if (AMurdarHUD* HUD = AMurdarHUD::Get(this))
	{
		HUD->ClearSubtitles();
		HUD->ShowSubtitle(FText::Format(S->StationLine, Args), 5.f);
		if (Sentence.bConfiscate) { HUD->ShowSubtitle(S->ConfiscatedLine, 2.5f); }
	}
	Publish(TEXT("Event.Consequence.Station"), Sentence.CellHours);
	UE_LOG(LogTemp, Log, TEXT("Consequence: station %s, heat %.0f, prior %d -> %.1f h, fine %.0f (owed %.0f), confiscate %d"),
		*Station->GetName(), HeatAtArrest, Prior, Sentence.CellHours, Sentence.Fine, Sentence.FineOwed, Sentence.bConfiscate ? 1 : 0);
	HeatAtArrest = 0.f;
	return true;
}

void UConsequenceSubsystem::Confiscate(AMurdarCharacter* Player) const
{
	UWeaponComponent* Weapons = Player ? Player->GetWeaponComponent() : nullptr;
	if (!Weapons) { return; }
	Weapons->RemoveAllWeapons(); // README §Patches 3
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this))
	{
		// The loadout records and the reserve are the truth for saves and checkpoint jumps: empty them too.
		State->SetWeapons({});
		for (TPair<EAmmoType, int32>& Ammo : State->MutableAmmo()) { Ammo.Value = 0; }
	}
}

void UConsequenceSubsystem::Recover()
{
	const AMurdarCharacter* Player = Cast<AMurdarCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
	if (!Health || Health->IsDead()) { return; }
	const float Since = GetWorld()->GetTimeSeconds() - LastPlayerDamageTime;
	const float Next = MurdarConsequence::Regenerate(Health->GetHealth(), Health->MaxHealth, Since, RecoverSeconds, GetDefault<UConsequenceSettings>()->ToRegen());
	if (Next > Health->GetHealth()) { Health->Heal(Next - Health->GetHealth()); }
}

void UConsequenceSubsystem::Publish(const TCHAR* TagName, float Magnitude) const
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Publish(FGameEvent(Tag(TagName), nullptr, FVector::ZeroVector, Magnitude));
	}
}

FString UConsequenceSubsystem::Describe() const
{
	const UConsequenceSettings* S = GetDefault<UConsequenceSettings>();
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	int32 Hospitals = 0, Stations = 0;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Hospitals += (It->PlayerStartTag == S->HospitalTag || It->ActorHasTag(S->HospitalTag)) ? 1 : 0;
		Stations += (It->PlayerStartTag == S->StationTag || It->ActorHasTag(S->StationTag)) ? 1 : 0;
	}
	return FString::Printf(TEXT("Consequences: %s mode, %d hospital(s), %d station(s), prior arrests %.0f, since last hit %.1f s"),
		IsStoryMode() ? TEXT("story (checkpoint)") : TEXT("open city"), Hospitals, Stations,
		State ? State->GetValue(Tag(TEXT("Stat.Arrests"))) : 0.f, GetWorld()->GetTimeSeconds() - LastPlayerDamageTime);
}
