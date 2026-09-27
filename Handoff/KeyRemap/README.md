# Remaparea tastelor (Key remap) — handoff for local Claude Code

## Pe scurt (română)

În Setări apare **„Taste”**: toate acțiunile pe care le poți schimba, pe jos și la volan, cu tasta de tastatură și
butonul de gamepad. Enter pe un rând → „Apasă o tastă…” → apeși. Dacă tasta e deja folosită **în același context**
(de ex. două acțiuni pe jos), cele două **se schimbă între ele**. Aceeași tastă poate servi însă o acțiune pe jos
și una la volan, fiindcă nu merg niciodată împreună. Tastele de pauză nu se pot lua. Există și „Resetează”. Salvarea
o face motorul (Enhanced Input User Settings), pe utilizator.

**Verified here:** `KeyRemapRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 6 tests / 15 checks pass. **Not
compiled:** the Unreal files — the Enhanced Input user-settings API moved between 5.3 and 5.8, so the calls are
marked ADAPT.

**Depends on:** Menus.

## Paste this prompt into local Claude Code

```
Read Handoff/KeyRemap/README.md. Menus must be integrated. One step at a time, building after each (editor closed),
reporting in Romanian:
1. Run the unit test (README §Tests).
2. README §Setup: turn on Enhanced Input user settings; list the IMCs (GASP on foot, vehicle) and which of their
   mappings are Player Mappable today. Making mappings player-mappable edits the IMC assets: ask me first.
3. Copy Handoff/KeyRemap/Source/Murdar_GameDev/UI/KeyRemap/* into Source/Murdar_GameDev/UI/KeyRemap/. Fix the ADAPT
   API calls against the 5.8 headers (EnhancedInputUserSettings.h). Build.
4. Apply README §Patches 1-2. Build.
5. Run README §In-game tests; write Docs/KEY_REMAP_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `UI/KeyRemap/KeyRemapRules.h` | pure: slot acceptance, conflicts by context, rebind with swap, reserved keys — unit-tested |
| `UI/KeyRemap/KeyRemap.h/.cpp` | settings (contexts, reserved), gather rows, rebind through the engine, reset |

## Setup
- Project Settings > Enhanced Input > **Enable User Settings** on.
- In each IMC (with the user): the mappings the player may change get **Player Mappable Key Settings** with a
  unique **Name** (`Interact`, `Reload`, `Handbrake`…) and a Romanian **Display Name** („Interacționează”). Keyboard
  in slot 1, gamepad in slot 2 (the engine's First / Second).
- Project Settings > Murdar Key Remap: Contexts = („Pe jos”, the GASP IMC), („La volan”, the vehicle IMC).
- The key-polling systems from the handoffs (dialogue choices, radio, headwear, pause) use their own key lists in
  their settings; they are not remapped here (listed in the report as a follow-up if the user wants them in).

## Patches

### 1. The menu widget can capture one key — `UI/Menus/SMurdarPauseMenu.h/.cpp`
```cpp
public:
	/** The next key pressed goes here instead of navigating (Escape cancels with an invalid key). */
	void CaptureNextKey(TFunction<void(FKey)> InCapture) { Capture = MoveTemp(InCapture); }
private:
	TFunction<void(FKey)> Capture;
```
First lines of `OnKeyDown`:
```cpp
	if (Capture)
	{
		const TFunction<void(FKey)> C = MoveTemp(Capture);
		Capture = nullptr;
		C(KeyEvent.GetKey() == EKeys::Escape ? EKeys::Invalid : KeyEvent.GetKey());
		return FReply::Handled();
	}
```
Also capture mouse buttons (thumb buttons are popular): in `SMurdarPauseMenu`, override `OnMouseButtonDown` to do the
same when `Capture` is set.

### 2. The "Taste" page — `UI/Menus/PauseMenuSubsystem.cpp`
A row „Taste” at the end of `ShowSettings()` opening `ShowKeys()`:
```cpp
#include "UI/KeyRemap/KeyRemap.h"
void UPauseMenuSubsystem::ShowKeys()
{
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const TArray<MurdarKeyRemap::FRow> Rows = MurdarKeyRemap::Gather(PC);
	FMenuPage Page;
	Page.Title = LOCTEXT("Keys", "Taste");
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const MurdarKeyRemap::FRow R = Rows[i];
		const FText Label = FText::Format(LOCTEXT("KeyRow", "{0} ({1}, {2})"), R.DisplayName, R.ContextName,
			R.Slot == MurdarKeys::ESlot::Gamepad ? LOCTEXT("Pad", "gamepad") : LOCTEXT("Kb", "tastatură"));
		Page.Rows.Add({ Label, [R] { return R.Key.GetDisplayName(); }, nullptr, [Weak, Rows, i]
		{
			if (!Weak.IsValid()) { return; }
			Weak->Message = LOCTEXT("Press", "Apasă o tastă… (Esc = renunță)");
			Weak->Widget->CaptureNextKey([Weak, Rows, i](FKey Key)
			{
				if (!Weak.IsValid()) { return; }
				const MurdarKeys::EResult Res = Key.IsValid() ? MurdarKeyRemap::Rebind(Weak->GetWorld()->GetFirstPlayerController(), Rows, i, Key) : MurdarKeys::EResult::Same;
				Weak->Message = Res == MurdarKeys::EResult::Swapped ? LOCTEXT("Swapped", "Schimbate între ele.")
					: Res == MurdarKeys::EResult::Reserved ? LOCTEXT("Reserved", "Tasta asta nu se poate folosi.")
					: Res == MurdarKeys::EResult::WrongSlot ? LOCTEXT("Wrong", "Tastă de alt tip (tastatură / gamepad).") : FText::GetEmpty();
				Weak->ShowKeys();
			});
		}, nullptr });
	}
	Page.Rows.Add({ LOCTEXT("Reset", "Resetează"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { MurdarKeyRemap::ResetToDefaults(Weak->GetWorld()->GetFirstPlayerController()); Weak->ShowKeys(); } }, nullptr });
	Page.Rows.Add({ LOCTEXT("Back", "Înapoi"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->Message = FText::GetEmpty(); Weak->ShowSettings(); } }, nullptr });
	Page.Footer = [Weak] { return Weak.IsValid() ? Weak->Message : FText::GetEmpty(); };
	Page.Back = [Weak] { if (Weak.IsValid()) { Weak->ShowSettings(); } };
	Widget->SetPage(MoveTemp(Page));
}
```
(Declare `void ShowKeys();` in the header. The page lists many rows: if it doesn't fit, wrap the rows box of
`SMurdarPauseMenu` in an `SScrollBox`.)

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/KeyRemap/Source/Murdar_GameDev/UI/KeyRemap Handoff/KeyRemap/Tests/key_remap_rules_test.cpp -o kr && ./kr` → `15 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| KEY-01 | Setări → Taste | every mappable action, keyboard and pad, with its key |
| KEY-02 | Interacționează → F | E no longer interacts, F does, on foot |
| KEY-03 | Interacționează → R (Reload is R) | „Schimbate între ele.”; Reload on the old key |
| KEY-04 | an on-foot action → the handbrake key | allowed (different context) |
| KEY-05 | → Escape | „Tasta asta nu se poate folosi.” |
| KEY-06 | restart the game | changes kept |
| KEY-07 | Resetează | defaults back |
