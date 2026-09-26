# AI LOD + plan de streaming (World Partition) — handoff for local Claude Code

## Pe scurt (română)

Azi fiecare pieton își rulează „creierul” la 20 Hz și fiecare mașină semnalizarea la 20 Hz, fie că e lângă tine, fie
la 200 m după un bloc. Sistemul ăsta îi ordonează la fiecare 0,25 s: **aproape și în câmpul vizual** primii, apoi
restul pe distanță. Cei angajați (poliție în urmărire, NPC-uri cu țintă, actori marcați „Mission”) rămân mereu la
viteză maximă, în afara bugetului. Din rang rezultă intervalele de tick: creierul pietonului, mișcarea, animația
doar când se vede, semnalizarea mașinilor și — doar departe și nevăzut — deciziile șoferului de trafic. **Fizica
mașinilor nu e niciodată încetinită.** Urcarea în rang e imediată, coborârea doar după 3 treceri (fără pâlpâit).
Plus: raza de streaming a mașinii tale crește cu viteza (World Partition), în trepte, cu histerezis.
Se oprește cu `murdar.AILod 0`, pentru comparații în profiler.

**Verified here:** `SignificanceRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 7 tests / 35 checks pass (writing
them caught a real bug: engaged units were eating the full-rate budget). **Not compiled:** the Unreal files. Written
against the real source (`UPedestrianComponent` 20 Hz tick, `UVehicleSignalsComponent` 20 Hz, `UTrafficDriverComponent`
PrePhysics, `AMurdarNPCAIController` ticking only with a goal/target, `AMurdarPoliceAIController::GetState`,
`UMurdarAISettings::DespawnDistanceCm` 22 000, think timers already staggered).

## Paste this prompt into local Claude Code

```
Read Handoff/AILod/README.md. One step at a time, reporting in Romanian:
1. Run the unit test (README §Tests).
2. Before copying anything, measure: PIE on the busiest map, `stat game`, `stat unit`, `stat anim`, and an Insights
   trace of 30 s driving through traffic. Save the numbers in Docs/AI_LOD_REPORT.md (BEFORE).
3. Copy Handoff/AILod/Source/Murdar_GameDev/AI/Lod/* into Source/Murdar_GameDev/AI/Lod/. Add "WorldPartition"-related
   module deps only if the build asks (UWorldPartitionStreamingSourceComponent lives in Engine). Check the ADAPT in
   UpdateStreaming against the 5.8 headers. Build (editor closed).
4. README §Checks 1-3 (root motion, driver tick, streaming source). Ask me before editing any Blueprint.
5. Measure again (AFTER) with `murdar.AILod 1` and `0`; run README §In-game tests. Report IMPLEMENTED/TESTED/FAILED/
   BLOCKED/NEXT with the numbers.
```

## Files (new)
| File | What |
|---|---|
| `AI/Lod/SignificanceRules.h` | pure: score, tiers with budgets, hysteresis, intervals per tier, speed streaming radius — unit-tested |
| `AI/Lod/SignificanceSubsystem.h/.cpp` | 4 Hz pass over AI pawns; applies intervals on tier change; cvar `murdar.AILod` |

No patches to existing code: it only calls `SetComponentTickInterval` and sets the mesh's
`VisibilityBasedAnimTickOption`, and only on AI pawns.

## Checks before trusting it

1. **Root motion.** `OnlyTickPoseWhenRendered` stops the pose off-screen. If pedestrians move with root motion
   (GASP motion matching drives the capsule), an unrendered one would stop walking. Check the pedestrian AnimBP /
   movement: if root motion drives them, change `ApplyTier` to `OnlyTickMontagesWhenNotRendered` (or keep
   `AlwaysTickPose`) and rely on the brain/movement intervals only.
2. **Traffic driver at 10 Hz.** Only for Dormant cars (> 300 m and out of view). With despawn at 220 m this is rare
   (police patrols far away). Watch one with the traffic debug: it must hold its lane on a curve.
3. **Streaming source.** `UpdateStreaming` does nothing unless the player's car has a
   `WorldPartitionStreamingSourceComponent` (step 3 of the plan below).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/AILod/Source/Murdar_GameDev/AI/Lod Handoff/AILod/Tests/significance_rules_test.cpp -o st && ./st` → `35 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| LOD-01 | busy street, `murdar.AILod 1` vs `0` | game thread time lower with 1 (numbers in the report) |
| LOD-02 | turn the camera from an empty street to a crowd | the crowd animates at once (promotion is immediate) |
| LOD-03 | walk away from a crowd, look back after 5 s | nobody frozen mid-step in view; they walk on normally |
| LOD-04 | a pursuit, the police unit 200 m behind, out of view | it keeps full rate (engaged) and still catches up |
| LOD-05 | shoot near pedestrians 100 m away | they react (flee) within ~0.5 s |
| LOD-06 | a car 180 m ahead through a long straight street | signals still blink (Minimal: 4 Hz) |
| LOD-07 | an actor tagged `Mission` far away | full rate |

(Add a `MurdarLod` cheat that prints `USignificanceSubsystem::Describe()` if useful — same pattern as the others.)

## Streaming plan (World Partition) — for the whole city

Not code yet: the order to do it in, and the rules the systems above already assume.

1. **Check what the maps are.** Open the Freeroam / Sandbox map: World Settings → *Enable Streaming*. If it is a
   classic level, keep it for now; convert only the big city map (Tools → Convert Level), on a copy, with the user.
2. **Grids.** Main grid: cell 128 m, loading range 256 m (base streaming radius 250 m, above). Streets are narrow and
   blocks tall: most of what is further is hidden anyway. Landmarks (the tall blocks, the Casa Poporului skyline)
   in a separate grid (cell 512 m, range 1 km) or always-loaded with HLOD.
3. **The car is the streaming source.** Add `WorldPartitionStreamingSourceComponent` to the player's car
   Blueprint (with the user) — the radius then grows with speed (250 → 600 m at 140 km/h, steps of 50 m) so a fast
   car doesn't outrun the city. On foot, the player controller's default source is enough.
4. **HLOD.** Instanced HLOD for props (lamps, benches, kiosks), merged/simplified for buildings. Rebuild HLODs in
   the packaging step (`Tools/Package_Murdar.bat` — add the HLOD builder commandlet before BuildCookRun).
5. **Data layers.** A runtime *Night* layer (lit windows, neon, street-lamp lights) activated on `Event.Time.Dusk`
   and off on `Dawn` (Handoff/TimeOfDay); per-mission layers for staged scenes (activated by `Event.Mission.Started`).
6. **AI must stand on loaded ground.** Invariant: `DespawnDistanceCm` (220 m) < base streaming radius (250 m). Keep
   it when either changes. Population spawns only where the ZoneGraph lanes / navmesh are loaded (ZoneGraph data
   streams with its cells): `ManagePopulation` must skip spawn points whose cell isn't loaded (ADAPT: query
   `UWorldPartitionSubsystem::IsStreamingCompleted` for a small source at the point, or check the lane exists).
7. **Runtime-spawned actors don't stream** (cars, pedestrians, the player's car). Police units far away on unloaded
   ground: despawn or park them (the response director), never let them fall.
8. **Traffic `FindLeader`** scans every car for every car (O(N²)). With more cars after streaming, give the traffic
   subsystem a per-lane list (cars sorted by distance along each ZoneGraph lane) and look up only the car ahead on
   the same and the next lane. Measure first; do it when the profile shows it.
