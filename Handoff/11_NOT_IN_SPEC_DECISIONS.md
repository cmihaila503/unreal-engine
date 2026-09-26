# 11 — Missing and not in the spec: decisions and proposals

The spec never asks for these. Local Claude: **do not implement any of them without the user's OK** — present
each proposal, get a yes/no, then record the decision in `Docs/POLICE_VEHICLE_AI_ARCHITECTURE.md` (new §8
"Decisions outside the spec"). IDs match `Docs/POLICE_VEHICLE_AI_GAP_ANALYSIS.md` part C.

## C1 — Suspect (fleeing NPC) driver — prerequisite of Phase 11

Spec §27–30 need a criminal car that flees; only the police side is specified.

Proposal:
- `AMurdarSuspectAIController : AAIController`, possesses an `AMurdarVehicle`, reuses `UVehiclePursuitComponent`
  in `DriveTo` (one driver implementation, spec RECON §2.3 "do not run two drivers").
- Flee planner at 2 Hz over the lane graph: at each junction ahead, score exits by (distance from the known police
  units − their ETA to that exit) + a preference for exits with more branches; pick the best, with a *Nerve01*
  value adding randomness (low nerve = worse choices, runs red lights, crashes more).
- Knowledge: the suspect only knows the police units it sees or hears (sirens are hearing stimuli since Phase 6) —
  same rule as the police (spec §1.17, symmetric).
- Bail-out: stopped/boxed/disabled or nerve < threshold → exits the car on foot → `AMurdarNPCAIController` in Flee
  (the Phase 12 handoff, suspect side).
- Memory: needs RECON C7 first (faction track per target handle, not player-only).

## C2 — Save / load during a pursuit

Facts (PROJECT_OVERVIEW §4.1): saves are tags and numbers only (`UNarrativeStateSubsystem`, < 10 KB, no object
refs); heat (`Stat.Heat`) and bribes are mirrored in; `MurdarReach` / checkpoints **autosave**.

Proposal: pursuits are not saved (they can't be — no object refs by design). A checkpoint autosave reached while a
unit is in Pursuit/Combat/Searching on the player is **deferred** until the pursuit ends, not dropped (blocking it
could lose story progress). Loading clears all police AI state; heat and bribes come back from the save as today.

## C4 — Police collateral (civilians, pedestrians)

Proposal: pedestrians on the road are a hard obstacle class for the whiskers (never "suppressed as traffic");
PIT/Ram/SideSweep abort if a pedestrian is within the risk zone (spec §21 already says it for PIT); a police-caused
collision publishes `Event.Police.Collateral` (not a crime, not heat for the player). Witnesses may react later.

## C5 — Player-facing feedback

Constraint (PROJECT_OVERVIEW §1, §5): the project's rule is **"felt, not shown" — no bar, no number**. A wanted-level
HUD or a minimap search circle would break it; do not propose them.

Proposal within the rule: the player reads the police through the world — sirens (Phase 6), audible radio barks on
the existing `Event.Police.*` tags via `AMurdarHUD::ShowSubtitle` + a voice line ("unitatea 4, suspect pierdut pe
…"), the tension meter reacting to `Event.Police.PursuitStarted/Combat` (it already listens to the bus), and the
officers' own behaviour (lights, pulling alongside). If the user wants any explicit indicator, that's their call.

## C6 — Hiding / swapping cars

Proposal: `Zone.Cover` zones (garages, tunnels): a parked, empty car inside one cannot be confirmed by sight during
Searching. Swapping cars already works through `MatchesKnownVehicle()`; add a test for it.

## C7 — Surface grip, weather, night

Prerequisite (PROJECT_OVERVIEW §10): `UPhysicalMaterial` friction per surface (asphalt/gravel/mud) is **not
authored yet** — the wheel raycast already reads it. Do that first (manual asset task).

Proposal: the planner's grip follows the surface under the car: `LateralGripCms × (surface friction / asphalt
friction)`, same for `BrakingDecel`; weather later as a global multiplier on top. Sight radius × a light-level factor
at night. Measure with `griptest.py` per surface before choosing any number.

## C8 — World Partition / streaming

Proposal (decide before the real city map): LOD 3 units move along the lane graph as data (lane + distance + speed),
no actor physics; when their cell streams in they are placed on the lane at that point, never in the player's
view (spec §29). Requires the ZoneGraph data to be available for unloaded cells — **unknown, verify in 5.8** before
committing to this.

## C9 — Headless test runs

Proposal: `Tools/run_suite.ps1` → starts `UnrealEditor-Cmd.exe Murdar_GameDev.uproject <map> -game -unattended
-nullrhi -log` … ADAPT: the current rigs drive PIE through the Unreal MCP + Python bridge, which needs the editor.
Option A: keep the editor, add `policetest.py suite all` that runs everything and writes the report table.
Option B: port the rigs to UE Automation tests (`IMPLEMENT_COMPLEX_AUTOMATION_TEST`, latent commands) runnable with
`-ExecCmds="Automation RunTests Murdar.Police;Quit"`. A is a day; B is a week and is what makes Phase 14 repeatable.

## C10 — Deterministic tests

Proposal: every random choice in the AI takes an `FRandomStream` seeded from the unit id + a test seed
(`Murdar.AI.Seed` cvar); tests run at a fixed frame rate (`t.MaxFPS 60`, `-benchmark -fps=60` in headless runs);
criteria are "N of N runs" (3/3 today, 5/5 for Phase 14).

## C11 — Difficulty

Proposal: a difficulty setting chooses the profile mix of spawned units and scales heat gain; no separate code path.

## C12 — Siren concurrency

The siren is still silent (EDITOR_TASKS 8b). All project audio today is synthesised placeholder WAVs made by the
`Content/Python/synth_*.py` scripts (PROJECT_OVERVIEW §6) — a `synth_siren.py` in the same style (two-tone
Romanian-era "wail"/"hi-lo" sweep, loopable) unblocks Phase 6's audio with no licensing issue; it creates a *new*
asset, which the project already allows.

Then: a `USoundConcurrency` (max 4, stop quietest) on the siren loop; Doppler on the attenuation.

## C13 — Off-duty police obey traffic rules

Proposal: Patrol/Returning use the traffic controller's rules (junction reservations from 08, speed limit, no
navmesh shortcuts); only emergency relaxes them. Write it into ARCHITECTURE §3 Patrol row.

## C14 — Multiplayer — resolved

Single-player, `bReplicates = false` everywhere (PROJECT_OVERVIEW §1). Add one line to ARCHITECTURE §1:
"Single-player; no replication. All AI runs locally on the game thread." Nothing else to do.
