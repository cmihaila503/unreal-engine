# Refolosirea pietonilor (Pooling) — handoff for local Claude Code

## Pe scurt (română)

Acum, când te miști prin oraș, `UPopulationSubsystem` creează și distruge pietoni tot timpul. Crearea unui personaj
(mesh, animație, AI) costă o mică sacadare. Cu pool-ul, pietonii care ies din zonă sunt **parcați** (ascunși,
înghețați, fără coliziune, sub hartă) și **refolosiți**, resetați, când trebuie un pieton nou. Pool-ul se umple încet
(unul pe cadru, doar când jocul merge fluent), cu cel mult 16 parcați. Morții și cei căzuți (ragdoll) nu se refolosesc.

**Mașinile nu sunt încă în pool:** resetarea unei mașini fizice (starea KinetiForge, avariile, luminile, controllerul)
cere o trecere separată, după ce se vede cât câștigă pietonii.

**Verified here:** `PoolRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 4 tests / 9 checks pass. **Not
compiled:** the Unreal files. Written against the real source (`UPopulationSubsystem::TrySpawnPedestrian` →
`UMurdarAILibrary::SpawnNPC`, its despawn loop destroying controller and pawn, `AMurdarCharacter::IsRagdoll`,
`UHealthComponent::Revive/IsDead`).

## Paste this prompt into local Claude Code

```
Read Handoff/Pooling/README.md. One step at a time, reporting in Romanian:
1. Run the unit test (README §Tests). Measure first: Insights trace, 60 s walking and driving through a busy street;
   note hitches > 20 ms and the time in SpawnNPC / Destroy (Docs/POOLING_REPORT.md, BEFORE).
2. Apply README §Patches 2 (ResetForPool — read AMurdarNPCAIController and UPedestrianComponent and tell me every piece
   of state that must be cleared). Copy Handoff/Pooling/Source/Murdar_GameDev/AI/Pooling/* into
   Source/Murdar_GameDev/AI/Pooling/. Build (editor closed).
3. Apply README §Patches 1. Build. Measure again (AFTER). Run README §In-game tests.
4. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT with the numbers.
```

## Patches

### 1. `AI/PopulationSubsystem.cpp` — spawn and despawn through the pool
- Both `UMurdarAILibrary::SpawnNPC(GetWorld(), ENPCFaction::Civilian, …, nullptr, MurdarTags::Sector_Sandbox)` in
  `TrySpawnPedestrian` become `UActorPoolSubsystem::Get(this)->AcquirePedestrian(<same transform>, MurdarTags::Sector_Sandbox)`
  (fall back to SpawnNPC when the pool is null).
- In the despawn loop, for pedestrians: replace `C->Destroy(); A->Destroy();` with
  `Pool->ReleasePedestrian(Cast<AMurdarCharacter>(A));` (cars keep the old path).
- `GetOrCreateAmbient()` + `BeginStroll/BeginChat` after acquiring stay as they are: they re-arm the street life.

### 2. `AI/MurdarNPCAIController.h/.cpp` — back to a blank civilian
```cpp
/** The pool is putting this person away: forget everything (goals, targets, fear, remarks, hold-fire). */
void ResetForPool();
```
ADAPT: clear the goal / target / last-seen memory, stop the think-timer work that depends on them, disable the actor
tick (it only runs with a goal), reset `UPedestrianComponent` to Idle (its mode, confront target, home car, panic).

### 3. Cheat
```cpp
/** MurdarPool: parked / reused / spawned / destroyed. */ UFUNCTION(Exec) void MurdarPool();
```

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Pooling/Source/Murdar_GameDev/AI/Pooling Handoff/Pooling/Tests/pool_rules_test.cpp -o pl && ./pl` → `9 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| POL-01 | stand still 20 s | `MurdarPool`: parked climbs to 8, one at a time |
| POL-02 | walk around busy streets 2 min | reused ≫ spawned; fewer hitches than BEFORE |
| POL-03 | a reused pedestrian | walks / chats normally, no leftover panic or confront, visible, collides |
| POL-04 | kill a pedestrian, walk away | the body is destroyed, never reused |
| POL-05 | park 16+ | extra ones destroyed (parked never above 16) |
