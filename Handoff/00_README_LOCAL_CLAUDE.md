# Handoff — changes prepared in a cloud session, to be applied by local Claude Code

Prepared 2026-09-26 in a cloud session that had **only the five Police AI Docs files and PROJECT_OVERVIEW.md**,
not the source. Nothing here was
compiled or run. Every code block is a starting point written against the names the Docs mention
(`AMurdarPoliceAIController`, `UPoliceDrivingProfile`, `UVehiclePursuitComponent`, `EPoliceState`, `Pick()`,
`EnterState()`, `UGameEventSubsystem`, `UFactionMemorySubsystem`, `UPoliceResponseDirector` …). Wherever the real
member name was not in the Docs the code says `// ADAPT:` — look it up in the source, do not guess.

## Assets rule (2026-09-27, approved by the user — overrides every older "do not modify .uasset/.umap" / "never save the level from a script" line in this file and in every Handoff README)

The Unreal editor is connected to local Claude Code through **MCP**. Local Claude **may create and modify
`.uasset` and `.umap`** — through the MCP editor tools (or editor Python), never by writing the binary files
directly. This covers Input Mapping Contexts (player-mappable), materials (`PaintColor`), skeleton sockets,
sound classes/mixes, DataAssets, Blueprints, and placing gameplay actors (garages, safehouses, payphones, job
points, gang zones) in levels.

Every time:
1. Checkpoint first: `git status`; commit uncommitted asset/level work (or ask me once) before changing anything.
2. Say in one line what you change (asset path + property), then change it.
3. Save only the assets/levels you changed — never "Save All".
4. Read the value back after saving to confirm it stuck (Python traps below still apply).
5. City map: place actors only where I marked them; if nothing is marked, propose positions and ask.
6. List every changed asset in the IMPLEMENTED report.

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
Assets/levels may be modified through MCP — follow §Assets rule. Do not change several tuning parameters at once (spec §54).
```

## Files

**Read `AUDIT_VS_SOURCE_2026-09-26.md` first — it checks every file below against the real source and overrides them (02, 08 and Civilians withdrawn; Heat needs rework).** Then `REVIEW_2026-09-26.md` — it re-rates these files and gives the recommended order (01, 04, 06,
02-if-reproduced, civilians Phase 0, then the rest one by one with the user's OK).


| # | File | Kind | Risk |
|---|---|---|---|
| 01 | `01_DOCS_CHANGES.md` + `docs.patch` | copy Docs edits | none |
| 02 | ~~`withdrawn/02_...`~~ | already in the code | — |
| 03 | `03_A3_STANDDOWN_INTERRUPT.md` | bug fix (exploit) | low |
| 04 | `04_C3_LIFETIME_SAFETY.md` | robustness audit | low |
| 05 | `05_A7_FIXED_RATE_WHISKERS.md` | FPS independence (T38) | **medium — driving regression** |
| 06 | `06_A9_MAGIC_NUMBERS.md` | spec §1.10–11 cleanup | low (values unchanged) |
| 07 | `07_A10_HEAT_AGGRESSION.md` | spec §26 / PVA-T21 | medium — behaviour change |
| 08 | ~~`withdrawn/08_...`~~ | already in the code | — |
| 09 | `09_PHASE10_ORDERS_RESPONDING_RETURNING.md` | Phase 10 start | medium |
| 10 | `10_TESTS_TO_ADD.md` | new tests for all of the above + missing PVA IDs | none |
| 11 | `11_NOT_IN_SPEC_DECISIONS.md` | decisions the spec never asks for (suspect driver, save, …) | needs the user |
| 12 | `12_TUNING_LOG_ENTRIES.md` | pre-filled §54 entries, results TBD | none |
| 13 | `13_PROJECT_OVERVIEW_UPDATES.md` | what in PROJECT_OVERVIEW.md is stale since 2026-09-16 | none |
| — | `Heat/` | **needs rework — see AUDIT** · Heat v2: decay, repeats, hysteresis, delayed witness reports — design in `Docs/HEAT_SYSTEM.md`; model unit-tested; own README and prompt | medium — changes how long pursuits last |
| — | ~~`withdrawn/Civilians/`~~ | duplicate of UPopulationSubsystem/UPedestrianComponent · was: new system: civilian pedestrians — spec in `Docs/CIVILIAN_PEDESTRIAN_SPEC.md`, own README and prompts; independent of 01–13 | Phase 0 first |

## New systems (written against the real source, each in its own folder)

Unlike 01–13, these were written with the project's source open (kept out of this repo). Every folder has its own
README with a paste-prompt, patches, manual steps, in-game tests and a Romanian summary; the pure rules of each
are unit-tested here (g++ C++17, `-Wall -Wextra -Wshadow`, zero warnings). The Unreal files were **not compiled**.
Integrate in this order — later ones use earlier ones:

| Order | Folder | What | Needs | Unit checks | Risk |
|---|---|---|---|---|---|
| 1 | `TimeOfDay/` | game clock, sun, night, headlights, density by hour, `SkipHours` | — | 31 | low |
| 2 | `Missions/` | missions/objectives as data assets | — | 27 | low |
| 3 | `Interaction/` | one "E" for placed things → `Event.Interact` | — | 14 | low — `Input_Interact`, HUD prompt |
| 4 | `Dialogue/` | conversations as data, choices in the prompt line, **playable police bribe**, voiced barks | 3 | 47 | low — HUD prompt, keys 1/2 while choosing |
| 5 | `Economy/` | wallet `Stat.Money`, shops via dialogue, bribes charged when accepted, cash on bodies | (4) | 34 | low |
| 6 | `Consequences/` | death → hospital, arrest → station (time, fine, confiscation, repeat offenders), regen + doctor | 1, 5 | 30 | medium — death/arrest when the map has the starts |
| 7 | `WorldState/` | save brings back where you were, your car, opted-in placed actors | — | 21 | medium — `FNarrativeState` v2 |
| 8 | `RearviewParanoia/` | „Paranoia în retrovizoare” | (1) | 46 | medium — the car's input |
| 9 | `Music/` | dynamic score from stress, wanted, shots, tails | (4, 8) | 37 | low |
| 10 | `Menus/` | Slate pause menu, save only when calm, settings | (2, 4, 7) | 38 | low |
| 11 | `AILod/` | AI significance LOD + streaming radius + World Partition plan | — | 35 | low — measure before/after |
| 12 | `VehicleTheft/` | locked cars, break-in + hotwire, carjack, stolen reports → a stop | (Interaction) | 43 | medium — `Input_Interact` car branch, HUD car prompt |
| 13 | `Garage/` | car colours in police descriptions, respray (repair + loses the description, cools a chase out of sight), storage garage, impound lot | 5, 6, 7, 3, 12 | 30 | medium — `MatchesKnownVehicle` now checks colour |
| 14 | `Safehouse/` | bed (sleep → skip, heal, save), money stash safe from arrest, wardrobe, hiding cools heat | 3, 5, 1 | 26 | low |
| 15 | `Jobs/` | pager + payphone; generated delivery / smuggling / car order / debt collection on the mission runtime | 2, 3, 5 | 29 | low |
| 16 | `PaperMap/` | the paper map page in the pause menu: known places, the job circled, a cross where you are | 10, 15 | 19 | low |
| 17 | `Weather/` | hourly weather chain, blended look, wet roads (grip), fewer people, gloom, rain/fog | 1 | 24 | medium — scales road physical-material friction at runtime (restored) |
| 18 | `Radio/` | stations on the world clock, DJ / ads / news about you, streamer mode, ducks under score and talk | 10, 9 | 25 | low |
| 19 | `Gangs/` | territories, respect (saved) with hysteresis stance, taxa dialogue, members around; (gang-vs-gang needs AI teams) | 4, 1 | 25 | medium — adds SetFaction to the NPC controller |
| 20 | `Disguise/` | on-foot description by outfit + headwear; change unseen = not recognised at a distance, chase cools | 14, 5 | 14 | medium — a gate in ReportSighting |
| 21 | `FrontEnd/` | title screen, loading screen with tips, save slots (3 + auto) with chapter/day/hour | 10 | 17 | medium — module class + PeekSave |
| 22 | `KeyRemap/` | Enhanced Input user-settings remapping page, swap on clash, per context | 10 | 15 | medium — IMC assets need Player Mappable settings (asset edit, ask) |
| 23 | `Cutscenes/` | Level Sequence scenes from events, never mid-chase, queue, hold-to-skip, letterbox, facts | 3, 4 | 16 | low |
| 24 | `Hints/` | one-time contextual hints, spaced, never while busy, saved | 10 | 12 | low |
| 25 | `Localization/` | loc_lint.py (found 33 untranslatable strings in the project), RO native + EN, language option | 10 | 8 py tests | low |
| 26 | `Pooling/` | reuse pedestrians instead of spawn/destroy (cars later), warmed in quiet frames | — | 9 | medium — PopulationSubsystem spawn/despawn + ResetForPool |
| 27 | `Automation/` | run all rule tests (30 files), generate Unreal automation tests (Murdar.Rules.*), headless run, nightly chain, GitHub workflow | — | 30 files | none |
| 28 | `AIQuality/` | **AI stops getting stuck**: stuck watchdog (nudge → break the rule → recycle unseen), junction deadlock fixes, traffic drives round people/bodies/you, cover + peek for gunmen, navmesh flee/hide, pedestrians keep right, spatial index (kills the O(N²) scans → more density), soak test `Murdar.AI.Soak 600 tour` with PASS/FAIL | — (26 if integrated) | 101 | medium — patches traffic, junctions, NPC controller, population; each fix has its own switch. **Can be done before 12–27** (touches only existing AI) |
| 29 | `CarWeight/` | the car heavy in the air and against walls: extra gravity only when all wheels are off, air pitch/roll damping + leveling, damped landing, no-bounce body material, capped depenetration, impact shake + hit-stop + thud; scale audit script (car / lanes / blocs) and camera targets | — | 37 | medium — every car gets a component; camera asset values change (via script, read back) |
| 30 | `Gps/` | GTA-style minimap (heading-up, zooms with speed), big map in the pause menu (M), your waypoint + the mission pin **at the same time**, each with its route along the traffic lanes | 17, 10 (16, Missions) | 41 | medium — patches SPaperMap, pause menu, Missions; switches off BP_MapManager's map after the user's OK |

"(n)" = works better with n, builds without it. One folder at a time; build and test each before the next.

```
Read Handoff/00_README_LOCAL_CLAUDE.md §New systems. Integrate the folders in the order of that table, one per
session step: read the folder's README and follow its own paste-prompt; build and run its tests; report in Romanian
IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT and wait for my OK before the next folder. You may create/modify .uasset/.umap
through MCP — follow §Assets rule (checkpoint first, save only what you changed, read back).
```

Background for every item: `Docs/POLICE_VEHICLE_AI_GAP_ANALYSIS.md` (IDs A*, B*, C* refer to it).

## Project rules that apply to every file (from Docs/PROJECT_OVERVIEW.md)

- Work in the **main checkout** only — never launch the editor from a git worktree (full rebuild + all shaders).
- Build: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Murdar_GameDevEditor Win64
  Development -Project="E:\Unreal Engine\Murdar_GameDev\Murdar_GameDev.uproject" -WaitMutex` (editor closed).
- **Levels:** save only the level you changed, explicitly, after the §Assets rule checkpoint (`L_Sandbox` had
  uncommitted work — commit it first). Never "Save All". Save named assets explicitly.
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
