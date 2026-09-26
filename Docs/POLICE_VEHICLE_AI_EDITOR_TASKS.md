# Police Vehicle AI — manual editor tasks

What the code cannot (and per spec §57 must not) do for you. Ordered by the phase that needs it. Each item says
what unblocks and how to check it. New assets created by scripts are listed at the end; existing `.uasset`/`.umap`
are never modified by automation.

## Phase 2 — road network: DONE by script on 2026-09-20 (the user authorised generated test maps)

`Content/Python/setup_test_intersection.py` generates `Content/Murdar/Maps/PoliceAI_Test_Intersection` (3×3 grid,
9 junctions, blocks, navmesh, patrol/traffic points, chapter `DA_Chapter_PoliceTest`). Re-run it to regenerate; it
never touches L_Sandbox. ZoneGraph is enabled in the .uproject; lane profile `TwoWay_350` lives in
`Config/DefaultPlugins.ini`. Real-city roads are still hand-authored ZoneShape actors — the steps below describe that.

## Road network by hand (for the real map)

Status: step 1 is **done** (ZoneGraph is enabled in the .uproject, the module dependency exists since Phase 2).
Step 2 is done for `PoliceAI_Test_Intersection` by script; `PoliceAI_Test_StraightRoad` does not exist yet. Step 3
(lane tags) — check `Config/DefaultPlugins.ini`: only the `TwoWay_350` profile is recorded here; the `Emergency` and
`Pedestrian` tags are not confirmed. Steps are kept for the real city map.

1. ~~**Enable the ZoneGraph plugin**~~ (done) — Edit ▸ Plugins ▸ "ZoneGraph" (Runtime/AI), restart. (Or add
   `{"Name": "ZoneGraph", "Enabled": true}` to `Murdar_GameDev.uproject` — the code side will add the module
   dependency when Phase 2 starts.)
2. **A test map with roads** — `Content/Murdar/Maps/PoliceAI_Test_StraightRoad` and `PoliceAI_Test_Intersection`
   (spec §45): a flat floor, a two-lane road 400+ m long (spline `ZoneShape`, lane profile "TwoLane" 2 × 350 cm,
   opposite directions), one 4-way intersection (polygon `ZoneShape`), a Car navmesh (`NavMeshBoundsVolume`,
   agent "Car"). Build ZoneGraph (Build ▸ Build ZoneGraph) and the navmesh. Check: the ZoneGraph debug draw shows
   lanes with arrows in both directions; `Murdar.Pursuit.Debug 1` shows a route.
3. Lane tags in Project Settings ▸ ZoneGraph: `Vehicle`, `Emergency` (reserved for later), `Pedestrian`.

## Needed before Phase 6 — emergency driving

4. **Siren audio**: a looping `SoundWave`/`MetaSound` (`Content/Murdar/Audio/Police/S_Siren_Loop`), attenuation
   ~150 m. **Light bar**: emissive material with a `Flash` scalar (or two `SpotLight`s) on the police vehicle
   definition's body. The component will look for a socket/component named `LightBar` and a `USoundBase` on the
   profile; until they exist it logs once and drives without.
5. `DA_Vehicle_Police`: check it points at the police body mesh with the light bar (currently the sedan body).

## Needed before Phase 8/10 — dispatch

6. **Police stations / spawn origins**: actors tagged `PoliceStation` (any actor, 2–3 per sector), placed at road
   edges with a clear 8 m in front (spawn checks for overlap but not for a wall 3 m ahead).
7. **Sectors**: `AZoneVolume`s with `Sector.*` tags covering the map (one exists: `Sector.Sandbox`).
8. **Patrol points**: actors tagged `PatrolPoint` (L_Sandbox has 4) — on the road, not in the middle of the floor.

## Phase 6 — siren and lights (code done, assets missing)

8b. A siren loop (`USoundBase`): set `PoliceSirenSound=/Game/...` under `[/Script/Murdar_GameDev.MurdarAISettings]` in
    `Config/DefaultGame.ini` (or per car on `UPoliceEmergencyComponent::SirenSound`). Until then the siren is silent and
    everything else (state, lights, traffic yielding, events) runs.
8c. A light-bar mesh, if wanted: `UPoliceEmergencyComponent::EnsureParts` makes two point lights on the roof; swap them
    there.

## Needed before Phase 9 — tactics test maps (spec §45)

9. `PoliceAI_Test_PIT` (long straight, 600 m), `PoliceAI_Test_BoxIn` (wide road, 4 lanes), `PoliceAI_Test_Roadblock`
   (T-junction with one exit), `PoliceAI_Test_Recovery` (dead end, parked cars, a 7.5 m gap between blocks),
   `PoliceAI_Test_LostPursuit` (two parallel roads with a cross street), `PoliceAI_Test_Traffic` (needs traffic
   AI — out of this spec's scope, see RECON C4), `PoliceAI_Test_WorldEvent` (any of the above + a suspect start).

## Collision / channels

10. Nothing to do now. Trace channel `Obstacle` (`ECC_GameTraceChannel3`) exists and is unused; the whiskers query
    by object type. If parked-car meshes ever need to be seen as obstacles but not block traces, this is the channel.

## Created by scripts (new assets only)

| Asset | Script | Phase |
|---|---|---|
| `/Game/Murdar/AI/Police/Police_{Normal,Disciplined,Aggressive,Elite,Inexperienced,Tactical}` (`UPoliceDrivingProfile`) | `Content/Python/setup_police_profiles.py` | 1 |
| `Config/DefaultGame.ini` `[MurdarAISettings] DefaultPoliceProfile` | hand edit | 1 |

To assign a profile to one crew: select its `AMurdarPoliceAIController` (or the spawner that creates it) ▸ Police ▸
Profile. Empty = the settings default (Police_Normal).
