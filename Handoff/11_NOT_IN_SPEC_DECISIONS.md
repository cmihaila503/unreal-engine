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

Proposal: pursuits are not saved. Saving is blocked while any unit is in Pursuit/Combat/Searching on the player;
loading clears all police AI state except heat and bribes (already saved). One line in the save code + a HUD
"can't save now" message.

## C4 — Police collateral (civilians, pedestrians)

Proposal: pedestrians on the road are a hard obstacle class for the whiskers (never "suppressed as traffic");
PIT/Ram/SideSweep abort if a pedestrian is within the risk zone (spec §21 already says it for PIT); a police-caused
collision publishes `Event.Police.Collateral` (not a crime, not heat for the player). Witnesses may react later.

## C5 — Player-facing feedback

Proposal: HUD wanted indicator from `EWantedLevel`; a search circle on the minimap from the police's *belief*
(`PredictTrack` + search legs), never the truth; radio barks on the existing `Event.Police.*` tags. UI assets are a
manual task.

## C6 — Hiding / swapping cars

Proposal: `Zone.Cover` zones (garages, tunnels): a parked, empty car inside one cannot be confirmed by sight during
Searching. Swapping cars already works through `MatchesKnownVehicle()`; add a test for it.

## C7 — Surface grip, weather, night

Proposal: `UMurdarAISettings::WorldGripScale` (1 dry, ~0.7 wet) multiplied into `LateralGripCms` and
`BrakingDecel`; sight radius × a light-level factor at night. Measure with `griptest.py` on the wet surface before
choosing the number.

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

Proposal: a `USoundConcurrency` asset (max 4, stop quietest) on the siren loop; Doppler enabled on the attenuation.
Manual asset task when the siren sound exists (EDITOR_TASKS 8b).

## C13 — Off-duty police obey traffic rules

Proposal: Patrol/Returning use the traffic controller's rules (junction reservations from 08, speed limit, no
navmesh shortcuts); only emergency relaxes them. Write it into ARCHITECTURE §3 Patrol row.

## C14 — Multiplayer

Question for the user: is MURDAR single-player? If yes, one line in ARCHITECTURE §1 ("no replication; all AI
server=local"). If not, this whole system needs an authority model before Phase 11.
