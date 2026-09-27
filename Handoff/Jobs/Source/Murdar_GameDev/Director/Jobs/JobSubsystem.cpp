#include "Director/Jobs/JobSubsystem.h"

#include "Director/Jobs/JobPoint.h"
#include "Director/Jobs/JobSettings.h"
#include "Director/Economy/EconomySubsystem.h"         // Handoff/Economy
#include "Director/GameEventSubsystem.h"
#include "Director/Interaction/InteractableComponent.h" // Handoff/Interaction
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "AI/FactionMemorySubsystem.h"
#include "AI/MurdarAISettings.h"                        // UMurdarAILibrary::SpawnNPC
#include "Character/MurdarCharacter.h"
#include "Character/MurdarHUD.h"
#include "Vehicle/MurdarVehicle.h"
#include "Vehicle/VehicleDefinition.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MurdarJobs"

namespace
{
	constexpr float TickSeconds = 0.25f;
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	std::string ToStd(const FGameplayTag& T) { return T.IsValid() ? std::string(TCHAR_TO_UTF8(*T.GetTagName().ToString())) : std::string(); }
	MurdarJobs::FPoint ToPoint(const AJobPoint* P)
	{
		const FVector L = P->GetActorLocation();
		MurdarJobs::FPoint Out; Out.X = float(L.X); Out.Y = float(L.Y); Out.Z = float(L.Z); Out.bBorder = P->bBorder;
		return Out;
	}
}

UJobSubsystem* UJobSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UJobSubsystem>() : nullptr;
}

bool UJobSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UJobSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	GatherPoints();
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UJobSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	NextPageAt = InWorld.GetTimeSeconds() + MurdarJobs::NextPageIn(FMath::FRand(), GetDefault<UJobSettings>()->ToTuning());
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UJobSubsystem::Tick4Hz, TickSeconds, true);
}

void UJobSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Runtime.Reset();
	Super::Deinitialize();
}

void UJobSubsystem::GatherPoints()
{
	for (TActorIterator<AJobPoint> It(GetWorld()); It; ++It)
	{
		if (It->bPickup && !It->bBorder) { PickupActors.Add(*It); Pickups.push_back(ToPoint(*It)); }
		if (It->bDrop) { DropActors.Add(*It); Drops.push_back(ToPoint(*It)); }
	}
	UE_LOG(LogTemp, Log, TEXT("Jobs: %d pickups, %d drops"), PickupActors.Num(), DropActors.Num());
}

FText UJobSubsystem::PlaceOf(int32 Index, bool bDrop) const
{
	const TArray<TWeakObjectPtr<AJobPoint>>& List = bDrop ? DropActors : PickupActors;
	const AJobPoint* P = List.IsValidIndex(Index) ? List[Index].Get() : nullptr;
	return P && !P->PlaceName.IsEmpty() ? P->PlaceName : LOCTEXT("There", "locul știut");
}

void UJobSubsystem::Say(const FText& Line) const { if (AMurdarHUD* HUD = AMurdarHUD::Get(this)) { HUD->ShowSubtitle(Line); } }

void UJobSubsystem::Publish(const TCHAR* TagName, float Magnitude) const
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Publish(FGameEvent(Tag(TagName), nullptr, FVector::ZeroVector, Magnitude, Tag(TEXT("Job"))));
	}
}

void UJobSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (bInHandler) { return; }
	TGuardValue<bool> Guard(bInHandler, true);
	const bool bOurs = E.Payload == Tag(TEXT("Job"));
	// Story missions (not our own Event.Mission.* echoes) keep the pager quiet.
	if (!bOurs && E.Tag == Tag(TEXT("Event.Mission.Started"))) { ++StoryMissions; }
	if (!bOurs && (E.Tag == Tag(TEXT("Event.Mission.Succeeded")) || E.Tag == Tag(TEXT("Event.Mission.Failed")))) { StoryMissions = FMath::Max(0, StoryMissions - 1); }

	if (E.Tag == Tag(TEXT("Event.Interact")) && E.Payload.MatchesTag(Tag(TEXT("Interact.Payphone"))))
	{
		if (IsRunning()) { Say(LOCTEXT("Busy", "Ai deja o treabă.")); }
		else if (PendingContact >= 0) { StartJob(PendingContact); }
		else { Say(LOCTEXT("NoOne", "Nu răspunde nimeni.")); }
		return;
	}
	if (E.Tag == Tag(TEXT("Event.Interact")) && E.Payload == Tag(TEXT("Interact.Job.Debtor")) && IsRunning())
	{
		const bool bCaught = bDebtorAsked; // asked once and he ran: this time he's cornered
		bDebtorAsked = true;
		if (MurdarJobs::DebtorPays(bCaught, FMath::FRand(), GetDefault<UJobSettings>()->ToTuning()))
		{
			Say(LOCTEXT("Pays", "Bine, bine… Uite banii. Spune-i că-i dau restul săptămâna viitoare."));
			Publish(TEXT("Event.Job.Collected"));                  // for others (our own handler is guarded)...
			Apply(Runtime->OnEvent("Event.Job.Collected", "")); // ...so the runtime hears it directly
		}
		else
		{
			// ADAPT: make the debtor flee (UPedestrianComponent panic away from the player) if that is exposed.
			Say(LOCTEXT("Runs", "N-am bani! Lasă-mă-n pace!"));
		}
	}
	if (Runtime.IsValid()) { Apply(Runtime->OnEvent(ToStd(E.Tag), ToStd(E.Payload))); }
}

void UJobSubsystem::PageNow()
{
	NextPageAt = GetWorld()->GetTimeSeconds();
}

void UJobSubsystem::TickPager(float Now)
{
	const UJobSettings* S = GetDefault<UJobSettings>();
	if (PendingContact >= 0 && Now > PageExpiresAt) { PendingContact = -1; } // nobody called back
	if (PendingContact >= 0 || Now < NextPageAt || S->Contacts.Num() == 0) { return; }
	const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld());
	if (!MurdarJobs::CanPage(IsRunning(), StoryMissions > 0, Mem ? int(Mem->GetWantedLevel()) : 0))
	{
		NextPageAt = Now + 30.f; // try again soon
		return;
	}
	// A contact he knows.
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	TArray<int32> Known;
	for (int32 i = 0; i < S->Contacts.Num(); ++i)
	{
		const FGameplayTag& F = S->Contacts[i].RequiredFact;
		if (!F.IsValid() || (State && State->HasFact(F))) { Known.Add(i); }
	}
	NextPageAt = Now + MurdarJobs::NextPageIn(FMath::FRand(), S->ToTuning());
	if (Known.Num() == 0) { return; }
	PendingContact = Known[FMath::RandHelper(Known.Num())];
	PageExpiresAt = Now + S->PageExpirySeconds;
	if (USoundBase* Beep = S->PagerBeep.LoadSynchronous()) { UGameplayStatics::PlaySound2D(this, Beep); }
	Say(S->Contacts[PendingContact].Page);
	Publish(TEXT("Event.Job.Paged"), float(PendingContact));
}

bool UJobSubsystem::StartJob(int32 ContactIndex, int32 Kind)
{
	const UJobSettings* S = GetDefault<UJobSettings>();
	if (IsRunning() || !S->Contacts.IsValidIndex(ContactIndex)) { return false; }
	const FJobContact& C = S->Contacts[ContactIndex];
	if (Kind < 0) { Kind = C.Kinds.Num() > 0 ? int32(C.Kinds[FMath::RandHelper(C.Kinds.Num())]) : 0; }
	if (Kind == int32(EJobKind::CarDelivery) && S->WantedCars.Num() == 0) { Kind = int32(EJobKind::Delivery); }

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	MurdarJobs::FPoint Here;
	if (Player) { const FVector L = Player->GetActorLocation(); Here.X = float(L.X); Here.Y = float(L.Y); Here.Z = float(L.Z); }
	Job = MurdarJobs::Generate(MurdarJobs::EKind(Kind), Pickups, Drops, Here, FMath::FRand(), FMath::FRand(), C.PayMultiplier, S->ToTuning());
	PendingContact = -1;
	if (!Job.bValid)
	{
		Say(FText::Format(LOCTEXT("Nothing", "{0}: N-am nimic pentru tine azi."), C.Name));
		return false;
	}
	JobContact = ContactIndex;
	bDebtorAsked = false;
	WantedCar = Job.Kind == MurdarJobs::EKind::CarDelivery ? S->WantedCars[FMath::RandHelper(S->WantedCars.Num())] : nullptr;
	Runtime = MakeUnique<MurdarMission::FMissionRuntime>(MurdarJobs::ToSpec(Job, Pickups, Drops));
	if (Job.Kind == MurdarJobs::EKind::Collection) { SpawnDebtor(); }
	Publish(TEXT("Event.Mission.Started"));
	Apply(Runtime->Start(GetWorld()->GetTimeSeconds()));
	return true;
}

void UJobSubsystem::SpawnDebtor()
{
	const AJobPoint* P = PickupActors.IsValidIndex(Job.Pickup) ? PickupActors[Job.Pickup].Get() : nullptr;
	if (!P) { return; }
	AMurdarCharacter* Man = UMurdarAILibrary::SpawnNPC(this, ENPCFaction::Civilian, P->GetActorTransform());
	if (!Man) { return; }
	UInteractableComponent* Ask = NewObject<UInteractableComponent>(Man, TEXT("JobDebtor"));
	Ask->Verb = LOCTEXT("Ask", "Cere banii");
	Ask->InteractionTag = Tag(TEXT("Interact.Job.Debtor"));
	Ask->RangeCm = 250.f;
	Ask->Priority = 2.f;
	Ask->RegisterComponent(); // registers with UInteractionSubsystem in its BeginPlay
	Debtor = Man;
}

bool UJobSubsystem::InTargetCar() const
{
	const AMurdarVehicle* Car = Cast<AMurdarVehicle>(UGameplayStatics::GetPlayerPawn(this, 0));
	return Car && !WantedCar.IsNull() && Car->Definition && FSoftObjectPath(Car->Definition.Get()) == WantedCar.ToSoftObjectPath();
}

void UJobSubsystem::Tick4Hz()
{
	const float Now = GetWorld()->GetTimeSeconds();
	TickPager(Now);
	if (!IsRunning()) { return; }
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector L = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	auto HasFact = [this, State](const std::string& F)
	{
		if (F == "Job.InTargetCar") { return InTargetCar(); }
		const FGameplayTag T = FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(F.c_str())), false);
		return State && T.IsValid() && State->HasFact(T);
	};
	Apply(Runtime->Tick(Now, { float(L.X), float(L.Y), float(L.Z) }, HasFact));
}

void UJobSubsystem::Apply(const std::vector<MurdarMission::FStep>& Steps)
{
	const UJobSettings* S = GetDefault<UJobSettings>();
	const FText Contact = S->Contacts.IsValidIndex(JobContact) ? S->Contacts[JobContact].Name : FText::GetEmpty();
	const UVehicleDefinition* Want = WantedCar.LoadSynchronous();
	const FText Model = Want && !Want->DisplayName.IsEmpty() ? Want->DisplayName : LOCTEXT("ACar", "o mașină");
	for (const MurdarMission::FStep& Step : Steps)
	{
		switch (Step.Kind)
		{
		case MurdarMission::EStepKind::ObjectiveStarted:
		{
			const std::string& Id = Runtime->GetSpec().Objectives[Step.Objective].Id;
			if (Id == "pickup") { Say(FText::Format(LOCTEXT("Pickup", "{0}: Ia pachetul de la {1}. Și nu-l deschide."), Contact, PlaceOf(Job.Pickup, false))); }
			else if (Id == "drop" && Job.Kind == MurdarJobs::EKind::Smuggling) { Say(FText::Format(LOCTEXT("Smuggle", "Du-l la {0}. Fără poliție pe urme, altfel nu vine nimeni."), PlaceOf(Job.Drop, true))); }
			else if (Id == "drop" && Job.Kind == MurdarJobs::EKind::CarDelivery) { Say(FText::Format(LOCTEXT("CarDrop", "Adu-l la {0}. Întreg, dacă se poate."), PlaceOf(Job.Drop, true))); }
			else if (Id == "drop") { Say(FText::Format(LOCTEXT("Drop", "Du-l la {0}."), PlaceOf(Job.Drop, true))); }
			else if (Id == "get_car") { Say(FText::Format(LOCTEXT("GetCar", "{0}: Am un client care vrea {1}. Găsește-mi unul."), Contact, Model)); }
			else if (Id == "collect") { Say(FText::Format(LOCTEXT("Collect", "{0}: Un tip de la {1} îmi datorează {2} lei. Ia-i."), Contact, PlaceOf(Job.Pickup, false), FText::AsNumber(FMath::RoundToInt(Job.Pay * 2.f)))); }
			break;
		}
		case MurdarMission::EStepKind::ObjectiveCompleted:
			break;
		case MurdarMission::EStepKind::Succeeded:
			Finish(true, TEXT("done"));
			break;
		case MurdarMission::EStepKind::Failed:
			Finish(false, FString(UTF8_TO_TCHAR(Step.Reason.c_str())));
			break;
		}
	}
}

void UJobSubsystem::Finish(bool bSuccess, const FString& Reason)
{
	float Pay = Job.Pay;
	if (bSuccess && Job.Kind == MurdarJobs::EKind::CarDelivery)
	{
		const AMurdarVehicle* Car = Cast<AMurdarVehicle>(UGameplayStatics::GetPlayerPawn(this, 0));
		Pay = MurdarJobs::CarDeliveryPay(Pay, Car ? Car->GetDamage() : 1.f);
	}
	if (bSuccess)
	{
		if (UEconomySubsystem* Economy = UEconomySubsystem::Get(this)) { Economy->Earn(Pay, TEXT("job")); }
		Say(FText::Format(LOCTEXT("Paid", "Bravo. Ai {0} lei."), FText::AsNumber(FMath::RoundToInt(Pay))));
		Publish(TEXT("Event.Mission.Succeeded"), Pay);
	}
	else
	{
		Say(LOCTEXT("Failed", "Ai stricat treaba. Nu mai suna azi."));
		Publish(TEXT("Event.Mission.Failed"));
		NextPageAt = GetWorld()->GetTimeSeconds() + GetDefault<UJobSettings>()->PageSeconds.Y; // the contact sulks
	}
	UE_LOG(LogTemp, Log, TEXT("Job %s: %s (%.0f lei)"), bSuccess ? TEXT("done") : TEXT("failed"), *Reason, bSuccess ? Pay : 0.f);
	if (APawn* D = Debtor.Get()) { D->SetLifeSpan(30.f); }
	Debtor.Reset();
}

void UJobSubsystem::AbortJob(const FString& Reason)
{
	if (Runtime.IsValid()) { Apply(Runtime->Abort(TCHAR_TO_UTF8(*Reason))); }
}

FString UJobSubsystem::Describe() const
{
	const float Now = GetWorld()->GetTimeSeconds();
	return FString::Printf(TEXT("Jobs: %d pickups, %d drops | %s | page %s | next page in %.0f s"),
		PickupActors.Num(), DropActors.Num(),
		IsRunning() ? *FString::Printf(TEXT("RUNNING kind %d objective %d pay %.0f"), int32(Job.Kind), Runtime->GetObjective(), Job.Pay) : TEXT("idle"),
		PendingContact >= 0 ? *FString::Printf(TEXT("pending (contact %d, %.0f s left)"), PendingContact, PageExpiresAt - Now) : TEXT("-"),
		FMath::Max(0.f, NextPageAt - Now));
}

#undef LOCTEXT_NAMESPACE
