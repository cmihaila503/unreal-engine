# Police Vehicle AI — gap analysis (2026-09-26)

Written from the five Police AI Docs files (plus `PROJECT_OVERVIEW.md`, added the same day) (spec, recon, architecture, editor tasks, test report). **The C++ source,
`Tools/`, `Config/` and `POLICE_VEHICLE_AI_TUNING_LOG.md` were not available**, so every item below is a finding
from the documents, to be confirmed against the code before it is acted on. Nothing here was compiled or run.

Three parts:
- **A. Suspected bugs** — behaviour the documents describe that is wrong, risky, or contradicts the spec.
- **B. Not done** — what the spec asks for that the reports do not show as done.
- **C. Missing, not in the spec** — what a shipped system needs that the spec never mentions.

Priority: **P1** = breaks the spec's hard rules (§1, §53, §58) or will bite in Phase 10–14; **P2** = correctness /
robustness; **P3** = polish, docs.

---

## A. Suspected bugs (debug list)

| ID | P | Finding | Evidence | Check / fix |
|---|---|---|---|---|
| A1 | P1? | **Suspected (not reproduced) infinite loop Recovery → Disabled → Patrol → Pursuit → Recovery.** Disabled returns to Patrol after *DisabledSeconds* "if upright and not wrecked"; a car disabled by a *recovery timeout* (not damage, not roll) is upright and not wrecked, so it comes back, re-acquires the same target in the same impossible place, fails recovery again. Spec §53: "no system may stay in an infinite loop"; §58 "infinite recovery loops". | ARCHITECTURE §3 Recovery / Disabled rows | Carry the strike count across Disabled → Patrol (per target, per location); after N strikes the unit must not be re-assigned to that target; or Disabled-by-timeout → Returning, not Patrol. Test: PVA-T35 on a map where the target stays unreachable for 5 min. |
| A2 | P1 | **Give-up cooldown doubles with no cap** ("cooldown ×2 per strike"). After a few strikes a unit ignores the player for minutes/hours; with no ceiling it overflows into "never". | ARCHITECTURE §3 Recovery row | Add `RecoveryGiveUpCooldownMaxSeconds` to the profile (owner/unit/min/max per spec §1.11); reset the strike count on a successful pursuit or after a quiet period. |
| A3 | decision | **StandDown ignores Lethal ("paid is paid").** A bribed unit that watches the player commit murder does nothing until *StandDownSeconds* elapse. Exploit: bribe, then shoot the next cop. | ARCHITECTURE §3 StandDown row | Let a *new witnessed* crime whose heat (the memory's existing crime table) exceeds the worst one bribed away, or a crime against this unit, interrupt StandDown. **May be intended** (bribery is a core mechanic) — the user decides; minimum: a crime against the bribed crew ends the deal. |
| A4 | P2 | **Traffic has no junction priority** → traffic-on-traffic contacts (damage 0.04–0.22) in Phases 2 and 3; spec §10 requires detecting cross traffic; police in emergency cross "with increased priority" — relative to what, if nobody has priority? | TEST_REPORT Phase 2, 3 | First-come reservation of the junction zone (ZoneGraph lane overlap = conflict), right-hand priority as the tie-break, emergency vehicles reserve early. Test: 6 traffic cars, 10 min, 0 contacts. |
| A5 | P2 | **Traffic car overshoots a dead-end stub onto the navmesh.** A civilian car leaving the road network is a navigation bug: it will be seen by the player on the pavement. | TEST_REPORT Phase 2, 3 | Traffic must stop at the lane end (end-of-lane = speed 0 at the last point) and re-plan; never fall back to navmesh for civilians. |
| A6 | P2 | **Units 3–4 queue 120–162 m behind** with 4 units at heat 80; "second-unit tangles at low speed near a stopped target"; "unit-on-unit avoidance when the primary brakes (PVA-T37)" still open after Phase 9. | TEST_REPORT Live recordings, Phase 8 | PVA-T37 is untested and is exactly the Phase 14 regression risk. Add a unit-to-unit rule in the whiskers: a police car ahead in the same lane is followed at a time gap, not avoided as an obstacle. |
| A7 | P2? | **Whiskers run per frame** (premise weak for T38 — see Handoff 05 note) (7 sweeps / car / frame). Cost and *behaviour* scale with frame rate (detection reach is evaluated more often at high FPS, obstacle brake reacts earlier) → PVA-T38 FPS independence risk, PVA-T40 cost risk at 25–100 cars. | RECON §3.2 | Fixed-rate whiskers (e.g. 30 Hz accumulator) independent of frame time; run PVA-T38 at 30/60/144 FPS (`t.MaxFPS`) with the same route and compare arrival times and damage. |
| A8 | P2 | **Sync pathfinding** (`FindPathSync`, ≤ 0.67 Hz per car) and the per-car lane Dijkstra on the game thread: fine for 3 cars, a hitch at 25+ (§48). | RECON §3.2, §8 | Measure in Phase 13 first (spec: no budget before benchmark). Stagger re-plans across cars; the `FindPathAsync` API is already verified in RECON §5. |
| A9 | P2 | **"Implementation constants" are magic numbers** by the spec's own rule 10: `StoppedKph 4`, `ReorderDistanceCm 600`, `StandDownDriveOffCm 6000`; also `PITRecoverDuration`, the 80 m trail, 1.5 s re-plan, 20 % shorter rule, 10 m corner window in the driver. | ARCHITECTURE §3, §4; RECON §3.2 | Give each a `UPROPERTY` with unit + ClampMin/ClampMax + a one-line reason (spec §1.11). Car-side ones stay on the pursuit component, driver-side ones go on the profile. |
| A10 | P2 | **PVA-T21 (heat aggression) is effectively not satisfied**: only Lethal → Combat → Ram. Spec §26 lists 10 things heat must change (follow distance, lane-change frequency, intercept frequency, PIT willingness, roadblocks, unit count, risk tolerance, response time…) and says `Behavior ≠ Heat`. | TEST_REPORT Phase 9 | A per-heat curve on the profile (`UCurveFloat` or a small table) scaling *profile* values, not replacing them; T21 test = same scene at heat 40/60/80, compare follow gap, tactic counts. |
| A11 | P3 | **PVA-T06 negative case** (pass refused because of oncoming traffic) is implemented but not scripted. | TEST_REPORT Phase 4 | Script it: obstacle + an oncoming car 3 s away → expect brake, no pass, 0 damage. |
| A12 | P3 | **Doc drift** (fixed in this commit): RECON grip 1.1 g / 750 vs measured 0.9 g / 850; RECON §6 unknowns already resolved; ARCHITECTURE "planned" text listing states that already exist; EDITOR_TASKS step 1 already done. | — | Done. |
| A13 | P3 | **The driver aims at the target actor "only while somebody sees it"** — but `HasContact()` also counts the faction track younger than 12 s. If the radio track is fresh because a *witness civilian* reported it, the driver still only gets the estimate — correct. Confirm that `SetTargetEstimate` is used for radio-only contact and the actor pointer is never handed to the driver in that case (spec §1.17). | ARCHITECTURE §2 vs RECON §3.1 | Code check + PVA-T23 variant: target hidden, civilian witness reporting → unit must drive to the report, not the car. |

---

## B. Spec items not done

### B1. Phases

| Phase | Status from the docs | What is left |
|---|---|---|
| 10 Police Integration | not started | `ReceiveOrder` (intent/target/location/role) from the director; `Responding` and `Returning` states; director-picked roadblocks (§23: junction / narrow road / choke point); crime severity + available units + ETA in escalation (§36); arrest + combat handoff wired to the director; acceptance B–E, G–N. |
| 11 World Police Events | not started | `UPoliceWorldEventDirector`; **multi-target memory** (RECON C7: the faction memory tracks the player only — the biggest single blocker); suspect NPC driver (a civilian/criminal car that flees = a second driver brain, see C1); event lifecycle, off-screen simulation, completion; PVA-T29–T32. |
| 12 Vehicle/Ped Handoff | partial (police side exists: `Dismount`, `SpawnOfficer`, `AbandonCar`) | suspect exits the car (vehicle → ped for the *suspect*), driver + officer as distinct entities (§32: today the crew is a counter), cover/negotiate/perimeter roles, PVA-T27/T28. |
| 13 LOD + Performance | not started | LOD 0–3 (§41), reduced think/whisker rates, event LOD, benchmark 5/10/25/50/100, budget table (§48), PVA-T38–T40. |
| 14 Full regression | not started | all PVA tests in one run, + vehicle physics regression (`VEHICLE_HANDLING_TESTS.md`) + foot AI tests. |

### B2. Tests (§46) with no PASS in the report

T01 lane following, T02 speed control, T03 intersection — covered in spirit by Phase 2/3 runs but never reported
under their IDs (report them explicitly, with the drift / speed-limit numbers you already have).
T06 negative case (A11). T21 (A10). T27–T32. T36 traffic density (low vs high traffic → different behaviour).
T37 multi-unit collision avoidance (A6). T38 FPS independence (A7). T39 AI LOD. T40 performance.

### B3. Other spec requirements

| Spec | Requirement | Status |
|---|---|---|
| §4 | Profile fields: `EmergencySpeedMultiplier, MaximumRisk, LaneChangeAggression, OvertakeAggression, BrakingConfidence, CollisionRiskTolerance, PITRiskTolerance, RoadblockPreference, BoxInPreference, InterceptPreference` | Only speeds/distances/timeouts/PIT permission are reported on `UPoliceDrivingProfile`. The *preference* fields are what makes PVA-T22 more than "Normal PITs, Disciplined sweeps". |
| §5 | States AVAILABLE, RESPONDING, APPROACHING_TARGET, POSITIONING, ATTEMPTING_INTERCEPT/PIT, BOXING_IN, ROADBLOCKING, STOP_CONFIRMED, SECURING_VEHICLE, OFFICERS_EXITING, FOOT_HANDOFF, RETURNING | Tactics run as sub-modes (`TacticNote`) — acceptable if written down as a deviation (now in ARCHITECTURE §3). The handoff states (Phase 12) and Responding/Returning (Phase 10) are genuinely missing. |
| §10 | Traffic lights, stop signs | None exist (ZoneGraph has none; RECON §4). Needs an annotation or actor + traffic obeying it + police in emergency crossing on red with a risk check. |
| §13, §52 | Civilian traffic: slow / change lane / pull over / stop / continue, **not all the same** | Yielding exists (kerb + 15 km/h). No variation per driver, no lane change, no "continue if no room", no emergency corridor. |
| §25 | Multi-unit pin | Not in the Phase 9 table (T14–T20 do not test it). "multi-unit pin at speed (formation hold)" is listed as open. |
| §29 | Realistic spawn with origin/ETA | Police still spawn only through the `MurdarPolice` cheat (RECON §3.1); stations are an editor task (EDITOR_TASKS 6). PVA-T32. |
| §34 | Radio messages with Source/Timestamp/Confidence/Target/Location/Urgency; INTERCEPT_REQUEST, ROADBLOCK_REQUEST, PIT_OPPORTUNITY | `FPoliceRadioMessage` (RECON C10) not reported as done; only bus events. |
| §40 | Return to patrol, no visible despawn | `Returning` missing; despawn policy unspecified in the code docs. |
| §44 | Debug views: Show Road Network, Lanes, Chase Slots, Intercept Points, PIT Evaluation, Obstacles, Recovery, World Events, Decision Trace | Two cvars exist (`Murdar.Police.Debug`, `Murdar.Pursuit.Debug`). The `FGameplayDebuggerCategory` from RECON §5 was not built. |
| §45 | Test maps StraightRoad, Traffic, PIT, BoxIn, Roadblock, Recovery, LostPursuit, WorldEvent | Only `PoliceAI_Test_Intersection` exists. Phase 9 tactics were tested on the grid map. |
| §54 | Tuning log | Exists locally (not uploaded here). |
| §57 | Editor tasks: siren audio, light bar, officer sockets, seats, spawn zones, stations, world event zones | Siren sound and light bar missing (code runs silent); sockets/seats/zones not listed as done. |

---

## C. Missing and not in the spec

Things a real game needs that the spec never asks for. None of these is a spec violation; each is a decision
the project will be forced to make later, usually at a worse time.

| ID | P | Topic | Why it matters | Suggestion |
|---|---|---|---|---|
| C1 | P1 | **A suspect (criminal NPC) driver** | World events (§27–30) need an NPC car that *flees*: route choice away from police, panic, crashes, giving up. The spec specifies only the police side. Without it Phase 11 has nobody to chase. | `AMurdarSuspectAIController` reusing `UVehiclePursuitComponent` in `DriveTo`, with a flee planner over the lane graph (maximise distance to known units, prefer junctions), a nerve/panic value, and a "bail out on foot" trigger feeding Phase 12. |
| C2 | P1 | **Save / load** | Heat and bribes are saved (narrative state), but an active pursuit, units, roles, the search, a world event in progress are not mentioned. Load during a chase = undefined. | Decide explicitly: pursuit state is not saved, loading clears it, and the save point is blocked during Pursuit/Combat (common in the genre). Write it down. |
| C3 | P1 | **Actor lifetime / dangling pointers** | Targets die, cars get destroyed, officers despawn, the level unloads — the director, memory and search hold references to units and targets across 0.5–1 s timers. | Every cross-object reference `TWeakObjectPtr`; every timer cleared in `EndPlay`; a test that destroys the target and a unit mid-chase and checks for no crash and a clean `PursuitEnded`. |
| C4 | P2 | **Police vs civilians collateral** | What happens when a cop hits a civilian car or a pedestrian? Today `Crime.HitPedestrian` is a *player* crime. Nothing says whether police are careful around pedestrians, whether the player gets blamed, whether witnesses react. | Police-caused collisions produce an event (not a crime); pedestrians on the road are a hard obstacle class for the whiskers (never "suppressed as traffic"); PIT/Ram aborted if a pedestrian is within the risk zone (spec §21 lists pedestrians only for PIT). |
| C5 | P2 | **Player-facing feedback** | The AI can be perfect and the player still can't read it. The project rule is "felt, not shown" (PROJECT_OVERVIEW §5), so no wanted HUD. | Sirens, radio barks through `ShowSubtitle` + voice on the existing bus events, the tension meter reacting to pursuit events. |
| C6 | P2 | **Hiding / line-of-sight breaking** | Garages, tunnels, alleys, changing cars. `MatchesKnownVehicle()` exists; there is no notion of the player hiding the car or swapping it during a search. | Known-vehicle description vs. the car the player is in (already there); add "parked and out of the car inside a covered zone" → search can't confirm. Tag zones `Zone.Cover`. |
| C7 | P2 | **Weather, night, surface** | Grip changes (rain, gravel) change braking and corner speeds; the planner uses a fixed `LateralGripCms 850`. Night changes sight radius. Per-surface `UPhysicalMaterial` friction is not authored yet (PROJECT_OVERVIEW §10). | Read grip from the surface (KinetiForge has surface response in `UVehicleEffectsComponent`) or scale `LateralGripCms` by a world grip factor; sight radius by light level. |
| C8 | P2 | **World streaming / big maps** | A real city will use World Partition. ZoneGraph data is per level; units off-screen (LOD 3) may be on unloaded cells. | Decide how LOD 3 moves without physics or lanes loaded (abstract graph position + ETA), how a unit re-materialises when the cell loads (spec §29 "no spawn next to the player" still applies). |
| C9 | P2 | **Headless automated tests / CI** | Tests are Python over an open editor + PIE by hand. Phase 14 "full regression" by hand will take hours and nobody will rerun it. | `UnrealEditor-Cmd.exe Murdar_GameDev.uproject -ExecCmds="..." -nullrhi -unattended` or Automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) wrapping the same rigs; one command runs every PVA test and writes a table. |
| C10 | P2 | **Determinism of tests** | Physics + frame timing + random picks → one run passes, the next fails. Reports already use "3 runs"; no seed is mentioned. | Fixed timestep for test runs (`-benchmark -fps=60` or `t.MaxFPS` + fixed physics), a seed for any random choice in the AI, and pass criteria over N runs (e.g. 5/5). |
| C11 | P3 | **Difficulty / accessibility** | Six profiles exist; nothing maps a difficulty setting onto them or onto heat thresholds. | A difficulty setting picks the profile mix and scales heat gain — not a separate code path. |
| C12 | P3 | **Siren audio concurrency** | 7–14 units with sirens at once (the live recordings) will stack loops. | Sound concurrency group (max 3–4 audible), Doppler on, one "distant sirens" bed for LOD 2–3. |
| C13 | P3 | **Police off-duty traffic behaviour** | Patrol cars without emergency must obey the same rules as traffic (lights, priority, speed limit). Today patrol is *PatrolSpeedKph* toward points. | Patrol = traffic rules + the profile's cruise speed; emergency is the only licence to break them (spec §6 implies this but never states it). |
| C14 | — | **Multiplayer / replication** | Resolved: single-player, `bReplicates = false` everywhere (PROJECT_OVERVIEW §1). | One line in the architecture doc. |
| C15 | P2 | **Siren sound is missing** | Phase 6 runs silent. | Placeholder via a `synth_siren.py` like the project's other `synth_*` scripts (new asset only). |

---

## D. Suggested order

1. A1, A2, A3 (P1 rule breaks, small code changes, testable on the grid map).
2. C3 (lifetime safety) before adding more subsystems in Phase 10–11.
3. Phase 10 with `ReceiveOrder` + Responding/Returning + director roadblocks; A10 (heat curve) inside it.
4. C1 (suspect driver) + multi-target memory (RECON C7) — the real prerequisites of Phase 11.
5. A4/A5 junction priority — before T36 and before any world event is shown to the player.
6. C9/C10 test automation — before Phase 13/14, otherwise the regression won't be repeatable.

To act on sections A and C, the project source (`Source/`, `Config/`, `Tools/`, `Content/Python/`,
`Plugins/KinetiForge/Source`, `Murdar_GameDev.uproject`, `Docs/`) has to be in this repository.
