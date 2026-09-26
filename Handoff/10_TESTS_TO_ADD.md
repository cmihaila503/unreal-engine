# 10 — Tests to add

Harness stays the project's: Python over PIE (`Tools/policetest.py`, `chaserun.ps1`). Each test writes one line
`ID | setup | numbers | PASS/FAIL` that goes straight into `Docs/POLICE_VEHICLE_AI_TEST_REPORT.md`.
Suggest adding a `policetest.py suite <name>` subcommand that runs a list and prints the table (see 11 C9).

## For the changes in this handoff

| Test | File | Pass criterion |
|---|---|---|
| LOOP-01 impossible target, 300 s | 02 | Disabled ≤ 1×, cooldown ≤ `GiveUpCooldownMaxSeconds`, BackupRequired at strike N |
| LOOP-02 target leaves the bad spot | 02 | re-acquired ≤ 0.5 s after moving `GiveUpForgetDistanceCm` |
| BRIBE-01..04 | 03 | as listed in 03 |
| LIFE-01..04 destroy target / primary / officer, PIE stop ×10 | 04 | no crash, no timer warnings, roles clean |
| FPS-01 lap + T04/T05 at 30/60/144 | 05 | columns agree; = PVA-T38 |
| MAGIC-01 regression | 06 | numbers unchanged |
| AGGR-01 heat 40/60/80 | 07 | = PVA-T21 |
| JUNC-01..04 | 08 | as listed in 08 |
| ORDER-01..04 | 09 | as listed in 09 |

## Missing PVA IDs that can be closed without new features

| ID | How |
|---|---|
| PVA-T01 lane following | Re-use the Phase 3 lap; report mean / max lateral offset from the lane centre (the rig already logs lane offset for traffic). Criterion proposal: mean < 50 cm, max < 150 cm outside junctions. |
| PVA-T02 speed control | Same lap with the speed limit set below the cap: time over the limit + 5 km/h < 2 % of the lap; corner entry speed ≤ the arc cap. |
| PVA-T03 intersection | After 08: straight / left / right through a junction with cross traffic, 0 contacts. |
| PVA-T06 negative | Obstacle in lane + an oncoming traffic car inside `OncomingGapSeconds` → no pass, brake, 0 damage; then the oncoming car clears → pass. |
| PVA-T36 traffic density | After 08: `MurdarTraffic 0` vs `MurdarTraffic 12`, ghost chase heat 60; expect lower mean speed, more Follow vs PIT, fewer lane changes with dense traffic. Needs `LaneChangeAggression` (spec §4) to exist to show a *difference*, not just a slowdown. |
| PVA-T37 multi-unit collision avoidance | Ghost chase, 4 units, primary brakes hard (ghost stops) → 0 unit-on-unit contacts, followers stop at ≥ 1 car length. If it fails: gap-analysis A6 fix (police ahead = followed at a time gap, not avoided). |

## Still blocked (later phases)

T27–T28 (Phase 12), T29–T32 (Phase 11, needs the suspect driver — 11 C1), T39–T40 (Phase 13).
