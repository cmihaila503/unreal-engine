# Timpul zilei (Time of Day) — handoff for local Claude Code

## Pe scurt (română)

Un ceas de joc: o zi durează 48 de minute reale (reglabil). Soarele se mișcă, se face noapte, mașinile AI aprind
farurile la amurg, pe stradă sunt mai puțini oameni noaptea și mai mulți la 8 și la 18, iar ora se salvează odată cu
povestea. Fără UI: ora se citește din cer. Un capitol își poate alege ora de început sau poate îngheța timpul.
„Paranoia în retrovizoare” folosește noaptea de aici, în loc de setarea fixă.

**Verified here:** `TimeOfDayRules.h` (clock, sun angles, light level, hourly curves) — g++ C++17 `-Wall -Wextra
-Wshadow`, 4 tests / 28 checks pass. **Not compiled:** the Unreal files. Written against the real source
(`UNarrativeStateSubsystem::Get/GetValue/SetValue`, `Event.State.Loaded`, `UGameEventSubsystem::Subscribe/Unsubscribe`,
`AMurdarVehicle::IsAIDriven/SetHeadlights/AreHeadlightsOn`, `UPopulationSubsystem::ManagePopulation`, `UChapterDefinition`).

## Paste this prompt into local Claude Code

```
Read Handoff/TimeOfDay/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests). It must pass.
2. Copy Handoff/TimeOfDay/Source/Murdar_GameDev/Director/TimeOfDay/* into Source/Murdar_GameDev/Director/TimeOfDay/.
   Resolve every // ADAPT:. Build.
3. Apply README §Patches 1-5 in order, building after each.
4. Do README §Manual (sun Movable, sky light) on the Freeroam map — ask me before touching the level.
5. Run README §In-game tests; write Docs/TIME_OF_DAY_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
Never save the level from a script.
```

## Files (new)

| File | What |
|---|---|
| `Director/TimeOfDay/TimeOfDayRules.h` | pure rules, unit-tested |
| `Director/TimeOfDay/TimeOfDaySettings.h` | Project Settings ▸ Game ▸ Murdar Time of Day |
| `Director/TimeOfDay/TimeOfDaySubsystem.h/.cpp` | the clock (world subsystem, 4 Hz timer on game time) |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Stat.TimeOfDay",DevComment="Game clock, minutes since midnight (saved)")
+GameplayTagList=(Tag="Stat.Day",DevComment="Game day number (saved)")
+GameplayTagList=(Tag="Event.Time.Dusk",DevComment="It has become night")
+GameplayTagList=(Tag="Event.Time.Dawn",DevComment="It has become day")
+GameplayTagList=(Tag="Event.Time.NewDay",DevComment="Midnight passed; Magnitude = day number")
```

### 2. Population follows the hour — `AI/PopulationSubsystem.cpp`, `ManagePopulation`
Scale the caps where they are compared (lines ~388-403 today):
```cpp
#include "Director/TimeOfDay/TimeOfDaySubsystem.h"
// at the top of ManagePopulation():
const UTimeOfDaySubsystem* Tod = UTimeOfDaySubsystem::Get(this);
const float People = Tod ? Tod->GetPeopleScale() : 1.f;
const float Traffic = Tod ? Tod->GetTrafficScale() : 1.f;
const int32 MaxPeds = FMath::RoundToInt(S->MaxPedestrians * People);
const int32 MaxCars = FMath::RoundToInt(S->MaxTrafficCars * Traffic);
// then use MaxPeds / MaxCars instead of S->MaxPedestrians / S->MaxTrafficCars in the two comparisons.
```
Police and parked cars stay unscaled (patrols run all night; parked cars are *more* at night — leave them).
Existing people above the new cap are not removed: they despawn normally once out of view.

### 3. A chapter sets its hour — `Director/ChapterDefinition.h` + `ChapterDirector.cpp`
```cpp
// UChapterDefinition, next to FactsOnEnter
/** Hour the chapter starts at on a Fresh entry (-1 = keep the clock). */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter|Time", meta = (ClampMin = "-1", ClampMax = "23.99"))
float StartHour = -1.f;
/** Stop the clock while this chapter runs (a mission that must stay at night). */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter|Time")
bool bFreezeTime = false;
```
```cpp
// UChapterDirector, where a chapter is entered (ADAPT: the function applying FactsOnEnter):
if (UTimeOfDaySubsystem* Tod = UTimeOfDaySubsystem::Get(this))
{
	if (Mode == EChapterEntryMode::Fresh && Chapter->StartHour >= 0.f) { Tod->SetHour(Chapter->StartHour); } // ADAPT: enum/var names
	Tod->SetFrozen(Chapter->bFreezeTime);
}
```
Existing chapter assets keep `-1` / `false` (new properties take the class default) — nothing changes until a chapter sets them.

### 4. Rearview uses real night — `AI/Rearview/RearviewSubsystem.cpp`, `UpdateDarkness` (only if Rearview is integrated)
Before the sun-pitch fallback:
```cpp
#include "Director/TimeOfDay/TimeOfDaySubsystem.h"
if (!bNight)
{
	if (const UTimeOfDaySubsystem* Tod = UTimeOfDaySubsystem::Get(this)) { bDark = Tod->IsNight(); return; }
}
```
(`MurdarNight` and `bForceNight` still win.)

### 5. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarTime <hour 0-24>: set the clock. */          UFUNCTION(Exec) void MurdarTime(float Hour);
/** MurdarTimeScale <real seconds per day> (-1 = settings, 0 = frozen). */ UFUNCTION(Exec) void MurdarTimeScale(float RealSecondsPerDay);
/** MurdarClock: print the clock, light, density. */  UFUNCTION(Exec) void MurdarClock();
```
```cpp
#include "Director/TimeOfDay/TimeOfDaySubsystem.h"
void UDirectorCheats::MurdarTime(float Hour) { if (UTimeOfDaySubsystem* T = UTimeOfDaySubsystem::Get(this)) { T->SetHour(Hour); } MurdarClock(); }
void UDirectorCheats::MurdarTimeScale(float S) { if (UTimeOfDaySubsystem* T = UTimeOfDaySubsystem::Get(this)) { T->SetDayLengthOverride(S); } }
void UDirectorCheats::MurdarClock() { if (UTimeOfDaySubsystem* T = UTimeOfDaySubsystem::Get(this)) { UE_LOG(LogTemp, Display, TEXT("%s"), *T->Describe()); } } // ADAPT: print like the others
```

## Manual (editor)

1. **The sun must be Movable** (DirectionalLight ▸ Mobility ▸ Movable) and be the atmosphere sun light; the sky
   (SkyAtmosphere) follows it by itself. Static/Stationary lights cannot rotate at runtime.
2. **Sky light**: Real Time Capture on (Lumen SW handles the rest), or the ambient stays at noon at midnight.
3. Street lamps: no system here; a later step can switch lamp actors on `Event.Time.Dusk` / `Dawn`.
4. Exposure: check auto-exposure at night (Post Process ▸ Exposure min/max) so night is dark, not grey.

## `// ADAPT:`
| Where | What |
|---|---|
| `Sun()` | the first directional light is the sun; a level with several must mark the sun (tag it and look it up by tag) |
| Patch 3 | entry-mode enum and variable names in `UChapterDirector` |

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/TimeOfDay/Source/Murdar_GameDev/Director/TimeOfDay Handoff/TimeOfDay/Tests/time_of_day_test.cpp -o tod && ./tod` → `28 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| TOD-01 | `MurdarTimeScale 120`, watch 2 min | a full day: sunrise ~6:00, noon high, sunset ~20:00, night |
| TOD-02 | `MurdarTime 22` | traffic cars' headlights on within 5 s; `Event.Time.Dusk` in the log |
| TOD-03 | `MurdarTime 3` with population on | noticeably fewer people and cars (caps × 0.05 / 0.10) as the old ones despawn out of view |
| TOD-04 | `MurdarSave`, `MurdarTime 12`, `MurdarLoad` | the clock returns to the saved hour |
| TOD-05 | change map through a chapter | the hour carries over |
| TOD-06 | pause the game 30 s | the clock does not move |
| TOD-07 | Rearview (if integrated): `MurdarTime 23`, `MurdarRearview` | `night` without `MurdarNight 1` |
| TOD-08 | police / traffic / chase regression | unchanged by day; at night only headlights and density differ |
