# 03 — A3: StandDown ignores new crimes ("paid is paid")

> **Decision first (review 2026-09-26).** "Paid is paid" may be *intended*: bribery is a first-class mechanic of
> the game (PROJECT_OVERVIEW §1) and a crew that stays bought is a legitimate 1990s-Romania design. This file was
> first written as a bug fix; it is really a design question. **Ask the user before applying.** If they keep
> "paid is paid", apply only the narrow part: a crime committed *against this crew or its car* ends the deal
> (nobody stays bought while being shot at).

## Problem

ARCHITECTURE §3, StandDown row: "Lethal does **not** interrupt (paid is paid)". A bribe buys off what already
happened; as written it also buys *StandDownSeconds* of immunity for whatever the player does next, including
shooting the unit that was just paid. That is an exploit, not a rule.

## Rule to implement

StandDown is interrupted by a crime that is **(a)** reported after the bribe **and** **(b)** either more severe
than the worst crime the bribe covered, or committed against this unit (its crew or its car).
Everything else keeps today's behaviour (a bribed unit does not re-engage for heat alone).

## Change 1 — severity = the faction memory's existing crime heat table (no new table)

`UFactionMemorySubsystem::ReportCrime` already maps each crime to a heat amount, "kept deliberately on one screen"
(PROJECT_OVERVIEW §4.8): speeding 6, reckless 10, weapon brandished 20, assault 22, hit police 25, hit pedestrian 30,
evading 30, shots fired 35, murder 80, refused bribe 15. That *is* a severity scale. Adding a second table would
break spec §1.12 (one source of truth) and the two would drift.

Expose it read-only, next to where the table lives:

```cpp
// UFactionMemorySubsystem.h (public)
/** Heat a crime adds (the ReportCrime table). Used as the crime's severity by StandDown (bribe coverage) and
 *  escalation (spec §36). 0 for tags that are not crimes. */
float GetCrimeHeat(FGameplayTag CrimeTag) const;   // ADAPT: return the same value ReportCrime uses — refactor
                                                    // ReportCrime to call this, so there is one lookup
```

Order check (does "more severe" read right with these numbers?): speeding < reckless < weapon < assault < hit police
< hit pedestrian = evading < shots < murder. Hit police ranks below hit pedestrian — that's fine here, because a crime
**against this unit** interrupts StandDown regardless of severity (rule (b) above).

## Change 2 — the controller

```cpp
// AMurdarPoliceAIController.h (private)
double BribeTime = -1.0;
float BribeCoveredHeat = 0.f;
bool bStandDownBroken = false;
FString StandDownBreakReason;
```

- On entering StandDown (the bribe / ticket path): `BribeTime = Now`; `BribeCoveredHeat` = the max `GetCrimeHeat` of
  the *witnessed* crimes in the faction memory's crime list (RECON §3.3, 64 kept) with a time ≤ now. Unwitnessed
  crimes never enter that list (PROJECT_OVERVIEW §4.8) — correct: the police can't forgive what they don't know.
  ADAPT: crime record field names.
- Important: the bus event fires for a crime only if it is published for witnessed crimes. ADAPT: check whether
  `Crime.*` is published before or after the witness filter; the rule must use witnessed crimes only (a bribed cop
  who didn't see the murder has no reason to react).
- Subscribe to `Crime` (the parent tag — the bus matches the hierarchy, RECON §3.5) in `OnPossess`; unsubscribe in `OnUnPossess` / `EndPlay` — see 04. The bus supports
  unsubscribing, even from inside a callback (deferred to the end of the publish; PROJECT_OVERVIEW §4.1).

```cpp
void AMurdarPoliceAIController::OnCrimeEvent(const FGameEvent& Ev)
{
    if (CurrentState != EPoliceState::StandDown) // ADAPT: state member name
    {
        return;
    }
    const UFactionMemorySubsystem* Memory = GetWorld()->GetSubsystem<UFactionMemorySubsystem>();
    const float Heat = Memory ? Memory->GetCrimeHeat(Ev.Tag) : 0.f;

    // ADAPT: how a crime names its victim. Payload/Source per FGameEvent {Tag, Source, Location, Magnitude, Payload}.
    const bool bAgainstUs = IsOwnCrewOrCar(Ev /* victim */);

    if (bAgainstUs || Heat > BribeCoveredHeat)
    {
        bStandDownBroken = true;
        StandDownBreakReason = FString::Printf(TEXT("%s after bribe (heat %.0f > %.0f%s)"),
            *Ev.Tag.ToString(), Heat, BribeCoveredHeat, bAgainstUs ? TEXT(", against us") : TEXT(""));
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
3. Bribe after a shooting (covered 35), then speed (6) → stays in StandDown.
4. Bribe, then hit the paid unit's car (`Crime.HitPolice` or `Event.Vehicle.Crashed` with this car — ADAPT which
   one the project raises) → interrupts even if its heat is lower than covered.
