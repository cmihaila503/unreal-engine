# Muzică dinamică (Dynamic music) — handoff for local Claude Code

## Pe scurt (română)

În mare parte liniște — orașul, motorul, (mai târziu) radioul. Muzica vine doar când e ceva în joc și spune doar
cât: **neliniște** (stres mare, oprit de poliție), **suspans** (te urmărește cineva — din Paranoia în retrovizoare),
**urmărire** (fugi de poliție), **luptă** (focuri lângă tine, heat letal), **după** (respiri după o urmărire).
Urcă imediat (primul foc trebuie să cadă pe muzică), coboară doar după o pauză, ca o urmărire care se oprește 5
secunde după un bloc să nu taie muzica și s-o reia. Intensitatea urcă repede și coboară încet. Merge cu un MetaSound
(recomandat: parametrii `Intensity` și `Mood`) sau cu straturi (stems) care intră pe rând. Plus „stings” pe
evenimente (ai scăpat, misiune reușită, urmăritorul s-a dat de gol) și muzica se lasă mai jos sub dialog.

**Verified here:** `MusicRules.h` — g++ C++17 `-Wall -Wextra -Wshadow`, 9 tests / 37 checks pass. **Not compiled:**
the Unreal files. Written against the real source (`UTensionSubsystem::GetStressAlpha` and its own heartbeat/drone,
`UFactionMemorySubsystem::GetWantedLevel` (None/Stop/Pursuit/Lethal), `Event.Combat.Shot` (Location),
`Event.Police.PursuitEnded`) and the handoffs' events (`Event.Rearview.*`, `Event.Dialogue.Started/Ended`,
`Event.Mission.*`). None of those handoffs is required: missing events just never arrive.

## Paste this prompt into local Claude Code

```
Read Handoff/Music/README.md. One step at a time, reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Music/Source/Murdar_GameDev/Director/Music/* into Source/Murdar_GameDev/Director/Music/. Fix the ADAPT
   include path. Build (editor closed).
3. Apply README §Patches 1-2. Build.
4. There is no music content yet: tell me what README §Content needs; with placeholder loops if I have none, run
   README §In-game tests with `MurdarMusic` (the log shows the mood and intensity even without sound).
5. Write Docs/MUSIC_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Music/MusicRules.h` | pure: target mood, intensity, mood machine (up at once / down after wait), glide, layer gain — unit-tested |
| `Director/Music/MusicSettings.h` | Project Settings > Game > Murdar Music |
| `Director/Music/MusicSubsystem.h/.cpp` | 10 Hz: inputs → mood → MetaSound parameters or stem volumes; stings; dialogue duck |

## Patches

### 1. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarMusic [mood 0-5|-1]: show the music state; force a mood (0 silence, 1 unease, 2 suspense, 3 aftermath,
 *  4 pursuit, 5 combat), -1 = back to automatic. */
UFUNCTION(Exec) void MurdarMusic(int32 Mood = -2);
```
```cpp
#include "Director/Music/MusicSubsystem.h"
void UDirectorCheats::MurdarMusic(int32 Mood)
{
	UMusicSubsystem* M = UMusicSubsystem::Get(GetWorld());
	if (!M) { return; }
	if (Mood >= -1) { M->ForceMood(Mood, Mood >= 0); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *M->Describe()); // ADAPT: print like the others
}
```

### 2. Tension drone — `Director/TensionSubsystem.cpp` (`UpdateAudio`), optional but recommended
The drone and the score both answer to stress. When music plays, the drone should step back:
```cpp
#include "Director/Music/MusicSubsystem.h"
...
	// The score carries the tension when it plays; the drone fills the silence between.
	const UMusicSubsystem* Music = UMusicSubsystem::Get(this);
	const float MusicDuck = Music ? 1.f - 0.7f * Music->GetIntensity() : 1.f;
	// multiply the drone's target volume by MusicDuck (ADAPT: where the drone volume is set). Leave the heartbeat.
```

## Content (Project Settings > Game > Murdar Music)

Either:
- **MetaSound** `MS_Murdar_Score` with inputs `Intensity` (float 0..1) and `Mood` (int 0..5), looping; inside,
  crossfade layers on Intensity and switch sections on Mood at bar lines. Set `MusicMetaSound`.

Or **stems** (same length and tempo, looping; set each SoundWave's *Virtualization Mode* to **Play When Silent** so
quiet stems keep their place):
| Stem | FadeInStart | FullAt |
|---|---|---|
| pad / drone | 0.05 | 0.3 |
| pulse (bass, ticking) | 0.3 | 0.5 |
| drums | 0.6 | 0.8 |
| lead / brass | 0.85 | 1.0 |

Stings (optional): `Event.Police.PursuitEnded`, `Event.Mission.Succeeded`, `Event.Mission.Failed`,
`Event.Rearview.TailBlown`, `Event.Consequence.Station`.

Sound: for a 90s Bucharest crime story, think less orchestra, more a detuned synth pad, a bass line, a drum machine
— and silence most of the time.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Music/Source/Murdar_GameDev/Director/Music Handoff/Music/Tests/music_rules_test.cpp -o mt && ./mt` → `37 checks, 0 failed`.

| ID | Test | Pass (`MurdarMusic` shows it) |
|---|---|---|
| MUS-01 | drive calmly | `silence`, intensity 0, after 30 s no stems playing |
| MUS-02 | `MurdarHeat 60` (pursuit) | `pursuit` within 0.1 s; intensity reaches ~0.75 in ~1 s |
| MUS-03 | lose the police | `aftermath` ~6 s after the chase ends, `silence` ~26 s after it ended; intensity falls slowly |
| MUS-04 | shoot near a pedestrian | `combat` at once; back down ~18 s after the last shot |
| MUS-05 | at night, get a rearview tail (`MurdarTail`) | `suspense` |
| MUS-06 | talk to someone during music | ", ducked" and quieter |
| MUS-07 | `MurdarMusic 5`, then `MurdarMusic -1` | forced combat, then automatic |
| MUS-08 | stems: listen at the pursuit → aftermath change | layers leave one by one, no clicks, still in time |

## Design notes

- The mood is chosen from things the player did or is going through, never from a timer or a script.
- Up is instant, down waits (DownDelay 6 s + MinDwell 8 s): no pumping.
- No radio here. A car radio (GTA-style stations, 90s Romanian pop/manele/news) is a separate system; when it
  plays, this score should duck the way it ducks for dialogue.
