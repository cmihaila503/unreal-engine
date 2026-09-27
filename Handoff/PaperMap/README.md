# Harta de hârtie (Paper map) — handoff for local Claude Code

## Pe scurt (română)

Fără minimap în joc. În meniul de pauză e **„Harta”**: desenul orașului (poți scana chiar schița ta, curată), cu
semne de creion pentru locurile pe care le știi (casa, garajele, contactele, telefoanele), **locul treburii curente
încercuit cu roșu** (contactul ți l-a zis, tu l-ai notat) și o cruce unde ești. Rotița sau trigger-ele fac zoom,
tragi harta cu mouse-ul, stick-ul sau săgețile, iar Esc / B / M te întoarce la meniu. Un loc apare pe hartă doar
după ce îl știi (un fapt).

**Verified here:** `PaperMapRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 6 tests / 19 checks pass. **Not
compiled:** the Unreal files (a custom-painted Slate leaf widget).

**Depends on:** Menus (the pause menu), Jobs (the circled place; builds without a running job).

## Paste this prompt into local Claude Code

```
Read Handoff/PaperMap/README.md. Menus and Jobs must be integrated. One step at a time, building after each (editor
closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/PaperMap/Source/Murdar_GameDev/UI/PaperMap/* into Source/Murdar_GameDev/UI/PaperMap/. Build; fix the
   ADAPT on FSlateDrawElement::MakeLines if 5.8 wants FVector2f.
3. Apply README §Patches 1.
4. README §Setup needs a map texture: ask me for the image (a clean scan of my drawing) and the two landmarks.
5. Run README §In-game tests; write Docs/PAPER_MAP_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `UI/PaperMap/PaperMapRules.h` | pure: world↔paper, view clamp, zoom at cursor, paper→screen, which marks — unit-tested |
| `UI/PaperMap/PaperMapSettings.h` | Project Settings > Game > Murdar Paper Map |
| `UI/PaperMap/MapMarkerComponent.h` | put on a place: kind, pencil label, the fact that makes it known |
| `UI/PaperMap/SPaperMap.h/.cpp` | the widget (custom paint, mouse / keys / pad) |
| `UI/PaperMap/PaperMapLibrary.h/.cpp` | gathers the marks and builds the widget |

## Patches

### 1. The pause menu gets "Harta" — `UI/Menus/PauseMenuSubsystem.h/.cpp`
```cpp
// .h, private:
	TSharedPtr<class SPaperMap> MapWidget;
	void OpenMap();
	void CloseMap();
```
```cpp
#include "UI/PaperMap/PaperMapLibrary.h"
#include "UI/PaperMap/SPaperMap.h"
...
// ShowMain(), after the "Continuă" row:
	Page.Rows.Add({ LOCTEXT("Map", "Harta"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->OpenMap(); } },
		[] { return MurdarPaperMap::IsAvailable(); } });
...
void UPauseMenuSubsystem::OpenMap()
{
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	MapWidget = MurdarPaperMap::Build(GetWorld(), FSimpleDelegate::CreateLambda([Weak] { if (Weak.IsValid()) { Weak->CloseMap(); } }));
	if (!MapWidget.IsValid() || !GEngine || !GEngine->GameViewport) { return; }
	Widget->SetVisibility(EVisibility::Collapsed);
	GEngine->GameViewport->AddViewportWidgetContent(MapWidget.ToSharedRef(), 51);
	FSlateApplication::Get().SetKeyboardFocus(MapWidget);
}

void UPauseMenuSubsystem::CloseMap()
{
	if (MapWidget.IsValid() && GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(MapWidget.ToSharedRef()); }
	MapWidget.Reset();
	if (Widget.IsValid()) { Widget->SetVisibility(EVisibility::Visible); FSlateApplication::Get().SetKeyboardFocus(Widget); }
}
```
And in `Close()`: `CloseMap();` first (closing the menu from the map with the pause key).

## Setup (with the user)
- **The drawing:** scan the paper map (your sketch redrawn clean, or a drawn city in 90s tourist-map style), square,
  2048 or 4096 px, into `Content/Murdar/UI/T_PaperMap` (UI texture group, no mips needed). Set it in Project Settings
  > Murdar Paper Map.
- **Calibrate:** pick two landmarks far apart (Podu Roș, the customs). Read their world X/Y in the editor and where
  they sit on the image (U,V = pixel / size); solve `WorldAtUV0` / `WorldAtUV1` (two equations per axis). If the
  drawing's "up" isn't world −X, rotate the image, not the code.
- **Marks:** add `MapMarker` to the safehouse (Label „casa”, `KnownFact` = its owned fact), garages, contacts' places,
  payphones (no fact = always known), each with a short pencil label.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/PaperMap/Source/Murdar_GameDev/UI/PaperMap Handoff/PaperMap/Tests/paper_map_rules_test.cpp -o pm && ./pm` → `19 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| MAP-01 | no texture set | no „Harta” row |
| MAP-02 | pause → Harta | the drawing, zoomed ×2 on your cross |
| MAP-03 | wheel over a corner | zooms toward the cursor; never shows past the paper |
| MAP-04 | drag / arrows / d-pad / stick | pans |
| MAP-05 | a job running | its place circled red with the place name |
| MAP-06 | a safehouse not bought | no mark; after buying it: „casa” |
| MAP-07 | Esc / B / M | back to the menu, focus on the rows |
| MAP-08 | stand at a known landmark | the cross sits on it on the drawing (calibration right) |
