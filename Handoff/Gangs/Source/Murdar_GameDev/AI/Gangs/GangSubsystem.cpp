#include "AI/Gangs/GangSubsystem.h"

#include "AI/Gangs/GangMemberComponent.h"
#include "AI/Gangs/GangSettings.h"
#include "AI/MurdarAISettings.h"        // UMurdarAILibrary::SpawnNPC
#include "AI/MurdarNPCAIController.h"
#include "Character/MurdarCharacter.h"
#include "Director/Dialogue/DialogueDefinition.h"   // Handoff/Dialogue
#include "Director/Dialogue/DialogueSubsystem.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Weapons/WeaponDefinition.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	const TArray<FGangDefinition>& Gangs() { return GetDefault<UGangSettings>()->Gangs; }
}

UGangSubsystem* UGangSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGangSubsystem>() : nullptr;
}

bool UGangSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGangSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Stances.Init(MurdarGangs::EStance::Wary, Gangs().Num());
	Members.SetNum(Gangs().Num());
	for (int32 i = 0; i < Gangs().Num(); ++i) { Stances[i] = MurdarGangs::StanceOf(GetRespect(i), MurdarGangs::EStance::Wary, MurdarGangs::FTuning()); }
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UGangSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UGangSubsystem::Tick1Hz, 1.f, true);
}

void UGangSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Super::Deinitialize();
}

float UGangSubsystem::Day() const
{
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	return S ? S->GetValue(Tag(TEXT("Stat.Day"))) : 0.f;
}

float UGangSubsystem::GetRespect(int32 G) const
{
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	if (!Gangs().IsValidIndex(G) || !S) { return 0.f; }
	return S->GetValue(Gangs()[G].RespectStat, Gangs()[G].StartRespect);
}

void UGangSubsystem::AddDeed(int32 G, MurdarGangs::EDeed Deed)
{
	UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	if (!Gangs().IsValidIndex(G) || !S) { return; }
	const MurdarGangs::FTuning T;
	const float R = MurdarGangs::Apply(GetRespect(G), Deed, T);
	S->SetValue(Gangs()[G].RespectStat, R);
	const MurdarGangs::EStance Before = Stances[G];
	Stances[G] = MurdarGangs::StanceOf(R, Before, T);
	if (Stances[G] == MurdarGangs::EStance::Hostile && Before != MurdarGangs::EStance::Hostile) { TurnHostile(G); }
	UE_LOG(LogTemp, Log, TEXT("Gang %s: respect %.0f (%d)"), *Gangs()[G].Gang.ToString(), R, int32(Stances[G]));
}

int32 UGangSubsystem::GangOf(const AActor* Actor) const
{
	const UGangMemberComponent* M = Actor ? Actor->FindComponentByClass<UGangMemberComponent>() : nullptr;
	if (!M) { return -1; }
	for (int32 i = 0; i < Gangs().Num(); ++i) { if (Gangs()[i].Gang == M->Gang) { return i; } }
	return -1;
}

void UGangSubsystem::OnBusEvent(const FGameEvent& E)
{
	// Magnitude 2 = the health component saw the player do it.
	if (E.Tag == MurdarTags::Event_Actor_Died && E.Magnitude >= 2.f)
	{
		const int32 G = GangOf(E.Source.Get());
		if (G >= 0) { AddDeed(G, MurdarGangs::EDeed::KilledMember); }
		return;
	}
	if (E.Tag == Tag(TEXT("Event.Time.NewDay")))
	{
		UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
		for (int32 G = 0; S && G < Gangs().Num(); ++G) { S->SetValue(Gangs()[G].RespectStat, MurdarGangs::Drift(GetRespect(G), 1.f, MurdarGangs::FTuning())); }
		return;
	}
	// The taxa: the gang's dialogue publishes these (payload = its DialogueTag).
	const bool bPaid = E.Tag == Tag(TEXT("Event.Gang.TaxPaid")), bRefused = E.Tag == Tag(TEXT("Event.Gang.TaxRefused"));
	if (bPaid || bRefused)
	{
		for (int32 G = 0; G < Gangs().Num(); ++G)
		{
			const UDialogueDefinition* D = Gangs()[G].TaxDialogue.LoadSynchronous();
			if (!D || D->DialogueTag != E.Payload) { continue; }
			if (bPaid)
			{
				AddDeed(G, MurdarGangs::EDeed::PaidTax);
				if (UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this)) { S->SetValue(Gangs()[G].PaidUntilStat, MurdarGangs::PaidUntil(Day(), MurdarGangs::FTuning())); }
			}
			else
			{
				AddDeed(G, MurdarGangs::EDeed::HurtMember);
				AddDeed(G, MurdarGangs::EDeed::HurtMember); // a refusal in front of his mates is an insult
			}
		}
	}
}

int32 UGangSubsystem::TerritoryHere() const
{
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	for (int32 i = 0; S && i < Gangs().Num(); ++i)
	{
		for (const FGameplayTag& Z : Gangs()[i].Territory) { if (S->HasFact(Z)) { return i; } }
	}
	return -1;
}

void UGangSubsystem::Tick1Hz()
{
	const int32 Here = TerritoryHere();
	if (Here != CurrentTerritory)
	{
		CurrentTerritory = Here;
		if (Here >= 0)
		{
			const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
			const float PaidUntil = S ? S->GetValue(Gangs()[Here].PaidUntilStat) : 0.f;
			if (MurdarGangs::DemandsTax(Stances[Here], Day(), PaidUntil, FMath::FRand(), MurdarGangs::FTuning())) { AskForTax(Here); }
		}
	}
	for (int32 G = 0; G < Gangs().Num(); ++G) { KeepMembers(G); }
}

void UGangSubsystem::KeepMembers(int32 G)
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	TArray<TWeakObjectPtr<APawn>>& List = Members[G];
	List.RemoveAll([&](const TWeakObjectPtr<APawn>& W)
	{
		// Gone, or left far behind: despawn quietly.
		if (!W.IsValid()) { return true; }
		if (Player && FVector::Dist(W->GetActorLocation(), Player->GetActorLocation()) > GetDefault<UGangSettings>()->DespawnDistance) { W->Destroy(); return true; }
		return false;
	});
	if (G != CurrentTerritory || !Player) { return; }
	const UNarrativeStateSubsystem* S = UNarrativeStateSubsystem::Get(this);
	const float Minutes = S ? S->GetValue(Tag(TEXT("Stat.TimeOfDay")), 720.f) : 720.f;
	const int32 Want = MurdarGangs::MembersAround(Stances[G], Minutes < 360.f || Minutes >= 1200.f, MurdarGangs::FTuning());
	if (List.Num() < Want) { SpawnMember(G, Stances[G] == MurdarGangs::EStance::Hostile); } // one per second: they drift in
}

APawn* UGangSubsystem::SpawnMember(int32 G, bool bHostile)
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Player || !Nav) { return nullptr; }
	const FVector2D R = GetDefault<UGangSettings>()->SpawnRadius;
	FNavLocation At;
	// A few tries for a point in the ring (not on top of him).
	for (int32 Try = 0; Try < 6; ++Try)
	{
		if (Nav->GetRandomReachablePointInRadius(Player->GetActorLocation(), R.Y, At) && FVector::Dist(At.Location, Player->GetActorLocation()) >= R.X) { break; }
		At.Location = FVector::ZeroVector;
	}
	if (At.Location.IsZero()) { return nullptr; }
	const FRotator Face = (Player->GetActorLocation() - At.Location).GetSafeNormal2D().Rotation();
	AMurdarCharacter* Man = UMurdarAILibrary::SpawnNPC(this, bHostile ? ENPCFaction::Hostile : ENPCFaction::Civilian,
		FTransform(Face, At.Location + FVector(0.f, 0.f, 90.f)), bHostile ? Gangs()[G].Weapon.LoadSynchronous() : nullptr);
	if (!Man) { return nullptr; }
	UGangMemberComponent* Tagged = NewObject<UGangMemberComponent>(Man, TEXT("GangMember"));
	Tagged->Gang = Gangs()[G].Gang;
	Tagged->RegisterComponent();
	Members[G].Add(Man);
	return Man;
}

void UGangSubsystem::TurnHostile(int32 G)
{
	// The ones already about stop pretending. ADAPT: AMurdarNPCAIController needs SetFaction (README §Patches 2);
	// without it they are replaced by hostile ones as they despawn.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	for (const TWeakObjectPtr<APawn>& W : Members[G])
	{
		if (AMurdarNPCAIController* Brain = W.IsValid() ? Cast<AMurdarNPCAIController>(W->GetController()) : nullptr)
		{
			Brain->SetFaction(ENPCFaction::Hostile);
			if (PC && PC->GetPawn()) { Brain->NotifyAttacked(PC->GetPawn()); }
		}
	}
}

void UGangSubsystem::AskForTax(int32 G)
{
	UDialogueDefinition* D = Gangs()[G].TaxDialogue.LoadSynchronous();
	UDialogueSubsystem* Talk = UDialogueSubsystem::Get(this);
	if (!D || !Talk) { return; }
	// The nearest of theirs asks; if none is about yet, one walks up.
	APawn* Asker = Members[G].Num() > 0 ? Members[G][0].Get() : SpawnMember(G, false);
	if (Asker) { Talk->StartDialogue(D, Asker); }
}

FString UGangSubsystem::Describe() const
{
	static const TCHAR* S[] = { TEXT("HOSTILE"), TEXT("wary"), TEXT("friendly") };
	FString Out = FString::Printf(TEXT("Gangs: in territory %d\n"), CurrentTerritory);
	for (int32 G = 0; G < Gangs().Num(); ++G)
	{
		Out += FString::Printf(TEXT("  %-20s respect %4.0f %-8s members %d\n"), *Gangs()[G].Gang.ToString(), GetRespect(G), S[int32(Stances[G])], Members[G].Num());
	}
	return Out;
}
