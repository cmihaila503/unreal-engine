# 06 — A9: remaining literals → owned tunables (spec §1.10–11)

**No value changes.** Each literal becomes a property with the *same* default. If any test number moves, the
conversion was wrong.

Find them first — the Docs name some, the code has more:

```
grep -nE "[^a-zA-Z_][0-9]+(\.[0-9]+)?f?[^0-9a-zA-Z_]" Source/Murdar_GameDev/AI/MurdarPoliceAIController.cpp \
  Source/Murdar_GameDev/AI/VehiclePursuitComponent.cpp Source/Murdar_GameDev/AI/MurdarRoadNavigation.cpp \
  | grep -vE "0\.f|1\.f|0\.5f|2\.f|//|UE_LOG|Printf|FColor|DrawDebug"
```

Allowed to stay literal: 0, 1, 0.5 in math, unit conversions (100 cm/m, 3.6, 1/27.78), array indices, debug
colours/sizes, `ThinkInterval` if already a UPROPERTY.

## Known from the Docs

| Literal | Where (Docs) | Owner | Property | Unit | Default | Min–Max | Reason (tooltip) |
|---|---|---|---|---|---|---|---|
| 4 | `StoppedKph` (ARCH §3) | profile | `StoppedKph` | km/h | 4 | 1–10 | Below this the car counts as stopped (traffic stop, block parked). |
| 600 | `ReorderDistanceCm` (ARCH §3) | profile | `ReorderDistanceCm` | cm | 600 | 200–3000 | ADAPT: read what "reorder" guards in the code and write it here. |
| 6000 | `StandDownDriveOffCm` (ARCH §3) | profile | `StandDownDriveOffCm` | cm | 6000 | 1000–20000 | How far a bribed unit drives off before idling. |
| 8000 | target trail length "last 80 m" (ARCH §4) | pursuit comp. | `TargetTrailLengthCm` | cm | 8000 | 2000–20000 | Positions kept to follow a turning target's line. |
| 1.5 | path re-plan interval (RECON §3.2) | pursuit comp. | `RePlanIntervalSeconds` | s | 1.5 | 0.25–5 | Minimum time between route re-plans. |
| 0.2 (20 %) | "kept unless 20 % shorter" (RECON §3.2) | pursuit comp. | `RePlanShorterFraction` | — | 0.2 | 0.05–0.5 | A new plan must be this much shorter to replace the committed one. |
| 1000 | corner window "over 10 m" (RECON §3.2) | pursuit comp. | `CornerWindowCm` | cm | 1000 | 300–3000 | Distance over which turn angle is summed for the corner speed. |
| 70 / 35 | heading brake "> 70° above 35 km/h" (RECON §3.2) | pursuit comp. | `HeadingBrakeDeg`, `HeadingBrakeMinKph` | deg, km/h | 70, 35 | 30–150, 10–80 | Brake instead of steer when this far off heading at speed. |
| 0.55 / 0.6 | PIT in / recover (RECON §3.2) | pursuit comp. | `PITSteerSeconds`, `PITRecoverDuration` | s | 0.55, 0.6 | 0.2–2 | ADAPT: if already properties, skip. |
| 3 / 1.5 / 2.5 / 2.5 / 3 / 7 | stuck < 3 km/h 1.5 s, no progress 2.5 s over 2.5 m, reverse 3 s, 7 m backed (RECON §3.2) | pursuit comp. | `StuckKph`, `StuckSeconds`, `NoProgressSeconds`, `NoProgressCm`, `EscapeSeconds`, `EscapeMaxBackCm` | | same | sensible ±3× | ADAPT: may already be properties after Phase 5. |
| 0.45 | ram lead (RECON §3.2) | pursuit comp. | `RamLeadSeconds` | s | 0.45 | 0.1–1.5 | Aim this far ahead of the target when ramming. |

Car-side values stay on `UVehiclePursuitComponent` (they describe the car), driver-side on the profile — same split
as RECON §8. Add `meta=(ClampMin, ClampMax, Units)` to the existing pursuit tunables that lack them (RECON C9).

## Tests

Phase 3 lap + ghost chase heat 60 + PVA-T35: identical numbers within run-to-run noise. Log one tuning-log entry for
the whole conversion (Old = literal, New = property, same value).
