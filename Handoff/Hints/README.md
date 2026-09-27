# Indicii discrete (Hints) — handoff for local Claude Code

## Pe scurt (română)

Tutorialul fără tutorial: o frază scurtă, **o singură dată**, exact când contează („» Ține E la ușă ca să spargi
geamul.” prima dată când găsești o mașină încuiată). Nu apare niciodată în timpul unei urmăriri, al unei conversații
sau al unei scene: așteaptă puțin să se liniștească, altfel renunță. Nu apar două indicii la rând, iar faptul că
l-ai văzut se salvează. Se pot opri din Setări.

**Verified here:** `HintRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 3 tests / 12 checks pass. **Not
compiled:** the Unreal files.

**Depends on:** Menus (the on/off setting). Hint moments come from the other handoffs' events.

## Paste this prompt into local Claude Code

```
Read Handoff/Hints/README.md. Menus must be integrated. One step at a time, building after each (editor closed),
reporting in Romanian:
1. Run the unit test (README §Tests).
2. Apply README §Patches 1 (bHints setting). Copy Handoff/Hints/Source/Murdar_GameDev/Director/Hints/* into
   Source/Murdar_GameDev/Director/Hints/. Build.
3. Apply README §Patches 2. Fill the hints from README §Setup (only for systems already integrated).
4. Run README §In-game tests; write Docs/HINTS_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Patches

### 1. On/off — `UI/Menus/MurdarPlayerSettings.h` + settings page
`UPROPERTY(config) bool bHints = true;` — add to `MurdarMenu::FSettings`, `Read`/`WriteAndApply`, and a `BoolRow`
„Indicii”.

### 2. Tags + cheat
```ini
+GameplayTagList=(Tag="Hint",DevComment="Hint identities; set as a fact once shown")
```
plus one `Hint.*` per hint below.
```cpp
/** MurdarHints: state. */ UFUNCTION(Exec) void MurdarHints();
```

## Setup (Project Settings > Murdar Hints) — suggestions, one per system

| Hint | On event | Text |
|---|---|---|
| `Hint.Car.Locked` | none — VehicleTheft calls `UHintSubsystem::Get(this)->Offer(Hint.Car.Locked)` when `GetPrompt` first returns „Sparge geamul” | „Ține-te lângă ușă până pornește. Dacă pleci, o iei de la capăt.” |
| `Hint.Car.Stolen` | `Event.Vehicle.ReportedStolen` | „Mașina a fost dată în urmărire. O vopsitorie o face de nerecunoscut.” |
| `Hint.Police.Stop` | `Event.Police.Demand` | „Tu alegi: plătești, taci și iei amenda, sau fugi.” |
| `Hint.Police.Chase` | `Event.Police.PursuitStarted` | „Pierde-i din vedere. Ascunde-te, schimbă mașina sau vopsește-o.” |
| `Hint.Pager` | `Event.Job.Paged` | „Caută un telefon public și sună.” |
| `Hint.Rain` | `Event.Weather.Changed` (payload-less; RequiredFacts none) | „Pe ploaie asfaltul nu mai ține. Frânează din timp.” |
| `Hint.Night` | `Event.Time.Dusk` | „Noaptea se vede cine vine din spate: farurile.” |
| `Hint.Hospital` | `Event.Consequence.Hospital` | „Te-au cârpit. Banii ascunși acasă nu-i ia nimeni.” |
| `Hint.Radio` | `Event.Player.EnteredVehicle` | „, și . schimbă postul.” |
| `Hint.Map` | `Event.Mission.Started` | „Locurile de care auzi le găsești pe hartă, în meniu.” |

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Hints/Source/Murdar_GameDev/Director/Hints Handoff/Hints/Tests/hint_rules_test.cpp -o hi && ./hi` → `12 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| HNT-01 | get in a car the first time | „» , și . schimbă postul.” once; never again (also after a load) |
| HNT-02 | two moments 5 s apart | the second waits ≥ 25 s |
| HNT-03 | a hint's moment during a chase, chase ends quickly | shown after |
| HNT-04 | … chase lasts > 20 s | never shown (lapsed, not marked: it can come next time) |
| HNT-05 | Setări → Indicii: Nu | none |
