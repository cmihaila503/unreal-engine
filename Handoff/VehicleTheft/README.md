# Furtul de mașini (Vehicle theft) — handoff for local Claude Code

## Pe scurt (română)

- **Mașini parcate încuiate** (mai multe noaptea): „Sparge geamul” — stai la ușă ~1 s (geamul, zgomot, poate
  alarmă cu avariile pornite), apoi ~2,5 s pornești fără cheie. Dacă pleci de lângă ușă, se anulează.
- **Mașini din trafic** oprite la semafor sau care se târăsc în coloană: „Scoate-l din …” — șoferul e dat jos, se
  ceartă cu tine, iar mașina e a ta. Niciodată mașini de poliție.
- **Raportul la poliție:** victima unui carjack sună în 15–35 s; un martor în 30–75 s; dacă nu te-a văzut nimeni,
  proprietarul găsește locul gol după 4–8 minute. După raport, **prima patrulă care te vede în ea** raportează
  furtul (heat 15 = te oprește, nu te urmărește). Revopsirea (Handoff/Garage) închide dosarul.
- Mașinile pe care le-ai condus o dată și mașina capitolului (tag `Owned`) sunt ale tale: nu se încuie, nu se raportează.

Repară și o problemă existentă: azi HUD-ul arată „Urcă în …” lângă o mașină din trafic, dar `Enter` refuză
mașinile conduse de AI, deci E nu face nimic.

**Verified here:** `TheftRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 8 tests / 43 checks pass. **Not
compiled:** the Unreal files. Written against the real source (`AMurdarVehicle::Enter` refusing `bAIDriven`,
`EndAIDriving`, `UVehicleSubsystem::GetVehicles/FindEnterable`, `UTrafficDriverComponent::StopDriving` and its
confront flow, `UMurdarAILibrary::SpawnNPC`, `UPedestrianComponent::BeginConfront`, `UFactionMemorySubsystem::
ReportCrime/TimeSinceLastSighting/GetThreat`, `HeatStop 12 / HeatPursuit 40`, `UVehicleSignalsComponent::SetHazards`,
`AMurdarCharacter::Input_Interact`, `AMurdarHUD::UpdateModel`).

## Paste this prompt into local Claude Code

```
Read Handoff/VehicleTheft/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/VehicleTheft/Source/Murdar_GameDev/Vehicle/Theft/* into Source/Murdar_GameDev/Vehicle/Theft/. Build.
3. Apply README §Patches 1-5. For patch 3, read UPedestrianComponent's confront code: if a null HomeCar is not
   handled, make it end the confront without a car (the victim walks off) and tell me.
4. Run README §In-game tests; write Docs/VEHICLE_THEFT_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Vehicle/Theft/TheftRules.h` | pure: locks, carjack check, break-in machine, witnesses, report delays, paper trail — unit-tested |
| `Vehicle/Theft/TheftSettings.h` | Project Settings > Game > Murdar Vehicle Theft |
| `Vehicle/Theft/VehicleTheftSubsystem.h/.cpp` | `TryTakeCar`, `GetPrompt`, break-in, carjack, alarm, reports |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Crime.CarTheft",DevComment="A reported stolen car recognised by a patrol")
+GameplayTagList=(Tag="Event.Vehicle.Stolen",DevComment="Source = the car")
+GameplayTagList=(Tag="Event.Vehicle.Carjacked",DevComment="Source = the car")
+GameplayTagList=(Tag="Event.Vehicle.WindowBroken",DevComment="Source = the car (noise)")
+GameplayTagList=(Tag="Event.Vehicle.Alarm",DevComment="Source = the car")
+GameplayTagList=(Tag="Event.Vehicle.ReportedStolen",DevComment="Source = the car")
```

### 2. `AI/FactionMemorySubsystem.cpp` — `ReportCrime`: car theft is not violence
Add `Crime.CarTheft` to the non-violent list (it must never push heat to Lethal by itself):
```cpp
	const bool bNonViolent = Crime.MatchesTagExact(MurdarTags::Crime_Speeding) || Crime.MatchesTagExact(MurdarTags::Crime_Reckless)
		|| Crime.MatchesTagExact(MurdarTags::Crime_Evading)
		|| Crime.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Crime.CarTheft"), false));
```

### 3. `Character/MurdarCharacter.cpp` — `Input_Interact`: the car branch goes through theft
```cpp
#include "Vehicle/Theft/VehicleTheftSubsystem.h"
...
	// (weapon pickup and interactable branches unchanged, from Handoff/Interaction)
	if (UVehicleTheftSubsystem* Theft = UVehicleTheftSubsystem::Get(this))
	{
		Theft->TryTakeCar(this);
		return;
	}
	if (UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(this))   // fallback, as before
	...
```

### 4. `Character/MurdarHUD.cpp` — the car prompt comes from theft
In the "Interaction prompt" block, replace the `FindEnterable` → „Urcă în %s” branch:
```cpp
#include "Vehicle/Theft/VehicleTheftSubsystem.h"
...
		else if (const UVehicleTheftSubsystem* Theft = UVehicleTheftSubsystem::Get(this); Theft && Character)
		{
			Model.PromptText = Theft->GetPrompt(Character); // „Urcă în …”, „Sparge geamul la …”, „Scoate-l din …”, progress
		}
```

### 5. `Director/ChapterDirector.cpp` — `EnsureChapterVehicle`: the chapter car is his
```cpp
			if (AMurdarVehicle* Spawned = Vehicles->SpawnVehicle(Class, nullptr, FTransform(...)))
			{
				Spawned->Tags.AddUnique(TEXT("Owned")); // never locked, never reported (Handoff/VehicleTheft)
			}
```
Cars placed by hand in a map for the player: add the actor tag `Owned` (ask the user before editing a map).

### 6. Cheat — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarTheft: locks, stolen cars and their report state. */ UFUNCTION(Exec) void MurdarTheft();
```
```cpp
#include "Vehicle/Theft/VehicleTheftSubsystem.h"
void UDirectorCheats::MurdarTheft() { if (const UVehicleTheftSubsystem* T = UVehicleTheftSubsystem::Get(GetWorld())) { UE_LOG(LogTemp, Display, TEXT("%s"), *T->Describe()); } }
```

## Content (optional)
Project Settings > Murdar Vehicle Theft: `GlassSound` (glass breaking), `AlarmSound` (looping 90s car alarm).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/VehicleTheft/Source/Murdar_GameDev/Vehicle/Theft Handoff/VehicleTheft/Tests/theft_rules_test.cpp -o tt && ./tt` → `43 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| THF-01 | walk to parked cars | some say „Urcă în”, some „Sparge geamul la” (more at night: `MurdarTime 23`) |
| THF-02 | E at a locked one, stay | „Spargi geamul… N%”, glass, then „Pornești fără cheie… N%”, then you're in |
| THF-03 | same, walk away at 50 % | cancelled; prompt back to „Sparge geamul” |
| THF-04 | several break-ins | sometimes the alarm: hazards blink ~25 s |
| THF-05 | traffic car at a red light, E at the driver door | „Scoate-l din …”; driver out, shouting; you drive off |
| THF-06 | same at 40 km/h / at a police car | no prompt, nothing happens |
| THF-07 | carjack, drive calmly, `MurdarTheft` | „report in ~15–35 s”, then REPORTED |
| THF-08 | after REPORTED, drive past a patrol | a stop (heat ≥ 12), not a chase; `Crime.CarTheft` in the log |
| THF-09 | the chapter car / a car you already drove | never locked, never reported |
| THF-10 | steal unseen, park it, `MurdarTheft` | report in 4–8 min |

## Design notes

- No lockpicking minigame: a few seconds of standing still at a door, in the open, is the tension.
- A theft alone never starts a chase: it's a description that makes a patrol stop you. What you do then decides.
- Stolen state isn't saved (like a pursuit). Reloading forgets the paper trail; the car itself comes back with
  Handoff/WorldState.
