# Casa sigură (Safehouse) — handoff for local Claude Code

## Pe scurt (română)

O garsonieră (sau mai multe) pe care o ai de la început sau o cumperi. Înăuntru:
- **Patul — „Dormi”:** doar când e liniște (fără poliție pe urme, fără focuri recente, fără misiune în curs).
  Noaptea dormi până la 8 dimineața, ziua tragi un pui de somn de 4 ore. Te vindeci complet și **jocul se salvează**.
- **Ascunzătoarea — „Ascunde banii” / „Ia banii ascunși”:** banii de aici nu-s la tine, deci nici amenda de la
  secție, nici nota de la spital nu-i pot lua.
- **Dulapul — „Schimbă-te”:** altă ținută (Handoff/Disguise o folosește ca să nu te recunoască martorii).
- **Te ascunzi:** înăuntru, dacă nu te-a văzut poliția de 10 s, căldura scade de 3 ori mai repede.

**Verified here:** `SafehouseRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 5 tests / 26 checks pass. **Not
compiled:** the Unreal files. Written against the real source (`UFactionMemorySubsystem::GetHeat/SetHeat/
HeatDecayPerSecond/TimeSinceLastSighting/GetWantedLevel`, `UHealthComponent::Heal/MaxHealth`, `UChapterDirector::
CaptureLoadout`, `UNarrativeStateSubsystem::Save/SetValue`) and the handoffs it builds on.

**Depends on:** Interaction, Economy, TimeOfDay (`SkipHours`). Consequences already takes only `Stat.Money`, so the
stash is safe without any change there.

## Paste this prompt into local Claude Code

```
Read Handoff/Safehouse/README.md. Interaction, Economy and TimeOfDay must be integrated. One step at a time,
building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Safehouse/Source/Murdar_GameDev/Director/Safehouse/* into Source/Murdar_GameDev/Director/Safehouse/.
   Build. (ADAPT: if HeatDecayPerSecond is not public in UFactionMemorySubsystem, add a getter.)
3. Apply README §Patches 1-2.
4. Ask me before placing a safehouse in a map: README §Setup.
5. Run README §In-game tests; write Docs/SAFEHOUSE_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Safehouse/SafehouseRules.h` | pure: sleep gate, sleep length, stash moves, hiding decay, outfits — unit-tested |
| `Director/Safehouse/Safehouse.h/.cpp` | the actor: interior box, bed / stash / wardrobe Interactables, purchase, hiding |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Stat.MoneyStash",DevComment="Lei hidden in the safehouse (saved; not on him)")
+GameplayTagList=(Tag="Stat.Outfit",DevComment="Current outfit index (saved)")
+GameplayTagList=(Tag="Event.Safehouse.Slept",DevComment="Magnitude = hours slept")
+GameplayTagList=(Tag="Event.Player.OutfitChanged",DevComment="Magnitude = outfit index")
+GameplayTagList=(Tag="Fact.Safehouse.Home",DevComment="The starting flat is his")
```
One `Fact.Safehouse.<Name>` per safehouse that can be bought.

### 2. Menu header shows the stash too (optional) — `UI/Menus/PauseMenuSubsystem.cpp` (`HeaderText`)
After the lei line: `+ „ (ascunși: N lei)”` from `Stat.MoneyStash` when it is above zero.

## Setup (with the user)
Blueprint child `BP_Safehouse_Home` of `ASafehouse`: the flat's interior (or a door + a small room), size `Interior`
to the room, move the three `LocalPoint`s onto the bed, a loose floorboard / the stove, the wardrobe. `OwnedFact` =
`Fact.Safehouse.Home`, `Price` 0, and add the fact to the first open-city chapter's `FactsOnEnter`. A second one to
buy: `Price` 5000, its own fact.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Safehouse/Source/Murdar_GameDev/Director/Safehouse Handoff/Safehouse/Tests/safehouse_rules_test.cpp -o sh && ./sh` → `26 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| SAF-01 | `MurdarTime 23`, sleep | fade, wake at 08:00, full health, subtitle „08:00”, save made |
| SAF-02 | sleep at 14:00 | 18:00 |
| SAF-03 | `MurdarHeat 20`, sleep | „Nu dormi cu poliția pe urme.” |
| SAF-04 | 1000 lei, „Ascunde banii”, get arrested | fine from 0 cash; stash still 1000 |
| SAF-05 | with empty pockets, „Ia banii ascunși” | cash back |
| SAF-06 | chase, lose them, run inside, wait | heat drops ~3× faster than outside |
| SAF-07 | a safehouse not owned, 6000 lei | bed says „Cumpără …”; buying sets the fact; bed becomes „Dormi” |
| SAF-08 | „Schimbă-te” three times | `Stat.Outfit` 1, 2, 0 |
