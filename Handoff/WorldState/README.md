# Starea lumii (World State save) — handoff for local Claude Code

## Pe scurt (română)

Salvarea de azi ține povestea (fapte, valori, arme, capitol + checkpoint), dar nu lumea: la „continuă” ești pus la
checkpoint, iar mașina ta e cea a capitolului, nouă, la locul ei. Cu asta, salvarea mai ține:
- **unde erai** (pe jos sau la volan) — nu și în capitolele de poveste, care rămân la checkpoint;
- **mașina ta** (ultima condusă): unde ai lăsat-o și cât e de stricată;
- **obiectele puse în hartă care cer asta** (`UWorldStateComponent`): o ușă lăsată deschisă, o ladă mutată, un stâlp
  de iluminat împușcat — distrus rămâne distrus.
Tot în același fișier și aceeași versiune ca restul (`FNarrativeState`), tot „etichete și numere”: căi soft, poziții,
numere. O salvare veche (fără lume) se încarcă exact ca înainte.

**Verified here:** `WorldStateRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 5 tests / 21 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`FNarrativeState` + `UNarrativeStateSubsystem::Save/Load` with
tagged `SerializeItem`, `Event.State.Loaded` Magnitude 1 = from file, `UChapterDirector::ContinueFromSave` →
`EnterChapter(Restore)` → `Event.Chapter.Entered`, `EnsureChapterVehicle`, `UVehicleSubsystem::SpawnVehicle/GetVehicles`,
`AMurdarVehicle::Definition/GetDamage/GetDriver/Enter`, `Event.Player.EnteredVehicle` Source = the car).

## Paste this prompt into local Claude Code

```
Read Handoff/WorldState/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/WorldState/Source/Murdar_GameDev/Director/WorldState/* into Source/Murdar_GameDev/Director/WorldState/.
   Fix the ADAPT include paths.
3. Apply README §Patches 1-3. Before patch 1, make a copy of an existing save (Saved/SaveGames/*.mrd) to test that
   old saves still load (WS-07). Patch 2: read how Damage01 is used in AMurdarVehicle and tell me what SetDamage01
   must refresh, and whether the car body material has a colour parameter.
4. Build, run README §In-game tests; write Docs/WORLD_STATE_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/WorldState/WorldStateRules.h` | pure decisions: apply or not, where the player comes back, per-actor action, sticky records — unit-tested |
| `Director/WorldState/WorldStateTypes.h` | `FMurdarWorldState` (goes inside `FNarrativeState`) |
| `Director/WorldState/WorldStateComponent.h/.cpp` | opt-in for placed actors (destroyed / transform / a float) |
| `Director/WorldState/WorldStateSubsystem.h/.cpp` | capture before save, apply after continue |

## Patches

### 1. The save — `Director/NarrativeState.h`, `Director/NarrativeStateSubsystem.h/.cpp`
`NarrativeState.h`:
```cpp
#include "Director/WorldState/WorldStateTypes.h"
...
	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<FCarriedWeaponRecord> Weapons;

	/** Where he and his car were, and placed actors that opted in (Handoff/WorldState). Empty in v1 saves. */
	UPROPERTY()
	FMurdarWorldState World;

	/** Bumped whenever the layout above changes; Load() refuses newer files and migrates older ones. */
	UPROPERTY()
	int32 Version = 2;   // 2: World
```
`NarrativeStateSubsystem.h` (public):
```cpp
	/** Fired at the start of Save(): systems write their part of the state (world state) before it is serialized. */
	FSimpleMulticastDelegate OnBeforeSave;

	FMurdarWorldState& MutableWorld() { return State.World; }
```
`NarrativeStateSubsystem.cpp`, first line of `Save()`:
```cpp
	OnBeforeSave.Broadcast();
```
And in `Load()` at "Version migrations go here": `// v1 -> v2: no World in the file; it stays empty and nothing is applied.`
(Tagged property serialization fills missing fields with defaults — WS-07 proves it on a real old file.)

### 2. `Vehicle/MurdarVehicle.h/.cpp` — `SetDamage01` and the paint colour
```cpp
/** Restore from a save: crash damage 0..1 as it was. */
void SetDamage01(float NewDamage01);

/** Body paint. Police descriptions use it (Handoff/Garage), saves keep it. Alpha 0 = never set. */
void SetPaintColor(const FLinearColor& Color);
FLinearColor GetPaintColor() const { return PaintColor; }

private:
	FLinearColor PaintColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> PaintMats;
```
```cpp
void AMurdarVehicle::SetDamage01(float NewDamage01)
{
	Damage01 = FMath::Clamp(NewDamage01, 0.f, 1.f);
	// ADAPT: refresh whatever AddCrashDamage derives from Damage01 (the steering pull DamagePullSign, smoke, a
	// disabled engine...). If AddCrashDamage only accumulates, the rest reads Damage01 and there is nothing to do.
}

void AMurdarVehicle::SetPaintColor(const FLinearColor& Color)
{
	PaintColor = FLinearColor(Color.R, Color.G, Color.B, 1.f);
	if (!Chassis) { return; }
	// One dynamic instance per body material slot, made once. ADAPT: the parameter name in the car body material
	// ("PaintColor" here); slots whose material lacks it simply ignore the call (glass, chrome, lights).
	if (PaintMats.Num() == 0)
	{
		for (int32 i = 0; i < Chassis->GetNumMaterials(); ++i) { PaintMats.Add(Chassis->CreateAndSetMaterialInstanceDynamic(i)); }
	}
	for (UMaterialInstanceDynamic* M : PaintMats) { if (M) { M->SetVectorParameterValue(TEXT("PaintColor"), PaintColor); } }
}
```
If the car body material has no colour parameter yet, add a `PaintColor` vector parameter multiplied into the base
colour (a material edit: ask the user). Until then the colour is still recorded and used by the police description.

### 3. Cheat — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarWorld: what the save holds about this map. */ UFUNCTION(Exec) void MurdarWorld();
```
```cpp
#include "Director/WorldState/WorldStateSubsystem.h"
void UDirectorCheats::MurdarWorld()
{
	if (const UWorldStateSubsystem* W = UWorldStateSubsystem::Get(GetWorld())) { UE_LOG(LogTemp, Display, TEXT("%s"), *W->Describe()); }
}
```

## Authoring

- Door: add `WorldState` to the door Blueprint, `bSaveValue` on; when it opens set `Value` = 1, closes = 0; bind
  `OnRestored` → snap to open/closed without the animation.
- Breakable lamp / crate: `bSaveDestroyed` (default on). Pushed crate: also `bSaveTransform`.
- Only actors placed in the level are saved; spawned ones log a warning and are ignored.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/WorldState/Source/Murdar_GameDev/Director/WorldState Handoff/WorldState/Tests/world_state_rules_test.cpp -o wt && ./wt` → `21 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| WS-01 | drive the chapter car 500 m, park, walk 20 m, `MurdarSave`, `MurdarContinue` | on foot where you stood; the car where you parked it, not at the checkpoint; only one car |
| WS-02 | crash the car (damage), save, continue | same damage (steering pull / smoke as before the save) |
| WS-03 | save at the wheel, continue | at the wheel of the car, where it was |
| WS-04 | shoot out a lamp with `WorldState`, save, continue | lamp gone |
| WS-05 | open a door with `WorldState` (Value), save, continue | door open |
| WS-06 | chapter with `Fact.Consequences.Checkpoint`: save away from it, continue | at the checkpoint, chapter car as before |
| WS-07 | continue from the old (v1) save copied before patch 1 | loads; checkpoint placement as before; log "WorldState applied" does not appear |
| WS-08 | save on map A, continue into a chapter on map B | nothing moved on B |
| WS-09 | `MurdarWorld` | map, records, car, player mode |

## Design notes

- The chapter still decides the story state; this only moves things afterwards, so a broken record can't break
  the chapter (worst case: you stand at the checkpoint).
- No traffic, no pedestrians, no dropped weapons: the city is repopulated anyway; only what is *yours* comes back.
- Impound lot (car confiscated after an arrest, Handoff/Consequences): a later step — mark the record and spawn the
  car at an `Impound` tag instead.
