# Misiuni (Missions) — handoff for local Claude Code

## Pe scurt (română)

Stratul care lipsea între sandbox și joc: **o misiune = obiective în ordine + condiții de eșec + rezultat**, scrise
ca date în editor (`DA_Mission_*`), fără cod. Folosește ce ai deja: fapte, zone (care sunt fapte), evenimente de pe
bus, checkpoint-uri, valori. O misiune pornește dintr-un eveniment (de obicei un trigger de capitol), fiecare obiectiv
apare o dată ca subtitrare, iar eșecul vine din moarte, arestare, un fapt („mașina distrusă”) sau timp. La succes:
fapte, recompense (de ex. bani — `Stat.Money`, sistemul de economie), checkpoint cu autosave. La eșec: reîncerci de la
checkpoint-ul misiunii. O misiune în curs nu se salvează (ca o urmărire); una terminată e un fapt, deci se salvează.

**Verified here:** `MissionRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 8 tests / 27 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`UChapterDirector::ReachCheckpoint/GoToCheckpoint`,
`UNarrativeStateSubsystem::Check/SetFact/AddValue/HasFact`, `AMurdarHUD::ShowSubtitle`, `MurdarTags::Event`,
`Event_Actor_Died`, `Event_Police_Arrested`, `Event_State_Loaded`, zone facts from `UZoneTriggerSubsystem`).

## Paste this prompt into local Claude Code

```
Read Handoff/Missions/README.md. One step at a time, building after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Missions/Source/Murdar_GameDev/Director/Missions/* into Source/Murdar_GameDev/Director/Missions/. Build.
3. Apply README §Patches 1-3.
4. Create the example mission from README §Example as a new asset (new assets are allowed; do not modify existing
   .uasset except adding a tagged actor to a test map if I agree).
5. Run README §In-game tests; write Docs/MISSIONS_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Missions/MissionRules.h` | pure runtime, unit-tested |
| `Director/Missions/MissionDefinition.h` | the data asset (`FMissionObjective`, `UMissionDefinition`, primary type `Mission`) |
| `Director/Missions/MissionSubsystem.h/.cpp` | runs missions, applies effects, 4 Hz |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Mission",DevComment="Mission identities; a mission's tag is set as a fact when it succeeds")
+GameplayTagList=(Tag="Event.Mission.Started",DevComment="Payload = mission tag")
+GameplayTagList=(Tag="Event.Mission.Objective",DevComment="An objective started; Magnitude = index; Payload = mission tag")
+GameplayTagList=(Tag="Event.Mission.Succeeded",DevComment="Payload = mission tag")
+GameplayTagList=(Tag="Event.Mission.Failed",DevComment="Payload = mission tag")
```
Each mission adds its own `Mission.<Name>` tag.

### 2. AssetManager — `Config/DefaultGame.ini`, next to the Chapter / Weapon / Vehicle scans
```ini
+PrimaryAssetTypesToScan=(PrimaryAssetType="Mission",AssetBaseClass=/Script/Murdar_GameDev.MissionDefinition,bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/Murdar/Missions")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
```
(ADAPT: copy the exact form of the existing Chapter line.)

### 3. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarMission [name]: list missions, or start one. */ UFUNCTION(Exec) void MurdarMission(const FString& Name);
/** MurdarMissionRetry: retry the last failed mission. */ UFUNCTION(Exec) void MurdarMissionRetry();
/** MurdarMissionAbort: fail the active mission. */      UFUNCTION(Exec) void MurdarMissionAbort();
```
```cpp
#include "Director/Missions/MissionSubsystem.h"
void UDirectorCheats::MurdarMission(const FString& Name)
{
	UMissionSubsystem* M = UMissionSubsystem::Get(this);
	if (!M) { return; }
	if (!Name.IsEmpty()) { M->StartMission(M->FindByName(Name)); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *M->Describe()); // ADAPT: print like the others
}
void UDirectorCheats::MurdarMissionRetry() { if (UMissionSubsystem* M = UMissionSubsystem::Get(this)) { M->RetryLastFailed(); } }
void UDirectorCheats::MurdarMissionAbort() { if (UMissionSubsystem* M = UMissionSubsystem::Get(this)) { M->AbortActive(TEXT("cheat")); } }
```

## Example (authoring) — `Content/Murdar/Missions/DA_Mission_Sandbox_Delivery`

„Du mașina la garaj fără să te prindă.” On the Freeroam / Sandbox map:
- Place an actor (any, e.g. a Target Point) at the "garage", actor tag `MissionGarage`.
- `MissionTag` = `Mission.Sandbox.Delivery`; `StartOnEvent` = `Event.Trigger`, `StartOnPayload` = a trigger the chapter
  already fires (or start it with `MurdarMission Delivery`).
- Objectives:
  1. `get_in` — Text „Urcă în Dacia.” — `CompleteOnEvent` = `Event.Player.EnteredVehicle`.
  2. `deliver` — Text „Du-o la garajul din spatele blocului. Fără poliție pe urme.” — `ReachActorTag` = `MissionGarage`,
     radius 600 cm, `TimeLimitSeconds` 240.
- `FailIfFacts`: none; `bFailOnArrest` on. Optional `FailOnEvents` = `Event.Police.PursuitStarted` (the "no heat" rule).
- Outcome: `ValueRewards` `Stat.Money` = 500 (once the economy exists), `SuccessText` „Bun. Ia-ți banii.”,
  `FailText` „Ai stricat tot.”.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Missions/Source/Murdar_GameDev/Director/Missions Handoff/Missions/Tests/mission_rules_test.cpp -o ms && ./ms` → `27 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| MIS-01 | `MurdarMission` | lists the example as `[available]` |
| MIS-02 | `MurdarMission Delivery`, get in the car | subtitle of objective 2 appears; `Event.Mission.Objective` in the log |
| MIS-03 | drive to the tagged actor | success subtitle; fact `Mission.Sandbox.Delivery` set (`MurdarState`); money +500 if economy is in |
| MIS-04 | start again | not `[available]` (done, not repeatable) |
| MIS-05 | start, then `MurdarHeat 50` (pursuit) with the optional fail event | fails; `MurdarMissionRetry` puts you at `RetryCheckpoint` and restarts |
| MIS-06 | start, die | fails (player death) — an NPC's death does not |
| MIS-07 | start, wait past 240 s on objective 2 | fails on objective time |
| MIS-08 | start, `MurdarSave`, `MurdarLoad` | mission no longer running; done missions stay done |
| MIS-09 | objective with a `ReachActorTag` missing in the level | error in the log naming the mission and tag; the objective never completes |

## Design notes

- One mission at a time (a second start is refused) — like GTA; parallel side jobs would need a list, not needed yet.
- Objective text is a subtitle, the only text the game already shows; no markers, no minimap ("felt, not shown").
  Guidance is the author's job: a phone call, a person waiting, a light over a door.
- `ValueRewards` cannot write honor — only `FHonorToken` (chapter director) can, and that stays so.
