# Dialog (Dialogue) — handoff for local Claude Code

## Pe scurt (română)

Conversații ca date (`DA_Dialogue_*`), fără cod: replici cu nume + voce, alegeri, efecte. Un om spune altceva în
funcție de fapte („îi datorezi bani” → altă replică) — câștigă intrarea cea mai specifică. Alegerile apar în linia de
prompt existentă din HUD („1  Dă-i 200     2  Taci”) și se aleg cu 1–4 / d-pad; cele pe care nu ți le permiți
(bani) nu apar deloc. O alegere lăsată în pace expiră: **tăcerea e un răspuns**.

Cel mai important câștig: **mita la poliție devine joc**. Azi codul poliției cere bani (`Police.Line.Demand`), dar
jucătorul nu are cum să plătească decât cu cheat-ul `MurdarBribe`. Cu `DA_Dialogue_TrafficStop` primești alegerile
la oprirea în trafic; plata merge prin `AMurdarPoliceAIController::ReceiveBribe` (care poate refuza la arestare, exact
cum e scris), iar tăcerea lasă poliția să-ți dea amenda ca acum. Banii se scad doar când mita e acceptată
(`Event.Police.Bribed` → sistemul de economie, handoff-ul următor).

Plus: vocile pentru replicile poliției (și alte „barks”) — o listă de variante per tag, niciodată aceeași de două ori
la rând, cu cooldown, redate din mașina care vorbește. Subtitrarea lor o face deja HUD-ul.

**Verified here:** `DialogueRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 13 tests / 47 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`AMurdarHUD::ShowSubtitle` + its reading-speed formula,
`AMurdarHUD::UpdateModel` prompt, `AMurdarPoliceAIController::Say/ReceiveBribe/PublishPolice` (Source = the unit's pawn),
`TrafficStopDemandDelaySeconds` 1.5 / `TrafficStopTalkSeconds` 10, `UNarrativeStateSubsystem`, `UInteractionSubsystem`
from `Handoff/Interaction`).

**Depends on:** `Handoff/Interaction` (for `PushBlock` and `Event.Interact`). Integrate that first.

## Paste this prompt into local Claude Code

```
Read Handoff/Dialogue/README.md. Handoff/Interaction must already be integrated. One step at a time, building after
each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Dialogue/Source/Murdar_GameDev/Director/Dialogue/* into Source/Murdar_GameDev/Director/Dialogue/.
   Fix the ADAPT include path of AMurdarPoliceAIController. Build.
3. Apply README §Patches 1-5. Check the IMC bindings: if 1-4 or the d-pad are bound to something that matters while
   driving, tell me and propose other keys for UDialogueSettings::ChoiceKeys.
4. Create the example assets from README §Examples (new assets only; do not modify existing .uasset/.umap).
5. Run README §In-game tests; write Docs/DIALOGUE_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Dialogue/DialogueRules.h` | pure: conditions, entry pick, runtime (lines/choices/timeout/effects), bark picker — unit-tested |
| `Director/Dialogue/DialogueDefinition.h` | the data asset (`UDialogueDefinition`, primary type `Dialogue`) |
| `Director/Dialogue/DialogueSettings.h` | Project Settings > Game > Murdar Dialogue: choice keys, bark voices, attenuation |
| `Director/Dialogue/DialogueSubsystem.h/.cpp` | runs it: subtitles, voices, choices, effects, barks |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Dialogue",DevComment="Conversation identities (Dialogue.TrafficStop, Dialogue.Gica...)")
+GameplayTagList=(Tag="Event.Dialogue.Started",DevComment="Payload = dialogue tag, Source = speaker")
+GameplayTagList=(Tag="Event.Dialogue.Ended",DevComment="Payload = dialogue tag, Source = speaker")
+GameplayTagList=(Tag="Event.Dialogue.Bark",DevComment="Voice a one-liner at Source. Payload = Bark.* tag with a voice in the settings")
+GameplayTagList=(Tag="Bark",DevComment="One-liners for Event.Dialogue.Bark (Bark.Civilian.Curse...)")
```

### 2. AssetManager — `Config/DefaultGame.ini`, next to the Mission / Chapter scans
```ini
+PrimaryAssetTypesToScan=(PrimaryAssetType="Dialogue",AssetBaseClass=/Script/Murdar_GameDev.DialogueDefinition,bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/Murdar/Dialogue")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
```
(ADAPT: same form as the existing Chapter line.)

### 3. `Character/MurdarHUD.cpp` — `UpdateModel`: choices take the prompt line
Choices beat every other prompt (you are being asked something). Two places, because the car branch returns early:
```cpp
#include "Director/Dialogue/DialogueSubsystem.h"
...
	if (Car)
	{
		... unchanged, including the "Oprește mai întâi" line ...
		if (const UDialogueSubsystem* Dlg = UDialogueSubsystem::Get(this); Dlg && Dlg->IsChoosing())
		{
			Model.PromptText = Dlg->GetChoicesPrompt();
		}
		Model.bArmed = false;
		...
		return;
	}
	...
	// after the whole "Interaction prompt" block:
	if (const UDialogueSubsystem* Dlg = UDialogueSubsystem::Get(this); Dlg && Dlg->IsChoosing())
	{
		Model.PromptText = Dlg->GetChoicesPrompt();
	}
```
If the prompt box truncates four choices, widen it in `MurdarHudWidget.cpp` (the `SBox` around `PromptText`).

### 4. `Character/MurdarCharacter.cpp` — 1/2 don't draw a weapon while choosing
```cpp
#include "Director/Dialogue/DialogueSubsystem.h"
...
void AMurdarCharacter::Input_EquipPrimary(const FInputActionValue&)
{
	// While a choice is open, 1/2 answer it (UDialogueSubsystem reads the keys); drawing a gun on Gică is not an answer.
	if (const UDialogueSubsystem* Dlg = UDialogueSubsystem::Get(this); Dlg && Dlg->IsChoosing()) { return; }
	WeaponComponent->EquipSlot(EWeaponSlot::Primary);
}
void AMurdarCharacter::Input_EquipSecondary(const FInputActionValue&)
{
	if (const UDialogueSubsystem* Dlg = UDialogueSubsystem::Get(this); Dlg && Dlg->IsChoosing()) { return; }
	WeaponComponent->EquipSlot(EWeaponSlot::Secondary);
}
```
(ADAPT: if Holster or anything else is on 3/4, guard it the same way.)

### 5. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarDialogue [name]: list conversations, or start one (no speaker). */ UFUNCTION(Exec) void MurdarDialogue(const FString& Name);
/** MurdarChoose <n>: pick choice n (1-based). */                         UFUNCTION(Exec) void MurdarChoose(int32 N);
```
```cpp
#include "Director/Dialogue/DialogueSubsystem.h"
void UDirectorCheats::MurdarDialogue(const FString& Name)
{
	UDialogueSubsystem* D = UDialogueSubsystem::Get(this);
	if (!D) { return; }
	if (!Name.IsEmpty()) { D->StartDialogue(D->FindByName(Name), nullptr); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *D->Describe()); // ADAPT: print like the others
}
void UDirectorCheats::MurdarChoose(int32 N) { if (UDialogueSubsystem* D = UDialogueSubsystem::Get(this)) { D->Choose(N - 1); } }
```

## Examples (authoring)

### `Content/Murdar/Dialogue/DA_Dialogue_TrafficStop` — the bribe, playable
- `DialogueTag` `Dialogue.TrafficStop`; `StartOnEvent` = `Event.Police.Demand`; `StartOnPayload` none;
  `bInterruptsOthers` on; `MaxDistanceCm` 2500 (driving off ends it — the police then chase you, as today).
- Entries: one, no conditions, `Node` = `offer`.
- Node `offer`: **no Text** (the unit already said „Actele la control...”, subtitled by the HUD); `ChoiceTimeout` 7
  (demand comes 1.5 s into the stop, the ticket at 10 s: 7 s leaves a second to spare); `DefaultChoice` 2.
  1. „Dă-i 200 de lei” — Condition Values `Stat.Money` Min 200 — Effect `PayPolice` 200.
  2. „Dă-i 500 (să nu te mai oprească nimeni pe aici)” — `Stat.Money` Min 500 — `PayPolice` 500 (≥ 500 buys the
     sector in `ReceiveBribe`).
  3. „Taci” — no effect (the unit writes the ticket by itself).

### `Content/Murdar/Dialogue/DA_Dialogue_Gica` — a man at a kiosk, remembers
On the test map, an actor with `Interactable` (Verb „Vorbește cu Nea Gică”, `InteractionTag` `Interact.Kiosk.Gica`).
- `StartOnEvent` `Event.Interact`, `StartOnPayload` `Interact.Kiosk.Gica`; `MaxDistanceCm` 400.
- Entries: (a) none → `hello`; (b) Required `Fact.Gica.Owes` → `debt`.
- `hello` (Speaker „Gică”): „Ce faci, mă? Vrei țigări?” → choices „Da (20 lei)” [`Stat.Money` ≥ 20; AddValue
  `Stat.Money` −20; Next `thanks`], „Dă-mi pe datorie” [SetFact `Fact.Gica.Owes`; Next `credit`], „Nu” [end].
- `thanks`: „Poftim.” · `credit`: „Bine, dar să nu uiți.”
- `debt`: „Și banii mei?” → „Poftim (50 lei)” [`Stat.Money` ≥ 50; AddValue −50; ClearFact `Fact.Gica.Owes`;
  Next `thanks`], „Mâine” [ChoiceTimeout 5, DefaultChoice this one; Next `angry`] · `angry`: „Mâine, mâine...”

### Police voices (optional, when recorded)
Project Settings > Murdar Dialogue > Bark Voices: `Police.Line.PullOver` → 2–3 variants, etc. Tags are the ones
`AMurdarHUD::PoliceLineText` already knows, so text and voice match.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Dialogue/Source/Murdar_GameDev/Director/Dialogue Handoff/Dialogue/Tests/dialogue_rules_test.cpp -o dt && ./dt` → `47 checks, 0 failed`.

Without the economy handoff, set money by hand: `MurdarValue Stat.Money 1000`.

| ID | Test | Pass |
|---|---|---|
| DLG-01 | `MurdarDialogue` | lists both examples |
| DLG-02 | `MurdarValue Stat.Money 1000`, speed near a unit (traffic stop) | after „Actele la control...” the prompt shows 3 choices |
| DLG-03 | press 1 | „Bine, bine... N-am văzut nimic.”; unit stands down; `Event.Police.Bribed` 200 in the log |
| DLG-04 | `MurdarValue Stat.Money 250`, new stop | only choices 1 and 3 |
| DLG-05 | new stop, press nothing | choices vanish at 7 s; ticket line at 10 s as before |
| DLG-06 | new stop, drive off during the choices | choices vanish; pursuit starts as before |
| DLG-07 | talk to Gică, choose „Dă-mi pe datorie”, talk again | second time „Și banii mei?” |
| DLG-08 | at Gică, press 1 with the choices open | no weapon drawn |
| DLG-09 | walk 5 m away mid-conversation | conversation ends; E prompt comes back |
| DLG-10 | during Gică, trigger a traffic stop (cheat heat) | police conversation takes over |
| DLG-11 | an entry pointing to a node that doesn't exist | error in the log naming the dialogue; nothing hangs |
| DLG-12 | with bark voices set: repeated PullOver orders | voiced at most every 4 s, variants alternate |

## Design notes

- Choices in the prompt line, not a new menu: the game has no menus on screen during play ("felt, not shown").
- Hidden, not greyed out: GTA shows you what you can't afford; here you simply don't think of offering money you
  don't have.
- No lip sync, no camera cuts: the talk happens in the world while you drive or stand. Cutscenes are a later layer.
- A conversation isn't saved (like a mission in progress): what it changed is.
- `Stat.Honor` can't be written from a dialogue — only through `FHonorToken`, as for missions.
