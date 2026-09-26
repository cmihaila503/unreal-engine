# 02 — A1 + A2: recovery loop and unbounded give-up cooldown

Spec: §1.20–21, §14 ("recovery timer configurable"), §53 ("no system may stay in an infinite loop"), §58
("infinite recovery loops", "AI blocat permanent").

## Problem (from ARCHITECTURE §3)

- **A1** Recovery → (*RecoveryTimeoutSeconds*) → Disabled → (*DisabledSeconds*, upright, not wrecked) → Patrol →
  sees the same target, still in the same unreachable place → Pursuit → Recovery → … A unit disabled *by a
  recovery timeout* is by definition upright and not wrecked, so the Disabled exit lets it straight back in.
- **A2** "Patrol with the target given up (cooldown ×2 per strike)" — no ceiling, no reset.

## First: confirm it reproduces

`Tools/policetest.py`: park the player car (auto-hold) where PVA-T35 put it (impossible road), heat 60, one unit,
run 300 s, `report trace` every 30 s. The bug is confirmed if the trace shows `Disabled` more than once, or
`Recovery` entered more than `RecoveryEscapesBeforeAbandon`+1 times for the same target position. If it does
not reproduce, write down why (e.g. the give-up cooldown already outlasts the run) and still apply the cap (A2).

## Change 1 — the profile (UPoliceDrivingProfile)

```cpp
// ---- Recovery: give-up memory (A1/A2, spec §14/§53) ----

/** Ceiling for the doubling give-up cooldown. Without it the cooldown grows without bound (×2 per strike)
 *  and a unit ends up ignoring a target for the rest of the session. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery",
    meta = (Units = "s", ClampMin = "10", ClampMax = "1800"))
float GiveUpCooldownMaxSeconds = 240.f;

/** A give-up is about a place, not a person: the target has to move this far from where we gave up
 *  before the strike stops counting. Two car lengths of margin over the recovery reposition distance. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery",
    meta = (Units = "cm", ClampMin = "500", ClampMax = "10000"))
float GiveUpForgetDistanceCm = 2500.f;

/** Strikes are forgotten after this long without a new one (pursuit went fine, or nothing happened). */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery",
    meta = (Units = "s", ClampMin = "30", ClampMax = "3600"))
float GiveUpForgetSeconds = 600.f;

/** Strikes on the same target/place after which the unit stops trying and asks the director for another
 *  unit/approach instead (spec §1.21). */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery",
    meta = (ClampMin = "1", ClampMax = "10"))
int32 GiveUpMaxStrikes = 3;
```

Defaults are the same on all six profile assets unless the user wants otherwise; re-run
`Content/Python/setup_police_profiles.py` only if it sets every field explicitly (check it — if it writes the
whole asset it will reset these; if it writes only named fields the class default applies).

## Change 2 — the controller keeps a give-up record

```cpp
// AMurdarPoliceAIController.h  (private)
enum class EPoliceDisabledCause : uint8 { Damage, Rolled, RecoveryTimeout };

struct FGiveUpRecord
{
    TWeakObjectPtr<AActor> Target;   // weak: the target may be destroyed (C3)
    FVector Where = FVector::ZeroVector;
    int32 Strikes = 0;
    double LastStrikeTime = 0.0;
};

FGiveUpRecord GiveUp;                    // one is enough: a unit chases one target
EPoliceDisabledCause DisabledCause = EPoliceDisabledCause::Damage;

void RecordGiveUpStrike(AActor* Target);
float GiveUpCooldownSeconds() const;
bool IsTargetGivenUp(const AActor* Target) const;
```

```cpp
// AMurdarPoliceAIController.cpp

void AMurdarPoliceAIController::RecordGiveUpStrike(AActor* Target)
{
    const double Now = GetWorld()->GetTimeSeconds();
    const FVector Where = Target ? Target->GetActorLocation() : GiveUp.Where;
    const UPoliceDrivingProfile& P = GetProfile(); // ADAPT: the resolved-profile accessor

    const bool bSame = GiveUp.Target.Get() == Target
        && FVector::Dist2D(GiveUp.Where, Where) < P.GiveUpForgetDistanceCm
        && Now - GiveUp.LastStrikeTime < P.GiveUpForgetSeconds;

    GiveUp.Strikes = bSame ? GiveUp.Strikes + 1 : 1;
    GiveUp.Target = Target;
    GiveUp.Where = Where;
    GiveUp.LastStrikeTime = Now;
}

float AMurdarPoliceAIController::GiveUpCooldownSeconds() const
{
    const UPoliceDrivingProfile& P = GetProfile();
    // ADAPT: the existing base cooldown field (the one that is doubled today)
    const float Base = P.GiveUpCooldownSeconds;
    const int32 Doublings = FMath::Clamp(GiveUp.Strikes - 1, 0, 16); // 16: keeps Pow finite; the Min caps anyway
    return FMath::Min(Base * FMath::Pow(2.f, Doublings), P.GiveUpCooldownMaxSeconds);
}

bool AMurdarPoliceAIController::IsTargetGivenUp(const AActor* Target) const
{
    if (!Target || GiveUp.Target.Get() != Target || GiveUp.Strikes == 0)
    {
        return false;
    }
    const UPoliceDrivingProfile& P = GetProfile();
    const double Now = GetWorld()->GetTimeSeconds();
    const bool bMoved = FVector::Dist2D(GiveUp.Where, Target->GetActorLocation()) >= P.GiveUpForgetDistanceCm;
    if (bMoved || Now - GiveUp.LastStrikeTime >= P.GiveUpForgetSeconds)
    {
        return false; // he left the bad place, or it has been long enough: fair game again
    }
    return Now - GiveUp.LastStrikeTime < GiveUpCooldownSeconds();
}
```

Wire-up (ADAPT to the real code):

1. Where Recovery gives the target up today (the "×2 per strike" branch): call `RecordGiveUpStrike(Target)` and
   replace the local doubling with `GiveUpCooldownSeconds()`. Delete the old doubling variable so there is one
   source of truth (spec §1.12).
2. Where Recovery times out → Disabled: set `DisabledCause = RecoveryTimeout` **and** call
   `RecordGiveUpStrike(Target)`. Set `Damage` / `Rolled` on the other two entries.
3. In `Decide()`, every branch that would `Pick(Pursuit|Combat|Suspicion, …)` on a target: if
   `IsTargetGivenUp(Target)`, do not pick it; stay in Patrol with reason `"target given up (strike N, X s left)"`.
   Exception: the target attacks **this** unit (a crime whose source is us) — self-defence overrides.
4. When `GiveUp.Strikes >= GiveUpMaxStrikes` on entering the give-up: publish `Event.Police.BackupRequired` with
   the location (the director can send a unit from another direction — Phase 10 order), and log the decision.
5. Disabled exit: if `DisabledCause == RecoveryTimeout`, exit to Patrol (the give-up blocks re-acquiring the same
   place). Do not change the Damage/Rolled exits.
6. Telemetry: add `GiveUpStrikes` and `GiveUpSecondsLeft` to `FPoliceUnitTelemetry` and the on-screen block.

A successful pursuit end (arrest, target lost normally, StandDown) does **not** reset strikes explicitly — the
time/distance forgetting handles it; one mechanism, not two.

## Tests

- The repro above: after the fix, `Disabled` at most once in 300 s, give-up cooldown in the trace never
  > `GiveUpCooldownMaxSeconds`, `BackupRequired` published at strike `GiveUpMaxStrikes`.
- Target moves 30 m away from the impossible spot → the unit re-acquires within one Think (0.25 s) + perception.
- Regression: PVA-T08, T09, T34, T35 (Phase 5 table) must still PASS with the same numbers.
