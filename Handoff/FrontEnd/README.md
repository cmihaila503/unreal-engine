# Ecran de titlu, încărcare, sloturi de salvare (FrontEnd) — handoff for local Claude Code

## Pe scurt (română)

- **Ecranul de titlu**, pe o hartă de meniu (o stradă liniștită la apus, în spate): **Continuă** (ultima salvare),
  **Joc nou** (întreabă dacă ai salvări; nu le șterge), **Încarcă** (lista sloturilor), **Setări** (pagina din meniul
  de pauză), **Ieșire**. Același aspect și aceleași controale ca meniul de pauză.
- **Ecran de încărcare** între hărți: negru, titlul, un sfat (niciodată același de două ori la rând) și o rotiță.
- **Sloturi de salvare:** 3 manuale + automat, fiecare cu capitolul, ziua și ora din joc. Din meniul de pauză,
  „Salvează” te lasă să alegi slotul (confirmă dacă suprascrii), iar „Încarcă” arată aceeași listă.

**Verified here:** `FrontEndRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 4 tests / 17 checks pass. **Not
compiled:** the Unreal files. Written against the real save code (`UNarrativeStateSubsystem::Save/Load` with the magic
+ version header, `.bak` fallback, `SavedAt`) and `UChapterDirector::EnterChapter/ContinueFromSave`.

**Depends on:** Menus.

## Paste this prompt into local Claude Code

```
Read Handoff/FrontEnd/README.md. Menus must be integrated. One step at a time, building after each (editor closed),
reporting in Romanian:
1. Run the unit test (README §Tests).
2. Apply README §Patches 1-2 (PeekSave, module + MoviePlayer). Build.
3. Copy Handoff/FrontEnd/Source/Murdar_GameDev/UI/FrontEnd/* into Source/Murdar_GameDev/UI/FrontEnd/. Build.
4. Apply README §Patches 3-4 (Menus). Build.
5. Ask me before creating the front-end map (README §Setup). Run README §In-game tests (packaged build for the
   loading screen); write Docs/FRONTEND_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `UI/FrontEnd/FrontEndRules.h` | pure: continue slot, clock line, slot labels, overwrite confirm, tips — unit-tested |
| `UI/FrontEnd/FrontEndSettings.h` | Project Settings > Game > Murdar Front End |
| `UI/FrontEnd/SaveSlots.h/.cpp` | slot list read from the files without loading them |
| `UI/FrontEnd/FrontEndSubsystem.h/.cpp` | the title screen |
| `UI/FrontEnd/MurdarLoadingScreen.h/.cpp` | the loading screen (MoviePlayer) |

## Patches

### 1. `Director/NarrativeStateSubsystem.h/.cpp` — read a save without applying it
```cpp
	/** The state in a slot's file, without touching the current one (menus list chapter / day / hour). */
	static bool PeekSave(FName Slot, FNarrativeState& Out);
```
```cpp
bool UNarrativeStateSubsystem::PeekSave(FName Slot, FNarrativeState& Out)
{
	TArray<uint8> Bytes;
	const FString Path = SlotToPath(Slot);
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) && !FFileHelper::LoadFileToArray(Bytes, *(Path + TEXT(".bak")))) { return false; }
	FMemoryReader Reader(Bytes, true);
	uint32 Magic = 0; int32 Version = 0;
	Reader << Magic << Version;
	if (Magic != SaveMagic || Version > FNarrativeState().Version) { return false; }
	FObjectAndNameAsStringProxyArchive Ar(Reader, /*bLoadIfFindFails*/ true);
	FNarrativeState::StaticStruct()->SerializeItem(Ar, &Out, nullptr);
	return !Reader.IsError();
}
```
(Same steps as `Load`, minus assigning `State` and publishing: keep them in step if `Load` changes.)

### 2. The module installs the loading screen — `Murdar_GameDev.cpp`, `Murdar_GameDev.Build.cs`
```cpp
#include "Murdar_GameDev.h"
#include "Modules/ModuleManager.h"
#include "UI/FrontEnd/MurdarLoadingScreen.h"

class FMurdarGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override { MurdarLoadingScreen::Install(); }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMurdarGameModule, Murdar_GameDev, "Murdar_GameDev");
```
Build.cs: add `"MoviePlayer"` to `PublicDependencyModuleNames`.

### 3. Settings from the title — `UI/Menus/PauseMenuSubsystem.h/.cpp`
```cpp
	/** The settings page over the title screen (no pause; back returns to the title). */
	void OpenSettingsFromFrontEnd();
private:
	bool bFromFrontEnd = false;
```
```cpp
#include "UI/FrontEnd/FrontEndSubsystem.h"
void UPauseMenuSubsystem::OpenSettingsFromFrontEnd()
{
	bFromFrontEnd = true;
	Open();                 // ADAPT: Open() pauses; on the title map that's harmless, or skip SetPause when bFromFrontEnd
	ShowSettings();
}
// In BackFromSettings(), where it would ShowMain():
	if (bFromFrontEnd) { bFromFrontEnd = false; Close(); if (UFrontEndSubsystem* F = UFrontEndSubsystem::Get(this)) { F->ShowMain(); } return; }
```

### 4. Pause menu: save and load by slot — `UI/Menus/PauseMenuSubsystem.cpp`
- „Salvează” opens a page from `MurdarSlots::Gather(false)`: each row `E.Label()`; picking an existing slot asks
  „Suprascrii?” (`MurdarFrontEnd::NeedsOverwriteConfirm`), then `CaptureLoadout` + `Save(E.Slot)`.
- „Încarcă salvarea” / „Ultimul checkpoint” become one „Încarcă” row opening `MurdarSlots::Gather(true)` (rows
  disabled when empty), confirm, `ContinueFromSave(E.Slot)`.
- `UMenuSettings::ManualSlot` is no longer used by the menu (the Safehouse bed still saves to `manual`: point it at
  `slot1`, or add `manual` to FrontEnd's `Slots`).

## Setup (with the user)
- `L_FrontEnd`: a short street piece at dusk (reuse the Sandbox), a camera actor, no player pawn needed (a spectator /
  the default pawn hidden). Project Settings > Maps & Modes: **Game Default Map** = `L_FrontEnd`.
- Project Settings > Murdar Front End: `FrontEndMap` = `L_FrontEnd`, `NewGameChapter` = the first chapter.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/FrontEnd/Source/Murdar_GameDev/UI/FrontEnd Handoff/FrontEnd/Tests/front_end_rules_test.cpp -o fe && ./fe` → `17 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| FE-01 | start with no saves | Continuă / Încarcă disabled; Joc nou starts chapter 1 without asking |
| FE-02 | save in slot 2, quit, start | Continuă loads slot 2; Încarcă shows „Slot 2 — <capitol> · Ziua N, HH:MM” |
| FE-03 | Joc nou with saves | asks; saves still listed afterwards |
| FE-04 | Setări from the title, Înapoi | back on the title |
| FE-05 | packaged: any map change | loading screen with a tip; tips vary |
| FE-06 | pause → Salvează → slot 1 (existing) | „Suprascrii?” |
| FE-07 | a corrupt slot file | listed as empty; the `.bak` is used if present |
