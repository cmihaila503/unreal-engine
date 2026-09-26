# Handoff — changes prepared in a cloud session, to be applied by local Claude Code

Prepared 2026-09-26 in a cloud session that had **only the five Police AI Docs files and PROJECT_OVERVIEW.md**,
not the source. Nothing here was
compiled or run. Every code block is a starting point written against the names the Docs mention
(`AMurdarPoliceAIController`, `UPoliceDrivingProfile`, `UVehiclePursuitComponent`, `EPoliceState`, `Pick()`,
`EnterState()`, `UGameEventSubsystem`, `UFactionMemorySubsystem`, `UPoliceResponseDirector` …). Wherever the real
member name was not in the Docs the code says `// ADAPT:` — look it up in the source, do not guess.

## Paste this prompt into local Claude Code

```
Read Docs/MURDAR_POLICE_VEHICLE_AI_SPEC.md (source of truth) and then every file in Handoff/ in numeric order.
They were written without access to the source code: treat each code block as a proposal, verify every name
against the source, adapt `// ADAPT:` lines, and report any file whose premise turns out false in the code
(the finding does not reproduce) instead of forcing the change in.

Apply one Handoff file at a time, in order:
  1. make the change,
  2. build (Build.bat Murdar_GameDevEditor Win64 Development, editor closed — ask me to close it if it is open),
  3. run the tests listed in that file + the regression named there,
  4. add the entry from Handoff/12_TUNING_LOG_ENTRIES.md to Docs/POLICE_VEHICLE_AI_TUNING_LOG.md with real numbers,
  5. update Docs/POLICE_VEHICLE_AI_TEST_REPORT.md,
  6. report IMPLEMENTED / TESTED / FAILED / BLOCKED / NEXT (spec §1.30) and wait for my OK before the next file.
Do not modify .uasset/.umap. Do not change several tuning parameters at once (spec §54).
```

## Files

| # | File | Kind | Risk |
|---|---|---|---|
| 01 | `01_DOCS_CHANGES.md` + `docs.patch` | copy Docs edits | none |
| 02 | `02_A1_A2_RECOVERY_LOOP_AND_COOLDOWN.md` | bug fix (spec §53, §58) | low |
| 03 | `03_A3_STANDDOWN_INTERRUPT.md` | bug fix (exploit) | low |
| 04 | `04_C3_LIFETIME_SAFETY.md` | robustness audit | low |
| 05 | `05_A7_FIXED_RATE_WHISKERS.md` | FPS independence (T38) | **medium — driving regression** |
| 06 | `06_A9_MAGIC_NUMBERS.md` | spec §1.10–11 cleanup | low (values unchanged) |
| 07 | `07_A10_HEAT_AGGRESSION.md` | spec §26 / PVA-T21 | medium — behaviour change |
| 08 | `08_A4_A5_TRAFFIC_JUNCTIONS.md` | traffic priority, dead-end overshoot | medium |
| 09 | `09_PHASE10_ORDERS_RESPONDING_RETURNING.md` | Phase 10 start | medium |
| 10 | `10_TESTS_TO_ADD.md` | new tests for all of the above + missing PVA IDs | none |
| 11 | `11_NOT_IN_SPEC_DECISIONS.md` | decisions the spec never asks for (suspect driver, save, …) | needs the user |
| 12 | `12_TUNING_LOG_ENTRIES.md` | pre-filled §54 entries, results TBD | none |
| 13 | `13_PROJECT_OVERVIEW_UPDATES.md` | what in PROJECT_OVERVIEW.md is stale since 2026-09-16 | none |

Background for every item: `Docs/POLICE_VEHICLE_AI_GAP_ANALYSIS.md` (IDs A*, B*, C* refer to it).

## Project rules that apply to every file (from Docs/PROJECT_OVERVIEW.md)

- Work in the **main checkout** only — never launch the editor from a git worktree (full rebuild + all shaders).
- Build: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Murdar_GameDevEditor Win64
  Development -Project="E:\Unreal Engine\Murdar_GameDev\Murdar_GameDev.uproject" -WaitMutex` (editor closed).
- **Never save the level from a script** (`L_Sandbox` has uncommitted work). Save named assets explicitly.
- **A changed or new `UPROPERTY` default does not reach assets that already serialise that property.** New
  properties (all of the ones in this handoff) take the class default, but verify on the six profile assets through
  `Tools/uepy.py` — read the values back.
- Python traps: bools drop the `b`; arrays of structs iterate as copies (write back); `EditorAssetLibrary` lies
  during PIE; curve keys are not settable (hence file 07 uses scalars).
- The AI decision functions are plain switches **on purpose** (StateTree migration path). Add cases; don't
  restructure them.
- "Felt, not shown": no new HUD bars/numbers (see 11 C5).
- The user works in Romanian — report in Romanian.

## Rule used everywhere

Every new tunable follows spec §1.11: one owner, unit, default, ClampMin/ClampMax, a one-line reason in the
tooltip. Driver *personality* → `UPoliceDrivingProfile`. Car *physics* → `UVehiclePursuitComponent`. Law / world →
`UMurdarAISettings`. If a default below changes current behaviour, the file says so.
