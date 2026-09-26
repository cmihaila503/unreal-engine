# 09 — Phase 10 start: director orders, Responding, Returning

Spec §51 (Police AI gives intents; vehicle AI decides *how*), §23 (director picks roadblocks), §29 (origin/ETA),
§36 (escalation), §40 (return to patrol). ARCHITECTURE §7: the order half of `ReceiveOrder` is missing.

This file only gives the skeleton Phase 10 builds on. It does **not** complete Phase 10 (acceptance B–E, G–N).
Apply 02–04 first.

## 1. The order

```cpp
// AI/PoliceOrder.h
UENUM(BlueprintType)
enum class EPoliceIntent : uint8 { None, Respond, Pursue, Stop, Intercept, Roadblock, Search, Return };

USTRUCT(BlueprintType)
struct FPoliceOrder
{
    GENERATED_BODY()
    UPROPERTY() EPoliceIntent Intent = EPoliceIntent::None;
    UPROPERTY() TWeakObjectPtr<AActor> Target;         // may be null (Respond / Search / Return)
    UPROPERTY() FVector Location = FVector::ZeroVector; // where: crime scene, roadblock point, search centre, patrol zone
    UPROPERTY() EPoliceRole Role = EPoliceRole::None;   // ADAPT: existing enum
    UPROPERTY() float Urgency01 = 0.f;                  // spec §34; drives emergency on/off
    UPROPERTY() double IssuedTime = 0.0;
    UPROPERTY() FString Reason;                          // shown in the decision trace
    UPROPERTY() int32 OrderId = 0;                       // director-unique, so a unit can report on *this* order
};
```

## 2. Controller

```cpp
/** Director → unit. The unit may refuse (disabled, in Combat, given-up target — see 02) and says why. */
bool ReceiveOrder(const FPoliceOrder& Order, FString& OutRefusal);
```

- Accept: store `CurrentOrder`, set the flag `bOrderPending`; the next `Decide()` maps it to a state:
  Respond → **Responding**; Pursue/Intercept → Pursuit (Intercept sets the tactic); Roadblock → Pursuit with
  `DriveIntercept` on the given point (reuse Phase 9); Search → Searching with the given centre; Return →
  **Returning**; Stop → Suspicion.
- Refuse when: Disabled, Recovery, Combat with contact, StandDown, or `IsTargetGivenUp(Order.Target)`.
- Self-dispatch from memory (today's behaviour) stays as the fallback when no director order exists — so nothing
  breaks for the `MurdarPolice` cheat.
- Knowledge rule (spec §1.17): an order's `Location` is the director's *belief* (the memory track), never the
  actor's live position. The director must build it from `UFactionMemorySubsystem`, not from `Target->GetActorLocation()`.

## 3. New states (add to EPoliceState + ARCHITECTURE §3 table)

| State | Entry | Exit / next | Interruption | Timeout | Failure | Recovery |
|---|---|---|---|---|---|---|
| **Responding** | `DriveTo(Order.Location)`; emergency on if `Urgency01 ≥ EmergencyUrgencyThreshold` (profile) | on arrival (*FixReachedCm*) → Searching (centre = location); target seen → Pursuit/Combat by wanted level | Combat, self-defence | *RespondTimeoutSeconds* (profile) → report failure, Returning | route impossible → Recovery → give-up (02) | Recovery |
| **Returning** | `DriveTo(nearest PatrolPoint / station)` at *PatrolSpeedKph*, emergency off, `Event.Police.Returning` | arrived → Patrol; new order → per order | any order, any crime seen | none (it's normal driving) | — | — |

Wire: Searching timeout and PursuitEnded now go to **Returning** instead of Patrol when a director exists (spec §40),
Patrol otherwise. Returning keeps the car in the world — no despawn (spec §40, §58). Despawn policy is decision C2/C8
in file 11.

## 4. Director side (UPoliceResponseDirector)

- `IssueOrder(Unit, Order)` → calls `ReceiveOrder`; on refusal try the next unit by ETA.
- ETA = lane-route length / *ResponseSpeedKph* (MurdarRoad routing already exists); pick units by ETA, not straight
  distance (spec §29, §36).
- Roadblock pick (spec §23): from `MurdarRoad::JunctionsAhead` of the *tracked* target, choose the junction with the
  fewest alternative exits that a free unit reaches with margin (*InterceptMarginSeconds*). Reuses Phase 9 logic —
  move the choice from the unit to the director, keep the unit's execution.
- Escalation (spec §36) = the existing Phase 8 backup timer + `EvalAggression` (file 07) + crime severity (file 03).
- Keep the director at 2 Hz, no steering (spec §1.28).

## Tests (first slice of Phase 10)

1. Order Respond to a point 300 m away → Responding, siren on (urgency 0.8), arrives, Searching, then Returning,
   Patrol. No despawn.
2. Order refused by a Disabled unit → the director re-issues to the next unit by ETA within 0.5 s.
3. Two free units: A is 80 m straight-line but 200 m by road (a block in between), B is 120 m straight-line and
   150 m by road → B goes (ETA by road, not by straight line).
4. Regression: everything in Phase 7–9 with the cheat spawn (no director orders) unchanged.
