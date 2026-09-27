# Cutscene-uri (Cutscenes) — handoff for local Claude Code

## Pe scurt (română)

Scene din Sequencer ca date (`DA_Cutscene_*`): pornesc dintr-un eveniment (un trigger de capitol, o misiune reușită,
un „E” la o ușă), doar dacă faptele se potrivesc, o singură dată dacă așa scrie, și **niciodată în mijlocul unei
urmăriri**: așteaptă să se liniștească (sau renunță dacă a trecut prea mult). Una pe rând, restul la coadă. Cât
rulează: controlul e oprit, HUD-ul și prompt-urile dispar, un dialog în curs e oprit, benzi negre sus și jos. **Ții
apăsat** Space / Enter / A o secundă ca să sari scena (o apăsare scurtă nu o sare). La final: fapte, eveniment.

**Verified here:** `CutsceneRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 3 tests / 16 checks pass. **Not
compiled:** the Unreal files (Level Sequence player).

**Depends on:** Interaction, Dialogue (abort). Missions / chapters start them through events.

## Paste this prompt into local Claude Code

```
Read Handoff/Cutscenes/README.md. Interaction and Dialogue must be integrated. One step at a time, building after
each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Add "LevelSequence", "MovieScene" to Build.cs. Copy Handoff/Cutscenes/Source/Murdar_GameDev/Director/Cutscenes/*
   into Source/Murdar_GameDev/Director/Cutscenes/. Build.
3. Apply README §Patches 1-3.
4. Ask me before making a test sequence (README §Example). Run README §In-game tests; write
   Docs/CUTSCENES_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Cutscenes/CutsceneRules.h` | pure: play / wait / drop, the queue, hold-to-skip — unit-tested |
| `Director/Cutscenes/CutsceneDefinition.h` | the data asset (primary type `Cutscene`) |
| `Director/Cutscenes/CutsceneSubsystem.h/.cpp` | start by events, play, letterbox, skip, facts |

## Patches

### 1. Tags + AssetManager
```ini
+GameplayTagList=(Tag="Cutscene",DevComment="Cutscene identities; set as a fact once played")
+GameplayTagList=(Tag="Event.Cutscene.Started",DevComment="Payload = cutscene tag")
+GameplayTagList=(Tag="Event.Cutscene.Ended",DevComment="Payload = cutscene tag")
```
`DefaultGame.ini`: a `PrimaryAssetTypesToScan` entry for `Cutscene` (`/Script/Murdar_GameDev.CutsceneDefinition`,
`/Game/Murdar/Cutscenes`), like the Mission one.

### 2. HUD hidden during a scene — `Character/MurdarHUD.cpp`
`SetActorHiddenInGame` stops `DrawHUD` but not the Slate panels: in `UpdateModel` (or where the widgets are shown),
collapse `Widget` and `PlayerWidget` while `UCutsceneSubsystem::Get(this)->IsPlaying()`. Subtitles can stay (scenes
use them).

### 3. Cheat
```cpp
/** MurdarCutscene <name>: play it now (gates apply). */ UFUNCTION(Exec) void MurdarCutscene(const FString& Name);
```
```cpp
#include "Director/Cutscenes/CutsceneSubsystem.h"
void UDirectorCheats::MurdarCutscene(const FString& Name) { if (UCutsceneSubsystem* C = UCutsceneSubsystem::Get(GetWorld())) { C->Play(C->FindByName(Name)); } }
```

## Example
`LS_Test_Intro`: 10 s, a camera cut across the street, the player's car in shot (possessable or spawnable).
`DA_Cutscene_Test_Intro`: `CutsceneTag` `Cutscene.Test.Intro`, `StartOnEvent` `Event.Trigger` + a test trigger (or
`MurdarCutscene Intro`), `bOnce`.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Cutscenes/Source/Murdar_GameDev/Director/Cutscenes Handoff/Cutscenes/Tests/cutscene_rules_test.cpp -o cs && ./cs` → `16 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| CUT-01 | `MurdarCutscene Intro` | plays; bars; HUD gone; no control |
| CUT-02 | tap Space | nothing |
| CUT-03 | hold Space 1 s | skips; control back; fact set |
| CUT-04 | play it again | nothing (once) |
| CUT-05 | trigger it during `MurdarHeat 60` | waits; plays when the chase ends (within 2 min) |
| CUT-06 | during a conversation | the conversation stops, the scene plays |
