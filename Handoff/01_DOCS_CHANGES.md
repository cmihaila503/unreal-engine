# 01 — Docs changes

The simplest path: copy these files from this repo over the project's `Docs/` (they are the project's own Docs
with the edits below applied; no other content changed):

- `Docs/POLICE_VEHICLE_AI_ARCHITECTURE.md`
- `Docs/POLICE_VEHICLE_AI_RECON.md`
- `Docs/POLICE_VEHICLE_AI_EDITOR_TASKS.md`
- `Docs/POLICE_VEHICLE_AI_GAP_ANALYSIS.md` (new)

If the project's copies changed since 2026-09-26, apply `Handoff/docs.patch` instead (`git apply --3way
Handoff/docs.patch` from the project root) and resolve by hand.

## What changed and why

| File | Section | Change | Reason |
|---|---|---|---|
| ARCHITECTURE | §3 "Planned" paragraph | Split into *Arrived* (Recovery, Disabled, Searching) and *Still planned* (Responding, Returning, the four Phase 12 handoff states). Positioning/Roadblocking recorded as a deviation: they run as tactics, not states. | The text listed Searching as planned and as arrived in the same paragraph. |
| ARCHITECTURE | §4 modes | "Tactics planned on top (Phase 9)" replaced by what Phase 9 actually delivered (SideSweep/BoxRear modes, SetChaseSlot, roadblock = DriveTo+hold, lane following through RouteAim). | Stale since Phase 9. |
| ARCHITECTURE | §7 interfaces | `ReceiveOrder`: only the role half is reported delivered (Phase 8); the order half is Phase 10. Marked "confirm in code". | The doc said "to be added in Phase 8". |
| RECON | §2.4 | Note: police car measured 0.73–0.92 g, `LateralGripCms` is 850, not 750; the 1.1 g figure is not the police car. | Test report "Live recordings" contradicts it. |
| RECON | §6 | Lane A* item moot (own Dijkstra since Phase 3); sleeping-car item resolved in Phase 1. Remaining: SignificanceManager, N-car perf, ADAS cruise. | The doc said "remaining unknowns are the last four". |
| EDITOR_TASKS | Road network by hand | Status line; step 1 (enable ZoneGraph) struck as done; StraightRoad map still missing; lane tags unconfirmed. | Stale since Phase 2. |

**Verify before committing:** the ARCHITECTURE §7 claim that `ReceiveOrder` does not exist — `grep -r ReceiveOrder
Source/`. If it exists, revert that sentence.
