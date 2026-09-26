# 04 — C3: actor lifetime / dangling references

Not in the spec. Needed before Phase 10–11 add more cross-references (orders, world events, suspects).

## Why

Timers at 0.25–1 s (controller, memory, director) and bus lambdas hold references to units, targets, officers,
cars. Targets die, cars are destroyed or abandoned (`AbandonCar`), officers despawn, PIE stops, levels unload.
A raw `AActor*` held across a timer tick is a crash; a lambda capturing `this` and outliving it is a crash.

## Audit (do this first, report the list)

```
grep -rn "AActor\*\|APawn\*\|AMurdarVehicle\*\|AMurdarPoliceAIController\*" Source/Murdar_GameDev/AI Source/Murdar_GameDev/Director
grep -rn "Subscribe(" Source/Murdar_GameDev
grep -rn "SetTimer(" Source/Murdar_GameDev/AI
```

For every **member** (not local/parameter) found in the police/pursuit/director/memory classes classify:

| Kind | Rule |
|---|---|
| `UPROPERTY() TObjectPtr<>` / `UPROPERTY() AActor*` | GC-safe but can point at a *pending-kill* actor: check `IsValid(x)` before use. |
| raw pointer without UPROPERTY | **bug**. → `TWeakObjectPtr<>` and `.Get()` + null check at every use. |
| inside a USTRUCT held in a TArray/TMap in a subsystem (e.g. roles per unit, tracks) | `TWeakObjectPtr<>`; prune invalid entries at the start of each subsystem tick. |
| bus `Subscribe` lambda capturing `this` | capture `TWeakObjectPtr<ThisClass> WeakThis = this;` and early-out if `!WeakThis.IsValid()`; **and** unsubscribe in `EndPlay`/`OnUnPossess`/`Deinitialize`. If the bus has no unsubscribe, that is the finding — report it, add one. |
| `SetTimer` on an actor/component | `GetWorld()->GetTimerManager().ClearAllTimersForObject(this)` in `EndPlay` (actors/components) / `OnUnPossess` (controllers — the pawn changes) / `Deinitialize` (subsystems). |

Pattern:

```cpp
// header
UPROPERTY() TWeakObjectPtr<AActor> Target;   // was: AActor* Target;

// use
AActor* T = Target.Get();
if (!T) { OnTargetInvalid(); return; }       // spec §53: target invalid -> terminate pursuit

// lambda
TWeakObjectPtr<AMurdarPoliceAIController> WeakThis(this);
Handle = Bus->Subscribe(Tag, [WeakThis](const FGameEvent& Ev)
{
    if (AMurdarPoliceAIController* Self = WeakThis.Get()) { Self->OnCrimeEvent(Ev); }
});

// teardown
void AMurdarPoliceAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    // ADAPT: Bus->Unsubscribe(Handle);
    Super::EndPlay(Reason);
}
```

## `OnTargetInvalid()` — one path for spec §53 "target invalid → terminate pursuit"

If the target becomes invalid while the unit is in Pursuit/Combat/Searching/Suspicion/TrafficStop:
`Pick(Patrol, "target invalid")`, publish `Event.Police.PursuitEnded`, driver `SetTarget(nullptr)`. The director
must drop the unit's role on its next tick (it already does when a unit leaves Pursuit/Combat — confirm).

## Tests (Python rig)

1. Chase with 2 units, `Destroy()` the target car mid-chase (player out of it first) → no crash, both units
   `PursuitEnded` within 0.5 s, back to Patrol, director holds no roles.
2. Chase with 2 units, `Destroy()` the Primary's car → no crash, PVA-T26 reassignment still ≤ 0.8 s.
3. Dismount officers, `Destroy()` one officer, then the car → no crash, `Crew` counter consistent.
4. Stop PIE mid-chase 10 times in a row (`StartPIE/StopPIE`) → no crash, no "timer on destroyed object" warnings
   in the log.
