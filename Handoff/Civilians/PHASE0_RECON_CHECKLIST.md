# Phase 0 — civilian recon checklist

Output: `Docs/CIVILIAN_PEDESTRIAN_RECON.md`, same conventions as `POLICE_VEHICLE_AI_RECON.md` ([V] verified in a
header / compiled, [U] unknown). No civilian code in this phase.

## 1. Cost of one civilian — the number everything else depends on

Measure with Unreal Insights (`-trace=cpu,frame`) or `stat game` / `stat anim` / `stat ai` in PIE on `L_Sandbox`,
`t.MaxFPS 0`, same camera each run. Spawn with `MurdarNPC Civilian` (unarmed if possible).

| Case | 1 | 10 | 25 | 50 |
|---|---|---|---|---|
| standing, in view | | | | |
| standing, behind the camera | | | | |
| walking (navmesh, in view) | | | | |
| hidden + all ticks off (the pool state from `SetBodyActive(false)`) | | | | |

Per case: game thread ms total, anim (motion-matching search) ms, CharacterMovement ms, NPC brain ms, render
thread ms, memory per character. Write down what dominates.

Decision this feeds: if 25 walking civilians cost more than the user's frame budget allows (ask the user for the
target FPS on the RTX 3060 — PROJECT_OVERVIEW §7 says VRAM is shared with a local model), the spec's LOD table
(§7) moves up to Phase 2, or a lighter civilian body/AnimBP is needed (same skeleton, no motion matching at LOD ≥ 1).

## 2. Foot brain (`AMurdarNPCAIController`)

- States and transitions as they are now; how the Civilian faction differs (witness report, Flee).
- How it spawns: `UMurdarAILibrary::SpawnNPC` exact signature, faction enum, weapon choice, location convention.
- Think timer and perception: how to pause/resume them (for the pool). Is there a reset of state/memory?
- Death: where it's handled (to call `NotifyCivilianDied`), corpse timer.
- How a civilian's witness report reaches `UFactionMemorySubsystem` today: instant? line of sight? which crimes?
- Movement: does it use `MoveTo`/path following or its own steering on `MurdarNav` paths? Walk speed: does GASP
  derive gait from `MaxWalkSpeed` or from its own gait inputs?

## 3. Dormancy

The in-car driver is made dormant ("no motion-matching search, no movement update", PROJECT_OVERVIEW §4.7). Find
the code (`AMurdarVehicle::Enter`?) and document exactly what it turns off, and whether it's reusable for a pooled
civilian.

## 4. Navigation for sidewalks

- Human navmesh coverage on `L_Sandbox` and `PoliceAI_Test_Intersection`.
- Option A: `UNavArea` subclasses (Sidewalk / Road / Crosswalk costs) + `NavModifierVolume`s (manual) — verify
  `UNavArea::DefaultCost` / `FixedAreaEnteringCost` and a query filter in 5.8 headers.
- Option B: ZoneGraph lanes tagged `Pedestrian` (lane tag set in `DefaultPlugins.ini`? the police EDITOR_TASKS
  step 3 was never confirmed).
- Recommend one; the spec leans to A (navmesh is what the foot brain already uses).

## 5. Cars ↔ people

- Do the pursuit whiskers see civilian pawns (object type Pawn is in the list — confirm), and is braking for them
  ever suppressed (the "bodies we are not closing on" rule)?
- Does `AMurdarTrafficAIController` see pedestrians?
- Run-over path: works on NPC civilians exactly as on the player? (`Tools/hit.py`)

## 6. Zones and settings

- `AZoneVolume`: class, box, tag member name.
- `UMurdarAISettings`: `NPCPawnClass` and how SpawnNPC uses it.
- SmartObjects plugin: enabled; any smart object definitions in content? (Phase 6)

## 7. Report

ENGINE / EXISTING FOOT AI / COST PER CIVILIAN (table) / VERIFIED APIs / UNKNOWN APIs / CONFLICTS with the spec /
changes needed to the prepared Phase 1 code / GATA or NOT GATA.
