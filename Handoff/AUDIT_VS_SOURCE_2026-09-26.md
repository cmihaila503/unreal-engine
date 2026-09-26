# Audit: Handoff vs. the real source (2026-09-26, evening)

The user sent `Murdar_GameDev.zip` (the `Source/Murdar_GameDev` module, 112 files, 47 of them changed on 2026-09-26).
Everything in `Handoff/` before this file was written from the Docs only; the Docs turned out to be days behind the
code. This file re-checks every item against the source. **Read it before any other Handoff file; it overrides them.**
(The source was read in a private scratch folder and is not committed here — this repo is public.)

## Pe scurt (română)

- **Pietonii există deja** — `UPopulationSubsystem` (inel de spawn, nimic la vedere, oameni, trafic, poliție, mașini
  parcate), `UPedestrianComponent` (plimbare, pauze, grupuri, traversare la semafor, se uită la accidente, fug de
  focuri, **sună la poliție**), `UPedestrianNetworkSubsystem`, `UStreetEventDirector`. Codul meu Phase 1 pentru civili
  era o dublură → **retras** (`withdrawn/Civilians`).
- **Bucla de recovery și cooldown-ul (02) sunt deja rezolvate** în cod (plafon ×8 = 15→120 s, blocare la Patrol/Combat)
  → **retras**.
- **Semafoare și prioritate de dreapta (08) există** (`AMurdarTrafficLight`, `UTrafficSubsystem::MayEnter`) → **retras**.
- **Heat v2 trebuie refăcut parțial**: întârzierea martorului există deja (6–11 s, un singur apelant pe crimă);
  scăderea începe deja abia după 20 s fără contact; există o regulă nouă (crimele non-violente nu duc la Lethal) pe
  care modelul meu n-o știa. Rămân utile: histerezis, scădere mai lentă după crime grave și în căutare.
- **Rămân valabile**: 03 (mita — decizie de design), 04 (siguranța pointerilor), 05 (whiskers — doar măsurare),
  06 (constante), 07 (agresivitate după heat), 09 (ordine Phase 10), 11 (suspect NPC etc.), 13 (overview, acum și mai
  vechi).
- **Găsite în cod acum**: martorul spune „Alo, 112?” — în România anilor '90 poliția era **955** (112 a apărut în
  2004); martorul sună pe loc, din stradă — fără mobile în anii '90, ar trebui o cabină.

## Item by item

| Item | Premise | Source says | Verdict |
|---|---|---|---|
| 01 Docs changes | Docs stale | Docs are *more* stale than thought (population, pedestrians, traffic lights, street events, Arrest state, LoseSightSeconds 20 not 12) | **valid**, extend via 13 |
| 02 A1 recovery loop / A2 uncapped cooldown | loop possible, ×2 forever | `ImpossibleStrikes` + `ImpossibleUntil = Now + ImpossibleRouteCooldownSeconds × min(2^(n−1), 8)`; set on *every* Recovery entry; Patrol/Lethal/Combat check `bStillImpossible` | **withdrawn — already done** |
| 03 A3 StandDown vs Lethal | StandDown ignores Lethal | confirmed: Lethal branch has `State != EPoliceState::StandDown`; a bribe also caps heat below Stop | **valid — design decision for the user**; `GetCrimeHeat(tag)` in 03 must become a tag → `MurdarCrime::*` map (the table is constants passed as `Severity`, not a lookup) |
| 04 C3 lifetime | raw pointers, bus teardown | bus: `FHandle Subscribe(Tag, TFunction)`, `Unsubscribe(FHandle&)`; the population/traffic/pedestrian code uses `TWeakObjectPtr` lists | **valid** as an audit; fewer findings expected |
| 05 A7 whiskers | per frame | `ProbeWhiskers` called on the per-frame path | **valid — measure first** (as already reframed) |
| 06 A9 literals | `StoppedKph`, `ReorderDistanceCm`, `StandDownDriveOffCm` | exist as named `constexpr` in the controller .cpp; new literals too, e.g. `PlanCall` delay `FRandRange(6.f, 11.f)`, the ×8 cooldown cap | **valid, low priority** (they are named) |
| 07 A10 heat aggression | one Lethal step | not re-checked in depth; profile has no aggression keys | **valid** |
| 08 A4 junction priority / A5 dead end | none | `UTrafficSubsystem::MayEnter` (lights, occupancy, priority to the right), `AMurdarTrafficLight`, traffic is now `UTrafficDriverComponent` | **withdrawn — already done** (A5 not re-checked; re-open only if the overshoot is still seen) |
| 09 Phase 10 orders | no `ReceiveOrder` | still none; but `EPoliceState::Arrest` exists (Phase 10 started) | **valid**; `Arrest` must be added to the state table and to the order mapping |
| 10 tests | — | — | valid where its item is valid |
| 11 not-in-spec | suspect driver, save, … | C1 suspect driver still missing (street events have no criminal chase); C14 single-player confirmed | **valid** |
| 12 tuning log | — | drop the 02 / 08 entries | update |
| 13 overview stale | — | even more stale | **valid** |
| Heat v2 | — | see below | **rework** |
| Civilians | no civilians | full population + pedestrian life exist | **withdrawn** |
| `Tools/Package_Murdar.bat` | — | unrelated to source | valid |

## Heat v2 — what survives

| Part of `MurdarHeatModel.h` | Source | Keep? |
|---|---|---|
| decay delay 10 s | decay only while `bContactLost`, which needs `LoseSightSeconds` (20 s) without a sighting | **drop** — already there, longer |
| severity-scaled decay (murder ~4× slower) | none | keep |
| search-scaled decay | none | keep |
| diminishing repeats | the police controller already rate-limits speeding reports (`LastSpeedingReport`) | keep only for non-speeding repeats, or drop — check with the user |
| wanted hysteresis | none (`GetWantedLevel` is plain thresholds) | keep |
| non-violent crimes capped below Lethal | **exists** (user decision 26 Sep) — the model does not know it | **must add** before integrating |
| witness report queue (delay, dedupe, cancel on death, stale fix) | **exists**: `UPedestrianComponent::PlanCall` (6–11 s, "if still up"), `TryClaimCall` (one caller per crime), `ReportCrime(…, CallWhere, …)` (crime's place) | **drop** `UWitnessReportSubsystem`; keep only "flush on save" and corroboration if wanted |

`Handoff/Heat/` is therefore **not to be applied as is**. A reworked version would be: the three kept rules as a small
patch to `UFactionMemorySubsystem::Tick1Hz` / `GetWantedLevel` + the non-violent cap preserved.

## New findings in the source

| # | Where | Finding | Suggestion |
|---|---|---|---|
| N1 | `PedestrianComponent.cpp` call lines | "Alo, 112?" — Romania's 112 dates from 2004; in the 1990s police was **955** | "Alo, 955?" / "Alo, poliția?" |
| N2 | `PedestrianComponent` Call mode | the witness calls on the spot ("on the phone") — no mobiles in 1990s Romania | walk to a phone booth / a shop / flag a patrol; the delay becomes travel time (the idea from the civilian spec §0.4) |
| N3 | `PlanCall` | delay `FRandRange(6.f, 11.f)` is a literal (spec §1.10) | a setting in `UMurdarAISettings` next to the Population block |
| N4 | `MurdarPoliceAIController.cpp` | ×8 cap on the impossible-route cooldown is a literal | profile field `ImpossibleRouteCooldownMaxMultiplier` |

## Recommended order now

1. 01 + 13 (docs), 04 (lifetime audit), N1–N4 (small, local).
2. 03 — only after the user decides "paid is paid".
3. 09 (Phase 10 orders, with `Arrest`), 11 C1 (suspect driver → world events).
4. Heat: rework as above, then apply.
5. 05, 06, 07 one at a time with measurements.
