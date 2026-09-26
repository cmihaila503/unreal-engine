# Paranoia în retrovizoare — handoff for local Claude Code

Design: `DESIGN.md` (Romanian). Written 2026-09-26 **against the real source** (`Murdar_GameDev.zip` sent by the user),
unlike the older Handoff files. Every engine/project API used below was read in that source; the few points that could
not be settled from reading are marked `// ADAPT:`.

**Verified here:** `RearviewRules.h` (brake check, sudden turn, U-turn / round-the-block, night visibility with brake
lights, exposure, bait / stop choices) — compiles with g++ C++17 `-Wall -Wextra -Wshadow`, 18 tests / 46 checks pass
(`Tests/rearview_rules_test.cpp`). The tests caught two real bugs, both fixed: an emergency stop was read as a brake
check mid-brake; a sudden turn's rate was measured over the straight road before it, so every turn looked gentle.

**v1.1 (review, same day):** see `DESIGN.md` §Review — ordinary turns no longer count as tests; U-turn and round-the-
block added; decoy civilians; headlight flash; the stop test; the player's brake lights; human reaction delays; the
mirror camera moved out of the car body.
**Not compiled:** the Unreal files (no engine here).

## Paste this prompt into local Claude Code

```
Read Handoff/RearviewParanoia/DESIGN.md and README.md. Then, one step at a time, building after each
(Build.bat, editor closed) and reporting in Romanian:
1. Run the unit tests (README §Tests). They must pass before anything else.
2. Copy Handoff/RearviewParanoia/Source/Murdar_GameDev/AI/Rearview/* into Source/Murdar_GameDev/AI/Rearview/.
   Resolve every // ADAPT: against the source. Build.
3. Apply README §Patches (tags ini, the car's mirror input, cheats). Build.
4. Do README §Manual (input action asset, IMC mapping) — tell me what you did in the editor, or ask me to do it.
5. Run README §In-game tests and write Docs/REARVIEW_TEST_REPORT.md.
Do not modify existing .uasset/.umap except the IMC_Vehicle mapping in step 4. Never save the level from a script.
Report IMPLEMENTED / TESTED / FAILED / BLOCKED / NEXT.
```

## Files (all new; nothing existing is replaced)

| File | What |
|---|---|
| `AI/Rearview/RearviewRules.h` | pure rules, unit-tested |
| `AI/Rearview/RearviewSettings.h` | every tunable (Project Settings ▸ Game ▸ Murdar Rearview), unit + range + reason |
| `AI/Rearview/RearviewSubsystem.h/.cpp` | world subsystem, 10 Hz: darkness, the player's tests, civilian honk, starts tails, adds the mirror to his car |
| `AI/Rearview/RearviewTailController.h/.cpp` | the follower's brain (4 Hz), drives through `UVehiclePursuitComponent` like police do |
| `AI/Rearview/MirrorViewComponent.h/.cpp` | hold-to-look mirror camera, engine ducked |

Build.cs: nothing new (`ZoneGraph`, `GameplayTags`, `DeveloperSettings`, `AIModule` are already dependencies — verify).

## What it uses from the project (read in the source)

`UVehiclePursuitComponent` (`Follow`, `FollowDistance`, `PullAlongside`, `RamTarget`, `DriveTo`, `SetTargetEstimate`),
`AMurdarVehicle` (`GetSpeedKph`, `AreHeadlightsOn`, `SetHeadlights`, `BeginAIDriving`, `GetDriver`, `IsAIDriven`),
`UVehicleEffectsComponent::SetHorn`, `UVehicleSignalsComponent::GetTurn`, `UTrafficDriverComponent` (to know a civilian
car), `UPopulationSubsystem` (`IsVisibleToPlayer`, `IsClearOfBodies`, `TrafficDefinitions`, `CarClass`),
`UVehicleSubsystem::SpawnVehicle`, `MurdarRoad::NearestLane`, `UFactionMemorySubsystem` (`GetWantedLevel`,
`GetKnownVehicle`, `ReportSighting`), `UNarrativeStateSubsystem::HasFact`, `UGameEventSubsystem::Publish`.
The police controller is **not** touched.

## `// ADAPT:` points

| Where | What |
|---|---|
| `RearviewSubsystem.cpp` `UpdateDarkness` | the level's sun = first `ADirectionalLight`; check the pitch convention on your sky (Freeroam map) |
| `RearviewSubsystem.cpp` `StartTail` | clearance numbers passed to `IsClearOfBodies` — reuse the population's own if it has them |
| `MirrorViewComponent.cpp` | the engine sound is found by the component name `EngineAudio` (it is, in `AMurdarVehicle`'s constructor) — a public getter is cleaner; confirm the car's Tick never re-activates `Camera` |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`, under `[/Script/GameplayTags.GameplayTagsSettings]`

```ini
+GameplayTagList=(Tag="Event.Rearview.BrakeCheck",DevComment="Player tapped the brakes hard at night (rearview)")
+GameplayTagList=(Tag="Event.Rearview.InspectionTurn",DevComment="Player turned sharply without signalling (rearview); Magnitude +1 right / -1 left")
+GameplayTagList=(Tag="Event.Rearview.LightsOut",DevComment="Player switched his lights off at night")
+GameplayTagList=(Tag="Event.Rearview.UTurn",DevComment="Player turned back the way he came (rearview)")
+GameplayTagList=(Tag="Event.Rearview.AroundTheBlock",DevComment="Player went round a block (rearview detection route)")
+GameplayTagList=(Tag="Event.Rearview.TailStarted",DevComment="A follower got behind him; Magnitude = role (1 undercover, 2 gang)")
+GameplayTagList=(Tag="Event.Rearview.TailBlown",DevComment="An undercover follower knows he's been made and breaks off")
+GameplayTagList=(Tag="Event.Rearview.GangAttack",DevComment="A gang follower stopped pretending and rams")
+GameplayTagList=(Tag="Fact.Gang.Hunting",DevComment="Story: a gang is looking for the player (raises gang tails)")
```

Then Project Settings ▸ Murdar Rearview ▸ `GangHuntingFact` = `Fact.Gang.Hunting`. (The tags are requested by name in
code — no `MurdarTags.h` edit. If you prefer native tags, move them there and replace the `Tag(TEXT(...))` calls.)

### 2. The car: brake pedal getter + mirror input — `Vehicle/MurdarVehicle.h/.cpp`

```cpp
// MurdarVehicle.h, public, next to AreHeadlightsOn(): the brake pedal (not the handbrake) — the rearview subsystem
// drives the player's brake lamps from it; the handbrake lights nothing (slowing down dark is the old trick).
float GetBrakeInput() const { return RawBrake; }
```

The player's car gets a `UVehicleSignalsComponent` from the rearview subsystem (created the same way
`UTrafficDriverComponent` creates it), so it finally has brake lights.

```cpp
// MurdarVehicle.h, next to HornAction / LightsAction
/** Hold: look in the interior mirror (rearview). */
UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> MirrorAction;

void Input_MirrorStarted(const FInputActionValue& Value);
void Input_MirrorCompleted(const FInputActionValue& Value);
```

```cpp
// MurdarVehicle.cpp, SetupPlayerInputComponent, next to the Horn bindings (same pattern, lines ~706-712)
if (MirrorAction)
{
	EIC->BindAction(MirrorAction, ETriggerEvent::Started, this, &AMurdarVehicle::Input_MirrorStarted);
	EIC->BindAction(MirrorAction, ETriggerEvent::Completed, this, &AMurdarVehicle::Input_MirrorCompleted);
	EIC->BindAction(MirrorAction, ETriggerEvent::Canceled, this, &AMurdarVehicle::Input_MirrorCompleted);
}

// (include "AI/Rearview/MirrorViewComponent.h")
void AMurdarVehicle::Input_MirrorStarted(const FInputActionValue&)
{
	if (UMirrorViewComponent* M = FindComponentByClass<UMirrorViewComponent>()) { M->SetMirrorView(true); }
}
void AMurdarVehicle::Input_MirrorCompleted(const FInputActionValue&)
{
	if (UMirrorViewComponent* M = FindComponentByClass<UMirrorViewComponent>()) { M->SetMirrorView(false); }
}
```

Also in `Exit()`: `if (UMirrorViewComponent* M = FindComponentByClass<UMirrorViewComponent>()) { M->SetMirrorView(false); }`
(leaving the car while looking in the mirror must not leave the camera on the mirror).

### 3. Cheats — `Director/DirectorCheats.h/.cpp` (same style as the others, compiled out of shipping)

```cpp
/** MurdarNight -1|0|1: settings decide / day / night (rearview works only at night). */
UFUNCTION(Exec) void MurdarNight(int32 Mode);
/** MurdarTail undercover|gang|civilian: put a follower (or a decoy) behind the player now. */
UFUNCTION(Exec) void MurdarTail(const FString& Role);
/** MurdarRearview: darkness, tests seen, tails and their state. */
UFUNCTION(Exec) void MurdarRearview();
```

```cpp
#include "AI/Rearview/RearviewSubsystem.h"

void UDirectorCheats::MurdarNight(int32 Mode)
{
	if (URearviewSubsystem* R = URearviewSubsystem::Get(this)) { R->SetNightOverride(Mode); }
}
void UDirectorCheats::MurdarTail(const FString& Role)
{
	URearviewSubsystem* R = URearviewSubsystem::Get(this);
	const bool bGang = Role.Equals(TEXT("gang"), ESearchCase::IgnoreCase);
	const MurdarRearview::ETailRole Which = bGang ? MurdarRearview::ETailRole::Gang
		: Role.Equals(TEXT("civilian"), ESearchCase::IgnoreCase) ? MurdarRearview::ETailRole::Civilian
		: MurdarRearview::ETailRole::Undercover;
	const bool bOk = R && R->StartTail(Which);
	// ADAPT: print like the other cheats
	UE_LOG(LogTemp, Display, TEXT("MurdarTail %s: %s"), *Role, bOk ? TEXT("started") : TEXT("no place behind him (drive on a road, at night)"));
}
void UDirectorCheats::MurdarRearview()
{
	if (URearviewSubsystem* R = URearviewSubsystem::Get(this)) { UE_LOG(LogTemp, Display, TEXT("%s"), *R->Describe()); }
}
```

Add the three to `Docs/PROJECT_OVERVIEW.md` §8.

## Manual (editor)

1. `Content/Murdar/Input/IA_Mirror` — Input Action, Digital (bool). Map it in `IMC_Vehicle` (suggested key: **Q**, or
   right mouse if free). Set `MirrorAction` on the vehicle Blueprint(s) (`BP_Vehicle_Sedan` and the police car class if
   separate) — or as the C++ default if the project sets input actions in C++ (check how `HornAction` is assigned).
2. Optional mirror mask: a post-process material with a rounded-rectangle frame and a horizontal UV flip
   (`1 - U`) on `SceneTexture:PostProcessInput0`; set it in Project Settings ▸ Murdar Rearview ▸ `MirrorMaskMaterial`.
   Without it the mirror view is a narrow, vignetted, **not mirrored** rear view.
3. Optional: an unmarked car definition for `UndercoverCar` (a plain Dacia) and a gang car for `GangCar`. Empty = a
   random traffic car, which is fine — a follower that looks like anybody is the point.
4. The player has no turn indicators. The inspection-turn rule counts a turn as "announced" when the car's
   `UVehicleSignalsComponent::GetTurn() != 0`, so adding indicator input (e.g. `,` / `.` → `SetTurn(-1/+1)`) later makes
   that part work with no change here.

## Tests

### Unit (no engine)

```
g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/RearviewParanoia/Source/Murdar_GameDev/AI/Rearview ^
    Handoff/RearviewParanoia/Tests/rearview_rules_test.cpp -o rv.exe && rv.exe
```
(or MSVC: `cl /std:c++17 /EHsc /I ... rearview_rules_test.cpp`). Expected: `46 checks, 0 failed`.

### In game (Freeroam map, population on)

| ID | Setup | Pass |
|---|---|---|
| RV-01 | `MurdarNight 1`, drive at 60 km/h, `MurdarTail undercover` | a car appears behind (not in view), holds ~40 m, headlights on |
| RV-02 | RV-01, brake from 70 to 45 km/h in ~1 s | undercover: drops back, no horn. `MurdarRearview` shows `BackingOff` |
| RV-03 | `MurdarTail gang`, same brake check | gang car comes up alongside for ~4 s, then back behind |
| RV-04 | with a traffic car behind you (no tail), same brake check | it honks |
| RV-05 | tail on, lights off (L) on an unlit road | follower goes `Lost`: high beams, speeds up; turn lights on / let it close in → `found him again` |
| RV-06 | tail on, sudden 90° turn into a side street at 30+ km/h | gang: follows. undercover: follows or drives on (skill) — run 5 times, both happen |
| RV-07 | repeat tests on one gang tail until blown | alongside, then rams; `Event.Rearview.GangAttack` in the log |
| RV-08 | repeat tests on an undercover tail until blown | breaks off, vanishes out of view; `TailBlown` in the log |
| RV-09 | heat Stop (`MurdarHeat 20`), undercover tail | the radio track updates (`MurdarFactions`) while it sees you |
| RV-10 | hold the mirror key | view snaps to the mirror, engine quieter; release → back; exit the car while holding → normal camera |
| RV-11 | `MurdarNight 0` | no tails start, no reactions (day) |
| RV-12 | police regression: chase recording + Phase 7–9 tests | unchanged (the police controller is not modified) |
| RV-13 | no tail, a traffic car 30–60 m behind, brake check | it flashes its headlights twice |
| RV-14 | undercover tail (`MurdarTail undercover`), pull over and wait 5 s — repeat with several tails | sometimes it stops ~40 m behind you (rookie); sometimes it drives past and parks further on with its lights off, and pulls out behind you when you pass it |
| RV-15 | gang tail, go round a block (three rights) | it follows round and is blown (alongside, ram) |
| RV-16 | undercover tail, U-turn | usually it drives on past you (you see it pass); sometimes it turns too (then blown soon) |
| RV-17 | tail on, lights off, then slow down with the brake pedal vs. the handbrake | pedal: the follower keeps you (brake lights); handbrake: it loses you |
| RV-18 | `MurdarTail civilian` (decoy) | an ordinary car appears behind and goes its own way; `MurdarRearview` counts a decoy |
| RV-19 | an ordinary junction turn at 15–20 km/h | no `InspectionTurn` in the log (only sudden turns at 25+ km/h count) |

## Known limits (v1)

- Night is a flag or the sun's pitch; street lights don't make a dark car visible (no light sampling).
- A pro "driving on" after an inspection turn ends that tail; a later version could re-acquire him at the side
  street's far end (`MurdarRoad::JunctionsAhead` has what's needed).
- Followers run red lights to keep up (the pursuit driver has no signal logic). That is a real-world tell and is left in
  on purpose; a v2 pro could stop at the red and lose him.
- A pro that drives on (sudden turn, U-turn) ends that tail; re-acquiring at the side street's far end is v2.
- The gang attack is ramming only; getting out and shooting is the foot AI's job later.
