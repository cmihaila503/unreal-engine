# Greutatea mașinii (CarWeight) — handoff for local Claude Code

## Pe scurt (română)

Mașina se simte grea la volan, dar la **sărituri și la impact pare de 10 kile**. Am căutat în cod: **nu e masa**
(1550 kg, inerție bună). Sunt șase cauze, iar handoff-ul le rezolvă pe toate, fiecare cu comutatorul ei:

| # | Ce simți | De ce (în cod) | Ce face handoff-ul |
|---|---|---|---|
| 1 | Mașina ricoșează din ziduri ca o minge | caroseria nu are material fizic, deci rămâne cel implicit, elastic | material fără elasticitate (Restitution 0, Min) și frecare 0,3: se oprește în zid și alunecă de-a lungul lui |
| 2 | E „scuipată” din obiecte | viteza de depenetrare e nelimitată | limitată la 200 cm/s |
| 3 | Coliziuni și aterizări grosiere la 30 FPS | fizica fără substepping | verificare și pornire (după ce verificăm KinetiForge) |
| 4 | Cade „ca pe Lună” | în aer nu există nimic în afară de gravitația reală | **gravitație 1,8× doar cât toate roțile sunt în aer**, crescută treptat în 0,25 s (o denivelare nu contează ca săritură) |
| 5 | Se dă peste cap din orice și saltă de 2–3 ori la aterizare | nimic nu frânează rotația în aer; suspensia aterizează ca pe arcuri | în aer: frânarea rotației înainte-spate și laterale (niciodată stânga-dreapta, aia e a ta) și o mână care o ține pe roți; la aterizare: amortizare 0,35 s |
| 6 | Impactul „nu se simte” | shake-ul există, dar nu există nicio oprire a timpului | după viteza impactului: shake, **hit-stop de 30–70 ms** la lovituri tari (doar la mașina ta), plus o bufnitură joasă |

Mai adaugă:

- **Auditul de mărime:** un script rulat în editor (prin MCP) măsoară mașina, banda de drum și ce selectezi (bloc,
  ușă, bordură) și îți spune dacă **mașina e prea mică** sau **drumurile prea late**. Poate să fie chiar cauza
  senzației de „mașină mică” din captura ta.
- **Camera:** FOV-ul trece de la 75→92 la **70→80**, iar brațul de la 5,5→7 m la **4,8→5,6 m**. Mașina pare de
  mărimea ei. Scriptul scrie valorile în asset-urile mașinilor doar dacă îi ceri.

**Verified here:** `CarWeightRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 6 grupuri de teste / **37
checks pass**. **Not compiled:** fișierele Unreal și scriptul Python. Totul e scris pe codul real
(`UVehicleEffectsComponent::UpdateDownforce/NotifyImpact`, `AMurdarVehicle::GetDriveAssembly`,
`UVehicleWheelComponent::GetIsWheelOnGround`, `UVehicleDefinition` Crash*/Fov*/Camera*). Apelurile de engine
nesigure sunt marcate `// ADAPT:`.

## Paste this prompt into local Claude Code

```
Read Handoff/CarWeight/README.md. The editor is connected through MCP (§Assets rule in
Handoff/00_README_LOCAL_CLAUDE.md: checkpoint first, save only what you change, read back, never save the level).
One step at a time, reporting in Romanian:

1. Unit test (README §Tests). Then run Handoff/CarWeight/Tools/scale_audit.py in the editor (report only) on the
   test map with the player's car placed; select a bloc, a door and a kerb first. Paste the report. If the
   DIAGNOSIS says the car or the lanes are the wrong size, STOP and ask me before changing any mesh or road.
2. BEFORE: record the three test drives in README §Tests (jump, wall at 50 km/h, kerb at 30 km/h) with
   `Murdar.CarWeight.Debug 1` off (it does not exist yet) - write what you see + the air time from a stopwatch
   (or the Chaos Visual Debugger) into Docs/CAR_WEIGHT_REPORT.md.
3. Copy Handoff/CarWeight/Source/Murdar_GameDev/Vehicle/Weight/* into Source/Murdar_GameDev/Vehicle/Weight/.
   Apply README §Patches 1-2. Create the asset PM_CarBody (§Setup) and set it in Project Settings > Murdar Car
   Weight. Build (editor closed). Run the three test drives again with `Murdar.CarWeight.Debug 1` = AFTER.
4. Substepping (§Patches 3): check KinetiForge first, report, then change only if it has none of its own.
5. Camera (§Patches 4): run scale_audit.py --camera, read the values back, drive, report.
6. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT with BEFORE/AFTER.
```

## Files

| File | What |
|---|---|
| `Vehicle/Weight/CarWeightRules.h` | pure rules: airborne tracker, extra gravity with ramp, air stabilise, landing settle, impact feel (shake / hit-stop / thud), scale check + diagnosis, camera targets |
| `Vehicle/Weight/CarWeightSettings.h` | Project Settings > Game > Murdar Car Weight (a switch per fix, all tuning, body material, thud sound) |
| `Vehicle/Weight/CarWeightComponent.h/.cpp` | on every car: body contact at BeginPlay, air forces / torques / settle each tick (TG_PrePhysics), impact feel |
| `Tools/scale_audit.py` | editor Python: car / lane / selection sizes vs reference, diagnosis, VehicleDefinition camera values (and `--camera` to write the targets) |

## Patches

### 1. Every car gets the component — `Vehicle/MurdarVehicle.h/.cpp`
```cpp
// .h
class UCarWeightComponent;
...
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UCarWeightComponent> Weight;
// .cpp, constructor, next to the other CreateDefaultSubobject calls
#include "Vehicle/Weight/CarWeightComponent.h"
	Weight = CreateDefaultSubobject<UCarWeightComponent>(TEXT("Weight"));
```
It ticks for AI-driven cars too (same physics for traffic and police as for you — a police car over a crest lands
like yours).

### 2. Impact feel goes through the component — `Vehicle/VehicleEffectsComponent.cpp`, `NotifyImpact`
Replace the camera-shake block
```cpp
	if (V->IsOccupied() && D->CrashShakeClass)
	{
		...StartCameraShake(D->CrashShakeClass, FMath::Lerp(0.3f, 1.5f, Severity));
	}
```
with
```cpp
	if (UCarWeightComponent* W = V->FindComponentByClass<UCarWeightComponent>())
	{
		W->OnImpact(Horizontal, Vertical, Where); // shake + hit-stop + thud, scaled by the delta-v (MurdarWeight::ImpactFeel)
	}
	else if (V->IsOccupied() && D->CrashShakeClass) { /* the old line, as a fallback */ }
```
Damage, sound, pedestrian damage and the bus event stay as they are. Leave the second shake block (line ~247, the
other crash path) alone unless it double-shakes — check in the test.

### 3. Substepping — Project Settings > Physics (DefaultEngine.ini)
First: does KinetiForge / AsyncTickPhysics run its own fixed step? (Look in the plugin's settings and in
`AsyncTickPhysics` for a fixed delta / substep count.) If it does, **leave the engine's substepping off** and report
its rate. If not: `bSubstepping=True`, `MaxSubstepDeltaTime=0.008333`, `MaxSubsteps=6`. Note: forces added from a
game-thread tick (UpdateDownforce, this component) apply once per frame, spread over the substeps — fine for these.

### 4. Camera — the VehicleDefinition assets
`scale_audit.py --camera` writes FovAtRest 70, FovAtSpeed 80, CameraDistance 480, CameraDistanceAtSpeed 560,
CameraPitch -7 into every `UVehicleDefinition`, saves only those assets and reads the values back. Also change the C++
defaults in `VehicleDefinition.h` to the same numbers (new cars start right). CameraPivot Z 125 → 115 by hand.

## Setup (with the user, via MCP)
- `PM_CarBody` (Physical Material): Friction 0.3 (Combine Average), Restitution 0 (Combine **Min**), Density
  irrelevant (mass is overridden). Set it in Project Settings > Game > Murdar Car Weight > Body Material.
- Optional: a sub-bass thud (a low 40–80 Hz hit, ~0.3 s) as Thump Sound.

## Tuning notes
- **Gravity multiplier:** 1.5 feels real-but-weighty, 1.8 the default, 2.2 arcade. Change one number at a time
  (spec §54) and write the reason in the tuning log.
- **Leveling torque 0.6:** enough that a crest does not flip you, not enough to save a real roll. At 0 you feel the
  raw physics; above 1.5 it looks like a hand from the sky.
- **Hit-stop:** only on the player's car, never twice within 1 s, never while time is already changed (menu,
  cutscene, slow-motion cheat).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/CarWeight/Source/Murdar_GameDev/Vehicle/Weight Handoff/CarWeight/Tests/car_weight_rules_test.cpp -o cw && ./cw`
→ `37 checks, 0 failed` (also run by `Handoff/Automation/Tools/run_rule_tests.py`).

| ID | Test | Pass |
|---|---|---|
| CW-01 | ramp / hill crest at 80 km/h | air time shorter than BEFORE (~20–30%), lands wheels-down, **settles in one movement** (no 2–3 bounces) |
| CW-02 | drive over a speed bump at 50 km/h | no "airborne" in the debug log (a bump is not a jump), no gravity kick |
| CW-03 | wall head-on at 50 km/h | the car **stops at the wall** (no bounce back), shake + a short freeze + thud; damage as before |
| CW-04 | scrape along a wall at 60 km/h | slides along it, does not stick or get flung away |
| CW-05 | push the car slowly into a pole | no violent "spit out" (depenetration capped) |
| CW-06 | roll the car deliberately on a steep bank | it still rolls over (leveling stops helping past 50°) |
| CW-07 | a police car and a traffic car over the same crest | they land like yours (same component), no hit-stop from their hits |
| CW-08 | pause during a hit-stop / slow-mo cheat on | time dilation never stuck at 0.05; the menu's pause wins |
| CW-09 | `scale_audit.py` | the report lists the car, the lane under it, the selection, a diagnosis; nothing saved without `--camera` |
| CW-10 | after `--camera` | FOV and arm read back as 70/80 and 480/560; the car looks its size (compare with the screenshot of 30 Sep) |
