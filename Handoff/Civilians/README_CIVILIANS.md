# Civilian pedestrians — handoff for local Claude Code

New system, not started in the project. Written in a cloud session **without the source**: nothing compiled.
Source of truth: `Docs/CIVILIAN_PEDESTRIAN_SPEC.md`. The project rules in `Handoff/00_README_LOCAL_CLAUDE.md`
("Project rules that apply to every file") apply here too.

## Paste this prompt into local Claude Code

```
Read Docs/CIVILIAN_PEDESTRIAN_SPEC.md (source of truth for civilians), Docs/PROJECT_OVERVIEW.md, and
Handoff/Civilians/README_CIVILIANS.md. Do Phase 0 only: follow Handoff/Civilians/PHASE0_RECON_CHECKLIST.md, write
Docs/CIVILIAN_PEDESTRIAN_RECON.md with measured numbers and every API cited from a header, and report
ENGINE/EXISTING FOOT AI/COST PER CIVILIAN/VERIFIED APIs/UNKNOWN APIs/CONFLICTS/GATA. Do not add any civilian code
yet. If the recon contradicts the spec or the prepared Phase 1 code, say so and propose the smallest change.
```

After Phase 0 is GATA and the user agrees:

```
Apply Phase 1 from Handoff/Civilians/: copy Source/Murdar_GameDev/AI/Civilians/* into the module, resolve every
// ADAPT: against the source and Phase 0's recon, add the cheats from CHEATS_SNIPPET.md to UDirectorCheats, build,
then run PED-T01..T04 with Handoff/Civilians/Tools/civtest.py (copied to Tools/). Record results in
Docs/CIVILIAN_PEDESTRIAN_TEST_REPORT.md and report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT. Stop before Phase 2.
```

## Files

| File | Goes to | What |
|---|---|---|
| `Source/Murdar_GameDev/AI/Civilians/CivilianPopulationSettings.h` | same path | every population tunable (Project Settings ▸ Game ▸ Murdar Civilians) |
| `Source/Murdar_GameDev/AI/Civilians/CivilianProfile.h` | same path | per-archetype data asset (Phase 1: body class; Phase 2: walk speed) |
| `Source/Murdar_GameDev/AI/Civilians/CivilianPopulationSubsystem.h/.cpp` | same path | spawn ring, visibility test, despawn, pool, stats, debug draw |
| `Tools/civtest.py` | `Tools/` | PED-T01..T04 rig (stats, render-based visible-pop watch) |
| `CHEATS_SNIPPET.md` | into `UDirectorCheats` | `MurdarCivilians`, `MurdarCivMax`, `MurdarCivDensity` |
| `PHASE0_RECON_CHECKLIST.md` | — | what Phase 0 must measure/verify |

## Every `// ADAPT:` in the Phase 1 code

| Where | What to resolve |
|---|---|
| `.cpp` includes | real paths of `UMurdarAILibrary` / `UMurdarAISettings` and `AZoneVolume` |
| `ZoneContains`, `ZoneTagOf` | how `AZoneVolume` exposes its box and its `Zone.*` tag (use `EncompassesPoint` if it's an `AVolume`) |
| `AcquirePawn` | real `UMurdarAILibrary::SpawnNPC` signature, faction enum name, whether it takes feet or actor location, unarmed civilians |
| `SetBodyActive` | brain dormancy: stop/restart the NPC controller's Think timer and perception; reset its state on reuse. Reuse the in-car dormancy path for the pawn half |
| `ReturnToPool` | whether the controller dies with its pawn |
| `NavAgentRadiusCm/HeightCm` | read the Human agent from wherever `MurdarNav` gets it; delete the duplicate settings if possible |
| Build.cs | nothing new expected (`NavigationSystem`, `GameplayTags`, `DeveloperSettings` are already there — verify) |

Also wire in the brain (small, Phase 1): when a civilian dies, call
`UCivilianPopulationSubsystem::NotifyCivilianDied(Pawn)` from wherever `AMurdarNPCAIController` handles death.

## Design decisions already taken (change only with the user)

- Actor-based, reusing `AMurdarCharacter` + `AMurdarNPCAIController` (Civilian faction). Not Mass Crowd.
- The population never steers; the brain does.
- Visibility = in the view cone (+margin) **and** unoccluded to head or chest. Spawns only when not visible;
  despawns only far + unseen for `DespawnUnseenSeconds`, never pinned ones.
- Density from the `Zone.*` of the zone the player is in; default elsewhere.
- Seeded random stream for repeatable tests (`RandomSeed`).
- Not saved; the street refills out of view after a load.

## Known limits of the Phase 1 code

- One body class in the pool. `PawnClassOverride` per archetype needs a pool per class (add when a second body
  exists — right now there is only the mannequin).
- Density follows the player's zone only, not the zone of each spawn point (a zone border near the player spawns at
  the player's zone density on both sides). Fine until zones are small; revisit in Phase 2.
- Horizontal FOV used as a round cone: conservative (refuses a few spawns that are actually off-screen vertically).
- Civilians just stand (Idle) in Phase 1.
