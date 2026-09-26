# Police Vehicle AI — Phase 0 recon

Spec: `Docs/MURDAR_POLICE_VEHICLE_AI_SPEC.md` (text of the PDF supplied 2026-09-19). This document answers the
spec's §60 questions against the project as it is, with every API cited from a header that exists on disk. Nothing
here is implemented; Phase 1 has not started. No `.uasset` / `.umap` was touched.

Verification convention: **[V]** = read in the engine/plugin/project source at the path given, signature quoted or
already compiled into the project. **[U]** = not verified, listed under *Unknown*.

---

## 1. Engine version

`C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version`: **5.8.1**, changelist 56057345, branch
`++UE5+Release-5.8`, promoted build (binary install, no engine source edits possible). Project `EngineAssociation`
`5.8`. One runtime module, `Murdar_GameDev` (~11 k lines C++). Editor is launched from the binary engine; builds go
through `Build.bat Murdar_GameDevEditor Win64 Development` with the editor closed (Live Coding is not used).

Module dependencies already present (`Source/Murdar_GameDev/Murdar_GameDev.Build.cs`): Core, CoreUObject, Engine,
InputCore, EnhancedInput, GameplayTags, PoseSearch, Chooser, EngineCameras, AssetRegistry, **KinetiForge,
AsyncTickPhysics, AIModule, GameplayTasks, NavigationSystem**, DeveloperSettings, PhysicsCore.

Plugins enabled in `Murdar_GameDev.uproject` that matter here: **KinetiForge** (project plugin, `Plugins/KinetiForge`),
SmartObjects, GameplayInteractions, Mover, PoseSearch/Chooser (animation). Not enabled, present in the engine and
relevant: **ZoneGraph**, StateTree/GameplayStateTree, SignificanceManager, MassGameplay/MassAI/MassCrowd,
NavCorridor. Absent from the engine: **MassTraffic** (CitySample-only), GameplayMessageRouter.

---

## 2. Existing vehicle system — KinetiForge, not Chaos

The spec says "Chaos Vehicles" throughout. The project does not use Chaos Vehicles anywhere; the user confirmed on
2026-09-19 that this was a mistake in the spec and the target is **KinetiForge**. Everything below reads "Chaos" in
the spec as "KinetiForge".

### 2.1 Shape

`AMurdarVehicle` (`Source/Murdar_GameDev/Vehicle/MurdarVehicle.h/.cpp`, 1004 lines) is a pawn with:

- `UVehicleDriveAssemblyComponent* DriveAssembly` (KinetiForge) — engine, gearbox, clutch, axles, wheels, input
  pipeline. Configuration comes from a **`UVehicleDefinition`** data asset (`Vehicle/VehicleDefinition.h`), applied in
  `PreRegisterAllComponents` via `ApplyDefinition()`; the definition is a primary asset of type `Vehicle`.
- `UVehicleEffectsComponent` (dust, skid, audio, surface response, impact → `AddCrashDamage`).
- Damage model: `Damage01` (0..1) from crash severity; `SetDrivingInputs` scales throttle by
  `1 − DamagePowerLoss·Damage01` and adds a steering pull. **Cars have no `UHealthComponent`** — a car cannot be
  "destroyed"; the police controller treats the player ramming it via the `Event.Vehicle.Crashed` bus event.
- Player entry/exit: `Enter(APawn*)`, `Exit(bForce)`; AI takeover: `BeginAIDriving()` (sets `bAIDriven`, wakes the
  body, starts the engine) and `ApplyAIInputs(Throttle, Brake, Steer, Handbrake)` → `SetDrivingInputs`.
- Spawning: `UVehicleSubsystem::SpawnVehicle(TSubclassOf<AMurdarVehicle>, UVehicleDefinition*, const FTransform&)`
  (`Vehicle/VehicleSubsystem.h:29`, not a UFUNCTION) — deferred spawn so the definition is applied before components
  register. Registry: `GetVehicles()`, `FindEnterable()`, `GetDrivenVehicle()`.
- Debug: `DescribeDynamics()`, cvars `Murdar.Vehicle.Physics.Debug`, `Murdar.Vehicle.SteerFeel`; `OverrideInputs()`
  (UFUNCTION, used by every Python test rig).

### 2.2 Input path (game thread → physics thread) [V]

```
AI (game thread)                      plugin (game thread)                        plugin (physics thread)
UVehiclePursuitComponent::ApplyInputs
  → AMurdarVehicle::ApplyAIInputs      → SetDrivingInputs                          UVehicleAsyncTickComponent::
      (damage scaling)                   → DriveAssembly->InputThrottle(v, true)     NativeAsyncTick(dt)
                                           InputBrake / InputSteering /              → per drive assembly:
                                           InputHandbrake(v, true)                     UpdateThrottle/Brake/Steering
                                         writes InputValues.Raw.* (plain floats,        reads InputValues.Raw.*,
                                         VehicleDriveAssemblyComponent.cpp:897)         smooths, applies curves,
                                                                                        solves suspension + tyres
```

- `UVehicleDriveAssemblyComponent::InputThrottle(float, bool bDirectInput)` [V] `VehicleDriveAssemblyComponent.h:190`;
  `InputBrake / InputSteering / InputHandbrake` likewise; `bDirectInput = true` also writes `Smoothened.*` so there
  is no input lag on top of the AI's own smoothing.
- Steering is scaled on the physics side by `InputConfig.HighSpeedSteeringScale` (a `UCurveFloat`, m/s → 0..1;
  `VehicleDriveAssemblyComponent.cpp:276`) and by `Steering.ResponseCurve` (linear). **The AI has to divide the wheel
  angle it wants by `lock × curve(speed)`** — fixed 2026-09-19 (`SteeringLockAtSpeedDeg()`, see the tuning log).
- Async physics: `UVehicleAsyncTickComponent` registers each drive assembly and runs it from
  `UAsyncTickActorComponent::NativeAsyncTick` (`Plugins/KinetiForge/Source/AsyncTickPhysics/Public/AsyncTickActorComponent.h:16`)
  at the project's fixed physics rate (90 Hz). There is **no lock** between the game-thread writes of
  `InputValues.Raw` and the physics-thread reads: plain float stores, torn reads impossible on x64, an input can be
  one physics step late. Established, accepted.
- Reading state back on the game thread: `GetVehicleSpeed()` (`VehicleDriveAssemblyComponent.h:247`, outputs
  m/s and km/h), `GetWheels()[i]->GetWheelState()` (slip angle/ratio, load, forces), `GetSteeringAngle()`,
  `GetCurrentGear()`, engine rpm. All POD copies written by the physics thread; the same one-step-stale caveat.

### 2.3 What the plugin already offers for AI driving [V]

`UVehicleADASComponent` (`Plugins/KinetiForge/Source/KinetiForge/Public/VehicleADASComponent.h`, 1578-line .cpp),
a static BlueprintCallable library, **not used by the project**:

- `UpdateCruiseControl(DriveAssembly, TargetSpeed m/s, MaxAcceleration m/s², DeltaTime, ShiftInterval)` — pedals for a
  target speed. Reusable for patrol / speed control (spec §9) instead of the pursuit component's P controller.
- `GetMaxSpeedToBrake(Distance, Accel, dt, EndSpeed)`, `GetMaxSpeedToTurn(PathPoints, AccelLimit, MinTurningRadius)`,
  `GetVehicleMaxDriveForce / MaxBrakeForce` — planning helpers.
- `GetPathPointsToTarget` (uses `UNavigationSystemV1::FindPathToLocationSynchronously`, .cpp:495),
  `FixPathCollisions` (sphere sweeps), `GetBestTargetInFOV` (ray fan, .cpp:589+), `UpdateAutoPilotSimple /
  UpdateAutoPilotNavMesh` (complete autopilots), `UpdateAutoPilotHumanLike` (marked "do not call, under development").

The project's `UVehiclePursuitComponent` already does routed pure pursuit, whiskers, stuck recovery, PIT/Block/Ram —
more than the autopilots. Recommendation: keep the project component as the driver; borrow `UpdateCruiseControl`
and `GetMaxSpeedToTurn` where a target-speed controller is needed (patrol, lane following). Do not run two drivers
on one car.

### 2.4 Physics-side facts the AI must respect (measured, `Docs/VEHICLE_*.md`)

- Steering lock 30° (road cars) / 32° (ARO), scaled by `C_SteeringSpeed`: full lock ≤ 15 km/h, 0.49 at 30 km/h,
  0.27 at 100 km/h. Turning circle 11.4 m at parking speed.
- Launch from rest: in gear 0.07 s, moving 0.20 s. Brake at rest = **reverse** (arcade gearbox).
- A body at rest goes to sleep; the plugin's tick does not run for a sleeping body (steering reads 0°). Anything
  that parks a cop and later expects it to move must `WakeAllRigidBodies()` (`AMurdarVehicle::BeginAIDriving`
  already does; a parked cop woken by throttle relies on Chaos waking on force — **not verified**, see §7).
- Lateral grip on asphalt ≈ 1.1 g; `UVehiclePursuitComponent::LateralGripCms` 750 (0.76 g) is the planner's cap.

---

## 3. Existing AI system

### 3.1 `AMurdarPoliceAIController` (`AI/MurdarPoliceAIController.h/.cpp`, 656 lines) [V]

- One patrol car's brain. `AAIController` with `UAIPerceptionComponent` + `UAISenseConfig_Sight` (radius 60 m,
  lose-sight 75 m, half-angle 80°, max age 2 s, auto-success 15 m). **No hearing sense.** Actor never ticks; a
  `Think()` timer at 4 Hz (`ThinkInterval = 0.25`).
- State model (`EPoliceState`): `Patrol, Suspicion, TrafficStop, Pursuit, Combat, StandDown`. `Decide()` is a plain
  switch reading the faction memory (`GetWantedLevel()`, `ObservePlayer()`, `MatchesKnownVehicle()`, `TrackAge()`);
  `EnterState()` publishes `Event.Police.*` and hands a mode to the pursuit component; per-state upkeep in `Think()`.
- Knowledge is never read from the player directly: `HasContact()` = own sight **or** the faction track younger than
  `LoseSightSeconds` (12 s); lost contact → `SearchLastFix()` drives to `PredictTrack()` (last fix + ≤ 2 s dead
  reckoning). This already satisfies spec rules 17–18 for one unit.
- Pursuit upkeep: `Block` when target < 45 km/h within 35 m (50 m hysteresis), else `Intercept` + `ExecutePITManeuver()`
  when `IsInPITWindow()` and the 6 s cooldown allows. Combat: `RamTarget()` against a car, `Stop()` + `Dismount()` →
  `SpawnOfficer()` against a man on foot (vehicle → pedestrian hand-off exists for the police side: an
  `AMurdarNPCAIController` officer spawned at the door via `UMurdarAILibrary::SpawnNPC`, `Crew` counter, `AbandonCar()`
  when the last one is out).
- Traffic stop: pull alongside (`PullAlongside`), bribe / weapon / ticket logic via the memory and the bus.
- Spawned **only** by the `MurdarPolice` cheat (`Director/DirectorCheats.cpp:273`: `SpawnVehicle` with
  `UMurdarAISettings::PoliceVehicleDefinition`, then `SpawnActor<AMurdarPoliceAIController>` + possess). The chapter
  director spawns the player's car from a tagged actor; **nothing in gameplay dispatches police**. Patrol points are
  any actor tagged `PatrolPoint`.
- Literals in `Think()/Decide()` that the spec would call magic numbers: 3500/5000 (block range), 45 (block speed),
  900/700/2500/3000 (distances), 1.5/8/4 s. They belong in a driving profile (spec §4).

### 3.2 `UVehiclePursuitComponent` (`AI/VehiclePursuitComponent.h/.cpp`, 1021 lines) [V]

Combat driver on the car (added by the controller on possess). Modes: `Idle, DriveTo, Follow, Intercept,
PullAlongside, PIT, Ram, Block, Stop`. Every mode reduces to `SteerTowards(AimPoint, DesiredKph, dt, bAllowReverse)`:

- Routing: straight line if `MurdarNav::IsLineBlocked` says clear, else a navmesh path on the **Car** agent
  (`MurdarNav::FindPath`, `UNavigationSystemV1::FindPathSync`, re-planned ≤ every 1.5 s, kept unless 20 % shorter)
  and a speed-scaled lookahead point along it. Corner speeds from the summed turn angle over 10 m.
- Steering: pure pursuit, arc capped at `PursuitArcMaxCm` (20 m), wheel angle divided by the speed-scaled lock.
- Speed: P controller on error, arc cap `v²κ ≤ LateralGripCms`, heading brake > 70° above 35 km/h, obstacle brake
  from closing speed and gap (`BrakingDecel`).
- Obstacles: `ProbeWhiskers` — 5 sweeps (0/±22/±50°) + 2 side rails per **frame**, object types
  WorldStatic/Dynamic/Vehicle/Pawn/PhysicsBody, target excluded. Reach = reaction + braking distance, cut at the next
  route corner. Braking for bodies we are not closing on is suppressed (traffic).
- Recovery: stuck (< 3 km/h wanting to move 1.5 s), no progress (2.5 s over 2.5 m), nose-in-wall, three-point-turn
  → 3 s reverse with the wheel turned, ends when the nose is on the route / 7 m backed / rear blocked.
- Tactics: PIT (window in the target frame, speed-scaled lock, 0.55 s in / 0.6 s recover), Block (draw level, pass,
  cut across, park), Ram (lead 0.45 s, back-off after contact via `NotifyContact`), Intercept (closing-speed
  solution, capped lead, slot beside the rear quarter when close).
- Telemetry: `FPursuitDebugState` (mode, aim, pedals, heading error, offsets in the target frame, limiter, desired
  speed, obstacle distance, route points); cvar `Murdar.Pursuit.Debug` draws aim/route/PIT box and one line per car.
- Cost: 7 sweeps + trig per frame per car, 1 line sweep at 4 Hz, 1 sync path ≤ 0.67 Hz. No LOD.

### 3.3 `UFactionMemorySubsystem` (`AI/FactionMemorySubsystem.h/.cpp`, world subsystem) [V]

The radio. Per faction (Civilian / Police / Hostile): `FFactionTrack` (location, velocity, time, visual or not,
source), members list, `ReportSighting / ReportContact / ReportCrime / ReportLostContact`, heat with decay
(`HeatStop 12 / HeatPursuit 40 / HeatLethal 75` → `EWantedLevel None/Stop/Pursuit/Lethal`), `FKnownVehicleRecord`
(the car description the police are looking for, goes stale after 120 s), `FThreatRecord`, crimes (64 kept), bribes
per sector/crew, `ObservePlayer()` (cheap mirror kept by bus listeners). Heat and bribes mirror into the narrative
state (save game). Tick: 1 Hz. This is the spec's "Police AI knowledge / Last Known Position" layer and its
`TARGET_SPOTTED / TARGET_LOST / LAST_KNOWN_LOCATION` messages already, single-target (the player only).

### 3.4 `AMurdarNPCAIController` (`AI/MurdarNPCAIController.h/.cpp`, 661 lines) [V]

Foot AI for police officers, hostiles, civilians on the player's own `AMurdarCharacter` kit. States `Idle, Engage,
Reload, SeekAmmo, Hide, Flee`; perception → faction memory (civilians are witnesses: they report the player hitting
people with a car, `Crime.HitPedestrian`). 5 Hz `Think()`, direct steering with navmesh where present. **No driving,
no traffic behaviour.** Spawned by `UMurdarAILibrary::SpawnNPC` (`AI/MurdarAISettings.cpp:42`) and the `MurdarNPC`
cheat.

### 3.5 Support systems [V]

- `UGameEventSubsystem` (`Director/GameEventSubsystem.h`): game-instance bus keyed by gameplay tag hierarchy,
  synchronous, game thread only, `Subscribe(Tag, TFunction)` / `Publish(FGameEvent)`; `FGameEvent {Tag, Source,
  Location, Magnitude, Payload}`. Police tags exist: `Event.Police.{HeatChanged, WantedChanged, Crime, Sighted,
  LostContact, PullOver, TrafficStop, Demand, Bribed, Released, PursuitStarted, PursuitEnded, Combat, PIT, Ram,
  Line}`, `Crime.*` (Speeding, Reckless, HitPedestrian, HitPolice, Weapon, Shooting, Assault, Murder, Evading),
  `Sector.*`, `Event.Vehicle.Crashed`, `Event.Player.{Entered,Exited}Vehicle`. This is the spec's §34 message channel;
  it lacks `Source / Timestamp / Confidence / Target / Urgency` fields (has Source, Location, Magnitude, Payload).
- `UMurdarAISettings` (`UDeveloperSettings`): `NPCPawnClass`, `PoliceWeapon`, `HostileWeapon`,
  `PoliceVehicleDefinition`, corpse/death timers. The natural home for profile defaults' *pointers*.
- `UChapterDirector` / `UChapterDefinition`: chapters, checkpoints, triggers, facts; `VehicleClass` per chapter,
  `VehicleStartTag`. `UZoneTriggerSubsystem` / `AZoneVolume`: `Event.Zone.Entered/Left` with `Zone.*` tags — usable
  for police sectors / world-event zones without new code.
- `UTensionSubsystem`: hidden stress meter; consumes crashes/shots; a pacing input for escalation, not a driver.
- Navigation config (`Config/DefaultEngine.ini:299-300`): two Recast agents, **Human** (r 40, h 180) and **Car**
  (r 280, h 200, step 30). `MurdarNav` (`AI/MurdarNavigation.h`): `FindPath(World, From, To, AgentRadius, AgentHeight,
  OutPoints, Querier)` [sync], `PointAlongPath`, `IsLineBlocked` (sphere sweep, static+dynamic). Navmesh bounds in
  `L_Sandbox` are ±170 m.
- Collision: profile `Vehicle` (object type Vehicle); trace channel `Obstacle` (`ECC_GameTraceChannel3`, default
  ignore) exists and is unused by the pursuit whiskers (they query by object type).
- Maps: **`Content/Murdar/Maps/L_Sandbox.umap` only** — a flat tiled floor with walls, no roads, one `Sector.Sandbox`.
- Test harness: Python over the editor's Python bridge (`Tools/uepy.py`), PIE driven through the Unreal MCP
  `StartPIE/StopPIE`; rigs `Tools/aitest.py`, `Tools/chase.py`, `Tools/circletest.py`, `Tools/handling.py`, …; cheats
  `MurdarPolice`, `MurdarHeat`, `MurdarCar`, `MurdarDrive`, `MurdarNPC`, `MurdarGod`. Reports and traps in
  `Docs/VEHICLE_HANDLING_TESTS.md` §10–11 and `Docs/POLICE_VEHICLE_AI_TUNING_LOG.md`.

---

## 4. Existing road / traffic system

**None.** There is no road graph, no lanes, no intersections, no traffic lights, no civilian traffic AI. Vehicles
drive on the Car navmesh agent + whiskers. The sandbox has no roads to describe.

Reuse candidate in the engine: **ZoneGraph** (`Engine/Plugins/Runtime/ZoneGraph`, not enabled). Verified API:

- `UZoneGraphSubsystem` (`ZoneGraphSubsystem.h`): `FindNearestLane(const FBox&, FZoneGraphTagFilter,
  FZoneGraphLaneLocation&, float& DistSqr)` :73, `FindOverlappingLanes` :76, `AdvanceLaneLocation(const
  FZoneGraphLaneLocation&, float Distance, FZoneGraphLaneLocation&)` :82, `FindNearestLocationOnLane` :88/:91,
  `GetLaneLength` :97, `GetLinkedLanes(LaneHandle, EZoneLaneLinkType, IncludeFlags, ExcludeFlags,
  TArray<FZoneGraphLinkedLane>&)` :106, `GetZoneGraphStorage(DataHandle)` :60, `GetRegisteredZoneGraphData()` :48.
- `UE::ZoneGraph::Query` free functions on `FZoneGraphStorage` (`ZoneGraphQuery.h:11-77`): lane length/width/tags,
  linked lanes, advance, location along lane (distance / ratio), nearest lane / overlaps.
- Lane model (`ZoneGraphTypes.h`): `FZoneLaneDesc { Width (150 default), EZoneLaneDirection Forward/Backward, Tags }`;
  lane profiles are `UZoneGraphSettings` data; lanes link as `Outgoing / Incoming / Adjacent (left/right,
  same/opposite direction)`; **intersections are polygon zone shapes whose internal lanes connect incoming to
  outgoing lanes** — that is the spec's §7 tree.
- Routing: `FZoneGraphAStar : FGraphAStar<FZoneGraphAStarWrapper, …>` with `FZoneGraphPathFilter`
  (`ZoneGraphAStar.h:58,187`) — header-only A* over lanes. Usage pattern needs a Phase 2 spike **[U]**.
- Authoring is manual: `AZoneShape` / `UZoneShapeComponent` spline (roads) and polygon (intersections) actors placed
  in the level; `AZoneGraphData` is built by the editor. This is exactly a §57 manual editor task. No traffic
  lights / stop signs in ZoneGraph itself — those are annotations (`ZoneGraphAnnotations` module, tag-based) or our
  own actors.
- MassTraffic (CitySample) is what Epic drives cars with on ZoneGraph; it is **not in the engine** and would drag in
  Mass. Not proposed.

Fallback when no lanes exist under a car (§53 "lane missing → road-level fallback"): the Car navmesh, exactly what
the pursuit component uses today.

---

## 5. Verified engine APIs (beyond the above)

| Need (spec §) | API | Where [V] |
|---|---|---|
| Perception (§15) | `UAIPerceptionComponent::GetCurrentlyPerceivedActors(TSubclassOf<UAISense>, TArray<AActor*>&)`, `OnTargetPerceptionUpdated`; `UAISenseConfig_Sight` fields used above | compiled in `MurdarPoliceAIController.cpp:279` |
| Sirens as stimuli (§6, §52) | `UAISense_Hearing::ReportNoiseEvent(WorldContext, Location, Loudness, Instigator, MaxRange, Tag)`; `UAISenseConfig_Hearing` | `Runtime/AIModule/Classes/Perception/AISense_Hearing.h:106` |
| Sync path (§7) | `UNavigationSystemV1::FindPathSync(FNavAgentProperties, FPathFindingQuery)`, `GetNavDataForProps` | compiled in `MurdarNavigation.cpp:41` |
| Async path (§49) | `uint32 UNavigationSystemV1::FindPathAsync(const FNavAgentProperties&, FPathFindingQuery, const FNavPathQueryDelegate&, EPathFindingMode::Type)`; `FNavPathQueryDelegate(uint32, ENavigationQueryResult::Type, FNavPathSharedPtr)` | `NavigationSystem.h:668`, `NavigationTypes.h:657` |
| Timers (§1.13, no per-frame brains) | `FTimerManager::SetTimer(FTimerHandle&, UserClass*, TMethodPtr, Rate, bLoop, FirstDelay)` | `TimerManager.h:167`, in use |
| Debug overlay (§42–44) | `FGameplayDebuggerCategory`, `IGameplayDebugger::RegisterCategory(FName, FOnGetCategory, State, Slot)` | `Runtime/GameplayDebugger/Public/GameplayDebuggerCategory.h:48`, `GameplayDebugger.h:78`; module `GameplayDebugger` (Runtime) exists |
| Console debug (§44) | `TAutoConsoleVariable`, `DrawDebugHelpers`, `GEngine->AddOnScreenDebugMessage` | in use (`Murdar.Pursuit.Debug`) |
| LOD (§41) | `USignificanceManager::RegisterObject(UObject*, FName Tag, FManagedObjectSignificanceFunction, EPostSignificanceType, FManagedObjectPostSignificanceFunction)` | `Plugins/Runtime/SignificanceManager/.../SignificanceManager.h:121` (plugin not enabled) |
| Sleep handling (§2.4) | `UPrimitiveComponent::WakeAllRigidBodies()`, `IsAnyRigidBodyAwake()`, `PutAllRigidBodiesToSleep()` | `PrimitiveComponent.h:2797/2935/2923` |
| Spline roads (alternative to ZoneGraph) | `USplineComponent::GetLocationAtDistanceAlongSpline`, `GetSplineLength`, `FindInputKeyClosestToWorldLocation` | `Components/SplineComponent.h:727/696` |
| State machine option | `StateTree` / `GameplayStateTree` plugins present | `Engine/Plugins/Runtime/StateTree` (not enabled) |
| AIController move (not used: cars steer themselves) | `AAIController::MoveToLocation / MoveTo` | `AIController.h:187/196` — **not applicable** to a KinetiForge pawn (no `UPathFollowingComponent` movement) |

## 6. Unknown / not verified [U]

*2026-09-20 update:* the ZoneGraph A* usage, authoring without Mass, and `AZoneGraphData` building in the editor are
now **verified** — `AI/MurdarRoadNavigation.cpp` compiles and routes on a generated 156-lane grid (see the test
report, Phase 2). Remaining unknowns are the last four below.

- `FZoneGraphAStar` usage: constructing `FZoneGraphAStarWrapper` from storage and running `FindPath` with a
  `FZoneGraphPathFilter` — header read, never compiled here. Phase 2 spike before any design depends on it.
- ZoneGraph lane profile / tag authoring workflow in 5.8's editor, and whether `AZoneGraphData` builds for a level
  with no Mass plugins enabled (it should — ZoneGraph is a standalone plugin — but not tried).
- Whether a **sleeping** KinetiForge car wakes when the AI applies throttle (Chaos wakes on `AddForce`; the plugin
  applies forces from the async tick, which may not run for a sleeping body). The parked-cop → resume case must be
  tested in Phase 1; the fix, if needed, is `WakeAllRigidBodies()` in `ApplyAIInputs` when inputs go non-zero.
- `USignificanceManager` behaviour with actors whose brains are timers (we would scale timer rates ourselves;
  the manager only computes a number). May not be needed — a distance band in one subsystem is smaller.
- Perf numbers for N cars: none measured beyond 1–3 cars. Phase 13.
- `UVehicleADASComponent::UpdateCruiseControl` quality on the project's engine friction/clutch settings — untested.

---

## 7. Conflicts with the spec, and the smallest compatible change (rule 31)

| # | Spec says | Project is | Proposed |
|---|---|---|---|
| C1 | Chaos Vehicles everywhere (§0, §50) | KinetiForge (`UVehicleDriveAssemblyComponent`) | Read "Chaos" as KinetiForge; user confirmed. Input interface is `AMurdarVehicle::ApplyAIInputs`. |
| C2 | 19 new `UMurdarPolice*` components (§3) | 3 classes already cover PoliceVehicleAI (controller), Pursuit+Tactics+ObstacleAvoidance+Recovery (pursuit component), knowledge/radio (faction memory) | Keep and extend them; add only what has no owner today (§8). Names in §3 are "proposals to validate" per the spec itself. |
| C3 | Road/lane layer (§7–10, §13) | none; flat sandbox, no roads, Car navmesh only | Enable **ZoneGraph**; author roads manually (§57); `UMurdarRoadNavigation` wraps ZoneGraph with navmesh fallback. Needs a test map with roads before Phase 2 can be measured. |
| C4 | Civilian traffic reacts to sirens (§6, §13, §52) | no civilian traffic AI exists at all (foot NPCs only) | Out of this spec's scope to build traffic; Phase 6 emits the stimuli (`ReportNoiseEvent`, bus event) so traffic can react when it exists. Flagged, not silently dropped. |
| C5 | Siren / lights controlled by AI state (§6, §19) | no siren audio, no light bar, no assets; only headlights | Component + state hooks in C++; **assets are a manual editor task** (siren sound, light meshes/materials). |
| C6 | Units dispatched with origin/ETA (§29, §36) | police exist only through the `MurdarPolice` cheat | New `UPoliceResponseDirector` (world subsystem). Spawn points = tagged actors / police stations (manual). |
| C7 | Multi-target (suspect NPC, world events, §27–31) | faction memory tracks **the player only** | Generalise `FFactionTrack`/known-vehicle to a target handle (player or suspect pawn); pursuit component already takes any `AActor*`. Medium-size change to the memory, Phase 11. |
| C8 | Unit DISABLED (§35, §39) | cars have no health; `Damage01` only; flipped/stuck undefined | Define disabled = `Damage01 ≥ threshold` **or** roll > X for T s **or** recovery failed N times; no vehicle HP added. |
| C9 | No magic numbers (§1.10–11) | `Think()` literals (3500, 5000, 45, 900…) ; pursuit component has UPROPERTY tunables | Move controller literals into `UPoliceDrivingProfile` (data asset) in Phase 1; pursuit tunables stay where they are (they have owner/unit/default; add min/max meta). |
| C10 | Messages with Source/Timestamp/Confidence/Target/Urgency (§34) | `FGameEvent {Tag, Source, Location, Magnitude, Payload}` | Add a `FPoliceRadioMessage` struct carried by the faction memory (confidence, target handle, urgency); keep the bus for notifications. Do not fork the bus. |
| C11 | Test maps `PoliceAI_Test_*` (§45) | one map | Manual editor task list in `POLICE_VEHICLE_AI_EDITOR_TASKS.md`; automated PVA tests stay Python-over-PIE like every other test in this project (spec §46 does not prescribe the harness). |
| C12 | "Nu folosi `AddImpulse` pentru control" (§1.14) | control is pedals only | Already compliant; PIT/Ram are contact through the tyre solver. |
| C13 | Physics thread never reads UObjects (§1.13) | plugin's async tick reads its own components (plugin-owned); project AI is game-thread only | Compliant on the project side; do not put AI on the async tick. |
| C14 | `AIController::MoveTo` style navigation implied by "AI framework" | cars cannot use path following components | Cars steer themselves through the pursuit component; navigation queries only. |

---

## 8. Proposed architecture (validated against what exists)

Spec §2/§3 mapped onto the project. **Existing** = keep, extend in place. **New** = no owner today.

```
UGameEventSubsystem (existing bus)  ──────────────────────────────────────────── notifications
UFactionMemorySubsystem (existing: heat, wanted, radio track, known car, crimes)  ── knowledge in/out
   └─ new: target handle (player | suspect pawn), FPoliceRadioMessage (confidence, urgency), search points
UPoliceResponseDirector (NEW, world subsystem)             ── dispatch, roles/chase slots, escalation, roadblock picks
UPoliceWorldEventDirector (NEW, world subsystem, Phase 11) ── crimes without the player, suspect spawn, lifecycle, LOD
   │ orders (intent: PURSUE/STOP/INTERCEPT/ROADBLOCK/SEARCH/RETURN + role)
   ▼
AMurdarPoliceAIController (existing; states → gain POSITIONING/ROADBLOCKING/SEARCHING/RECOVERY/DISABLED/RETURNING;
   literals → UPoliceDrivingProfile; emergency state)                      ── PoliceVehicleAI + EmergencyDriving
   ├─ UPoliceEmergencyComponent (NEW, on the car): siren/lights on state, ReportNoiseEvent, bus event
   ├─ UVehiclePursuitComponent (existing): Driving + Tactics + ObstacleAvoidance + Recovery + telemetry
   │     └─ new modes: LaneFollow (ZoneGraph), Roadblock (park across lane), SideSweep; slot offsets from role
   └─ UMurdarRoadNavigation (NEW, static/namespace like MurdarNav): ZoneGraph lane queries + A*, navmesh fallback
AMurdarVehicle::ApplyAIInputs → KinetiForge (existing; authoritative physics)
Officer hand-off: existing Dismount/SpawnOfficer/AbandonCar (extend for suspect NPC in Phase 12)
Telemetry: FPursuitDebugState (existing) + FPoliceUnitTelemetry (NEW) → FGameplayDebuggerCategory + cvars
```

Data (§4): `UPoliceDrivingProfile : UPrimaryDataAsset` with the spec's fields, one asset per personality
(Disciplined/Normal/Aggressive/Elite/Inexperienced/Tactical); the controller reads the profile, the pursuit
component keeps its physical tunables (grip, decel, lookahead) because those describe the *car*, not the *driver*.

Threading (§49): all AI on the game thread — controller timers (4 Hz), pursuit component tick (per frame, one car
= 7 sweeps), directors on 1–2 Hz timers. Path queries: sync today (≤ 1 per 1.5 s per car); switch to
`FindPathAsync` only if Phase 13 measures a spike. Physics thread: KinetiForge only, fed by `InputValues.Raw`.
Nothing of ours runs there.

Phase order stays the spec's. Phase 1's "car receives a destination and drives a valid route" is **already true**
(`DriveTo`); Phase 1 therefore becomes: profile asset, state model extension, telemetry struct, intent interface —
and the parked-car wake test. Phase 2 needs the manual road authoring first (blocker, not a code task).

---

## 9. Report (spec §60)

- **ENGINE VERSION:** UE 5.8.1 binary (CL 56057345). Project module `Murdar_GameDev`, C++.
- **EXISTING VEHICLE SYSTEM:** KinetiForge (project plugin) on `AMurdarVehicle` + `UVehicleDefinition`; async physics
  at 90 Hz; AI input via `ApplyAIInputs` → `InputValues.Raw` floats; plugin ADAS library present, unused.
- **EXISTING AI SYSTEM:** `AMurdarPoliceAIController` (6 states, 4 Hz, sight perception),
  `UVehiclePursuitComponent` (routed pure pursuit, whiskers, recovery, PIT/Block/Ram/Intercept/Follow/Alongside),
  `UFactionMemorySubsystem` (heat, wanted level, radio track, known car, crimes, bribes), `AMurdarNPCAIController`
  (foot AI, witness), `UGameEventSubsystem` (tag bus). Police spawn only via cheat.
- **EXISTING ROAD/TRAFFIC SYSTEM:** none. Car navmesh agent (r 280) only. ZoneGraph available in the engine, not enabled.
- **VERIFIED APIs:** §2.2, §2.3, §3, §4, §5 above (KinetiForge input/state, ZoneGraph subsystem/query/types/A*,
  AIModule perception + hearing, NavigationSystem sync/async, GameplayDebugger, SignificanceManager, TimerManager,
  PrimitiveComponent sleep, SplineComponent).
- **UNKNOWN APIs:** §6 (ZoneGraph A* usage, ZoneGraph authoring without Mass, sleeping-car wake on AI throttle,
  SignificanceManager fit, ADAS cruise control quality, N-car perf).
- **CONFLICTS:** §7, C1–C14. Blocking for later phases: C3 (no roads — manual authoring before Phase 2), C5 (no
  siren/light assets — manual), C4 (no traffic AI — stimuli only).
- **PROPOSED ARCHITECTURE:** §8.
- **FILES CREATED:** `Docs/POLICE_VEHICLE_AI_RECON.md` (this), `Docs/MURDAR_POLICE_VEHICLE_AI_SPEC.md` (spec text),
  `Docs/POLICE_VEHICLE_AI_TUNING_LOG.md` (pre-Phase-0 fix), `Tools/circletest.py`.
- **FILES MODIFIED (pre-Phase-0 bug fix, 2026-09-19, documented in the tuning log):**
  `Source/Murdar_GameDev/AI/VehiclePursuitComponent.h/.cpp` — speed-scaled steering lock, arc cap, Block lane change,
  Block swing across a stopped target.
- **TESTS RUN:** circling repro (behind 25/80 m, flank 40/60 m), moving chase 0→95 km/h, `DriveTo` 134 m at 90 km/h;
  all in the tuning log with numbers. No Phase 0 code to test.
- **GATA / NOT GATA:** Phase 0 **GATA** — every API named is cited from a header on disk or already compiled; unknowns
  are listed as unknowns, not assumed. Phase 1 may start; Phase 2 is blocked on manual road authoring (C3).
