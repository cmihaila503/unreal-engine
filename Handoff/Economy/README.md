# Economie (Economy) — handoff for local Claude Code

## Pe scurt (română)

Portofelul: banii sunt valoarea `Stat.Money` din starea narativă, deci se salvează singuri. Sistemul e singurul loc
care îi mută după reguli: cumpărături „totul sau nimic”, **mita se scade doar când polițistul o acceptă**
(`Event.Police.Bribed`, deci un refuz la arestare nu te costă), pierderi pentru arestare/spital (sistemul următor),
bani lăsați de morți (un portofel pe jos, opțional, pe clase). Magazinele sunt dialoguri: o alegere publică
`Event.Economy.Buy.<Obiect>`, economia verifică prețul, ia banii și dă muniția / arma / faptul. Prețurile sunt rotunde
ca prețurile reale și pot crește pe zi de joc (inflația anilor '90, oprită implicit). Pe HUD nu apare nimic: suma e
în meniul de pauză și în cheat-ul `MurdarMoney`.

**Verified here:** `EconomyRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 7 tests / 34 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`UNarrativeStateSubsystem::SetValue/GetValue/AddValue`,
`UFactionMemorySubsystem::RecordBribe` → `Event.Police.Bribed` with Magnitude = amount, `AMurdarPoliceAIController::
ReceiveBribe` refusing at an arrest, `UWeaponComponent::AddAmmo/GetAmmo/GetAmmoCapacity/GiveWeapon`, `AAmmoPickup`'s
overlap setup, `HealthComponent` publishing `Event.Actor.Died` with Source = the dead actor).

**Works with:** `Handoff/Dialogue` (shops, bribe choices), `Handoff/TimeOfDay` (`Stat.Day` for inflation),
`Handoff/Missions` (`ValueRewards` `Stat.Money`). None is required to build.

## Paste this prompt into local Claude Code

```
Read Handoff/Economy/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Economy/Source/Murdar_GameDev/Director/Economy/* into Source/Murdar_GameDev/Director/Economy/.
   Fix the ADAPT include paths (WeaponDefinition.h, WeaponComponent.h). Build.
3. Apply README §Patches 1-2.
4. In the editor (config only, no .uasset changes): Project Settings > Game > Murdar Economy — fill Items as in
   README §Setup; tell me which pawn classes exist for pedestrians/gangs and ask before filling CashDropClasses.
5. Run README §In-game tests; write Docs/ECONOMY_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Economy/EconomyRules.h` | pure: nice prices, inflation, pay/take/loss, pocket cash, "1.250 lei" — unit-tested |
| `Director/Economy/EconomySettings.h` | Project Settings > Game > Murdar Economy |
| `Director/Economy/EconomySubsystem.h/.cpp` | the wallet: Earn/Spend/Take/LoseFraction/Buy, bribe and death listeners |
| `Director/Economy/MoneyPickup.h/.cpp` | cash on the ground (dropped wallet, placed stash with a once-ever fact) |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Stat.Money",DevComment="Cash on the player, lei (saved). Moved by UEconomySubsystem")
+GameplayTagList=(Tag="Fact.Economy.Started",DevComment="Starting cash was given (new game)")
+GameplayTagList=(Tag="Event.Economy.Changed",DevComment="Cash changed; Magnitude = delta")
+GameplayTagList=(Tag="Event.Economy.Bought",DevComment="Payload = item (Event.Economy.Buy.*), Magnitude = price")
+GameplayTagList=(Tag="Event.Economy.CantAfford",DevComment="Payload = item, Magnitude = price")
+GameplayTagList=(Tag="Event.Economy.Buy",DevComment="Publish Event.Economy.Buy.<Item> to buy it (dialogue Publish effect)")
+GameplayTagList=(Tag="Event.Economy.Buy.Ammo.Pistol",DevComment="Shop item")
+GameplayTagList=(Tag="Event.Economy.Buy.Ammo.Shotgun",DevComment="Shop item")
+GameplayTagList=(Tag="Event.Economy.Buy.Cigarettes",DevComment="Shop item (a fact, flavour)")
```
If `Stat.Money` already exists (Missions / Dialogue handoffs used it), keep one line.

### 2. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarMoney [delta]: show cash and prices, or add/remove lei. */ UFUNCTION(Exec) void MurdarMoney(float Delta = 0.f);
/** MurdarBuy <item>: buy e.g. Ammo.Pistol (as a shop would). */    UFUNCTION(Exec) void MurdarBuy(const FString& Item);
```
```cpp
#include "Director/Economy/EconomySubsystem.h"
void UDirectorCheats::MurdarMoney(float Delta)
{
	UEconomySubsystem* E = UEconomySubsystem::Get(GetWorld());
	if (!E) { return; }
	if (Delta > 0.f) { E->Earn(Delta, TEXT("cheat")); }
	else if (Delta < 0.f) { E->Take(-Delta, TEXT("cheat")); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *E->Describe()); // ADAPT: print like the others
}
void UDirectorCheats::MurdarBuy(const FString& Item)
{
	if (UEconomySubsystem* E = UEconomySubsystem::Get(GetWorld()))
	{
		E->Buy(FGameplayTag::RequestGameplayTag(FName(*(TEXT("Event.Economy.Buy.") + Item)), false));
	}
}
```
Also update the `MurdarBribe` help line: it now costs money (it goes through `RecordBribe` → `Event.Police.Bribed`).

## Setup (Project Settings > Game > Murdar Economy)

- `StartingCash` 300 (a few bribes' worth: the first stop is a choice, not a wall).
- Items:
  | Key | Name | BasePrice | Gives |
  |---|---|---|---|
  | `Event.Economy.Buy.Ammo.Pistol` | Cartușe pistol | 60 | ammo Pistol ×12 |
  | `Event.Economy.Buy.Ammo.Shotgun` | Cartușe pușcă | 90 | ammo Shotgun ×8 |
  | `Event.Economy.Buy.Cigarettes` | Kent | 20 | fact `Fact.Has.Cigarettes` |
- `DailyInflation` 0 for now. Turn it on (0.005) only once shop dialogue choices are written without numbers.
- `CashDropClasses`: ask the user (pedestrian / gang Blueprint classes). Police out by default.

Shop dialogue (with `Handoff/Dialogue`): a choice „Cartușe (60 lei)” with effect `Publish` `Event.Economy.Buy.Ammo.Pistol`.
No money condition needed: if he can't pay, the economy says „N-ai destui bani.” and nothing changes. In
`DA_Dialogue_Gica`, the cigarettes choice can become `Publish Event.Economy.Buy.Cigarettes` instead of `AddValue −20`.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Economy/Source/Murdar_GameDev/Director/Economy Handoff/Economy/Tests/economy_rules_test.cpp -o et && ./et` → `34 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| ECO-01 | new game, `MurdarMoney` | `300 lei`, the three items with prices |
| ECO-02 | `MurdarSave`, `MurdarMoney 1000`, `MurdarLoad` | back to 300 |
| ECO-03 | `MurdarBuy Ammo.Pistol` with a pistol and room | −60; reserve +12; `Event.Economy.Bought` |
| ECO-04 | same with full reserve | „Nu mai ai unde să le pui.”; no money taken |
| ECO-05 | `MurdarMoney -290`, `MurdarBuy Ammo.Pistol` | „N-ai destui bani.”; still 10 lei |
| ECO-06 | traffic stop, pay 200 via the dialogue | −200 only after „Bine, bine...” |
| ECO-07 | arrest, offer less than the price (dialogue / `MurdarBribe 50`) | „Cu atât vrei să scapi?”; money unchanged |
| ECO-08 | `MurdarBribe 5000` with 300 | cash 0 (never negative); warning in the log |
| ECO-09 | with CashDropClasses set, kill a pedestrian | a small box by the body sometimes; walking on it adds 20–150 |
| ECO-10 | drive over a dropped wallet | nothing (on foot only) |
| ECO-11 | placed `AMoneyPickup` with `TakenFact`, take it, save, load | gone after the load |
| ECO-12 | mission with `ValueRewards` `Stat.Money` 500 | +500 |

## Design notes

- One number, no bank and no stash yet: a stash is a `AMoneyPickup` with a `TakenFact`; a real safehouse stash
  (money you keep when arrested) comes with the consequences system if the design wants it.
- Money moves in exactly three ways: this subsystem, mission rewards (`AddValue`) and dialogue `AddValue`. The last
  two don't publish `Event.Economy.Changed`; if the music or the menu ever need every change, route them here.
- No HUD counter, no "+500" pop-up: the man who pays you says it; the menu shows the total.
