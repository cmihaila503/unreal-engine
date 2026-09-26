# MURDAR — project handoff

Written for a fresh Claude Code session with no prior context. Everything below was read out of the repository
(source, config, assets, git history) on 2026-09-16. Where something is a placeholder or missing, it says so.

---

## 1. What this is

A third-person, story-driven crime game in the GTA mould, set in what the code's own comments place in 1990s
Romania (the player character is called **Mihai**; cars are a Dacia and a Golf 2; the police take bribes, and the
sector/bribe system is a first-class mechanic). Single-player, no networking (`bReplicates = false` everywhere).

Two pillars are already built and playable in a sandbox level:

- **On foot** — GASP motion-matching locomotion, two weapon slots with full-body armed locomotion, a melee brawl
  with stamina, health/death/respawn.
- **In a car** — a custom physics vehicle with a deliberate "feel layer", enter/exit, and a police AI that patrols,
  pulls you over, takes a bribe, chases, PITs and rams.

Over the top of both sits a **narrative director**: chapters, checkpoints, gameplay-tag facts, authored triggers,
saves, and a hidden tension meter.

**Design philosophy visible throughout the code:** systems are event-driven and tick as little as possible; state
is data (gameplay tags + Data Assets), not code; feedback is *felt* rather than displayed ("no bar, no number" —
honor and stress are deliberately hidden from the UI, and honor is even compiler-locked so only the director can
write it).

### Two design documents referenced but NOT in the repo

The source comments repeatedly cite two documents that do not exist anywhere in this repository:

- a **"manifest"** with numbered sections — §6.4 (the brawl), §6.5 (vehicle feel layer), §6.9 (director cheats),
  §9 (version stamp on the HUD)
- **`DEPENDENCIES.md`** — §7 (two weapon slots, no inventory), §10 (tick discipline)

If you need the original intent behind a decision, ask the user for these. They are the authority the code defers to.

---

## 2. Tech stack

| | |
|---|---|
| Engine | Unreal Engine **5.8** (`C:\Program Files\Epic Games\UE_5.8`) |
| Module | one runtime C++ module, `Murdar_GameDev` (~15,100 lines across 9 folders) |
| Rendering | Lumen software (`r.RayTracing=False`, `r.Substrate=False`), VSM on, DX12/SM6 |
| Physics | **Chaos async physics** at a fixed ~90 Hz step; classic substepping deliberately OFF |
| Version control | Git + Git LFS. `ProjectVersion=0.1`, `ProjectName=MURDAR` |
| Default map | `/Game/Murdar/Maps/L_Sandbox` with `GM_Sandbox` |

### Plugins

**`KinetiForge`** (in `Plugins/`, pinned at upstream `df4fbf2`, © Zhengyi Miao) is the big one: a **complete custom
vehicle physics system, not a wrapper over Chaos Vehicles**. It has its own engine, clutch, gearbox, differential,
axle, wheel, suspension and aero components, a curve-based tire model with combined slip, ABS and TCS, plus an
async-tick spring arm. It ships `AsyncTickPhysics` as a second module. Treat its `Content/` and template assets as
third-party: subclass, don't edit (see §6).

Epic plugins enabled: PoseSearch, Chooser, MotionWarping, AnimationWarping, AnimationLocomotionLibrary,
AnimationLayering, CurveExpression, RigLogic, HairStrands, SmartObjects, GameplayInteractions, GameplayCameras,
Mover, NetworkPrediction, MovieSceneAnimMixer, DrawDebugLibrary, ModelingToolsEditorMode — plus a
ModelContextProtocol / MCP toolset group.

### Build dependencies (`Murdar_GameDev.Build.cs`)

`Core CoreUObject Engine InputCore EnhancedInput GameplayTags PoseSearch Chooser EngineCameras AssetRegistry
KinetiForge AsyncTickPhysics Slate SlateCore EngineSettings AIModule GameplayTasks NavigationSystem
DeveloperSettings`, and editor-only `UnrealEd AssetTools EditorScriptingUtilities AnimationBlueprintLibrary`.
`PublicIncludePaths` includes the module root, so includes are written `"Weapons/..."`, `"Character/..."` etc.

---

## 3. How to build, run and script it — read this before anything else

### Build

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" \
  Murdar_GameDevEditor Win64 Development \
  -Project="E:\Unreal Engine\Murdar_GameDev\Murdar_GameDev.uproject" -WaitMutex
```

Incremental builds take ~50 s. Binaries, `DerivedDataCache`, `Intermediate` and `Saved` live in the main checkout
only — **never launch the editor from a git worktree**, it means a full rebuild plus every shader from scratch.

### Launch

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" \
  "E:\Unreal Engine\Murdar_GameDev\Murdar_GameDev.uproject"
```

### Drive the running editor from the shell — `Tools/uepy.py`

This is the single most useful thing in the repo. `Tools/uepy.py` executes Python **inside the running editor** over
PythonScriptPlugin remote execution (enabled in `DefaultEngine.ini`, multicast `239.0.0.1:6766`). Binary `.uasset`
files, Blueprint defaults, Data Assets, curve assets, PIE and live physics measurement are all scriptable.

```bash
python Tools/uepy.py path/to/script.py      # run a file in the editor
python Tools/uepy.py -c "import unreal; print(1)"
```

Running `python script.py` directly fails — there is no `unreal` module outside the editor.

**API traps that will cost you time if you don't know them:**

1. **Bool properties drop the `b`.** `bDisableTractionControl` → `disable_traction_control`.
2. **Iterating an `unreal.Array` of structs yields copies.** `for ax in axles: ax.set_editor_property(...)` silently
   does nothing. You must write back: `axles[i] = ax`, then `obj.set_editor_property("axles", axles)`. This failure
   is invisible — always read values back to verify.
3. **`EditorAssetLibrary` misbehaves during PIE.** `load_asset()` returns None and `does_asset_exist()` lies. End
   PIE before editing assets.
4. **`UCurveFloat.FloatCurve` is not editor-exposed**, so curve keys cannot be set via `set_editor_property`.
   Author curves by **CSV import** (`AssetImportTask` + `CSVImportFactory`, `import_type = ECSV_CURVE_FLOAT`).
   The CSV needs a header row (`,Value`) or the import silently produces nothing, and the importer **zeroes the
   first data row's value** — put a sacrificial `0.00,0.00` row first.
5. **`AssetImportTask.save = True` does not write to disk.** Call `EditorAssetLibrary.save_loaded_asset(obj)`.
   `unreal.load_object(None, "/Game/.../X.X")` finds an unsaved package when `load_asset` won't.
6. **First PIE start blocks the editor** for a minute or two compiling shaders; the process shows
   `Responding=False` and the bridge stops answering. Poll `is_in_play_in_editor()`, don't assume a crash.

**Measuring anything over time:** a Python loop blocks the game thread and stops the world ticking. Instead, split
across separate `uepy.py` calls — call 1 registers `unreal.register_slate_post_tick_callback(fn)` writing samples
into `builtins.SAMPLES` (globals persist between calls); sleep in the shell; call 2 unregisters and reports.

PIE control lives on `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)`:
`editor_request_begin_play()`, `editor_request_end_play()`, `is_in_play_in_editor()`.

**Never save the level from a script.** `L_Sandbox` usually carries uncommitted work. PIE does not dirty it, but
`save_all_dirty_levels()` / `save_current_level()` would. Save named assets explicitly.

### Test scripts already written (`Tools/`)

| Script | What it does |
|---|---|
| `uepy.py` | the bridge (above) |
| `chase.py` | pursuit scenarios: `setup`, `open X Y`, `to X Y`, `jump`, `go <steer> <secs> <throttle>`, `obstacles`, `watch`. Prints cop state, pursuit mode, limiter, throttle/brake/steer, heading error, route length |
| `crew.py` | police crew / dismount state |
| `hit.py` | run-over and impact tests |
| `navtest.py` | navmesh path checks |
| `radio.py` | faction-memory / shared-knowledge inspection |

---

## 4. Architecture by module

Everything lives under `Source/Murdar_GameDev/`.

### 4.1 `Director/` — the narrative spine

The backbone the rest of the game hangs off. Five subsystems plus data assets.

**`UGameEventSubsystem`** (GameInstance) — a game-wide message bus keyed by **gameplay-tag hierarchy**. Publishers
don't know subscribers. Subscribing to `Event.Combat` receives `Event.Combat.Shot` and `.Hit`. Synchronous, game
thread only, no allocation on publish; unsubscribing from inside a callback is deferred to the end of the publish.
`FGameEvent` carries `{Tag, Source actor, Location, Magnitude, Payload tag}`.

**`UNarrativeStateSubsystem`** (GameInstance, survives map changes and pawn swaps) — the story's memory:
- `Facts` — a `FGameplayTagContainer`; presence = true (`Fact.Chapter1.TookBribe`)
- `Values` — tag → float (`Stat.Stress`, `Stat.Honor`, `Stat.KmDriven`, `Stat.Heat`)
- the chapter/checkpoint cursor, the player's carried weapons and reserve ammo
- Save/Load to slots (default `"auto"`), versioned (`Version = 1`), atomic writes, < 10 KB, tags and numbers only
  (no hard object refs) so saves survive code changes
- Every mutation publishes `Event.State.*`
- **`FHonorToken`** is a capability token whose constructor is private with `UChapterDirector` as the only friend —
  so hidden honor can be written *only* by authored story beats. The "no bar, no number" rule enforced by the
  compiler.

**`UChapterDirector`** (World) — applies a chapter to the running world: places the player at a checkpoint, hands
out the kit, sets entry facts, spawns the chapter's car, and answers `IsVerbAllowed(tag)`. Three entry modes:
`Fresh` (story start — facts + starting kit), `Jump` (debug/checkpoint, keep what you carry), `Restore` (from save).
Handles map changes: `EnterChapter()` on a chapter in another level records the target and opens the level; the new
world's director picks it up in `OnWorldBeginPlay`. A level with no pending chapter enters the first chapter that
lists it, so pressing Play in `L_Sandbox` still gives you the sandbox kit.

**`UZoneTriggerSubsystem`** (World) — evaluates the current chapter's authored triggers. Purely event-driven, no
tick: re-evaluates arming on every bus event, fires armed triggers on their `FireOnEvent`. Also owns the
**distance clock**: samples the player pawn at 10 Hz, accumulates `Stat.KmDriven`, publishes `Event.Player.Moved`.

**`UTensionSubsystem`** (World) — the hidden stress meter, 0..100. Listens to the bus (shots, damage, deaths,
crashes), adds stress above 85 km/h, decays after 5 s of calm (slower above 60). Output is **felt, never shown**:
a post-process blend via `UTensionCameraModifier` (vignette, desaturation, grain, chromatic aberration), a heartbeat
loop and a drone whose volume tracks the value. Bands `Calm / Uneasy / Tense / Panic` publish
`Event.Tension.Changed` for dialogue to read. Written into `Stat.Stress` so it saves.

**Data assets:**
- `UChapterDefinition` (`DA_Chapter_*`, primary asset type `"Chapter"`) — level, `AllowedVerbs`, `FactsOnEnter`,
  `StartingKit` + `StartingAmmo`, `VehicleClass`, `Checkpoints[]`, `Triggers[]`, `Order`
- `FCheckpointDefinition` — name, `PlayerStartTag`, `VehicleStartTag`, facts to set/clear, optional loadout override
- `FTriggerDefinition` — **arms** when its conditions hold (required/forbidden facts, zone presence, `MinKmDriven`)
  and **fires** on the first matching `FireOnEvent` (or immediately if empty). Firing writes facts, may reach a
  checkpoint, and always publishes `Event.Trigger` with its own `Trigger.*` tag as payload. Player-initiated by
  construction: a trigger never takes control, it only says "now this is true."

**`AZoneVolume`** — a named box. While the player's pawn (foot *or* car) is inside, the zone's `Zone.*` tag is a
fact, and `Event.Zone.Entered/Left` fire. No logic beyond that.

**`MurdarTags.h`** — native gameplay tags: the full `Event.*` tree, `Tension.*`, the police set
(`Wanted.*`, `Crime.*`, `Police.Line.*`, `Sector.*`), `Verb.*` (Fire, Aim, DropWeapon, PickupWeapon, Traversal,
Sprint, Drive, Melee), `Stat.*` and code-set `Fact.*`. Content-authored tags live in `Config/DefaultGameplayTags.ini`.

**`UDirectorCheats`** — see §8.

### 4.2 `Character/` — the player

**`AMurdarCharacter`** — C++ base; the GASP Blueprint `SandboxCharacter_CMC` is **reparented** to it. Locomotion and
camera stay in the Blueprint; this class owns the gameplay components (weapon, health, stamina, melee) and binds
weapon/melee input on top of whatever the Blueprint binds, via its own `IMC_Weapons` mapping context added at
higher priority whenever a player controller possesses it — and removed when another controller takes over (so
weapon keys never leak onto the car).

Also owns **`Knockdown(velocity, seconds)`**: hit by a car → full ragdoll carrying that velocity, then back on his
feet if still alive and the body has come to rest. Nothing animated — the physics asset rides the bonnet, slides off
the roof and rolls into the gutter on its own. Death → player returns to the checkpoint, NPC is removed after
`CorpseSeconds`.

**`UMurdarAnimInstance`** — C++ base for the GASP AnimBP. Everything gameplay-driven that animation binds to:
- `WeaponOverlay` (0 unarmed / 1 pistol / 2 rifle) read by the chooser tables' Float Range column;
  `DidWeaponOverlayChange()` makes motion matching re-search immediately on a swap
- `AimAlpha`, `AimSpineRotationHalf` (applied twice, on spine_03 and spine_05), `AimPitch/Yaw` with limits
- `LeftHandIKLocation` in **hand_r bone space** — constant per weapon, computed once on equip, so no per-frame
  socket lookups and no one-frame lag
- `LowReadyAlpha` + per-weapon arm/elbow rotations (muzzle drops when armed and not aiming)
- a procedural melee pose (guard / windup / punch rotations + torso yaw) standing in until montages exist

Critically: this class **pulls** its state in `NativeUpdateAnimation`; gameplay never pushes into an AnimInstance,
so the parallel anim update never races a game-thread writer.

**`AMurdarHUD`** — two layers: a Slate widget for panels and a thin canvas pass for crosshair + hit marker. Owns the
subtitle queue (`ShowSubtitle`). Version stamp is `ProjectVersion` + compile date.

### 4.3 `UI/`

**`SMurdarHudWidget`** + **`FMurdarHudModel`** — pure Slate, no UMG asset. Shows speed/gear while driving, ammo
while armed, a stamina bar while fighting, the interaction prompt, subtitles and the version. The HUD fills the
model each frame from the pawn; the widget only reads it.

### 4.4 `Weapons/`

**`UWeaponDefinition`** (`DA_Weapon_*`, primary asset type `"Weapon"`) — everything static: kind, slot, mesh and
sockets (`grip_r` on the weapon aligned to `weapon_r` on the hand, `grip_l` for the left-hand IK target,
`b_gun_muzzleflash` muzzle, `BarrelAxis` because the bundle's rifles point down +Y and the SMG down +X), holster
socket, ballistics (damage, range, RPM, automatic, spread + aim/moving multipliers, pellets), a full recoil block
(pitch/yaw kick, max climb, recovery fraction and speed, aim multiplier, camera shake), ammo (type, magazine,
starting reserve, reload time, auto-reload), montages, sounds, impact decal and muzzle-flash light.

**`UWeaponComponent`** (on the character) — carries **up to two weapons, one per slot; there is no inventory**
(`DEPENDENCIES.md §7`). The ammo model is the important part: **the magazine lives on the weapon actor** (so a
dropped gun travels with exactly the rounds in it), while the **reserve is per `EAmmoType` on the carrier**. For the
player, `BindSharedAmmo` routes the reserve into the narrative state, so it survives pawn swaps, map changes and
saves. Handles equip/holster toggle, automatic vs semi fire, auto-reload on an empty trigger pull, reload cancel
with refund, drop (G) and pickup (E) with an 8 Hz focus scan over a pickup registry, and recoil that compensates
for player input so the automatic return never overshoots below where the player already aimed.

**`AMurdarWeapon`** — the instance in the world: mesh, magazine, fire, reload timer, muzzle-flash point light,
floating "E" prompt. Dropped and placed weapons are the same actor with physics on.

**`AAmmoPickup`**, **`UWeaponPickupSubsystem`** (per-world registry of ground weapons).

### 4.5 `Combat/` — the brawl (manifest §6.4)

**`UMeleeComponent`** — a small state machine: `Idle → Windup → Active → Recover`, plus `Dodge` and `Staggered`.
Light and heavy attacks each have a full `FMeleeAttackSpec` (windup/active/recover seconds, damage, range, radius,
stamina cost, hitstop, knockback). Combos chain inside a 0.35 s window with a shortened windup, up to 3. A dodge
costs stamina and grants 0.35 s of melee invulnerability. A **counter** is a light attack pressed within 0.22 s
before an incoming blow lands — the component listens for `Event.Combat.MeleeTelegraph` on the bus to know one is
coming. **Hitstop** freezes both bodies for 60–100 ms on contact ("the whole feel", per the manifest). Ticks only
while something is happening.

**`UMeleeCameraModifier`** pulls the camera in 75 cm while fists are out and blends back when the fight ends.

**`UStaminaComponent`** — stamina instead of hit points for the brawl. Attacks, dodges and sprinting spend it;
standing still regenerates after a 1.2 s delay; melee hits drain it. Below 10 you are **exhausted** (no dodge, wide
open) until you recover past 35. Ticks at 10 Hz on a timer. **`UMeleeDamageType`** is damage that tires rather than
wounds — it drains stamina first, and only an exhausted target bleeds.

### 4.6 `Health/`

**`UHealthComponent`** — hit points for anything shootable. Listens to the owner's `OnTakeAnyDamage`, so
`ApplyPointDamage`/`ApplyDamage` land here. No tick. `bCountsAsKill` distinguishes a person from a prop (a player
kill sets `Fact.Player.Killed`). Broadcasts `OnHealthChanged` / `OnDied`.

**`ATargetDummy`** — shooting-range mannequin with HP over its head, ragdolls on death, stands back up after
`RespawnTime`, procedural flinch. With `bSparring` it becomes a **sparring partner**: telegraphs and throws a slow
punch every ~2.8 s that can be countered — the reference implementation for the melee counter window.

### 4.7 `Vehicle/`

**`AMurdarVehicle`** — a Pawn whose root is a simulating static-mesh chassis. KinetiForge does the physics (a drive
assembly generates the axles); this class does what the manifest calls the **feel layer**:
- **asymmetric steering** — 50 ms to full lock, 200 ms back to centre; the asymmetry is what makes it feel like a
  wheel rather than a switch
- **a camera that lies** — position lag on (rotation lag deliberately OFF so the look stays on the mouse), FOV
  widening with speed, the spring arm stretching with speed, and a yaw lean towards *what the hands asked* rather
  than what the car did
- mouse look around the car that recentres after 2 s of no input, but only while rolling
- engine audio whose pitch follows the **throttle pedal**, not RPM
- speed-scaled perlin camera shake (`UVehicleSpeedShake`)
- the seat: enter/exit with a floor-traced exit position, refusing to leave above 8 km/h, hiding and parking the
  driver in the seat and making him dormant (no motion-matching search, no movement update) while inside
- checkpoint/teleport handling that zeroes velocity, and a `FellOutOfWorld` recovery
- damage routing: a fraction of body damage reaches the driver (a car is thin cover, not armour)

It also exposes an **AI driving path** (`BeginAIDriving` / `ApplyAIInputs` / `EndAIDriving`) that bypasses the feel
layer entirely — the car has no hidden driver, the controller is the crew. Note `AMurdarVehicle::Tick` runs
**only while a human drives**; AI cars do not tick it.

**`UVehicleDefinition`** (`DA_Vehicle_*`, primary asset type `"Vehicle"`) — body mesh, mass, centre-of-mass nudge,
downforce coefficient, drivetrain component classes, axle layout, steering/shift curves, and the whole feel block
(steering in/out times, FOV at rest/speed, camera distance/pitch/lag, shake band, look sensitivity and limits),
audio, effects (slip threshold, dust and skid materials, crash thresholds), headlights, seat offsets.

**`UVehicleEffectsComponent`** — what the car does to the world: tyre dust (a ring of camera-facing quads on one
instanced mesh — no Niagara), skid marks (pooled deferred decals), rolling and screech loops, horn, the crash
reaction (shake, sound, `Event.Vehicle.Crashed`), **run-overs** (above ~12 km/h the chassis overlaps Pawn instead of
blocking it so a 70 kg man is not a wall; the victim ragdolls with the car's velocity and the car loses a little
momentum), and — added most recently — **speed-squared downforce**. This component ticks for both player *and*
AI-driven cars, which is why the downforce lives here rather than in `AMurdarVehicle::Tick`.

**`UVehicleSubsystem`** — per-world registry: "which car can I get into from here?" and the one place that spawns
vehicles so the Definition is applied before components register.

### 4.8 `AI/`

**`UFactionMemorySubsystem`** (World) — **what the factions know about the player, and how they learned it.** This is
the most interesting system in the project. It is deliberately *not* omniscient: knowledge enters only through
witnesses and bribes.
- Factions: `Civilian` (never shoots, but is a witness whose sightings reach the police), `Police`, `Hostile`
- `Heat` 0..100 decays at 0.6/s while out of sight and **not at all while they can see you**. Thresholds derive
  `EWantedLevel`: ≥12 `Stop`, ≥40 `Pursuit`, ≥75 `Lethal`
- `ReportCrime` takes heat from a table kept deliberately on one screen: speeding 6, reckless 10, hit pedestrian 30,
  hit police 25, weapon brandished 20, shots fired 35, assault 22, **murder 80**, evading 30, refused bribe 15.
  **Unwitnessed crimes never enter the list — that is the whole point of a getaway.**
- `FKnownVehicleRecord` — the car the police *believe* you're in, with a `bSeenSwitching` flag and a 120 s staleness
  window, so swapping cars unseen actually works
- `FBribeRecord` — per sector *and* per crew, with an expiry; mirrored into the narrative state so it saves
- **`FFactionTrack` — "the radio"**: one shared fix per faction, written by whoever has eyes (or was hit), read by
  every member. Members act on it as if they'd seen you, but only shoot at what they can actually see.
  `PredictTrack()` adds capped dead reckoning
- Keeps a cheap **mirror** of player state (armed, which car, speed) updated by bus listeners, so a sighting costs a
  struct read rather than a scene query. Ticks at 1 Hz; nothing per frame

**`AMurdarPoliceAIController`** — one patrol car's brain. Perception feeds the faction memory; a **4 Hz `Think()`**
reads it back and picks a state; the state hands orders to the pursuit component. Nothing per frame. States:
`Patrol` (waypoints by actor tag) → `Suspicion` (pull alongside, lights, "trage pe dreapta") → `TrafficStop`
(both stopped: bribe / weapon / ticket) → `Pursuit` (intercept, PIT when the window opens) → `Combat` (ram, or
officers out) → `StandDown` (paid: drive off and forget him). Also models a **crew** (default 2): officers dismount
one at a time when the player is on foot, and when the last one is out the car stops being a police car and the
player can take it. The decision function is a plain switch **on purpose**, so it can be lifted into a StateTree one
state at a time.

**`UVehiclePursuitComponent`** — combat driving. The controller decides *what*, this decides *how*: turns a mode +
target into pedals and steering every frame (one tick, a handful of vector ops, one forward trace). Modes: `Idle`,
`DriveTo`, `Follow`, `Intercept`, `PullAlongside`, `PIT`, `Ram`, `Block`, `Stop`. All geometry is computed **in the
target's frame** (longitudinal X ahead, lateral Y right) because the PIT window is defined relative to the target
car, not the world. Notable machinery:
- pure-pursuit steering with speed-scaled lookahead, corner speeds from arc curvature, braking-distance planning
- **five whiskers plus two side rails** at bumper height; reports distance, *closing speed* (a car ahead running at
  your speed is traffic, not a wall) and a steer bias away from side hits
- navmesh routing when the straight line is blocked, with an **escape manoeuvre** (three-point turn) when stuck
- an `EPursuitLimiter` enum reported in debug so you can see *why* a car is slow before blaming the pathfinder
- `Block` mode (four phases: behind → level → hold his lane → park across his nose) for targets too slow to PIT

**`AMurdarNPCAIController`** — anyone on foot. Reuses the player's own kit: the pawn is an `AMurdarCharacter`, so
weapons, ammo, reloads and hit reactions are literally the same code. States: `Idle`, `Engage`, `Reload`,
`SeekAmmo`, `Hide`, `Flee`. 5 Hz `Think()` + a light tick to steer the body. Firing is **bursts with a
distance-scaled aim error, never a laser** (2.5° at 10 m by default), with a 0.4–0.9 s reaction delay from first
sight. Falls back to the faction radio when it has no eyes of its own.

**`MurdarNav`** — thin navmesh wrappers for brains that steer their own bodies: `FindPath`, `PointAlongPath`
(pure-pursuit lookahead, no corner-hugging), `IsLineBlocked`. Two agents exist in project settings: **Human (r 40)**
and **Car (r 280)**, with dynamic runtime generation.

**`UMurdarAISettings`** (`DefaultGame.ini`) — which pawn an NPC is, what a cop/thug carries, the police vehicle
definition, corpse and player-death timings. **`UMurdarAILibrary::SpawnNPC`** is the spawn entry point.

### 4.9 `AnimTools/` — editor-only content pipeline

`UMurdarAnimTools` is a Blueprint function library callable from Python, compiled out of non-editor builds. It
exists because GASP selects motion-matching databases through **chooser tables**, and full-body armed locomotion
means cloning that whole tree twice. It can clone a PoseSearch database swapping every animation for its
`<Name>_Rifle` / `<Name>_Pistol` counterpart, clone a nested chooser tree remapping every database result, add a
Float Range column bound to `WeaponOverlay` plus rows, clone montages and blend spaces the same way, copy GASP
animation metadata (curves re-timed, notifies, sync markers, `PoseSearchBranchIn` notifies re-pointed) in a single
recompression bracket, and **author the GASP locomotion curves** (`movedata_speed`, `enable_warping`,
`contact_l/_r` from foot height and speed) on clips that have no GASP twin.

---

## 5. Cross-cutting conventions

- **Tick discipline.** Almost nothing ticks per frame. Timers (1 Hz faction memory, 4 Hz police Think, 5 Hz NPC
  Think, 8 Hz pickup scan, 10 Hz stamina and distance clock) or event-driven. Components enable their tick only
  while doing something (recoil recovery, melee phases, vehicle effects while driven).
- **The bus over pointers.** Systems publish `Event.*` and nobody holds a pointer to anybody else.
- **State is data.** Gameplay tags for facts and events, `UPrimaryDataAsset` for definitions (Chapter, Weapon,
  Vehicle — all three registered with the AssetManager and scanned from their folders).
- **Pull, don't push, into animation.** The AnimInstance reads gameplay state itself.
- **Felt, not shown.** Honor is compiler-locked and never displayed; stress drives post-process and audio only.
- **Comments explain *why*.** The codebase's comments are unusually good and frequently record the bug that
  motivated a line. Read them — they are the real design doc in the absence of the manifest.
- Source files are LF in git, CRLF in the working tree (git warns on checkout; harmless).

---

## 6. Content

`Content/Murdar/` is the project's own; almost everything else is Epic sample or marketplace content.

| Folder | What |
|---|---|
| `Murdar/Maps` | `L_Sandbox` — the only real level |
| `Murdar/Chapters` | `DA_Chapter_Sandbox` — the only chapter |
| `Murdar/Weapons` | `DA_Weapon_AK47`, `DA_Weapon_AR4`, `DA_Weapon_SMG11` |
| `Murdar/Vehicles` | `DA_Vehicle_Sedan`, `DA_Vehicle_Police`, `BP_Vehicle_Sedan`, `BP_Engine_Placeholder`, `BP_Engine_Police`, `BP_MurdarWheel`, `BP_MurdarRearAxle`, curves `C_AutoUpShift`, `C_AutoDownShift`, `C_EngineTorque` |
| `Murdar/Input` | `IMC_Weapons`, `IMC_Vehicle` + all the `IA_*` actions |
| `Murdar/Audio` | 17 **synthesised placeholder** sounds (engine, tyres, horn, crash, weapons, punches, heartbeat, tension drone) — generated by the `synth_*.py` scripts, no licensing issues |
| `Murdar/FX` | `M_BulletHole`, `M_DustPuff`, `M_SkidMark` |
| `Murdar/Anim/MM` | ~72 cloned PoseSearch databases, 68 montages, choosers and blend spaces — the Rifle/Pistol armed locomotion tree |
| `Blueprints/` | GASP sandbox: `SandboxCharacter_CMC` (reparented to `AMurdarCharacter`), `SandboxCharacter_CMC_ABP` (reparented to `UMurdarAnimInstance`), `GM_Sandbox`, `PC_Sandbox`, traversal/camera/movement-mode helpers |

Third-party content: `Characters/UEFN_Mannequin` + `UE5_Mannequins` (GASP sample), `Anims/_FixedPistol` and
`_FixedRifle` (~1,300 clips from a Pistol & Rifle locomotion pack), `FPS_Weapon_Bundle` (weapon meshes),
`Audio/Foley` (MetaSound foley), `Levels/LevelPrototyping`.

`Content/Python/` holds the one-shot content-build scripts (`build_armed_locomotion.py`,
`build_armed_traversal.py`, `copy_anim_metadata.py`, `retarget_mocap.py`, `make_weapon_montages.py`,
`calibrate_grips.py`, `setup_vehicle_content.py`, `setup_police_content.py`, `setup_combat_content.py`, the
`synth_*` sound generators). Most are idempotent and documented in their own docstrings.

---

## 7. Configuration worth knowing

**`DefaultEngine.ini`**
- **Physics**: `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.011111` (~90 Hz), `bSubstepping=False` —
  KinetiForge's README requires async physics; the comment reads "async > substepping".
- **Low-VRAM block** (`[SystemSettings]`): pool size 500, Lumen surface cache atlas 2048, VSM 2048 pages, Nanite
  streaming pool 256. The comment explains why: the RTX 3060's 12 GB is **shared with a local Qwen model**. Flip
  back when the look is being tuned.
- **Navigation**: two agents (Human r40, Car r280), dynamic runtime generation, tile 2000 / cell 25.
- A long block of KinetiForge class/struct/enum/property **redirects** — do not delete, they keep older assets
  loading after the plugin's renames.
- Rendering: Lumen SW, no ray tracing, no Substrate, VSM on, DX12/SM6.

**`DefaultGame.ini`** — `ProjectVersion=0.1`; the three AssetManager primary-asset scans (Chapter / Weapon /
Vehicle); `UMurdarAISettings` values (police car = `DA_Vehicle_Police`, NPC pawn = `SandboxCharacter_CMC`, police
weapon = SMG11, hostile = AK47, corpse 20 s, player death 3 s).

---

## 8. Console cheats and cvars

All cheats are `UFUNCTION(Exec)` on `UDirectorCheats`, installed on every cheat manager and compiled out of
shipping. Names have no dots because Exec functions can't contain them.

```
MurdarChapters                     list chapters + checkpoints
MurdarChapter <name|order> [cp]    enter a chapter
MurdarCheckpoint <name>            jump inside the current chapter (keeps what you carry)
MurdarReach <name>                 reach it "naturally" (facts + autosave)
MurdarFact +Tag | -Tag             set / clear a fact
MurdarValue <Tag> <number>
MurdarState                        dump narrative state
MurdarSave [slot] / MurdarLoad [slot] / MurdarContinue [slot]
MurdarKit                          re-apply the chapter's starting kit
MurdarAmmo <Pistol|Rifle|Shotgun> <n>
MurdarGod                          toggle invulnerability
MurdarCar [Definition]             spawn the chapter's car 5 m ahead   (MurdarExit leaves it)
MurdarDrive <throttle> [steer] [seconds]    hold the pedals (negative throttle = brake/reverse)
MurdarStress <0..100>              set the tension meter
MurdarSay <text>                   show a subtitle
MurdarPolice [metres]              spawn an AI patrol car behind the player
MurdarHeat <0..100>                set police heat
MurdarBribe [amount]               pay the nearest crew
MurdarFactions                     dump faction memory + police states
MurdarNPC [Hostile|Police|Civilian] [m]     spawn an armed person ahead
MurdarTriggers                     list the chapter's triggers (armed/fired)
MurdarZones                        list zone volumes and whether the player is inside
```

Cvars (all `ECVF_Cheat`):

```
Murdar.Pursuit.Debug   draw aim points and the PIT window
Murdar.NPC.Debug       draw NPC goals, log states
Murdar.Melee.Debug     draw hit sweeps, log phases
Murdar.Melee.Pose      override the punch pose (six comma-separated rotations)
Murdar.Melee.PoseHold  freeze the melee blend at a phase alpha for screenshots
Murdar.Weapon.Debug    per-shot debug lines and impacts   (defaults to 1 while prototyping)
Murdar.Weapon.Hud      on-screen ammo/weapon debug text
Murdar.Vehicle.Hud     drivetrain/feel readout while driving
Murdar.Tension.Hud     stress readout
```

---

## 9. How it was built (commit history as a feature timeline)

```
0e40d67  UE 5.8 blank C++ project, Lumen SW, no Substrate/RT, Git LFS
98d6d9b  GASP locomotion (CMC character, UEFN mannequin anims, PoseSearch DBs) + L_Sandbox
5517f3a  Weapons + full-body armed locomotion (pistol/rifle) on GASP motion matching
586d2f6  Aim state, over-the-shoulder aim cameras, AK-47, aim-pose scaffold
16d65f2  Two-handed hold: left-hand Two Bone IK to grip_l
4c5226e→4ec7a5b  Socket-based hand placement + grip calibration scripts
ba85d6b→6ffca05  Low-ready hold, per-weapon low ready, upper-body aim steering
882025a  Weapons/anim review pass (IK lag, per-weapon cache)
736b1f2  Ammo types, timed reload, auto-reload, dry fire, ammo pickups, debug HUD
023e23b  Drop (G) / pick up (E) with prompt, same-slot swap, level-placed weapons
ec44f42  Ammo inventory on the character; weapons keep only their magazine
f06cbe9  Review pass 2: ground-weapon registry, exact reload accounting
ea2dcd7  Recoil + camera shake, sounds, muzzle flash, bullet holes, spread; Health + target dummies
a6e3317  Architecture audit: pull-model anim state, pickup subsystem, IMC lifecycle, fire clock
4ba4a8e  Game Director: narrative state + save/load, chapters, checkpoints, event bus, cheats
db4009a  Director audit fixes (bus reallocation UB, atomic saves, entry modes, AssetManager)
a0e45f5  Zones & triggers, distance clock
3099614→3aff86f  Orient-to-movement vs strafe; camera fix, crosshair, aim at crosshair point
e37adc6  161 more pack clips in the motion-matching databases
ea3f0da  Integrate KinetiForge (pinned df4fbf2)
ed89622  Vehicle system: KinetiForge car with feel layer, enter/exit, Director hooks
52fcb55  Wake chassis after spawn hitch, brake/reverse, Vehicle primary asset type
c0c1f0f  Speed camera shake, mouse look, dormant driver, fell-out recovery, drivetrain tuning
8c94d88  Vehicle effects, impacts and the tension meter
862e525  Slate HUD, melee + stamina, sparring dummy
28c2fd3  Police AI: faction memory, patrol-car controller, vehicular pursuit component
3e04458  Foot AI: armed NPCs, police dismount, run-over kills, player death/respawn
5ad31d9→e63fea9  AI logic fixes; Combat re-evaluated every Think
e9bbe9a  Shared faction knowledge (the radio)
90807d4  Police crew, physical run-overs, low-VRAM cvars
961b85b  AI navigation: navmesh routes, whisker avoidance, escape manoeuvre
770cea4  Cars drive like cops; in-car camera free and instant
e1f9594  Patrol car: real engine, side rails, no corner cutting
e4f4567  Pursuit: steer on curvature, brake on closing speed, stop losing the player
3c14052  Vehicle feel: damped body, real torque curve, downforce, spec mass
```

---

## 10. Current state — what works, what's placeholder, what's open

### Works end to end
Foot locomotion with armed overlays; two-slot weapons with the full ammo/reload/drop/pickup loop; melee with
stamina, combos, dodge, counter and hitstop; health, death, ragdoll, respawn; driving with the feel layer; police
patrol → traffic stop → bribe → pursuit → PIT/ram → stand down; foot NPCs that fight and seek ammo; the faction
radio; chapters, checkpoints, facts, triggers, zones, saves; the tension meter; the Slate HUD.

### Placeholder / scaffolding
- **All audio** is synthesised placeholder WAVs.
- **Melee has no animation assets** — the state machine drives a procedural arm pose; montages plug into the same
  phases when they exist.
- **Reload timing** is a number on the definition, not a montage length.
- `BP_Engine_Placeholder` is named that for a reason.
- Only **one level** (`L_Sandbox`) and **one chapter** (`DA_Chapter_Sandbox`) exist. There is no city, no traffic,
  no civilians beyond what the cheats spawn, no dialogue system (the bus and subtitle API are ready for one), no
  menus, no economy behind `MurdarBribe`.
- `L_Sandbox` and `SandboxCharacter_CMC` carry uncommitted local edits in the working tree.

### Known open items in the vehicle physics (most recent work)
The car was tuned against a GTA IV × GTA V hybrid spec the user supplied. Done and measured: damping raised from
0.15/0.30 to 0.50/0.72 (a drop test now settles in ~1 s with a single sub-centimetre overshoot, where before it
wallowed); a real mid-range-peaking torque curve replacing a flat 0.5 curve that made both engines produce half
their rated torque at every RPM; rear anti-roll bar 90 vs front 200; mass 1400 → 1550 kg; centre of mass 12 cm
lower; TCS off on the drive axle; downforce ∝ speed².

**Still open:** there is **no wheelspin off the line** — at 5400 rpm and full throttle all four wheels report zero
slip, so the clutch is absorbing the launch. That points at `FVehicleClutchConfig::Capacity` or the definition's
`AutoClutchRpmRange`, not at grip. Also untouched: `UPhysicalMaterial` `Friction` values per surface
(asphalt/gravel/mud — the wheel raycast already reads them, the assets just aren't authored) and the wall
material's `Restitution = 0`.

Wheel and axle tuning deliberately lives in **project-owned subclasses** (`BP_MurdarWheel`, `BP_MurdarRearAxle`)
rather than edits to KinetiForge's template assets under `/KinetiForge/Template/Vehicles/FWD/Sedan/`, so a plugin
update won't overwrite it. Keep it that way.

---

## 11. Gotchas for a new session

1. **The user works in Romanian.** Match their language.
2. **Read the code comments before assuming.** They record the specific bug behind most odd-looking lines.
3. **`AMurdarVehicle::Tick` runs only while a human drives.** Anything that must also apply to AI cars belongs in
   `UVehicleEffectsComponent::TickComponent`, which ticks whenever `SetDriven(true)` — from both `Enter()` and
   `BeginAIDriving()`.
4. **Changing a C++ `UPROPERTY` default does not change existing assets** that already serialise their own value.
   `MassKg`'s default was changed from 1100 to 1550 and had *zero* effect, because both Data Assets stored 1400.
   Always verify through the editor (see §3) rather than assuming a default propagated.
5. **Don't edit KinetiForge template content.** Subclass into `/Game/Murdar/Vehicles/`.
6. **Don't save the level from a script** — see §3.
7. The AI decision functions are plain switches **on purpose** (StateTree migration path). Don't "improve" them into
   something clever without asking.
8. Two design documents the code cites (the manifest, `DEPENDENCIES.md`) are **not in the repo** — ask for them.
