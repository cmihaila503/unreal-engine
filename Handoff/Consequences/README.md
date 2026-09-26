# Consecințe (Consequences: arrest, hospital, injury) — handoff for local Claude Code

## Pe scurt (română)

Azi moartea și arestarea fac același lucru: înapoi la ultimul checkpoint. Asta rămâne pentru capitolele de poveste.
În orașul deschis, jocul merge mai departe, ca în GTA:
- **Moarte → spital.** Te trezești la cel mai apropiat spital, au trecut 6 ore, nota e 10% din bani (minim 100 lei,
  niciodată mai mult decât ai). Heat-ul e șters.
- **Arestare → secție.** Cel mai apropiat post de poliție; ore la răcoare care cresc cu heat-ul din momentul arestării
  și cu fiecare arestare anterioară (`Stat.Arrests`, se salvează — „te țin minte”), o amendă, armele de foc și
  muniția confiscate. Înainte de asta, dialogul de mită la arestare (handoff Dialogue) îți dă o ultimă șansă.
- **Răni.** Viața revine singură doar până la 60%, după câteva secunde fără să fii lovit. Restul e la doctor (un
  obiect de economie care vindecă tot, cumpărat printr-un dialog).
Totul e spus printr-o singură subtitrare, fără numere noi pe HUD. O hartă fără spital/secție se poartă exact ca azi.

**Verified here:** `ConsequenceRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 8 tests / 30 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`AMurdarCharacter::HandleDied/AfterDeath`, `UChapterDirector::
HandleArrested/AfterArrest/PlacePlayerAt/GetPlayerCharacter`, `UHealthComponent::Heal/Revive/MaxHealth`,
`Event.Actor.Damaged` (Source = the hurt actor, Magnitude = damage, 0 when blocked), `Event.Police.Arrested`,
`UFactionMemorySubsystem::GetHeat/SetHeat`, `UWeaponComponent` slots map, `UNarrativeStateSubsystem::SetWeapons/
MutableAmmo`).

**Depends on:** `Handoff/Economy` and `Handoff/TimeOfDay` (this handoff adds `SkipHours` to TimeOfDay — take the
current TimeOfDay files). Optional: `Handoff/Dialogue` for the arrest bribe.

## Paste this prompt into local Claude Code

```
Read Handoff/Consequences/README.md. Economy and TimeOfDay handoffs must be integrated (TimeOfDay with SkipHours).
One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Consequences/Source/Murdar_GameDev/Director/Consequences/* into
   Source/Murdar_GameDev/Director/Consequences/. Fix the ADAPT include paths. Build.
3. Apply README §Patches 1-4. Patch 3 touches UWeaponComponent: read Holster/ReleaseWeapon first and tell me if
   RemoveAllWeapons needs to differ from the sketch.
4. Map changes need my OK: propose where to put a Hospital and a PoliceStation PlayerStart on the Freeroam/Sandbox map.
5. Run README §In-game tests; write Docs/CONSEQUENCES_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Consequences/ConsequenceRules.h` | pure: outcome, hospital bill, sentence, regen, nearest — unit-tested |
| `Director/Consequences/ConsequenceSettings.h` | Project Settings > Game > Murdar Consequences |
| `Director/Consequences/ConsequenceSubsystem.h/.cpp` | hospital / station / recovery |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Fact.Consequences.Checkpoint",DevComment="Set by a chapter that must rewind on death/arrest (story); absent = hospital/station")
+GameplayTagList=(Tag="Stat.Arrests",DevComment="How many times he was taken to the station (saved)")
+GameplayTagList=(Tag="Event.Consequence.Hospital",DevComment="Woke up in hospital; Magnitude = bill")
+GameplayTagList=(Tag="Event.Consequence.Station",DevComment="Released from the station; Magnitude = hours in the cell")
+GameplayTagList=(Tag="Event.Economy.Buy.Doctor",DevComment="Shop item: full heal (Consequences FullHealItems)")
```

### 2. `Character/MurdarCharacter.cpp` — `AfterDeath` asks first
```cpp
#include "Director/Consequences/ConsequenceSubsystem.h"
...
	// The player gets up at the last checkpoint with what the story left him there - or, in the open city, in hospital.
	SetRagdoll(false);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) { EnableInput(PC); }
	if (HealthComponent) { HealthComponent->Revive(); }
	if (UConsequenceSubsystem* Consequences = UConsequenceSubsystem::Get(this); Consequences && Consequences->HandlePlayerDeath(this))
	{
		return;
	}
	if (UChapterDirector* Director = UChapterDirector::Get(this))
	... unchanged
```

### 2b. `Director/ChapterDirector.cpp` — `AfterArrest` asks first
```cpp
#include "Director/Consequences/ConsequenceSubsystem.h"
...
void UChapterDirector::AfterArrest()
{
	bArrestInProgress = false;
	... unchanged EnableInput ...
	// In the open city the station decides; a story chapter (or a map without a station) rewinds as before.
	if (UConsequenceSubsystem* Consequences = UConsequenceSubsystem::Get(this); Consequences && Consequences->HandlePlayerArrest(GetPlayerCharacter()))
	{
		return;
	}
	GoToCheckpoint(GetCurrentCheckpoint());
}
```
Update the `HandleArrested` comment ("the same price as a death") to say the price is decided by `UConsequenceSubsystem`.

### 3. `Weapons/WeaponComponent.h/.cpp` — `RemoveAllWeapons` (confiscation)
```cpp
/** Confiscation: every carried weapon is destroyed (not dropped). Reserve ammo is the caller's business. */
UFUNCTION(BlueprintCallable, Category = "Weapons") void RemoveAllWeapons();
```
```cpp
void UWeaponComponent::RemoveAllWeapons()
{
	StopFire();
	SetAiming(false);
	// ADAPT: if Holster() plays an animation that touches EquippedWeapon later, clear that state here instead.
	EquippedWeapon = nullptr;
	for (TPair<EWeaponSlot, TObjectPtr<AMurdarWeapon>>& Slot : Weapons)
	{
		if (Slot.Value) { Slot.Value->Destroy(); }
	}
	Weapons.Reset();
	PublishEvent(MurdarTags::Event_Weapon_Holstered);
}
```

### 4. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarConsequences: mode, spawn points, prior arrests. */ UFUNCTION(Exec) void MurdarConsequences();
```
```cpp
#include "Director/Consequences/ConsequenceSubsystem.h"
void UDirectorCheats::MurdarConsequences()
{
	if (const UConsequenceSubsystem* C = UConsequenceSubsystem::Get(GetWorld())) { UE_LOG(LogTemp, Display, TEXT("%s"), *C->Describe()); }
}
```
To test: the existing kill / arrest paths (die to a dummy, or `MurdarHeat` + let a unit arrest you).

## Setup

- Map (with the user's OK): a `PlayerStart` with tag `Hospital` by the hospital door, one with `PoliceStation` by the
  station. Several of each are fine: the nearest wins.
- Story chapters that must rewind: add `Fact.Consequences.Checkpoint` to their `FactsOnEnter` (and clear it in the
  first open-city chapter's `FactsToClear`).
- Economy Items (Project Settings > Murdar Economy): `Event.Economy.Buy.Doctor` — Name „Doctor”, BasePrice 150.
  Consequences settings: `FullHealItems` = `Event.Economy.Buy.Doctor`. A doctor = an `Interactable` + a dialogue with
  the choice „Cârpește-mă (150 lei)” → Publish `Event.Economy.Buy.Doctor`.
- Arrest bribe (with Handoff/Dialogue): `DA_Dialogue_Arrest`, `StartOnEvent` `Event.Police.Arrest`, `bInterruptsOthers`,
  one node without text, choices „Dă-i 1000” [`Stat.Money` ≥ 1000, `PayPolice` 1000] and „Nimic”, ChoiceTimeout 4,
  DefaultChoice 1. `ReceiveBribe` refuses below `ArrestBribeBase + ArrestBribePerHeat × heat` and always when lethal.
  ADAPT: check how long a unit stays in the Arrest state before `Event.Police.Arrested`; the timeout must be shorter.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Consequences/Source/Murdar_GameDev/Director/Consequences Handoff/Consequences/Tests/consequence_rules_test.cpp -o ct && ./ct` → `30 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| CON-01 | map without the tagged starts: die, get arrested | checkpoint, exactly as before |
| CON-02 | with starts, 1000 lei, die | hospital start; subtitle with „Nota: 100 lei”; clock +6 h; heat 0 |
| CON-03 | `MurdarHeat 20`, get arrested | station; ~12 h; fine 200; weapons and reserve gone; `Stat.Arrests` 1 |
| CON-04 | arrested again at the same heat | hours and fine ×1.25 |
| CON-05 | arrested with 50 lei | fine 50, cash 0, never negative |
| CON-06 | `MurdarSave`, `MurdarLoad` after CON-03 | still no weapons; `Stat.Arrests` kept |
| CON-07 | chapter with `Fact.Consequences.Checkpoint` | checkpoint on death and arrest |
| CON-08 | take damage to 30 %, hide | after ~6 s health climbs to 60 % and stops |
| CON-09 | buy the doctor | full health |
| CON-10 | during a mission, die | mission fails (Missions handoff) and he wakes in hospital; `MurdarMissionRetry` still works |
| CON-11 | arrest with the bribe dialogue, pay enough | released on the spot; no station |

## Design notes

- The checkpoint stays the fallback everywhere: nothing changes until someone places the two starts.
- The player's car stays where he left it (an impound lot is a world-state feature, see Handoff/WorldState).
- Only firearms are confiscated; melee is his hands. The reserve goes too — otherwise the next gun shop sells a
  gun that is already loaded with the old ammo.
- Recovery never raises health above the cap and never touches it while he is being hit: fights stay dangerous.
