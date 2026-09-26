# MURDAR — Civilian Pedestrians: specification

Version 0.1 (2026-09-26). Status: proposal, written in a cloud session without the source; Phase 0 (local) must
validate every assumption before Phase 1 code is applied. Same working rules as the Police Vehicle AI spec
(`Docs/MURDAR_POLICE_VEHICLE_AI_SPEC.md` §1) unless this document says otherwise.

---

## 0. Purpose and principles

The street must feel inhabited: people walking, waiting, crossing, looking, running away, remembering. Civilians are
not decoration — they are the **witnesses** the whole police system already depends on ("unwitnessed crimes never
enter the list — that is the whole point of a getaway", PROJECT_OVERVIEW §4.8), the victims of run-overs, and
obstacles the police and traffic must respect.

Principles:

1. **A civilian is the same person as everyone else on foot.** Pawn = `AMurdarCharacter`, brain =
   `AMurdarNPCAIController` in the `Civilian` faction. One foot-AI system, extended — not a second one.
2. **Nobody appears or vanishes in view.** Spawn and despawn only where the player cannot see.
3. **Knowledge is local.** A civilian knows what it saw or heard; panic spreads person to person with delay, no
   hive mind (same rule as police §34).
4. **1990s Romania.** No mobile phones: a witness reports by reaching a phone booth, a police unit or a
   militia/police station, which takes time and can fail. (Proposal — confirm with the user; see §9.)
5. **Felt, not shown.** No HUD for crowd mood or witnesses. The player reads the street.
6. **Tick discipline.** Brains on timers, population on a 2 Hz timer, nothing per frame that doesn't have to be.
7. **Budgets are measured, not invented.** Phase 0 measures one civilian's cost; the budget follows from it.
8. **Every tunable** has one owner, unit, default, min/max, a reason (police spec §1.11).

## 1. Rules for Claude Code

The police spec's §1 rules 1–13, 22, 30–31 apply verbatim (phases in order, GATA criteria, compile after each
phase, no invented APIs, Phase 0 recon in `Docs/CIVILIAN_PEDESTRIAN_RECON.md`, no automatic `.uasset`/`.umap`
edits — *new* generated assets and generated test maps allowed as the user already allowed for police; no magic
numbers; report IMPLEMENTED / TESTED / FAILED / BLOCKED / NEXT; conflicts with the project → report, smallest
compatible change). Plus:

- Do not change police or traffic behaviour to make civilians work; if they must change, it's its own step with
  the police regression (Police Vehicle AI test report).
- Never save the level from a script.

## 2. What exists (from PROJECT_OVERVIEW / RECON — verify in Phase 0)

| Need | Existing | Gap |
|---|---|---|
| Body, animation, ragdoll, health | `AMurdarCharacter` (GASP motion matching), `Knockdown()`, `UHealthComponent` | Cost per character unknown at crowd scale. Visual variety: only the UEFN/UE5 mannequins. |
| Brain | `AMurdarNPCAIController`: `Idle, Engage, Reload, SeekAmmo, Hide, Flee`, 5 Hz Think, navmesh steering, Civilian faction reports crimes | No wandering, no routine, no crossing, no cower/curious, no panic spread. |
| Spawn | `UMurdarAILibrary::SpawnNPC`, `MurdarNPC Civilian` cheat | No population, no pooling, no despawn. |
| Navigation | Human navmesh agent (r 40), `MurdarNav::FindPath` | No sidewalk/road distinction. ZoneGraph `Pedestrian` lane tag planned (EDITOR_TASKS 3), not confirmed. |
| Witness | Faction memory: civilian sightings reach the police; `Crime.HitPedestrian` | Reporting is instant (verify). No reporting delay/channel. |
| Cars vs people | Run-overs (`UVehicleEffectsComponent`), whiskers query Pawn object type | Traffic doesn't yield to crossing pedestrians (verify). |
| Places | `AZoneVolume` with `Zone.*` tags, SmartObjects plugin enabled | No density per zone, no activities. |
| Dormancy | the driver in a car is made dormant (no motion-matching search, no movement update) | Reusable for pooled/far civilians — verify how. |

## 3. Architecture (proposal)

```
UCivilianPopulationSubsystem (World, 2 Hz timer)          who exists, where, how many, which LOD
   ├─ UCivilianPopulationSettings (DeveloperSettings)     budget, rings, rates, default density/archetypes
   ├─ UCivilianAreaSettings data: Zone.* tag → density, archetype weights
   └─ pool of inactive AMurdarCharacter (hidden, no collision, dormant brain)
AMurdarNPCAIController (existing, Civilian faction)       + Wander, Cross, Activity, Curious, Cower, Report; Flee exists
   └─ UCivilianProfile (PrimaryDataAsset)                 personality: speed, bravery, curiosity, witness reliability
UCivilianReactionComponent? (decide in Phase 4)           stimulus → reaction, panic spread (hearing + bus)
Navigation: navmesh + UNavArea classes (Sidewalk cheap, Road expensive, Crosswalk cheap) — or ZoneGraph ped lanes
Faction memory (existing)                                  + report delay / channel (Phase 5)
```

Decision recorded: **actor-based, not Mass**. Mass Crowd is in the engine but would be a second foot-AI with its own
representation, and would lose the shared kit (weapons, ragdoll, run-overs, witness logic). Revisit only if Phase 7
proves actors can't reach the budget the user wants.

## 4. Population

- Target count = `DensityPerHectare(zone at the player) × area of the spawn disc × DensityScale`, capped by
  `MaxCivilians`. Zone density comes from `Zone.*` tags; a default applies outside zones.
- Spawn only in the ring [`SpawnRingMinCm`, `SpawnRingMaxCm`] around the player, on the Human navmesh, at least
  `MinSeparationCm` from other civilians, and **not visible**: outside the camera's view cone (+margin) *or* occluded
  (line trace from the camera to head height).
- Despawn (to the pool) only when farther than `DespawnDistanceCm` **and** not seen for `DespawnUnseenSeconds`.
- Never despawn: a civilian who is a witness with an unreported crime, fleeing, knocked down, dead (corpses follow
  `CorpseSeconds`), or in an interaction with the player.
- Pool: deactivated pawns are hidden, collision off, movement and brain dormant; reused before spawning new.
- Rate limit: at most `MaxSpawnsPerUpdate` per update (no hitch bursts).
- A time-of-day density multiplier hook (no day/night system exists; default 1).
- Not saved (like pursuits). After load the street repopulates out of view.

## 5. Behaviour (states added to the foot brain)

| State | Entry | Exit |
|---|---|---|
| **Wander** | default for civilians | picks the next point on the sidewalk graph / nav area, prefers continuing direction |
| **Cross** | the route needs the road | waits at the kerb until the gap to approaching cars ≥ `CrossGapSeconds`; crosses at walking pace; in the road never stops unless blocked; emergency sirens → waits |
| **Activity** | a free SmartObject nearby (bench, kiosk, bus stop, phone booth) and profile likes it | activity time over, or a stimulus |
| **Curious** | noticed an event below its fear threshold (crash, fight, police stop) | looks from a distance `CuriousDistanceCm`, may become a witness; flees if it escalates |
| **Cower** | gunfire close, bravery low, no clear flee route | crouch/cover until quiet for `CowerQuietSeconds` |
| **Flee** (exists) | fear above threshold | away from the source, off the road, until `FleeDistanceCm` and quiet |
| **Report** (Phase 5) | witnessed a crime, reliability roll passed | goes to the nearest phone booth / police unit / station; report arrives in the faction memory then — not before |

Stimuli: gunshots (`Event.Combat.Shot` on the bus + hearing), hits/deaths nearby, car crash (`Event.Vehicle.Crashed`),
a car on the sidewalk, the player aiming at them, sirens, a fight (melee events), bodies. Panic spreads: a fleeing
civilian is itself a stimulus for others within `PanicSpreadRadiusCm`, weaker, with `PanicSpreadDelaySeconds`.

## 6. Cars and people

- Traffic and police see pedestrians (whiskers already include Pawn): verify, then make pedestrians in the lane
  ahead a **hard** obstacle for traffic (never suppressed as "traffic we aren't closing on").
- Traffic yields to civilians already in the road; civilians only enter with a gap (§5 Cross).
- The police tactical abort rules (PIT/Ram/SideSweep) include pedestrians in the risk zone (police spec §21, gap C4).
- Run-overs keep working exactly as today.

## 7. LOD

| LOD | Range (measured in Phase 7) | Brain | Movement | Anim | Perception |
|---|---|---|---|---|---|
| 0 | near / visible | 5 Hz | full | full GASP | sight + hearing |
| 1 | mid | 1 Hz | full | update-rate optimised | hearing only (bus) |
| 2 | far / not visible | 0.5 Hz | navmesh straight segments | none/frozen | bus only |
| pooled | — | off | off | off | off |

LOD reassigned on the population timer; never changes something the player is looking at in a visible way.

## 8. Debug and telemetry

`Murdar.Civ.Debug 1`: rings, each civilian's state/LOD, spawn rejections. Stats: active, pooled, spawned,
despawned, rejected (visible / nav / separation), **visible pops (must stay 0)**. Cheats: `MurdarCivilians`,
`MurdarCivMax <n>`, `MurdarCivDensity <scale>`, `MurdarCivPanic` (stimulus at the crosshair).

## 9. Open decisions for the user

1. Phone booths / no phones reporting (principle 4) — yes/no.
2. Visual variety: which character meshes/outfits (manual content task); until then everyone is a mannequin.
3. Target density for a street (e.g. 20–40 visible people) — the Phase 7 budget will say what's affordable.
4. Do civilians fight back (a hostile subset)? Default: never — `Hostile` faction exists for that.

## 10. Phases

| Phase | Implement | GATA |
|---|---|---|
| 0 Recon | measure one `AMurdarCharacter` NPC (game thread, anim, CMC) standing and walking; foot brain states/API; dormancy path; spawn API; nav areas vs ZoneGraph ped lanes; whether whiskers see pawns; how civilian witness reports reach the memory | `Docs/CIVILIAN_PEDESTRIAN_RECON.md` with numbers, no assumed API |
| 1 Population core | subsystem, settings, pool, spawn ring, visibility, despawn, stats, cheats; civilians stand idle | PED-T01–T04 |
| 2 Sidewalks + Wander | nav areas (or ped lanes), Wander state, profiles (speed) | PED-T05–T06 |
| 3 Crossing + traffic | Cross state, traffic yields, pedestrians as hard obstacles | PED-T07–T09 + police regression |
| 4 Reactions | Curious, Cower, Flee tuning, panic spread | PED-T10–T13 |
| 5 Witness + reporting | report channel/delay, reliability, description confidence | PED-T14–T16 |
| 6 Activities | SmartObjects: bench, kiosk, bus stop, phone booth | PED-T17 |
| 7 LOD + performance | LOD table, benchmark 10/25/50/100 | PED-T18–T20 |
| 8 Regression | all PED + police PVA + foot NPC + run-over tests | all PASS |

## 11. Tests

| ID | Test | Pass |
|---|---|---|
| PED-T01 | Population fills to target around a standing player | target ± 10 % within `target / MaxSpawnsPerUpdate × interval` seconds |
| PED-T02 | No visible pop | 10 min drive + walk, camera sweeping: 0 spawns and 0 despawns inside the view cone unoccluded |
| PED-T03 | Budget | active never > `MaxCivilians` |
| PED-T04 | Pool reuse | after the first fill, new spawns come from the pool (spawned-new count flat) |
| PED-T05 | Sidewalk discipline | walking civilians spend < 2 % of time on road areas outside crossings |
| PED-T06 | Wander variety | no two civilians on the same path loop; no one stuck > 5 s |
| PED-T07 | Crossing gap | 0 run-overs caused by a civilian entering the road with a gap < `CrossGapSeconds` |
| PED-T08 | Traffic yields | traffic stops for a civilian in the lane, 0 contacts |
| PED-T09 | Police regression | Police Vehicle AI Phase 3–9 numbers unchanged within noise with civilians present |
| PED-T10 | Gunshot reaction | civilians within hearing react within 0.3–1.2 s (profile spread), not all identically |
| PED-T11 | Panic spread | out-of-sight civilians start fleeing only via a fleeing neighbour, with delay |
| PED-T12 | Cower | low-bravery civilian with no flee route cowers, recovers after quiet |
| PED-T13 | Curious | a crash draws watchers at `CuriousDistanceCm`, not into the road |
| PED-T14 | Unwitnessed crime | crime with no civilian line of sight → no report, no heat |
| PED-T15 | Report delay | witnessed crime reaches the faction memory only when the witness reports |
| PED-T16 | Silencing the witness | a witness killed before reporting → no report |
| PED-T17 | Activities | SmartObjects claimed and released; no two users on one slot |
| PED-T18 | LOD transitions | no visible pops or animation snaps at LOD changes |
| PED-T19 | FPS independence | same behaviour at 30/60/144 FPS |
| PED-T20 | Performance | game-thread cost at 10/25/50/100 civilians, table in the report |

## 12. Not accepted

Spawning or despawning in view; civilians who all react identically at the same instant; teleporting a visible
civilian; civilians who know where the player is without seeing him; instant reports from nowhere; civilians
walking in the road as a habit; a second foot-AI system; per-frame brains; HUD indicators of witnesses or panic;
magic numbers; invented APIs.
