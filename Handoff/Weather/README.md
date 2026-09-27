# Vremea (Weather) — handoff for local Claude Code

## Pe scurt (română)

În fiecare oră de joc vremea se poate schimba: senin, înnorat, ploaie, furtună, ceață (ceața doar noaptea sau în
zori). Schimbările se văd treptat, în ~1,5 minute, nu dintr-o dată. Pe ploaie **drumul se udă** și se usucă după
(mai încet noaptea), iar **asfaltul ud ține mai puțin**: până la 75% aderență. Pe ploaie ies mai puțini oameni pe
stradă, iar la prânz, pe furtună, e întuneric ca seara (farurile se aprind, Paranoia în retrovizoare „vede” mai prost).
Ceața îngroașă ceața nivelului; ploaia cade în jurul camerei, cu sunet și tunete. Vremea se salvează.

Aderența rămâne definită **într-un singur loc** (frecarea materialelor fizice ale drumului, cum a vrut proiectul):
vremea o scalează temporar pe materialele din listă și o **restaurează** la final (important în editor, după PIE).

**Verified here:** `WeatherRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 5 tests / 24 checks pass (including
20 000 simulated hours). **Not compiled:** the Unreal files.

**Depends on:** TimeOfDay (the hour). Optional hookups: Rearview (darkness), TimeOfDay (headlights), Population.

## Paste this prompt into local Claude Code

```
Read Handoff/Weather/README.md. TimeOfDay must be integrated. One step at a time, building after each (editor
closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Add "Niagara" to Build.cs if missing. Copy Handoff/Weather/Source/Murdar_GameDev/Director/Weather/* into
   Source/Murdar_GameDev/Director/Weather/. Build.
3. Check the ADAPT in Apply(): does KinetiForge read UPhysicalMaterial::Friction per raycast? Tell me.
4. Apply README §Patches 1-4. Content (new assets, ask me): README §Content.
5. After PIE, check the road physical material in the editor: Friction must be back to its original value.
6. Run README §In-game tests; write Docs/WEATHER_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Weather/WeatherRules.h` | pure: Markov chain, looks, blend, wetness, grip, people, gloom — unit-tested |
| `Director/Weather/WeatherSettings.h` | Project Settings > Game > Murdar Weather |
| `Director/Weather/WeatherSubsystem.h/.cpp` | hourly change, blend, wet roads, grip, fog, rain, save |

## Patches

### 1. Tags
```ini
+GameplayTagList=(Tag="Stat.Weather",DevComment="Weather state index (saved)")
+GameplayTagList=(Tag="Stat.Wetness",DevComment="Road wetness 0..1 (saved)")
+GameplayTagList=(Tag="Event.Weather.Changed",DevComment="Magnitude = new state index")
```

### 2. Fewer people in the rain — `AI/PopulationSubsystem.cpp` (`ManagePopulation`)
Next to the TimeOfDay scale (Handoff/TimeOfDay patch 2): multiply the pedestrian cap by
`UWeatherSubsystem::Get(this) ? UWeatherSubsystem::Get(this)->GetPeopleScale() : 1.f`.

### 3. Gloom — headlights and the rearview
- TimeOfDay `SweepHeadlights` / `IsNight`: treat `LightLevel01 - Weather->GetGloom() * 0.6` as the light level.
- RearviewParanoia `UpdateDarkness`: `Darkness = FMath::Max(Darkness, Weather->GetGloom() * 0.7f)`.

### 4. Cheat
```cpp
/** MurdarWeather [0-4]: show; or force clear/cloudy/rain/storm/fog instantly. */ UFUNCTION(Exec) void MurdarWeather(int32 State = -1);
```
```cpp
#include "Director/Weather/WeatherSubsystem.h"
void UDirectorCheats::MurdarWeather(int32 State)
{
	UWeatherSubsystem* W = UWeatherSubsystem::Get(GetWorld());
	if (!W) { return; }
	if (State >= 0 && State < MurdarWeather::N) { W->SetState(MurdarWeather::EState(State), true); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *W->Describe());
}
```

## Content (new assets, with the user)
- `MPC_Weather` with scalars Rain, Wetness, Cloud, Fog, Wind. Road material: darken + raise specular/lower roughness
  with Wetness (puddles in the low spots with a mask); a „cloud” darkening on the sky/sun via Cloud.
- `NS_Rain`: GPU sprites in a box around the camera, user float `Intensity` scaling spawn rate.
- `WetAffected`: the asphalt / cobble physical materials. Leave dirt and mud out (they have their own feel).
- Sounds: a rain loop, 3 thunder one-shots.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Weather/Source/Murdar_GameDev/Director/Weather Handoff/Weather/Tests/weather_rules_test.cpp -o wet && ./wet` → `24 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| WEA-01 | `MurdarWeather 3` | storm at once; rain, thunder now and then, darker |
| WEA-02 | drive hard on wet asphalt (`MurdarWeather` shows wet ~1) | slides earlier; grip x0.75 |
| WEA-03 | stop the rain (`MurdarWeather 0`), wait | wetness falls over ~10 min (day) |
| WEA-04 | busy street, storm | fewer pedestrians spawn |
| WEA-05 | `MurdarTimeScale` high, watch several hours | changes now and then, blended |
| WEA-06 | end PIE, open the road physical material | original Friction |
| WEA-07 | save in rain, load | still raining, still wet |
