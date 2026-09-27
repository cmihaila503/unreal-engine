# Localizare RO / EN (Localization) — handoff for local Claude Code

## Pe scurt (română)

Jocul e scris în română (limba „nativă” a textelor), iar engleza vine ca traducere. Unreal adună singur textele
din `NSLOCTEXT` / `LOCTEXT` (și din asset-uri: dialoguri, misiuni, setări) și le dă traducătorului într-un fișier
`.po`. Ce **nu** e scris așa nu poate fi tradus niciodată. De aceea am făcut un **verificator** (`loc_lint.py`) care
găsește asemenea texte. Pe codul tău de acum a găsit **33**:
- **30 de replici ale pietonilor** în `AI/PedestrianComponent.cpp` („Ești nebun?! Era să mă calci!” …), ținute ca
  `TEXT("...")` și afișate prin `FString`;
- **3 în HUD** (`MurdarHUD.cpp`): „Oprește mai întâi”, „Urcă în %s” și numele de rezervă „mașină”. Tot acolo,
  „Ia %s (%d)” e la fel de netraductibil, dar e prea scurt ca verificatorul să-l prindă: se repară împreună.

Codul din toate handoff-urile trece verificarea (0 probleme). Mai jos: repararea celor 33, setarea țintei de
localizare și opțiunea „Limba” în Setări.

**Verified here:** `loc_lint.py` — 8 unit tests pass (`python3 -m unittest`); run on every handoff folder: 0
findings; run on the project source: 2 RAW + 31 DIAC. Nothing else here is code.

## Paste this prompt into local Claude Code

```
Read Handoff/Localization/README.md. One step at a time, reporting in Romanian:
1. Copy Handoff/Localization/Tools/loc_lint.py into Tools/. Run it on Source/Murdar_GameDev and show me the list.
2. Fix README §Fixes (pedestrian lines, HUD strings) with NSLOCTEXT; build (editor closed); run loc_lint again: 0.
3. README §Setup: create the localization target in the editor's Localization Dashboard (with me watching), gather,
   compile; add the "Limba" option (README §Patches). Build.
4. Test README §Tests; write Docs/LOCALIZATION_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Fixes (in the project's own code)

### Pedestrian lines — `AI/PedestrianComponent.cpp`
Turn each `TEXT("…")` line into `NSLOCTEXT("MurdarStreet", "NearMiss1", "Ești nebun?! Era să mă calci!")` and the
arrays into `TArray<FText>` (keys `NearMiss1..N`, `Pause1..N`, etc. — stable, never renumbered once translated).
`Say()` takes an `FText`.

### HUD — `Character/MurdarHUD.cpp`
```cpp
Model.PromptText = NSLOCTEXT("MurdarHUD", "StopFirst", "Oprește mai întâi");
Model.PromptText = FText::Format(NSLOCTEXT("MurdarHUD", "Enter", "Urcă în {0}"), CarName);        // CarName: FText
Model.PromptText = FText::Format(NSLOCTEXT("MurdarHUD", "Pick", "Ia {0}  ({1})"), WeaponName, FText::AsNumber(Ammo));
```
(`FText::Format` with named or indexed arguments lets a translator reorder the sentence; `FString::Printf` never can.)

## Setup (editor, Localization Dashboard)
- Target **Game**: gather from text in `Source/Murdar_GameDev` and from packages in `Content/Murdar` (dialogues,
  missions, chapter names, the settings' texts in `Config/DefaultGame.ini` — gather from INI too).
- **Native culture: ro**. Cultures: **ro**, **en**.
- Gather → Export (`.po` for the translator) → Import → Compile. Packaging: Project Settings > Packaging >
  Localizations to Package = ro, en.
- Audio: voiced lines (dialogue, radio) stay Romanian with English subtitles — dubbing isn't planned.

## Patches

### „Limba” in Setări — `UI/Menus/*`
`MurdarMenu::FSettings` gets `int Language` (0 ro, 1 en); `Read()` from
`UKismetInternationalizationLibrary::GetCurrentCulture()`; `WriteAndApply()`:
`UKismetInternationalizationLibrary::SetCurrentCulture(Language == 1 ? TEXT("en") : TEXT("ro"), /*bSaveToConfig*/ true);`
A `ChoiceRow` „Limba” — „Română” / „English”. Texts on screen update live (FText); cached `FString`s don't — which is
one more reason for the fixes above.

## Rules for new code (all handoffs follow them)
- Player-facing text is `FText` from `NSLOCTEXT` / `LOCTEXT` (C++) or an `FText` property (data assets, settings).
- Build sentences with `FText::Format`, never by concatenating strings.
- Keys are stable. Changing the Romanian text keeps the key (the translation is then marked for review).
- Numbers and money: `FText::AsNumber` (1.250 in ro, 1,250 in en).
- Run `python Tools/loc_lint.py Source/Murdar_GameDev --strict` before committing (0 findings).

## Tests

Unit: `python3 -m unittest Handoff/Localization/Tests/test_loc_lint.py` → `OK` (8 tests).

| ID | Test | Pass |
|---|---|---|
| LOC-01 | `loc_lint.py --strict` on the source | exit 0 |
| LOC-02 | Localization Dashboard: gather | pedestrian lines, HUD prompts, dialogue lines all in the manifest |
| LOC-03 | fill 5 English lines in the `.po`, compile, Setări → Limba → English | those 5 in English at once, rest in Romanian |
| LOC-04 | money in the pause header | 1.250 lei (ro) / 1,250 lei (en) |
