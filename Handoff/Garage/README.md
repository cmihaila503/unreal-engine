# Garaje: vopsitorie, garaj, depozit auto (Garage) — handoff for local Claude Code

## Pe scurt (română)

- **Culori:** mașinile primesc o culoare din paleta străzii anilor '90 (alb, roșu, bej, verde…), iar **poliția ține
  minte modelul și culoarea**, nu doar modelul cum face azi.
- **Vopsitoria:** intri cu mașina, oprești. Dacă poliția nu te vede în acel moment și ai bani: ecranul se
  întunecă 2 secunde, iar mașina iese în altă culoare, reparată. Dosarul de furt se închide. Dacă erai urmărit dar
  nevăzut, urmărirea se răcește la nivel de „te oprește” (ca în GTA). Cu poliția pe urme costă dublu.
- **Garajul tău:** lași mașina în boxă și ieși pe jos: e păstrată (și salvată). La ghișeu: „Scoate …”.
- **Depozitul auto:** dacă ești arestat lângă mașină (Handoff/Consequences), mașina merge la depozit. La ghișeu:
  „Plătește și ia …” (200 lei + 50 pe zi).

**Verified here:** `GarageRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 7 tests / 30 checks pass. **Not
compiled:** the Unreal files. Written against the real source (`FKnownVehicleRecord`, `MatchesKnownVehicle` matching
by model only, `ReportSighting`, `HeatStop/HeatPursuit`, `UVehicleSubsystem::SpawnVehicle`, `AMurdarVehicle::
Definition/GetDamage/GetSpeedKph`) and the handoffs it builds on.

**Depends on:** WorldState (paint API + saved records — take its current files), Economy, Interaction, VehicleTheft,
Consequences (for the impound).

## Paste this prompt into local Claude Code

```
Read Handoff/Garage/README.md. WorldState, Economy, Interaction, VehicleTheft and Consequences must be integrated
(WorldState with the paint patch and FStoredCarRecord — re-copy its files if an older version is in). One step at a
time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Garage/Source/Murdar_GameDev/Vehicle/Garage/* into Source/Murdar_GameDev/Vehicle/Garage/. Build.
3. Apply README §Patches 1-4.
4. Ask me before placing garages in a map: README §Setup.
5. Run README §In-game tests; write Docs/GARAGE_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Vehicle/Garage/GarageRules.h` | pure: colour distance, description match, new colour, prices, respray gate, heat after, slots — unit-tested |
| `Vehicle/Garage/GarageSettings.h` | Project Settings > Game > Murdar Garages (palette, prices, lines) |
| `Vehicle/Garage/MurdarGarage.h/.cpp` | the garage actor: bay, spawn point, counter (Interactable); respray / storage / impound |
| `Vehicle/Garage/GarageSubsystem.h/.cpp` | last car, impound after arrest, random paint for spawned cars |

## Patches

### 1. Police describe model + colour — `AI/FactionMemorySubsystem.h/.cpp`
```cpp
// FKnownVehicleRecord
	UPROPERTY(BlueprintReadOnly, Category = "Memory") FLinearColor Paint = FLinearColor(0.f, 0.f, 0.f, 0.f);
```
In `ReportSighting`, with the other `KnownVehicle.*` writes: `KnownVehicle.Paint = Obs.Vehicle->GetPaintColor();`
`MatchesKnownVehicle` — the same car counts only while it still looks the same:
```cpp
#include "Vehicle/Garage/GarageRules.h"
#include "Vehicle/Garage/GarageSettings.h"
...
	// Same car or same model — and the colour a witness would call the same (a respray breaks the description).
	const bool bSameModel = KnownVehicle.Vehicle.Get() == Vehicle || (KnownVehicle.Definition.Get() && KnownVehicle.Definition.Get() == Vehicle->Definition);
	if (KnownVehicle.Paint.A <= 0.f || Vehicle->GetPaintColor().A <= 0.f) { return bSameModel; } // colour unknown: as before
	return MurdarGarage::MatchesDescription(bSameModel, UGarageSettings::ToC3(KnownVehicle.Paint), UGarageSettings::ToC3(Vehicle->GetPaintColor()),
		GetDefault<UGarageSettings>()->SameColourDistance);
```
(ADAPT: if `HeatStop` / `HeatPursuit` are not public, add getters — the garage reads them.)

### 2. Every spawned car gets a colour — `Vehicle/VehicleSubsystem.cpp` (`SpawnVehicle`)
```cpp
#include "Vehicle/Garage/GarageSubsystem.h"
...
	// after the car is spawned and its definition applied:
	UGarageSubsystem::RandomPaint(Car); // keeps a colour already set (a saved car, a garage car)
```
Placed cars (level actors) get theirs on BeginPlay: in `AMurdarVehicle::BeginPlay`, call the same.

### 3. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Event.Garage.Resprayed",DevComment="Source = the car, Magnitude = price")
```

### 4. Cheat — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarGarage: last car, stored and impounded cars. */ UFUNCTION(Exec) void MurdarGarage();
```
```cpp
#include "Vehicle/Garage/GarageSubsystem.h"
void UDirectorCheats::MurdarGarage() { if (const UGarageSubsystem* G = UGarageSubsystem::Get(GetWorld())) { UE_LOG(LogTemp, Display, TEXT("%s"), *G->Describe()); } }
```

## Setup (with the user)
Blueprint children of `AMurdarGarage` with a mesh (a workshop, a garage door): `BP_Garage_Respray` (Kind Respray),
`BP_Garage_Home` (Storage, `GarageId` = `Home`, Slots 2), `BP_Garage_Impound` (Impound, `GarageId` = `Impound`).
Size the `Bay` box to the door, put `SpawnPoint` outside the door facing the street, and move the Counter's
`LocalPoint` to the office door.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Garage/Source/Murdar_GameDev/Vehicle/Garage Handoff/Garage/Tests/garage_rules_test.cpp -o gt && ./gt` → `30 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| GAR-01 | traffic and parked cars | varied colours |
| GAR-02 | `MurdarMoney 1000`, drive into the respray, stop | fade, new colour, repaired, −150 (+repair), line „Gata, șefu'…” |
| GAR-03 | `MurdarHeat 60`, lose the police, respray | double price; heat 11 (a stop, no chase); the same car no longer matches (`MurdarState`/log) |
| GAR-04 | respray with a patrol watching | „Cu gaborii în coadă? Pleacă de-aici!”, nothing done |
| GAR-05 | steal a car (THF), respray before the report | `MurdarTheft` shows „cleaned”, never reported |
| GAR-06 | leave a car in the home garage, walk out | car gone; `MurdarGarage` lists it; counter „Scoate …” brings it back outside |
| GAR-07 | store, `MurdarSave`, `MurdarContinue` | still stored |
| GAR-08 | get arrested next to your car (with an impound lot in the map) | car at the impound; fee 200 + 50/day |
| GAR-09 | counter with a car parked on the spawn spot | „Mută mașina din fața porții.”, no money taken |

## Design notes
- The description is model + colour, not a plate: 90s police had a radio and a witness, not a database.
- Respray also repairs, like GTA: one place, one decision.
