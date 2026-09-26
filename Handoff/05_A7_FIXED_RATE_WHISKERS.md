# 05 — A7: fixed-rate whiskers (FPS independence, PVA-T38; cost, PVA-T40)

**Medium risk: this touches the driver every car uses. Measure before and after; revert if any Phase 3–9 test
regresses.**

## Problem

RECON §3.2: `ProbeWhiskers` = 5 sweeps + 2 side rails **per frame**. At 144 FPS a car costs ~2.4× what it costs
at 60; at 30 FPS a car at 110 km/h moves ~1 m between probes. Detection and the obstacle brake therefore depend on
frame rate (spec §46 T38) and the cost scales with FPS × cars (T40).

> Review note: the premise is weaker than first written. Probing *more* often at high FPS doesn't change what a
> whisker sees much. The likelier sources of FPS-dependent driving are per-frame controllers that don't scale by
> `DeltaTime` correctly (steering smoothing, P gains, `FInterpTo` speeds, the "stuck for 1.5 s" timers if they count
> frames). Step 0 should therefore also grep `SteerTowards`/`ApplyInputs` for dt handling. If Step 0's columns
> agree, keep this file only for the **cost** half (Phase 13), not for T38.

## Step 0 — measure first (spec §54: one parameter, test dependencies, regression, log)

Run the Phase 3 lap (472 m, 110 km/h cap, 6 traffic cars) and the PVA-T04/T05 obstacle runs at `t.MaxFPS 30`,
`60`, `144`. Record: lap time, 0-flip count, obstacle first-detect distance, damage. If the three columns already
agree within noise, **stop here**, log the result, and only do the cost part (Step 2) if Phase 13 later asks.

## Step 1 — probe at a fixed rate, extrapolate between probes

```cpp
// UVehiclePursuitComponent.h
/** Whisker probe rate. Decoupled from frame rate so detection does not depend on FPS (PVA-T38). At 110 km/h and
 *  30 Hz the car moves ~1 m between probes, well inside the reaction distance the reach already includes. */
UPROPERTY(EditAnywhere, Category = "Pursuit|Obstacles", meta = (Units = "Hz", ClampMin = "10", ClampMax = "120"))
float WhiskerRateHz = 30.f;

float WhiskerAccumulator = 0.f;
float SecondsSinceProbe = 0.f;
// ADAPT: the struct/fields ProbeWhiskers fills today (obstacle distance, closing speed, side, hit actor…)
FWhiskerResult CachedWhiskers;
```

```cpp
// in TickComponent, where ProbeWhiskers() is called today:
WhiskerAccumulator += DeltaTime;
SecondsSinceProbe += DeltaTime;
const float Period = 1.f / WhiskerRateHz;
if (WhiskerAccumulator >= Period || bWhiskersDirty)   // bWhiskersDirty: mode/target change, escape start/end
{
    WhiskerAccumulator = FMath::Fmod(WhiskerAccumulator, Period);
    CachedWhiskers = ProbeWhiskers();                 // ADAPT: today it may write members directly
    SecondsSinceProbe = 0.f;
    bWhiskersDirty = false;
}

// Between probes: the gap shrinks by what we closed since the probe. Use closing speed, not our speed,
// so a car ahead moving with us does not "approach".
FWhiskerResult W = CachedWhiskers;
if (W.bHit)
{
    W.DistanceCm = FMath::Max(0.f, W.DistanceCm - W.ClosingSpeedCms * SecondsSinceProbe);
}
// … the speed limiter / avoidance read W, not CachedWhiskers
```

- `bWhiskersDirty` is set wherever the mode, target, or escape state changes — those need a fresh probe now.
- The side rails (lateral clearance for SideSweep/PIT gating) go through the same cache.
- The per-frame `SteerTowards` stays per frame; only the sweeps are rate-limited.

## Step 2 — optional stagger across cars (cost)

Initialise `WhiskerAccumulator = FMath::FRand() * Period` in `BeginPlay` so 20 cars don't all probe on the same
frame. (For deterministic tests — see 11 C10 — seed it from the unit id instead: `(UnitId % 8) / 8.f * Period`.)

## Tests

- The Step 0 matrix again at 30/60/144 FPS: the three columns agree within the noise of 3 runs; no column worse
  than the pre-change 60 FPS column.
- Regression: Phase 3 lap, PVA-T04/T05/T06/T07, Phase 4 wall case, ghost chase 2 units heat 60 and 80 (gap mean,
  damage, Escape seconds within the ranges in the test report).
- `stat game` / Insights: whisker cost per car at 144 FPS drops ~4.8× (30 vs 144 per second).
