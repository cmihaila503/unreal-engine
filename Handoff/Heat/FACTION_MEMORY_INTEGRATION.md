# Wiring the heat model into UFactionMemorySubsystem

The memory keeps owning heat — it's saved (`Stat.Heat`), mirrored into the narrative state, read by every police
brain. What changes is *how* heat moves: the arithmetic moves into `MurdarHeat::FModel` (pure, tested), the memory
feeds it. Nothing outside the memory needs to change except the civilian witness call sites (step 5).

ADAPT everything to the real member names. Do the steps in order and build after each.

## 1. Members

```cpp
// FactionMemorySubsystem.h
#include "AI/Heat/MurdarHeatModel.h"

private:
	TUniquePtr<MurdarHeat::FModel> HeatModel;
	TMap<FGameplayTag, int32> HeatCrimeTypes;   // tag -> stable id for the model's repeat tracking
	int32 HeatCrimeTypeOf(const FGameplayTag& Crime);
	bool bPoliceSearching = false;

public:
	/** Handoff 03 accessor — the existing crime table, read-only. Refactor ReportCrime to use it. */
	float GetCrimeHeat(FGameplayTag Crime) const;

	/** Called by UWitnessReportSubsystem when a civilian's report reaches the police. */
	void DeliverWitnessReport(FGameplayTag Crime, FVector Where, double CrimeTime, int32 CorroborationIndex, AActor* Witness);

	/** The director (or whoever knows) says whether any unit is in Searching — slows decay. */
	void SetPoliceSearching(bool bSearching) { bPoliceSearching = bSearching; }

	/** For MurdarHeatLog / debug. */
	const MurdarHeat::FModel* GetHeatModel() const { return HeatModel.Get(); }
```

## 2. Build the model at world start

```cpp
// Initialize / OnWorldBeginPlay
const float MaxCrimeHeat = /* ADAPT: max value of the crime table (murder, 80) */;
HeatModel = MakeUnique<MurdarHeat::FModel>(GetDefault<UMurdarHeatSettings>()->ToConfig(
	HeatStop, HeatPursuit, HeatLethal, MaxCrimeHeat));      // ADAPT: the existing threshold members (12/40/75)
// Restore: after the narrative state is loaded, HeatModel->SetHeat(LoadedStatHeat, Now, TEXT("load")).
```

## 3. Replace the heat arithmetic

| Today (ADAPT: find it) | After |
|---|---|
| `Heat += Table[Crime]` in `ReportCrime` (police-sourced) | `HeatModel->AddCrime(HeatCrimeTypeOf(Crime), GetCrimeHeat(Crime), Now, Crime.ToString())` then `WitnessReports->PoliceWitnessedCrime(Crime, Where)` |
| 1 Hz tick: `if (!seen) Heat -= 0.6 * dt` | `HeatModel->Tick(Now, dt, bSeenByPolice, bPoliceSearching)` |
| sighting (police perception) | `HeatModel->NotifySighting(Now)` in addition to what it does now |
| `GetHeat()` | `return HeatModel->GetHeat();` |
| `GetWantedLevel()` from thresholds | map `HeatModel->GetWanted()` → `EWantedLevel` (None/Stop/Pursuit/Lethal) — now with hysteresis |
| `MurdarHeat` cheat / bribe setting heat | `HeatModel->SetHeat(Value, Now, TEXT("cheat"/"bribe"))` |

After each change, mirror to `Stat.Heat` and publish `Event.Police.HeatChanged` / `WantedChanged` exactly where the
code does now — compare `GetWanted()` before/after the call to know when the level changed.

Remove the old `0.6` decay constant and any local heat float: the model is the single source (spec §1.12).

## 4. DeliverWitnessReport

```cpp
void UFactionMemorySubsystem::DeliverWitnessReport(FGameplayTag Crime, FVector Where, double CrimeTime,
	int32 CorroborationIndex, AActor* Witness)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Base = GetCrimeHeat(Crime);
	if (CorroborationIndex == 0)
	{
		HeatModel->AddCrime(HeatCrimeTypeOf(Crime), Base, Now, FString::Printf(TEXT("%s (witness)"), *Crime.ToString()));
		// ADAPT: add the crime to the crime list (the 64 kept) with Time = CrimeTime, source = Witness, witnessed = true
		// ADAPT: radio fix — ReportSighting-like update of the Police FFactionTrack at Where with Time = CrimeTime,
		//        visual = false, so PredictTrack() dead-reckons from an OLD fix (the report is stale by Now − CrimeTime).
		//        Only if this fix is newer than the current track's time.
		// Known vehicle: if the crime was committed from a car the witness saw, update FKnownVehicleRecord as today.
	}
	else
	{
		HeatModel->AddCorroboration(Base, CorroborationIndex, Now, FString::Printf(TEXT("%s (corroborated)"), *Crime.ToString()));
	}
	// publish Event.Police.Crime / HeatChanged / WantedChanged as the direct path does
}
```

## 5. Civilian call sites

`grep -rn "ReportCrime(" Source/` — for every call whose reporter is a **civilian** (e.g. `AMurdarNPCAIController`
civilians reporting `Crime.HitPedestrian`), replace it with
`GetWorld()->GetSubsystem<UWitnessReportSubsystem>()->WitnessCrime(WitnessPawn, CrimeTag, CrimeLocation)`.
Police-sourced calls stay as they are (instant).

List every call site you changed in the report, with who the reporter is. If a crime reaches the memory without any
witness actor (e.g. a system event), leave it on the direct path and say so.

## 6. Search flag

`UPoliceResponseDirector` tick (2 Hz): `Memory->SetPoliceSearching(AnyUnitInState(EPoliceState::Searching))`.

## 7. Save

At the start of the narrative state's `Save()` (ADAPT: `UNarrativeStateSubsystem`), call
`World->GetSubsystem<UWitnessReportSubsystem>()->FlushForSave()` **before** `Stat.Heat` is written. Otherwise a
checkpoint autosave right after a witnessed crime + reload would erase the witness.

## 8. Debug

- `Murdar.Heat.Debug 1` (cvar, cheat): on-screen `heat 43.2 PURSUIT | pending reports 2 | decay x0.49 (severity,
  search)` + the last 5 history lines. Add it in the memory's 1 Hz tick.
- `MurdarHeatLog` (cheat): prints the model's history (time, delta, heat after, reason).
- `MurdarWitnesses` (cheat): pending reports count, per witness: crime, due in N s.
