# Interacțiune (Interaction) — handoff for local Claude Code

## Pe scurt (română)

Un singur „E” pentru tot ce nu e armă sau mașină: o ușă, un telefon public, un om la chioșc, o ascunzătoare. Pui
`UInteractableComponent` pe orice actor, îi scrii verbul („Sună”, „Vorbește cu Nea Gică”) și un tag `Interact.*`.
Când îl folosești, sistemul publică `Event.Interact` pe bus — misiunile se pot termina pe el (`CompleteOnEvent` +
`CompleteOnPayload` există deja), dialogul (următorul sistem) pornește pe el, magazinele la fel. Condițiile sunt
fapte, deci ce se poate folosi urmează povestea. Alegerea țintei: te uiți la ea > e aproape > prioritatea autorului;
nu sare între două uși alăturate (stickiness) și nu merge prin pereți (un singur trace). Prompt-ul e cel existent din
HUD — nimic nou pe ecran. Ordinea „E”: armă la picioare > interactabil > mașină.

**Verified here:** `InteractionRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 5 tests / 14 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`AMurdarCharacter::Input_Interact`, `AMurdarHUD::UpdateModel`
prompt block, `UGameEventSubsystem::Publish`/`FGameEvent`, `UNarrativeStateSubsystem::Check/SetFact`,
`UMissionDefinition` objective fields).

## Paste this prompt into local Claude Code

```
Read Handoff/Interaction/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Interaction/Source/Murdar_GameDev/Director/Interaction/* into Source/Murdar_GameDev/Director/Interaction/. Build.
3. Apply README §Patches 1-4.
4. Do NOT modify existing .uasset/.umap. For the in-game tests, ask me before adding test actors to a map
   (or use a new test map / a Blueprint actor I create).
5. Run README §In-game tests; write Docs/INTERACTION_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Interaction/InteractionRules.h` | pure scoring + pick with stickiness, unit-tested |
| `Director/Interaction/InteractionSettings.h` | Project Settings > Game > Murdar Interaction |
| `Director/Interaction/InteractableComponent.h/.cpp` | the component authors put on actors |
| `Director/Interaction/InteractionSubsystem.h/.cpp` | registry, 8 Hz focus scan, `InteractFocused`, `PushBlock/PopBlock` |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Event.Interact",DevComment="The player used an interactable. Source = its actor, Payload = its Interact.* tag")
+GameplayTagList=(Tag="Interact",DevComment="Interaction identities (Interact.Phone.Kiosk, Interact.Door.Garage...)")
```
Each placed thing adds its own `Interact.<Name>` tag.

### 2. `Character/MurdarCharacter.cpp` — `Input_Interact`
Weapon first (unchanged), then the interactable, then the car:
```cpp
#include "Director/Interaction/InteractionSubsystem.h"
...
void AMurdarCharacter::Input_Interact(const FInputActionValue&)
{
	// A weapon at the feet wins over a placed interactable, which wins over a car door; each shows its own "E".
	if (WeaponComponent->GetFocusedPickup())
	{
		WeaponComponent->PickupFocused();
		return;
	}
	if (UInteractionSubsystem* Interaction = UInteractionSubsystem::Get(this))
	{
		if (Interaction->InteractFocused(this))
		{
			return;
		}
	}
	if (UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(this))
	{
		if (AMurdarVehicle* Car = Vehicles->FindEnterable(GetActorLocation()))
		{
			Car->Enter(this);
		}
	}
}
```

### 3. `Character/MurdarHUD.cpp` — `UpdateModel`, the "Interaction prompt" block
Insert the interactable between the pickup and the car (same order as patch 2):
```cpp
#include "Director/Interaction/InteractionSubsystem.h"
#include "Director/Interaction/InteractableComponent.h"
...
	// Interaction prompt: a weapon at the feet beats a placed interactable beats a car door (same order as Input_Interact).
	if (Weapons)
	{
		const UInteractionSubsystem* Interaction = UInteractionSubsystem::Get(this);
		const UInteractableComponent* Focused = Interaction ? Interaction->GetFocused() : nullptr;
		if (const AMurdarWeapon* Pickup = Weapons->GetFocusedPickup())
		{
			... unchanged ...
		}
		else if (Focused)
		{
			Model.PromptText = Focused->Verb;
		}
		else if (UVehicleSubsystem* Vehicles = UVehicleSubsystem::Get(this))
		{
			... unchanged ...
		}
	}
```

### 4. Cheat — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarInteract: list interactables, availability and the current focus. */ UFUNCTION(Exec) void MurdarInteract();
```
```cpp
#include "Director/Interaction/InteractionSubsystem.h"
void UDirectorCheats::MurdarInteract()
{
	if (const UInteractionSubsystem* I = UInteractionSubsystem::Get(this))
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *I->Describe()); // ADAPT: print like the other cheats
	}
}
```

## Authoring

- Add `Interactable` (component) to any actor. Set `Verb` (short, Romanian, imperative), `InteractionTag`.
- `LocalPoint`: drag the widget to the handle / receiver; the distance and the line-of-sight trace use it.
- A mission step „Sună-l pe Vali de la telefonul din colț”: objective `CompleteOnEvent = Event.Interact`,
  `CompleteOnPayload = Interact.Phone.Corner`; on the phone actor `RequiredFacts = Mission.X` if it should only ring
  during that mission (or leave it always usable — the mission only listens while the objective is active).
- Once ever (saved): `FactsOnUse = Fact.Stash.Taken` and `ForbiddenFacts = Fact.Stash.Taken`. `bOnce` alone is per load.
- A door that opens: Blueprint binds `OnInteracted` and plays its timeline. The component does nothing physical itself.
- Line of sight: a thing inside a closed box, or behind glass on the Visibility channel, won't be usable — move
  `LocalPoint` in front of the glass or set the glass to ignore Visibility.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Interaction/Source/Murdar_GameDev/Director/Interaction Handoff/Interaction/Tests/interaction_rules_test.cpp -o it && ./it` → `14 checks, 0 failed`.

Setup: two cubes 80 cm apart with `Interactable` (Verb „Ușa A” / „Ușa B”, tags `Interact.Test.A` / `.B`), a third
(„Telefon”, Priority 2) near a bench interactable („Bancă”), a fourth behind a wall.

| ID | Test | Pass |
|---|---|---|
| INT-01 | walk up to door A, look at it | prompt „Ușa A”; `MurdarInteract` shows it focused |
| INT-02 | turn slowly from A to B | prompt switches once, no flicker between them |
| INT-03 | stand between phone and bench, look between them | „Telefon” wins (priority) |
| INT-04 | stand next to the wall with the fourth behind it, in range | no prompt |
| INT-05 | drop a weapon next to door A | weapon prompt wins; E picks up the weapon, second E uses the door |
| INT-06 | stand by a car with nothing else around | car prompt as before; E enters |
| INT-07 | press E at door A | log/`MurdarEvents` (if present) shows `Event.Interact` with payload `Interact.Test.A` |
| INT-08 | door B with `bOnce` | second E does nothing; prompt gone |
| INT-09 | door with `RequiredFacts = Fact.Test`; `MurdarFact +Fact.Test` | prompt appears only after the fact |
| INT-10 | get in a car next to door A | no interactable prompt while driving |
| INT-11 | mission objective on `Event.Interact` + payload | objective completes on E |

## Design notes

- Why a registry and not an overlap sphere: authors place tens, not thousands; a list of weak pointers + distance
  reject at 8 Hz costs less than overlap events on every actor, and needs no collision setup on the thing.
- Why the camera facing: third person — you look at the phone with the camera, the body turns later.
- `PushBlock/PopBlock` is for dialogue (next system) and the arrest: nothing to use while talking.
- Weapons and cars keep their own focus code. Folding them in would touch working, tested code for no player-visible
  gain; the three-level order in patches 2 and 3 keeps the rule in one place per file.
