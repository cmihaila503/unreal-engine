# Deghizarea (Disguise) — handoff for local Claude Code

## Pe scurt (română)

Pe jos, poliția te descrie după **haine** și ce ai **pe cap** (șapcă, pălărie, ochelari). Dacă schimbi una dintre
ele **fără să te vadă**, de departe nu te mai recunosc; doar de aproape, ziua, te pot recunoaște după față (noaptea
doar foarte de aproape). O urmărire pierdută așa se răcește la nivelul „te oprește”, ca revopsirea la mașină. Hainele
le schimbi la dulapul din casa sigură; ce ai pe cap îl cumperi (economie) și îl pui / scoți cu o tastă.

**Verified here:** `DisguiseRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 3 tests / 14 checks pass. **Not
compiled:** the Unreal files.

**Depends on:** Safehouse (outfits), Economy (hats). Patch to FactionMemory (the recognition gate).

## Paste this prompt into local Claude Code

```
Read Handoff/Disguise/README.md. Safehouse and Economy must be integrated. One step at a time, building after each
(editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Disguise/Source/Murdar_GameDev/Director/Disguise/* into Source/Murdar_GameDev/Director/Disguise/. Build.
3. Apply README §Patches 1-3. For the look: tell me how the GASP character's mesh is set up (one mesh or modular
   parts) before filling Outfits; ask before editing the character Blueprint (a "head" socket for the hat).
4. Run README §In-game tests; write Docs/DISGUISE_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Disguise/DisguiseRules.h` | pure: recognition by look / distance / night, description, unseen change, heat, headwear cycle — unit-tested |
| `Director/Disguise/DisguiseSubsystem.h/.cpp` | settings (outfits, headwear, keys), the recognition gate, applying the look |

## Patches

### 1. The recognition gate — `AI/FactionMemorySubsystem.cpp` (`ReportSighting`)
At the top, after the null checks (on foot only — cars are described by model and colour, Handoff/Garage):
```cpp
#include "Director/Disguise/DisguiseSubsystem.h"
...
	UDisguiseSubsystem* Disguise = UDisguiseSubsystem::Get(GetWorld());
	if (Disguise && !Cast<AMurdarVehicle>(PlayerPawn) && WitnessFaction != ENPCFaction::Hostile && !Disguise->IsRecognisable(Witness, PlayerPawn))
	{
		return; // a stranger in other clothes: not him, as far as this witness can tell
	}
```
and after a sighting was accepted and the player is on foot: `if (Disguise && !Obs.Vehicle) { Disguise->NoteSighting(); }`.

### 2. Tags
```ini
+GameplayTagList=(Tag="Stat.Headwear",DevComment="Worn headwear index, 0 = none (saved)")
+GameplayTagList=(Tag="Stat.HeadwearOwned",DevComment="Bitmask of owned headwear (saved)")
+GameplayTagList=(Tag="Event.Economy.Buy.Cap",DevComment="Shop item: a cap")
+GameplayTagList=(Tag="Event.Economy.Buy.Hat",DevComment="Shop item: a hat")
+GameplayTagList=(Tag="Event.Economy.Buy.Glasses",DevComment="Shop item: sunglasses")
```
(`Stat.Outfit` and `Event.Player.OutfitChanged` come from Handoff/Safehouse.) Economy items with those keys, 40–120 lei.

### 3. Cheat
```cpp
/** MurdarDisguise: what he wears and what the police described. */ UFUNCTION(Exec) void MurdarDisguise();
```
```cpp
#include "Director/Disguise/DisguiseSubsystem.h"
void UDirectorCheats::MurdarDisguise() { if (const UDisguiseSubsystem* D = UDisguiseSubsystem::Get(GetWorld())) { UE_LOG(LogTemp, Display, TEXT("%s"), *D->Describe()); } }
```

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Disguise/Source/Murdar_GameDev/Director/Disguise Handoff/Disguise/Tests/disguise_rules_test.cpp -o dg && ./dg` → `14 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| DSG-01 | on foot, `MurdarHeat 20`, let a cop see you | `MurdarDisguise`: described (matches) |
| DSG-02 | out of sight, buy a cap, put it on (H) | DIFFERENT; heat unchanged (20 < pursuit) |
| DSG-03 | walk past a cop at 20 m | no stop; at 5 m by day he may |
| DSG-04 | `MurdarHeat 60` on foot, break line of sight, change clothes at the safehouse | heat 11 |
| DSG-05 | change while a cop watches | no effect (he saw you change) |
| DSG-06 | H cycles | only headwear you own, then none |
