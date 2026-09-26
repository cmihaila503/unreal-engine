# Police Vehicle AI — architecture

Living document: what exists after each phase, what the next phases add. Names are the project's, not the spec's
§3 proposals (see `POLICE_VEHICLE_AI_RECON.md` §7 C2 for the mapping). Physics is KinetiForge; "Chaos" in the spec
reads KinetiForge throughout.

## 1. Layers

```
knowledge   UFactionMemorySubsystem      heat, wanted level, radio track, known car, crimes, bribes   (existing)
            UGameEventSubsystem          tag bus: Event.Police.*, Crime.*, Event.Vehicle.*            (existing)
dispatch    UPoliceResponseDirector      units, roles, escalation, roadblock picks                   (Phase 8/10, new)
            UPoliceWorldEventDirector    crimes without the player, suspects, lifecycle, LOD        (Phase 11, new)
unit brain  AMurdarPoliceAIController    state model, 4 Hz Think(), perception → memory             (existing)
            UPoliceDrivingProfile        the personality: speeds, distances, timeouts, PIT permission (Phase 1, new)
            UPoliceEmergencyComponent    siren / lights from state, hearing stimulus, traffic yields to it (Phase 6, done)
driver      UVehiclePursuitComponent     modes → aim point + desired speed → pedals; whiskers; recovery (existing)
            MurdarRoad / UMurdarRoadTools ZoneGraph lanes: multi-start/multi-end lane Dijkstra -> polylines,
                                         lane tracking of a moving car (FLaneTrack), junction distance, navmesh fallback (Phase 2, done)
            AMurdarTrafficAIController   civilian cars between TrafficPoints on the lanes             (Phase 2, done)
car         AMurdarVehicle::ApplyAIInputs → KinetiForge InputValues.Raw → async physics (90 Hz)     (existing)
telemetry   FPoliceUnitTelemetry (unit), FPursuitDebugState (driver), decision trace, cvars         (Phase 1)
```

Threading: everything above the car is game thread — timers (4 Hz brain, 1 Hz memory, directors 1–2 Hz) and one
per-frame component tick per car (the driver). The physics thread runs only the plugin, fed by plain float inputs.
No UObject is read on the physics thread by project code.

## 2. Driving intent (spec §8, §50)

The driver's contract, per mode, per frame:

| produces | type | where |
|---|---|---|
| steering target | wheel angle → input in [−1, 1] via `SteeringLockAtSpeedDeg()` | `UVehiclePursuitComponent::SteerTowards` |
| acceleration target | throttle [0, 1] from speed error (P) | same |
| brake target | brake [0, 1] from speed error, arc cap, heading cap, obstacle closing speed | same |
| gear / reverse intent | brake-at-rest = reverse (plugin arcade gearbox); `bAllowReverse` gates the escape | same |
| handbrake | Stop / Block-parked only | `ApplyInputs` |

The AI never sets a transform, velocity or impulse (spec §1.14, §58). It reads the car back through the plugin's
game-thread getters (speed, wheel states, gear) — one physics step stale, accepted.

The target the driver chases is the actor only while somebody sees it; otherwise it is the radio's estimate
(`SetTargetEstimate`, Phase 7) — the wheel knows no more than the memory does.

## 3. State model — `EPoliceState`

Existing states with their contract as of Phase 1. Every timing/distance is a `UPoliceDrivingProfile` field
(named in *italics*) except the law (`SpeedLimitKph`, on the controller) and three implementation constants
(`StoppedKph` 4, `ReorderDistanceCm` 600, `StandDownDriveOffCm` 6000).

| State | Entry | Exit / next | Allowed interruption | Timeout | Failure | Recovery |
|---|---|---|---|---|---|---|
| **Patrol** | `SetTarget(null)`, next patrol point at *PatrolSpeedKph* (no points: park) | Pursuit (wanted ≥ Pursuit and the car matches / no description), Suspicion (wanted = Stop, in sight or fix < *NearFixDistanceCm*), Combat (Lethal + contact) | any | none | — | driver's own (whiskers, escape) |
| **Suspicion** | target = player, `PullAlongside`, `Event.Police.PullOver`, line | TrafficStop (stopped *TrafficStopStillSeconds*, < *TrafficStopRangeCm*), Pursuit (> *SuspicionTimeoutSeconds* and > *EvadeSpeedKph* → `Crime.Evading`), Patrol (he got out, or unseen > *SuspicionLoseSightSeconds*) | Combat | *SuspicionTimeoutSeconds* | player never stops | → Pursuit or Patrol |
| **TrafficStop** | `Stop`, `Event.Police.TrafficStop`; demand after *TrafficStopDemandDelaySeconds* | Combat (weapon seen / in hand < *WeaponNoticeDistanceCm*), StandDown (bribed / ticket after *TrafficStopTalkSeconds*), Pursuit (> *TrafficStopFleeKph*) | Combat | *TrafficStopTalkSeconds* | — | — |
| **Pursuit** | target = player, `Intercept`, `Event.Police.PursuitStarted` | Patrol (wanted = None; or no contact > *SearchSeconds*), Combat (on foot with contact; Lethal) | Combat | *SearchSeconds* without contact | contact lost | `SearchLastFix()` drives to `PredictTrack()` at *ResponseSpeedKph* |
| Pursuit upkeep | `Block` if target < *BlockBelowKph* (+*BlockHysteresisKph*) inside *BlockStartDistanceCm* (keep to *BlockKeepDistanceCm*), else `Intercept`; PIT when in window, *bAllowPIT*, > *PITCooldownSeconds* | | | | PIT abort = driver's `PITRecoverDuration` then previous mode | |
| **Combat** | target = current player pawn, `Event.Police.Combat`; in a car: routed `Intercept`, `Ram` only inside the ram window (*RamStartDistanceCm*, *RamAlignDeg*, in sight, target not turning, no junction ahead) and only as Primary; Blocker blocks; the rest hold chase slots | Patrol (wanted < Pursuit; or no contact > *SearchSeconds*), Pursuit (in a car and > *ReengageDistanceCm*) | — | — | dismount waits for a stop, forced after *DismountTimeoutSeconds* | search when nobody has him |
| Combat upkeep | driving target: `Ram`; on foot far (> *FootEngageDistanceCm*): `DriveTo` standoff *FootStandoffDistanceCm* at *PatrolSpeedKph*; on foot close: `Stop`, dismount every *DismountIntervalSeconds* | | | | | |
| **StandDown** | drive off 60 m at *StandDownSpeedKph*, `Event.Police.Released` | Patrol after *StandDownSeconds* | Lethal does **not** interrupt (paid is paid) | *StandDownSeconds* | — | — |
| **Recovery** (Phase 5) | *RecoveryEscapesBeforeAbandon* escapes in *RecoveryWindowSeconds*, or the route impossible; tactic dropped, `DriveTo` a lane point *RecoveryRepositionCm* back, `Event.Police.BackupRequired` | previous state when free (*RecoveryFreeKph* for *RecoveryFreeSeconds*, or arrived); Patrol with the target given up (cooldown ×2 per strike) when it was a chase and he has not moved | none (Lethal waits) | *RecoveryTimeoutSeconds* → Disabled | — | the driver's own escape, counted |
| **Searching** (Phase 7) | track stale (*LoseSightSeconds*) or the fix reached empty (*FixReachedCm*); legs of `DriveTo` over the junction exits, exit = my rank among searching units (+ leg count) | Pursuit / Combat on a report newer than the search's own; Patrol after *SearchSeconds* | Lethal (via contact) | *SearchSeconds* | nobody found | — |
| **Disabled** (Phase 5) | `Damage01 ≥ DisabledDamage01`, rolled > *DisabledRollDeg* for *DisabledRollSeconds*, or recovery timed out; `Stop`, target released, `Event.Police.UnitDisabled` (+ `PursuitEnded` if chasing) | Patrol after *DisabledSeconds* if upright and not wrecked; else never | none | — | — | — |

Planned (spec §5), to be added when their phase gives them behaviour, not before: `Responding` (Phase 6/8: an
order with a location), `Positioning` / `Roadblocking` (Phase 9), `Searching` (Phase 7: split out of Pursuit's
search upkeep so it can be distributed by §38), `Returning` (Phase 10). `Recovery` and `Disabled` arrived with Phase 5,
`Searching` with Phase 7 (legs over the exits of the junction the track leads to, one exit per searching unit by rank). The decision function stays a switch until then; StateTree is available in the engine if it grows past
readability.

Decision trace (spec §43): `Decide()` names the reason for every transition (`Pick(state, reason)`); `EnterState`
records `time old->new: reason` into an 8-entry ring (`GetDecisionTrace()`), logs it, and `GetTelemetry().DecisionReason`
carries the current one.

## 4. Driver modes — `EPursuitMode` (existing)

`Idle, DriveTo, Follow, Intercept, PullAlongside, PIT, Ram, Block, Stop, BoxRear, SideSweep` (the last two Phase 9). Each maps to an aim point and a desired
speed in the target's frame; `SteerTowards` does the rest. Tactics planned on top (Phase 9): `Roadblock` (park across
a lane at a point), `SideSweep`, chase-slot offsets for `Follow` (role-dependent), `LaneFollow` (Phase 2/3, ZoneGraph).

The aim point goes through the route layer (`RouteAim`): a clear line the tyres can hold at our speed is driven as a
line; otherwise the lane route (committed - a fresh plan replaces it only when clearly shorter), the navmesh path off
the roads. Two aims bypass it: the target's **trail** (its last 80 m of positions) when we tail a turning target or one
at a junction we are about to reach, and `Ram` — which is why Ram is only ordered from inside a window where the line
*is* the road. `FPursuitDebugState::RouteNote` says which the last frame used.

### Roles — `UPoliceResponseDirector` (Phase 8)

A world subsystem hands out `EPoliceRole` twice a second to every unit in Pursuit/Combat: Primary (nearest with eyes
on him, with hysteresis), Blocker (slow target, a unit within reach), Interceptor (far or ahead), Secondary, Support.
Units not chasing hold no role, which is how a role is released; a single unit is None and drives as before. The
controllers read the role each Think: only Primary (or None) PITs, blocks or rams; Secondary/Support hold a chase slot
behind the primary and a lane to the side (`SetChaseSlot`); Interceptors take a lane of their own. The director also
raises `Event.Police.BackupRequired` when a pursuit runs long with too few units. It never steers.

### Tactics (Phase 9)

`DriveIntercept` (controller, Interceptor role): `MurdarRoad::JunctionsAhead` on the target, the nearest junction ahead
of him that our lane route wins by *InterceptMarginSeconds*, `DriveTo` there, hold as a roadblock, `Block` when he
arrives, give it up when his road leaves it. Box-in: Primary `Block` (front), Blocker `BoxRear`, Support
`PullAlongside`. PIT: window gated by junction / turning / `PITMaxKph`, aborted mid-steer on obstacle, turn, junction,
speed or his braking. Side sweep: `SweepSide` (the side leaving *SweepClearanceCm* beyond him) → `PullAlongside` on it
→ `SideSweep` (lean in for *SweepSeconds*). `FPoliceUnitTelemetry::TacticNote` says which is running.

## 5. Telemetry — `FPoliceUnitTelemetry` (Phase 1)

`UnitId, State, Target, TargetConfidence (1 in sight; radio-only decays with track age to 0 at LoseSightSeconds),
Tactic, Role (Phase 8), DesiredKph, ActualKph, bEmergency, bSiren/bLights (Phase 6), StuckSeconds, RecoveryState,
DecisionReason, Profile`. On screen with `Murdar.Police.Debug 1` (one block per unit, red when emergency); driver
detail with `Murdar.Pursuit.Debug 1`. Tests read both through Python (`Tools/policetest.py telemetry`).

## 6. Data — `UPoliceDrivingProfile` (Phase 1)

`UPrimaryDataAsset`; class defaults = Police_Normal. Resolution order on the controller: own `Profile` → 
`UMurdarAISettings::DefaultPoliceProfile` (`DefaultGame.ini`, = `/Game/Murdar/AI/Police/Police_Normal`) → class
defaults. Six assets under `Content/Murdar/AI/Police/` (`Content/Python/setup_police_profiles.py`). The only
car-side value it sets is `UVehiclePursuitComponent::MaxSpeedKph` on possess.

## 7. Interfaces the later phases plug into

- Director → unit: an order (`intent, target, location, role`) — to be added in Phase 8 as `ReceiveOrder`; until
  then the unit self-dispatches from the memory exactly as today.
- Memory → many targets: `FFactionTrack`/`FKnownVehicleRecord` keyed by a target handle (Phase 11).
- Road layer → driver: `MurdarRoad::{NearestLane, RouteAlongLanes, AdvanceAlongLanes, TrackLane / AdvanceFromTrack,
  DistanceToJunction(FromTrack)}` returning polylines the existing `RouteAim` can consume (Phase 2), the target's
  lane-following lead point and the junction it is at (pursuit debug entry), navmesh path when no lane.
