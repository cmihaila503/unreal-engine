# 08 — A4 + A5: junction priority and the dead-end overshoot

Spec §10 (detect cross traffic, emergency crosses with raised priority but not blind), §13, §52.

## A5 first (small): traffic must never leave the lane network

TEST_REPORT Phase 2/3: "one traffic car overshot a road end onto the navmesh". For civilians the navmesh
fallback is wrong — a civilian on the pavement is visible nonsense.

- In `AMurdarTrafficAIController`: when the lane route ends (dead end / no outgoing lane), the desired speed must
  reach 0 at the last route point. Reuse the driver's braking-distance limiter with the end of the route as a hard
  obstacle at its distance (ADAPT: the function that already caps speed on `ObstacleDistance`).
- At the stop, pick a new `TrafficPoint` whose lane route starts **behind** (U-turn allowed only where the lane
  graph has one), or wait. Never call the navmesh fallback from the traffic controller: add a flag
  `bAllowNavmeshFallback` on the driver (default true for police, false set by the traffic controller on possess).

Test: `MurdarTraffic 6`, 10 min on `PoliceAI_Test_Intersection` → 0 cars off the lanes (log the lane offset every
second; > 500 cm = fail).

## A4: junction reservations

New world subsystem, game thread, 10 Hz prune. It never steers — drivers ask it, then brake themselves.

```cpp
// AI/MurdarJunctionSubsystem.h
UCLASS()
class UMurdarJunctionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Ask to cross junction JunctionId between EnterTime and ExitTime (world seconds). Returns true if granted.
     *  Conflicts: another reservation on an *intersecting* turn lane of the same junction overlapping in time.
     *  Emergency requests win over non-emergency ones that are not yet inside the junction. */
    bool Request(int32 JunctionId, int32 TurnLaneIndex, const AActor* Who, double EnterTime, double ExitTime, bool bEmergency);
    void Release(int32 JunctionId, const AActor* Who);
    /** Called by the driver each frame it is inside the junction, so a stopped car keeps its claim. */
    void MarkInside(int32 JunctionId, const AActor* Who);

private:
    struct FClaim { TWeakObjectPtr<const AActor> Who; int32 Lane; double Enter, Exit; bool bEmergency, bInside; };
    TMap<int32, TArray<FClaim>> Claims;
    bool LanesConflict(int32 JunctionId, int32 LaneA, int32 LaneB) const; // cached per junction
};
```

- `JunctionId` / `TurnLaneIndex`: ADAPT to what `MurdarRoad` already exposes (it knows "the junction it is at" and
  the turn lanes: 12 per junction on the test grid). The zone index in ZoneGraph storage is a stable id per
  junction polygon.
- `LanesConflict`: two turn lanes conflict if their polylines intersect in 2D (or share an exit lane). Compute once
  per junction from the lane polylines, cache as a bitset (12×12 on the grid).
- Tie-break: earlier `EnterTime` wins; equal within 0.5 s → the car coming from the right wins (right-hand
  traffic, TEST_REPORT Phase 2). Make both numbers `UMurdarAISettings` properties.
- **Driver side** (police and traffic): when `DistanceToJunction` < braking distance + margin, request with
  `EnterTime = now + dist/speed`, `ExitTime = EnterTime + junctionLength/speed + margin`. Refused → treat the
  junction entry line as an obstacle (stop before it), re-request every Think. Granted → go; `Release` on exit.
- Police in emergency: `bEmergency = true`. It still must request (spec §10: "nu cu ignorarea completă a lumii");
  a car already `bInside` is never overridden — the police car brakes for it.
- Pursuit exception: while a unit's target is *in* the junction, the unit may follow through on the target's
  claim window (else units stop at every junction behind a fleeing car). Log it in the trace.

## Tests

1. `MurdarTraffic 6`, 10 min: traffic-traffic contacts 0 (was 0.04–0.22 damage per run).
2. Two traffic cars arriving at the same time on crossing lanes → one waits, the right-hand one goes first.
3. Emergency unit + traffic car arriving together → traffic yields; traffic already inside → unit brakes, no contact.
4. Regression: Phase 3 lap time within +10 % (more waiting is expected with traffic), ghost chase gap means within
   the Phase 8 ranges.
5. PVA-T03 (intersection) and PVA-T36 (low vs high traffic density) can be reported after this.
