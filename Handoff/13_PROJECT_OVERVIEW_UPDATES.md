# 13 — PROJECT_OVERVIEW.md: what is stale since 2026-09-16

`Docs/PROJECT_OVERVIEW.md` in this repo is the user's copy, unchanged. It was written on 2026-09-16, **before** the
Police Vehicle AI work (spec phases 0–9, 2026-09-19 → 09-21). A fresh session reading it will get the police/driving
side wrong. Update these sections in the project's own copy (verify each against the source first):

| § | Says | Now (per the Police AI docs) |
|---|---|---|
| 2, Build.cs | dependency list | ZoneGraph added in Phase 2 (RECON / EDITOR_TASKS); RECON also lists `PhysicsCore` and not `Slate/SlateCore/EngineSettings` — reconcile against the real `Murdar_GameDev.Build.cs`. |
| 2, plugins | Epic plugin list | ZoneGraph enabled. |
| 2, physics | "Chaos async physics" | Correct for the scene; add: vehicles are **KinetiForge** (the spec's "Chaos Vehicles" means KinetiForge — RECON C1). |
| 3, Tools | `chase.py, crew.py, hit.py, navtest.py, radio.py` | + `policetest.py`, `circletest.py`, `chaserun.ps1`, `chaselog.py`, `griptest.py`, `acceltest.py`, `aitest.py`, `handling.py`. |
| 4.8, `AMurdarPoliceAIController` | 6 states | + Recovery, Disabled, Searching; `UPoliceDrivingProfile` (six assets), decision trace, `FPoliceUnitTelemetry`, roles. |
| 4.8, `UVehiclePursuitComponent` | "one forward trace", 9 modes | 5 whiskers + 2 rails per frame (see 05), modes + `BoxRear`, `SideSweep`; lane routing via `RouteAim`; recovery / escape counting; PIT abort. |
| 4.8, new classes | — | `UPoliceResponseDirector` (roles, backup), `UPoliceEmergencyComponent` (siren/lights, traffic yields), `MurdarRoad` / `UMurdarRoadTools` (ZoneGraph lane Dijkstra), `AMurdarTrafficAIController` (civilian traffic). |
| 4.8, `MurdarNav` | navmesh only | Lanes first, navmesh fallback. |
| 5, tick discipline | "almost nothing ticks per frame" | The pursuit driver ticks per frame by necessity (steering); its whiskers don't have to (05). |
| 6, content | one map, one chapter | + `PoliceAI_Test_Intersection` (generated), `DA_Chapter_PoliceTest`, `/Game/Murdar/AI/Police/Police_*` profiles; scripts `setup_test_intersection.py`, `setup_police_profiles.py`. |
| 7, DefaultGame.ini | AI settings | + `DefaultPoliceProfile`, `PoliceSirenSound` (empty). `DefaultPlugins.ini`: lane profile `TwoWay_350`. |
| 8, cheats/cvars | list | + `MurdarTraffic N`, `MurdarChaseRecord`, `Murdar.Police.Debug` (check `DirectorCheats` for the full list). |
| 9, history | ends at `3c14052` | add the Police AI phase commits. |
| 10, "no traffic" | no city, no traffic | civilian traffic exists on lane maps; still no city. Police spawn still only by cheat. |
| new § | — | Pointer to `Docs/POLICE_VEHICLE_AI_*.md` as the police/driving source of truth, and to `Handoff/` while it is being applied. |
