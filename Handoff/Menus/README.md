# Meniu de pauză și setări (Menus) — handoff for local Claude Code

## Pe scurt (română)

Jocul n-are încă niciun meniu. Ăsta e meniul de pauză, în Slate, în stilul HUD-ului (aceleași fonturi și culori):
**Continuă · Salvează · Încarcă salvarea · Ultimul checkpoint · Setări · Ieși din joc**. Se deschide cu Esc / P /
Start oriunde (pe jos sau la volan) și merge cu tastatura, gamepad-ul și mouse-ul.
- **Salvarea e permisă doar când nu se întâmplă nimic** (ca în GTA): nu cu poliția pe urme, nu la câteva secunde
  după focuri, nu în misiune, nu în mijlocul unei conversații, nu în mers. Motivul apare sub meniu.
- **Setări:** calitate grafică, afișare, rezoluție, VSync, limită FPS, volum general/muzică/efecte/voci,
  sensibilitate mouse, axa Y, mărimea subtitrărilor. Rezoluția și modul ferestrei revin singure în 12 s dacă nu
  confirmi (dacă monitorul nu le poate afișa, nu rămâi cu ecran negru).
- În colțul meniului: capitolul, ziua și ora, banii — singurul loc unde jocul le arată („felt, not shown” în joc).

**Verified here:** `MenuRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 7 tests / 38 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`SMurdarHudWidget` palette/fonts and its viewport ZOrder 10,
`UChapterDirector::ContinueFromSave/CaptureLoadout/GetCurrentChapter`, `UChapterDefinition::DisplayName`,
`UNarrativeStateSubsystem::Save/SaveExists/GetValue`, `UFactionMemorySubsystem::GetWantedLevel`,
`AMurdarVehicle::GetSpeedKph`, Build.cs already has Slate/SlateCore/UMG). No C++ PlayerController or GameMode exists,
so the menu polls its keys on the core ticker instead of binding input.

## Paste this prompt into local Claude Code

```
Read Handoff/Menus/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Menus/Source/Murdar_GameDev/UI/Menus/* into Source/Murdar_GameDev/UI/Menus/. Fix the ADAPT include
   paths; if UChapterDirector::CaptureLoadout is not public, tell me (the cheat MurdarSave calls it). Build.
3. Apply README §Patches 1-3.
4. Content (new assets, ask me first): README §Content — the sound mix and the four sound classes.
5. Run README §In-game tests; write Docs/MENUS_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `UI/Menus/MenuRules.h` | pure: focus, option/slider steps, save rule, resolutions, settings diff — unit-tested |
| `UI/Menus/MenuSettings.h` | Project Settings > Game > Murdar Menus (keys, sound mix/classes, save rule, slots) |
| `UI/Menus/MurdarPlayerSettings.h/.cpp` | the player's choices (GameUserSettings.ini): volumes, mouse, subtitles; applies graphics too |
| `UI/Menus/SMurdarPauseMenu.h/.cpp` | the Slate widget: pages of rows, keyboard/pad/mouse |
| `UI/Menus/PauseMenuSubsystem.h/.cpp` | open/close, pause, pages, save/load/quit, display confirm |

## Patches

### 1. Subtitle size — `UI/MurdarHudWidget.cpp` (`BuildSubtitle`)
```cpp
#include "UI/Menus/MurdarPlayerSettings.h"
...
	// ADAPT: where the subtitle STextBlock sets its font
	.Font_Lambda([] { return Font(FMath::RoundToInt(<current size> * UMurdarPlayerSettings::Get()->SubtitleScale()), <current bold>); })
```

### 2. Mouse sensitivity / invert — the GASP look input
GASP applies look in Blueprint (IA_Look → Add Controller Yaw/Pitch Input). Multiply the axis by
`Get Mouse Sensitivity` and negate Y when `Get Invert Y` (both BlueprintPure on `MurdarPlayerSettings`). This is a
Blueprint edit: **ask the user first** (existing .uasset). If look is in C++ somewhere, patch it there instead.

### 3. Keep the HUD prompt clean while paused — nothing to do
The menu is ZOrder 50 over the HUD (10) and dims the game; the HUD keeps drawing under it by design.

## Content (new assets; nothing else needs them)
- `Content/Murdar/Audio/SCM_Murdar` (Sound Class Mix), classes `SC_Master`, `SC_Music`, `SC_Effects`, `SC_Voices`.
  Set them in Project Settings > Murdar Menus. Put sounds in classes: music stems / MetaSound → SC_Music; dialogue
  and police voices → SC_Voices; the rest (weapons, cars, tension drone) → SC_Effects. Without the mix, the sliders
  are saved but change nothing (nothing is logged, so the test report must say whether the mix was set).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Menus/Source/Murdar_GameDev/UI/Menus Handoff/Menus/Tests/menu_rules_test.cpp -o mnt && ./mnt` → `38 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| MNU-01 | PIE, press P (Esc stops PIE) | game pauses, menu, cursor; header shows chapter / day+hour / money |
| MNU-02 | arrows, W/S, pad d-pad | focus moves, skipping disabled rows; Enter / A activates |
| MNU-03 | P / Esc / B on the main page | closes, game resumes, no cursor, input works on foot and in the car |
| MNU-04 | `MurdarHeat 60`, open, Save | Save dim; footer „Nu poți salva: poliția e pe urmele tale.” |
| MNU-05 | shoot, open within 20 s | „Încă se trage.” |
| MNU-06 | calm, stopped, Save; then `Încarcă salvarea` → Da | „Salvat.”; loading continues from it (and with Handoff/WorldState, where you were) |
| MNU-07 | Settings: Muzică 50 %, Aplică, quit, restart | still 50 % (GameUserSettings.ini) and audibly lower with the mix set |
| MNU-08 | change resolution, Aplică, wait | countdown „Revine singur în N s.”, reverts at 0 |
| MNU-09 | change resolution, Păstrează | kept after restart |
| MNU-10 | change something, Înapoi | „Renunți la modificări?”; Nu = stay, Da = discarded |
| MNU-11 | Ieși din joc → Nu / Da | Nu returns; Da quits (packaged) / ends PIE |
| MNU-12 | Subtitrări: Mari | subtitles bigger |

## Design notes

- A pause menu only. A title screen (New game / Continue) is the same widget with another first page, opened from
  an empty menu map — easy to add once the game has a front-end map.
- Confirm dialogs put **Nu** first so a stray Enter is harmless; Enter and Back don't auto-repeat.
- The save rule is the same idea as GTA's: saving is a quiet moment, not an escape hatch from a chase.
