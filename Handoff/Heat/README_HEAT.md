# Heat v2 — handoff for local Claude Code

Design and tests: `Docs/HEAT_SYSTEM.md` (Romanian summary at the top). Decisions by the user (2026-09-26): one
global scalar (no second "record" number, no per-sector heat); civilian-witnessed crimes enter heat when the witness
reports.

**What was verified here:** `MurdarHeatModel.h` (the rules) compiles with g++ C++17 `-Wall -Wextra -Wshadow`, no
warnings, and passes 15 unit tests / 42 checks (`Tests/heat_model_test.cpp`). **Not verified:** the Unreal files
(`MurdarHeatSettings.h`, `WitnessReportSubsystem.h/.cpp`) and the faction-memory wiring — not compiled, `// ADAPT:`
points listed below.

## Paste this prompt into local Claude Code

```
Read Docs/HEAT_SYSTEM.md, Docs/PROJECT_OVERVIEW.md and Handoff/Heat/README_HEAT.md. Then:
1. Run the unit tests as they are: g++ -std=c++17 -I Handoff/Heat/Source/Murdar_GameDev/AI/Heat
   Handoff/Heat/Tests/heat_model_test.cpp (or MSVC cl /std:c++17). They must pass before anything else.
2. Copy Handoff/Heat/Source/Murdar_GameDev/AI/Heat/* into the module. Resolve every // ADAPT: against the source.
3. Follow Handoff/Heat/FACTION_MEMORY_INTEGRATION.md step by step, building after each step. Handoff 03's
   GetCrimeHeat accessor is needed here even if 03's StandDown change is not applied.
4. Run HEAT-T01..T10 from Docs/HEAT_SYSTEM.md §4 and the police regression. Record results in
   Docs/HEAT_TEST_REPORT.md, report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT in Romanian, and list every
   ReportCrime call site you changed and who its reporter is.
Do not modify .uasset/.umap. Never save the level from a script.
```

## Files

| File | Goes to | Status |
|---|---|---|
| `Source/.../AI/Heat/MurdarHeatModel.h` | same path | **tested here** — pure C++, uses `std::` containers (fine in a UE module; if the project forbids std in headers, wrap it in a `.cpp` with the same tests) |
| `Source/.../AI/Heat/MurdarHeatSettings.h` | same path | not compiled |
| `Source/.../AI/Heat/WitnessReportSubsystem.h/.cpp` | same path | not compiled |
| `FACTION_MEMORY_INTEGRATION.md` | — | steps to wire the model into the memory |
| `Tests/heat_model_test.cpp` | `Tools/Tests/` or keep in Handoff | unit tests |

## `// ADAPT:` points

| Where | What |
|---|---|
| `WitnessReportSubsystem.cpp` includes | path of `FactionMemorySubsystem.h`; drop the civilian population include + `SetWitnessPinned` body if the civilian system isn't in yet |
| `IsPoliceNear` | the memory's police members list |
| `Update` | "witness dead" via `UHealthComponent` |
| memory | `GetCrimeHeat`, `DeliverWitnessReport`, the threshold members, the crime list / radio track update (see integration doc §4) |
| save | `FlushForSave` at the start of `UNarrativeStateSubsystem::Save` |

## Known limits

- The witness's delay is a timer; walking to a phone booth is the civilian foot AI's job later
  (`WitnessReachedPolice` is the hook).
- Witness reliability (not every witness reports) is not modelled yet — every civilian who sees a crime reports.
  Add a per-profile chance when the civilian profiles gain Phase 5 fields.
- A pure-C++ header with `std::string` reasons: converted to `FString` only for logging.
