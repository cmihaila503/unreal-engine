# Radioul din mașină (Radio) — handoff for local Claude Code

## Pe scurt (română)

Posturi de radio care **emit tot timpul**: când urci în mașină sau schimbi postul, intri la mijlocul piesei, ca în
GTA. Fiecare post are piesele lui, amestecate în altă ordine, un DJ între piese, reclame și **știri din oră în oră**
— iar știrile vorbesc despre **ce ai făcut tu**: urmărirea de aseară, arestarea, mașina furată de pe Păcurari. Radioul
merge doar în mașină, se aude „prin difuzorul unei Dacii” (fără înalte), coboară sub muzica de urmărire și sub
dialog. Postul ales se ține minte. **Modul streamer** (în setări) scoate piesele licențiate, restul rămâne.

Conținutul nu e aici: piesele originale „în stil '90”, DJ-ii inventați, reclamele și știrile se înregistrează (vezi
ce am vorbit despre drepturile muzicii).

**Verified here:** `RadioRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 7 tests / 25 checks pass. **Not
compiled:** the Unreal files.

**Depends on:** Menus (streamer mode setting), Music (ducking). Dialogue optional.

## Paste this prompt into local Claude Code

```
Read Handoff/Radio/README.md. Menus and Music must be integrated. One step at a time, building after each (editor
closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Apply README §Patches 1 (streamer mode) first, then copy Handoff/Radio/Source/Murdar_GameDev/Director/Radio/*
   into Source/Murdar_GameDev/Director/Radio/. Build.
3. Apply README §Patches 2-3. Check the car's input bindings: if , and . or the d-pad left/right are used while
   driving, tell me and propose other keys.
4. With placeholder sounds (any 3 loops + 1 voice clip), set up one station (README §Setup) and run README §In-game
   tests; write Docs/RADIO_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Radio/RadioRules.h` | pure: program building (seeded, links, streamer filter), position by clock, dial, news desk — unit-tested |
| `Director/Radio/RadioSettings.h` | Project Settings > Game > Murdar Radio (stations, news triggers, keys, sound) |
| `Director/Radio/RadioSubsystem.h/.cpp` | playback in the car, tuning, news, ducking |

## Patches

### 1. Streamer mode — `UI/Menus/MurdarPlayerSettings.h` + the settings page
```cpp
	UPROPERTY(config) bool bStreamerMode = false;
```
Add it to `MurdarMenu::FSettings` (`bool bStreamerMode`), to `Read()` / `WriteAndApply()`, and a `BoolRow`
„Mod streamer (fără muzică licențiată)” in `ShowSettings()`. After applying, call
`if (URadioSubsystem* R = URadioSubsystem::Get(World)) { R->Rebuild(); }`. (MenuRules' `operator==` must compare it.)

### 2. Tags
```ini
+GameplayTagList=(Tag="Stat.RadioStation",DevComment="Dial position (-1 off), saved")
+GameplayTagList=(Tag="News",DevComment="Radio stories about the player")
+GameplayTagList=(Tag="News.Chase",DevComment="")
+GameplayTagList=(Tag="News.Arrest",DevComment="")
+GameplayTagList=(Tag="News.CarTheft",DevComment="")
+GameplayTagList=(Tag="News.Shooting",DevComment="")
```

### 3. Cheat
```cpp
/** MurdarRadio [dir]: show; +1/-1 tunes. */ UFUNCTION(Exec) void MurdarRadio(int32 Dir = 0);
```
```cpp
#include "Director/Radio/RadioSubsystem.h"
void UDirectorCheats::MurdarRadio(int32 Dir) { if (URadioSubsystem* R = URadioSubsystem::Get(GetWorld())) { if (Dir) { R->Tune(Dir); } UE_LOG(LogTemp, Display, TEXT("%s"), *R->Describe()); } }
```

## Setup (Project Settings > Murdar Radio)
- Stations (start with two): „Radio Bahlui FM” (pop / dance '90), „Radio Manea Show” (manele), later a talk station
  (only DJ links = call-in show).
- News triggers: `Event.Police.PursuitEnded` → `News.Chase` (2), `Event.Consequence.Station` → `News.Arrest` (3),
  `Event.Vehicle.ReportedStolen` → `News.CarTheft` (1), `Event.Police.Crime` → `News.Shooting` (2, only once the heat is
  lethal — or leave it out).
- Each station's `Stories`: 1–3 clips per story in its own voice („Aseară, pe Copou, un șofer a scăpat de poliție…”).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Radio/Source/Murdar_GameDev/Director/Radio Handoff/Radio/Tests/radio_rules_test.cpp -o rt && ./rt` → `25 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| RAD-01 | get in a car | the saved station plays, mid-track |
| RAD-02 | `.` / `,` | static, station name 1.5 s, next / previous, then off |
| RAD-03 | get out | silence; get back in 30 s later: the song has moved on 30 s |
| RAD-04 | a chase starts | radio drops under the score |
| RAD-05 | escape a chase, keep driving to the next news slot | the station's chase story |
| RAD-06 | streamer mode on | licensed tracks never come; others do |
| RAD-07 | talk (dialogue) in the car | radio lower while talking |
