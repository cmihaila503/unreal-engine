# 07 — A10: graded heat aggression (spec §26, PVA-T21)

## Problem

Today heat changes behaviour in one step: Lethal → Combat → Ram. Spec §26 lists what heat must change (follow
distance, speed, lane-change frequency, intercept frequency, PIT willingness, roadblocks, unit count, risk
tolerance, response time, advanced tactics) and says **Behavior ≠ Heat**: heat + profile + crime severity + traffic
+ vehicle condition + units + road + knowledge.

## Design: one scalar, many consumers, the profile keeps its personality

`Aggression01 = clamp( HeatCurve(heat) + SeverityWeight · severity01 − DamageWeight · Damage01 , 0, 1 )`

Each profile field that heat should move gets a *calm* value (the existing field, unchanged) and an *aggressive*
value; the unit uses `Lerp(calm, aggressive, Aggression01)`. A Disciplined profile has a small calm→aggressive
spread, Aggressive a large one — so two profiles at the same heat still decide differently (PVA-T22).

```cpp
// UPoliceDrivingProfile.h
#include "Curves/CurveFloat.h"   // FRuntimeFloatCurve

/** Heat (0..100, the faction memory's scale) → aggression 0..1. Inline curve: no extra asset.
 *  Default: 0 below HeatStop, rises through Pursuit, 1 at Lethal. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression")
FRuntimeFloatCurve HeatToAggression;

/** How much the worst known crime (severity / max severity, see 03) adds to aggression. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression", meta = (ClampMin = "0", ClampMax = "1"))
float SeverityAggressionWeight = 0.25f;

/** How much our own car damage takes away from aggression (a wrecked car backs off). */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression", meta = (ClampMin = "0", ClampMax = "1"))
float DamageAggressionWeight = 0.5f;

// aggressive ends — calm ends are the existing fields
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression", meta = (Units = "cm", ClampMin = "500", ClampMax = "10000"))
float DesiredFollowDistanceAggressiveCm = 1500.f;       // ADAPT: calm = existing follow distance field
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression", meta = (Units = "s", ClampMin = "1", ClampMax = "60"))
float PITCooldownAggressiveSeconds = 3.f;               // calm = PITCooldownSeconds (6)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression", meta = (ClampMin = "0", ClampMax = "1"))
float InterceptPreferenceAggressive = 0.8f;             // calm = InterceptPreference (add it if missing, spec §4)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggression", meta = (Units = "s", ClampMin = "5", ClampMax = "120"))
float BackupRequestAggressiveSeconds = 10.f;            // calm = the director's 20 s first request (Phase 8)

float EvalAggression(float Heat, float Severity01, float Damage01) const
{
    const FRichCurve* C = HeatToAggression.GetRichCurveConst();
    const float H = (C && C->GetNumKeys() > 0) ? C->Eval(Heat) : Heat / 100.f;  // empty curve: linear
    return FMath::Clamp(H + SeverityAggressionWeight * Severity01 - DamageAggressionWeight * Damage01, 0.f, 1.f);
}
```

Defaults per profile (set in `setup_police_profiles.py`, ADAPT the heat thresholds from the memory: Stop 12,
Pursuit 40, Lethal 75):

| Profile | Curve keys (heat → a) | Spread (aggressive ends) |
|---|---|---|
| Disciplined | 12→0, 40→0.2, 75→0.5, 100→0.6 | small |
| Normal | 12→0, 40→0.3, 75→0.8, 100→1 | defaults above |
| Aggressive | 12→0.2, 40→0.6, 75→1 | follow 1000 cm, PIT cd 2 s |
| Elite | as Normal | intercept 0.9, PIT cd 3 s |
| Inexperienced | as Normal | follow 2500 cm (keeps distance even when angry) |
| Tactical | as Disciplined | intercept 1.0, backup 8 s |

## Consumers (one at a time — spec §54)

Order, each with its own tuning-log entry and ghost-chase regression:

1. Follow distance (chase slot gap) — `Lerp(calm, aggressive, A)`.
2. PIT cooldown.
3. Interceptor share: director gives the Interceptor role when `InterceptPreference(A) > 0.5` and 2+ units.
4. Backup request timing (director).
5. Ram window: keep the Lethal gate as is; A only widens `RamAlignDeg` by ≤ 30 %. Do **not** let heat alone
   authorise Ram (spec §1.27).

Controller: compute `A` once per Think, put it in `FPoliceUnitTelemetry::Aggression` and the decision trace
("aggr 0.72 = heat 0.60 + sev 0.18 − dmg 0.06").

## PVA-T21 test

Ghost chase, 2 units, Police_Normal, 120 s, heat fixed at 40 / 60 / 80 (disable decay for the test). Expected:
follow gap mean decreases with heat; PIT attempts per minute increase; backup requested earlier. Then PVA-T22:
same at heat 60 with Disciplined vs Aggressive — gap and PIT counts differ.
