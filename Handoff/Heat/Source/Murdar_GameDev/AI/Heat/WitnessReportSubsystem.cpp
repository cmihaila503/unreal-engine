#include "AI/Heat/WitnessReportSubsystem.h"

#include "AI/Heat/MurdarHeatSettings.h"
#include "AI/FactionMemorySubsystem.h"          // ADAPT: path
#include "AI/Civilians/CivilianPopulationSubsystem.h" // optional: only if the civilian population exists (see SetWitnessPinned)

#include "Engine/World.h"
#include "TimerManager.h"

bool UWitnessReportSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWitnessReportSubsystem::EnsureQueue()
{
	if (Queue)
	{
		return;
	}
	const UMurdarHeatSettings* S = GetDefault<UMurdarHeatSettings>();
	// The queue only uses the incident/corroboration fields of the config; thresholds are irrelevant to it.
	Queue = MakeUnique<MurdarHeat::FReportQueue>(S->ToConfig(0.f, 0.f, 0.f, 0.f));
}

void UWitnessReportSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnsureQueue();
	Rng.GenerateNewSeed();
	const UMurdarHeatSettings* S = GetDefault<UMurdarHeatSettings>();
	InWorld.GetTimerManager().SetTimer(UpdateTimer, this, &UWitnessReportSubsystem::Update,
		S->ReportUpdateIntervalSeconds, /*bLoop*/ true);
}

void UWitnessReportSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	Queue.Reset();
	Witnesses.Reset();
	WitnessIds.Reset();
	Super::Deinitialize();
}

uint64 UWitnessReportSubsystem::WitnessIdOf(AActor* Witness)
{
	if (const uint64* Id = WitnessIds.Find(Witness))
	{
		return *Id;
	}
	const uint64 Id = ++LastWitnessId;
	WitnessIds.Add(Witness, Id);
	Witnesses.Add(Id, Witness);
	return Id;
}

int32 UWitnessReportSubsystem::CrimeTypeOf(const FGameplayTag& Crime)
{
	if (const int32* Type = CrimeTypes.Find(Crime))
	{
		return *Type;
	}
	CrimeTags.Add(Crime);
	return CrimeTypes.Add(Crime, CrimeTags.Num());
}

bool UWitnessReportSubsystem::IsPoliceNear(const FVector& Where) const
{
	const UMurdarHeatSettings* S = GetDefault<UMurdarHeatSettings>();
	const float MaxSq = FMath::Square(S->NearbyPoliceReportDistanceCm);
	// ADAPT: the faction memory keeps a members list per faction (RECON §3.3). Iterate the Police members' pawns
	// (on foot or in a car). Replace the placeholder below with that list.
	TArray<AActor*> PoliceMembers;
	// ADAPT: GetWorld()->GetSubsystem<UFactionMemorySubsystem>()->GetMembers(EFaction::Police, PoliceMembers);
	for (const AActor* Cop : PoliceMembers)
	{
		if (IsValid(Cop) && FVector::DistSquared(Cop->GetActorLocation(), Where) <= MaxSq)
		{
			return true;
		}
	}
	return false;
}

void UWitnessReportSubsystem::SetWitnessPinned(AActor* Witness, bool bPinned) const
{
	// A witness with an unreported crime must not be despawned by the civilian population (spec CIVILIAN §4).
	// Remove this function body if the civilian population isn't in the project yet.
	if (UCivilianPopulationSubsystem* Pop = GetWorld()->GetSubsystem<UCivilianPopulationSubsystem>())
	{
		Pop->SetPinned(Cast<APawn>(Witness), bPinned);
	}
}

void UWitnessReportSubsystem::WitnessCrime(AActor* Witness, FGameplayTag Crime, FVector Where)
{
	EnsureQueue();
	UFactionMemorySubsystem* Memory = GetWorld()->GetSubsystem<UFactionMemorySubsystem>();
	if (!IsValid(Witness) || !Memory)
	{
		return;
	}
	const UMurdarHeatSettings* S = GetDefault<UMurdarHeatSettings>();
	const double Now = GetWorld()->GetTimeSeconds();

	const float BaseHeat = Memory->GetCrimeHeat(Crime); // Handoff 03: read-only accessor over the existing table
	if (BaseHeat <= 0.f)
	{
		return; // not a crime the police care about
	}

	const MurdarHeat::FVec W{float(Where.X), float(Where.Y), float(Where.Z)};
	const uint64 Incident = Queue->FindOrCreateIncident(CrimeTypeOf(Crime), W, Now);

	const float Delay = IsPoliceNear(Witness->GetActorLocation())
		? S->NearbyPoliceReportSeconds
		: Rng.FRandRange(S->ReportDelayMinSeconds, FMath::Max(S->ReportDelayMinSeconds, S->ReportDelayMaxSeconds));

	Queue->Queue(WitnessIdOf(Witness), Incident, BaseHeat, Now + Delay);
	SetWitnessPinned(Witness, true);
}

void UWitnessReportSubsystem::PoliceWitnessedCrime(FGameplayTag Crime, FVector Where)
{
	EnsureQueue();
	const MurdarHeat::FVec W{float(Where.X), float(Where.Y), float(Where.Z)};
	Queue->MarkIncidentKnown(Queue->FindOrCreateIncident(CrimeTypeOf(Crime), W, GetWorld()->GetTimeSeconds()));
}

void UWitnessReportSubsystem::WitnessReachedPolice(AActor* Witness)
{
	if (Queue)
	{
		if (const uint64* Id = WitnessIds.Find(Witness))
		{
			Queue->Expedite(*Id, GetWorld()->GetTimeSeconds());
		}
	}
}

void UWitnessReportSubsystem::SilenceWitness(AActor* Witness)
{
	if (Queue)
	{
		if (const uint64* Id = WitnessIds.Find(Witness))
		{
			Queue->CancelWitness(*Id);
		}
	}
	SetWitnessPinned(Witness, false);
}

bool UWitnessReportSubsystem::HasPendingReport(const AActor* Witness) const
{
	const uint64* Id = WitnessIds.Find(const_cast<AActor*>(Witness));
	return Queue && Id && Queue->HasPending(*Id);
}

void UWitnessReportSubsystem::Update()
{
	if (!Queue)
	{
		return;
	}
	// Dead or destroyed witnesses never report. ADAPT: "dead" — UHealthComponent on the pawn (PROJECT_OVERVIEW §4.6).
	for (auto It = Witnesses.CreateIterator(); It; ++It)
	{
		AActor* Witness = It->Value.Get();
		const bool bDead = !IsValid(Witness) /* || HealthOf(Witness)->IsDead() */; // ADAPT
		if (bDead)
		{
			Queue->CancelWitness(It->Key);
		}
	}

	Deliver(Queue->Collect(GetWorld()->GetTimeSeconds()));

	// Forget witnesses with nothing left to report (keeps both maps small).
	for (auto It = Witnesses.CreateIterator(); It; ++It)
	{
		if (!Queue->HasPending(It->Key))
		{
			WitnessIds.Remove(It->Value);
			It.RemoveCurrent();
		}
	}
}

void UWitnessReportSubsystem::FlushForSave()
{
	if (Queue)
	{
		Deliver(Queue->FlushAll(GetWorld()->GetTimeSeconds()));
	}
}

void UWitnessReportSubsystem::Deliver(const std::vector<MurdarHeat::FDelivery>& Deliveries)
{
	UFactionMemorySubsystem* Memory = GetWorld()->GetSubsystem<UFactionMemorySubsystem>();
	if (!Memory)
	{
		return;
	}
	for (const MurdarHeat::FDelivery& D : Deliveries)
	{
		if (!CrimeTags.IsValidIndex(D.CrimeType - 1))
		{
			continue;
		}
		AActor* Witness = nullptr;
		if (const TWeakObjectPtr<AActor>* W = Witnesses.Find(D.WitnessId))
		{
			Witness = W->Get();
		}
		// The memory adds heat (first report) or corroborates, and gives the police a fix of the crime's place and
		// time — an old fix, so they search where it happened, not where he is (police spec rule 17-18).
		Memory->DeliverWitnessReport(CrimeTags[D.CrimeType - 1], FVector(D.Location.X, D.Location.Y, D.Location.Z),
			D.CrimeTime, D.CorroborationIndex, Witness);

		if (Witness && !Queue->HasPending(D.WitnessId))
		{
			SetWitnessPinned(Witness, false);
		}
	}
}
