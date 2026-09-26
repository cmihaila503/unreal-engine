# 03 — A3: StandDown ignores new crimes ("paid is paid")

## Problem

ARCHITECTURE §3, StandDown row: "Lethal does **not** interrupt (paid is paid)". A bribe buys off what already
happened; as written it also buys *StandDownSeconds* of immunity for whatever the player does next, including
shooting the unit that was just paid. That is an exploit, not a rule.

## Rule to implement

StandDown is interrupted by a crime that is **(a)** reported after the bribe **and** **(b)** either more severe
than the worst crime the bribe covered, or committed against this unit (its crew or its car).
Everything else keeps today's behaviour (a bribed unit does not re-engage for heat alone).

## Change 1 — severity table (world rule, not personality → UMurdarAISettings)

```cpp
// UMurdarAISettings (DeveloperSettings, DefaultGame.ini)

/** Relative severity of each crime tag, used to decide whether a new crime is covered by a bribe
 *  (StandDown) and, later, by escalation (spec §36). Higher = worse. Unlisted tags count as 0. */
UPROPERTY(EditAnywhere, config, Category = "Police", meta = (Categories = "Crime"))
TMap<FGameplayTag, int32> CrimeSeverity;
```

`Config/DefaultGame.ini`, under `[/Script/Murdar_GameDev.MurdarAISettings]` (hand edit; tags from RECON §3.5):

```ini
+CrimeSeverity=(("Crime.Speeding", 1))
+CrimeSeverity=(("Crime.Reckless", 2))
+CrimeSeverity=(("Crime.Evading", 3))
+CrimeSeverity=(("Crime.HitPedestrian", 4))
+CrimeSeverity=(("Crime.Weapon", 4))
+CrimeSeverity=(("Crime.Assault", 5))
+CrimeSeverity=(("Crime.HitPolice", 6))
+CrimeSeverity=(("Crime.Shooting", 7))
+CrimeSeverity=(("Crime.Murder", 8))
```

ADAPT: check the exact ini syntax for a `TMap<FGameplayTag,int32>` by setting one entry in Project Settings and
reading what the editor writes to `DefaultGame.ini`; copy that form. (Setting it through the editor UI is fine too.)

## Change 2 — the controller

```cpp
// AMurdarPoliceAIController.h (private)
double BribeTime = -1.0;
int32 BribeCoveredSeverity = 0;
bool bStandDownBroken = false;
FString StandDownBreakReason;
```

- On entering StandDown (the bribe / ticket path): `BribeTime = Now`; `BribeCoveredSeverity` = the max severity of
  the crimes in the faction memory's crime list (RECON §3.3, 64 kept) with a time ≤ now. ADAPT: crime record
  field names.
- Subscribe to `Crime` (the parent tag — the bus matches the hierarchy, RECON §3.5) in `OnPossess`; ADAPT the
  unsubscribe (whatever handle `Subscribe` returns) in `OnUnPossess` / `EndPlay` — see 04.

```cpp
void AMurdarPoliceAIController::OnCrimeEvent(const FGameEvent& Ev)
{
    if (CurrentState != EPoliceState::StandDown) // ADAPT: state member name
    {
        return;
    }
    const UMurdarAISettings* S = GetDefault<UMurdarAISettings>();
    const int32* Sev = S->CrimeSeverity.Find(Ev.Tag);
    const int32 Severity = Sev ? *Sev : 0;

    // ADAPT: how a crime names its victim. Payload/Source per FGameEvent {Tag, Source, Location, Magnitude, Payload}.
    const bool bAgainstUs = IsOwnCrewOrCar(Ev /* victim */);

    if (bAgainstUs || Severity > BribeCoveredSeverity)
    {
        bStandDownBroken = true;
        StandDownBreakReason = FString::Printf(TEXT("%s after bribe (sev %d > %d%s)"),
            *Ev.Tag.ToString(), Severity, BribeCoveredSeverity, bAgainstUs ? TEXT(", against us") : TEXT(""));
    }
}
```

- In `Decide()`, StandDown case: if `bStandDownBroken`, clear it and let the normal wanted-level decision run
  (Pursuit / Combat by the current wanted level and contact) with `Pick(…, StandDownBreakReason)`. The bribe
  record in the memory stays (other crews/sectors keep it); only this crew's immunity ends.
- Do the decision in `Decide()`, not in the bus callback — the callback only records (keeps the 4 Hz brain the
  single place that changes state).

## Tests

1. Bribe, then `MurdarHeat` to Lethal with no new crime → unit stays in StandDown (unchanged behaviour).
2. Bribe after speeding, then commit `Crime.Shooting` in view → StandDown → Combat within ≤ 0.5 s; trace reason
   names the tag.
3. Bribe after a shooting (covered severity 7), then speed → stays in StandDown.
4. Bribe, then hit the paid unit's car (`Crime.HitPolice` or `Event.Vehicle.Crashed` with this car — ADAPT which
   one the project raises) → interrupts even if severity is lower than covered.
