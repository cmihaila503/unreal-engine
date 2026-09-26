# 12 — Tuning log entries (spec §54 format), to fill in with real numbers

Append each to `Docs/POLICE_VEHICLE_AI_TUNING_LOG.md` **only after** the change is built and tested. Replace every
`TBD` with measured values; if a test failed, say so and what was reverted.

---
Date: TBD
Parameter: GiveUpCooldownMaxSeconds, GiveUpForgetDistanceCm, GiveUpForgetSeconds, GiveUpMaxStrikes (new)
Owner: UPoliceDrivingProfile
Old: cooldown ×2 per strike, no ceiling; Disabled-by-timeout → Patrol without memory
New: 240 s ceiling; strikes forgotten after 2500 cm target movement or 600 s; 3 strikes → BackupRequired; Disabled-by-timeout keeps the strike
Reason: gap analysis A1/A2 — Recovery→Disabled→Patrol→Pursuit loop on an unreachable target (spec §53, §58)
Tests: LOOP-01, LOOP-02, PVA-T08/T09/T34/T35
Result: TBD
Side Effects: TBD

---
Date: TBD
Parameter: CrimeSeverity (new, UMurdarAISettings); StandDown interruption rule
Owner: UMurdarAISettings (table), AMurdarPoliceAIController (rule)
Old: StandDown not interruptible ("paid is paid")
New: interrupted by a crime after the bribe that is more severe than the covered one, or committed against this unit
Reason: gap analysis A3 — bribe-then-attack exploit
Tests: BRIBE-01..04
Result: TBD
Side Effects: TBD

---
Date: TBD
Parameter: (no tuning) weak references, timer and bus teardown
Owner: police controller, pursuit component, director, faction memory
Old: TBD (list the raw pointers / missing teardown found by the audit)
New: TWeakObjectPtr, ClearAllTimersForObject, unsubscribe, OnTargetInvalid()
Reason: gap analysis C3
Tests: LIFE-01..04
Result: TBD
Side Effects: TBD

---
Date: TBD
Parameter: WhiskerRateHz (new)
Owner: UVehiclePursuitComponent
Old: whiskers every frame
New: 30 Hz, extrapolated by closing speed between probes, dirty-flag on mode/target/escape change
Reason: gap analysis A7 — FPS-dependent detection and cost (PVA-T38, T40)
Tests: FPS-01 before and after (30/60/144), Phase 3 lap, PVA-T04–T07, ghost chase heat 60/80
Result: TBD (before: TBD / after: TBD)
Side Effects: TBD

---
Date: TBD
Parameter: literals → properties (list from 06), values unchanged
Owner: per 06 table
Old: literals
New: UPROPERTY with Units/ClampMin/ClampMax, same defaults
Reason: spec §1.10–11, gap analysis A9
Tests: MAGIC-01 (Phase 3 lap, ghost chase heat 60, PVA-T35)
Result: TBD (must be unchanged)
Side Effects: none expected

---
Date: TBD
Parameter: HeatToAggression, SeverityAggressionWeight, DamageAggressionWeight, *Aggressive ends — ONE consumer per entry (follow distance first)
Owner: UPoliceDrivingProfile
Old: single Lethal step
New: Aggression01 lerp between calm (existing) and aggressive values
Reason: spec §26, PVA-T21, gap analysis A10
Tests: AGGR-01 (heat 40/60/80), PVA-T22, ghost chase regression
Result: TBD
Side Effects: TBD

---
Date: TBD
Parameter: junction reservations (UMurdarJunctionSubsystem), bAllowNavmeshFallback=false for traffic, right-hand tie-break window
Owner: UMurdarJunctionSubsystem / UMurdarAISettings / AMurdarTrafficAIController
Old: no junction priority; traffic may fall back to the navmesh
New: reservations with conflict matrix, emergency priority except over cars already inside; traffic stops at lane ends
Reason: gap analysis A4/A5, spec §10, §13
Tests: JUNC-01..04, 10-min traffic soak, Phase 3 lap, ghost chase
Result: TBD
Side Effects: TBD (expect longer lap times with traffic)

---
Date: TBD
Parameter: FPoliceOrder / ReceiveOrder; states Responding, Returning; RespondTimeoutSeconds, EmergencyUrgencyThreshold (new)
Owner: UPoliceResponseDirector (orders), AMurdarPoliceAIController (states), UPoliceDrivingProfile (timeouts)
Old: self-dispatch only; Search/Pursuit end → Patrol
New: director orders with refusal; end → Returning → Patrol
Reason: Phase 10 start, spec §23, §29, §36, §40, §51
Tests: ORDER-01..04, Phase 7–9 regression with cheat spawn
Result: TBD
Side Effects: TBD
