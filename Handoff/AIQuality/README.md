# AI-ul care nu se mai blochează (AIQuality) — handoff for local Claude Code

## Pe scurt (română)

Am citit tot codul AI din proiect (trafic, intersecții, pietoni, NPC cu arme, populație, poliție) și am căutat
**de ce se blochează**. Nu e „AI prost” în general, sunt câteva găuri precise:

| # | Ce vezi în joc | De ce se întâmplă (în cod) |
|---|---|---|
| 1 | Mașinile stau la nesfârșit în spatele unui om oprit pe stradă, al unui cadavru sau al tău, pe jos | doar **mașinile** sunt considerate obstacole ocolibile; oamenii nu (`bLeaderIsObstacle` exclude `ACharacter`) |
| 2 | O intersecție se blochează de tot | o mașină „angajată” în intersecție care apoi **se oprește acolo** (coadă, avariată, ceartă cu tine) ține toate direcțiile care o intersectează pe roșu, pentru totdeauna (`MayEnter` → `IsCommitted`) |
| 3 | Traficul stă la o intersecție fără motiv | o **mașină de poliție parcată cu girofarul pornit** în apropiere oprește intersecția pentru totdeauna (`NonTrafficClear`) |
| 4 | O mașină avariată lângă o intersecție blochează banda | ocolirea este interzisă la mai puțin de 25 m de intersecție, deci mașinile din spate așteaptă la nesfârșit |
| 5 | Pietonii și NPC-urile se freacă de pereți și stau pe loc | pe jos **nu există nicio detecție de blocaj**; la o margine renunță la țintă și o primesc înapoi la fiecare 0,2 s |
| 6 | Mașini răsturnate sau blocate lângă tine rămân acolo | populația șterge doar ce e **departe**; ce e blocat aproape, dar nu se vede, rămâne |
| 7 | Poliția dispare treptat de pe străzi | o mașină de poliție `Disabled` (avariată) nu e ștearsă niciodată (se șterg doar cele din `Patrol`) și ocupă locul patrulelor noi |
| 8 | Pistolarii stau în câmp deschis și se mișcă doar lateral | nu există adăpost; ascunzătoarea poate ieși într-un perete sau peste drum, iar fuga e 15 m în linie dreaptă |
| 9 | FPS mic când e mult trafic | fiecare mașină trece de 3 ori, la fiecare decizie, prin **toate** mașinile și **toți** oamenii din lume (O(N²)); de-asta densitatea a rămas mică (8 mașini, 24 de oameni) |

**Ce aduce handoff-ul:**

- **Un „ceas de blocaj”** pentru fiecare mașină, pieton sau pistolar. Când cineva vrea să se miște și nu se
  mișcă, urcă pe 3 trepte:
  1. **la 4 s:** o ajustare mică (dă cu spatele spre bandă, un pas lateral);
  2. **la 12 s:** trece peste regula care îl ține (ocolește, forțează un drum pe navmesh, intră în intersecția
     blocată dacă nimeni nu trece prin ea);
  3. **la 30 s:** renunță; dacă nu-l vezi, dispare și populația pune altul în altă parte.

  Semaforul roșu și coada normală nu contează ca blocaj, decât dacă durează peste 90 s.
- **Reparații la intersecții:** o mașină oprită în intersecție nu mai rezervă intersecția. Girofarul contează doar
  dacă poliția **vine** spre intersecție.
- **Ocolirea oamenilor:** claxon, apoi ocolire după 5 s pentru un pieton, 8 s pentru tine pe jos și 1,5 s pentru
  un cadavru.
- **Adăpost pentru pistolari**, mai bun decât în GTA SA: aleg un loc acoperit pe navmesh, **ies să tragă o rafală
  și se ascund iar**, nu se adună doi în același loc, iar dacă îi flanchezi își schimbă poziția. Fuga și ascunderea
  aleg locuri reale, accesibile, nu pe carosabil.
- **Pietonii se feresc unii de alții și de tine** (țin dreapta).
- **Un index spațial.** În loc să caute prin toată lumea, fiecare întreabă doar „cine e lângă mine”. După ce
  măsurăm, putem **crește densitatea** (de exemplu 16 mașini și 40 de oameni).
- **Testul de anduranță („soak”):** `Murdar.AI.Soak 600 tour`. Jocul rulează singur 10 minute, te teleportează prin
  oraș, numără fiecare blocaj și scrie un fișier CSV plus rezultatul PASS/FAIL. Asta arată **cu cifre** dacă
  AI-ul s-a îmbunătățit, nu din impresie. Rulează-l **înainte** și **după** modificări.

Fiecare reparație are un comutator separat în Project Settings > Game > Murdar AI Quality, ca să poți compara.

**Verified here:** `AIQualityRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 10 test groups / **101 checks
pass**. **Not compiled:** the Unreal files. Every patch below was written against the real source
(`UTrafficDriverComponent::Think/FindLeader/UpdateJunction/NonTrafficClear/OppositeClear/DriveTick`,
`UTrafficSubsystem::MayEnter`, `AMurdarNPCAIController::Think/UpdateMovement/RouteTo/FindHidingSpot`,
`UPedestrianComponent`, `UPopulationSubsystem::CleanupFarActors`). Uncertain engine calls are marked `// ADAPT:`.
Supersedes `Handoff/AILod` item 8 (FindLeader O(N²)).

## Paste this prompt into local Claude Code

```
Read Handoff/AIQuality/README.md fully. The editor is connected through MCP (see §Assets rule in
Handoff/00_README_LOCAL_CLAUDE.md). One step at a time; build (editor closed) and report in Romanian after each:

0. Run the unit test (README §Tests). Copy Handoff/AIQuality/Source/Murdar_GameDev/AI/Quality/* into
   Source/Murdar_GameDev/AI/Quality/. Build. Nothing calls them yet - the game must behave exactly as before.
1. BEFORE numbers: PIE on the traffic test map, `Murdar.AI.Soak 600 tour` (switches in Project Settings > Murdar AI
   Quality do not matter yet - nothing reads them). Also `stat unit` in a busy street for 30 s (Game ms). Write both
   into Docs/AI_SOAK_REPORT.md (BEFORE). Keep the CSV. NOTE: before step 2 there are no watchdog reports, so the BEFORE
   soak counts only what step 2 starts counting - do step 2, set every switch except bWatchdog OFF, soak again: that is
   the real BEFORE.
2. Patches T1 (spatial index) + T6 (watchdog in traffic) + F1 (foot watchdog). Build. Soak with only bWatchdog +
   bSpatialIndex on = BEFORE. Stat unit again (the spatial index alone should lower Game ms with traffic).
3. Patches T2-T5, T7, J1, J2 (traffic + junctions). Build. Soak = AFTER-traffic. Run README §In-game tests TRF-*.
4. Patches F2-F4 (on foot: passing, flee/hide, cover). Build. Tests FOOT-*, COV-*.
5. Patches P1 (population + police cleanup). Build. Soak = AFTER. Tests POP-*.
6. Only if Game ms has room (< 10 ms with 8 cars / 24 people): step the density up (README §Density), soak and
   stat unit at each step, stop at the step before FPS drops under target.
7. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT with the soak summaries (BEFORE/AFTER lines) and the CSV paths.
Never disable a failing check to get PASS; a FAIL row in the CSV is a location to look at (Murdar.AI.StuckDraw 1,
Murdar.Traffic.Debug 1 at that spot).
```

## Files

| File | What |
|---|---|
| `AI/Quality/AIQualityRules.h` | pure rules: watchdog, junction fixes, obstacle policy, reverse steer, foot fixes, passing, spot scoring, cover cycle, recycle policy, spatial hash, soak stats |
| `AI/Quality/AIQualitySettings.h` | Project Settings > Game > Murdar AI Quality — one switch per fix + all tuning |
| `AI/Quality/AISpatialIndex.h/.cpp` | `UTickableWorldSubsystem`: one pass over the world every 0.1 s, grid queries for cars / people |
| `AI/Quality/AIWatchdogSubsystem.h/.cpp` | stuck register, recycling, gridlock detection, soak test + console commands |
| `AI/Quality/CoverSubsystem.h/.cpp` | cover / hide / flee spots on the navmesh, claims, per-second budget |

Build.cs: `NavigationSystem` and `ZoneGraph` are already dependencies (the AI uses both) — check; nothing new.

Console:
- `Murdar.AI.Soak [seconds=600] [tour]` / `Murdar.AI.Soak stop` → `Saved/Logs/AISoak_<time>.csv` + PASS/FAIL line.
- `Murdar.AI.Stuck` → who is stuck now, why, at which junction.
- `Murdar.AI.StuckDraw 1` → yellow / orange / red spheres over level 1 / 2 / 3 agents.

## Patches — traffic (`AI/TrafficDriverComponent.h/.cpp`)

### T1. Spatial index instead of three world scans
In `FindLeader`, `NonTrafficClear`, `OppositeClear` replace each
`for (TActorIterator<AMurdarVehicle> It(GetWorld()); It; ++It)` / `for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)`
with a query of the index (radius = what that function already filters by), falling back to the old loop when the
index is off:
```cpp
// top of the .cpp
#include "AI/Quality/AISpatialIndex.h"
#include "AI/Quality/AIQualitySettings.h"
namespace
{
	/** Cars within R of Where: the index when it is on, otherwise every car (the old behaviour). */
	void NearVehicles(const UObject* Ctx, UWorld* World, const FVector& Where, float R, TArray<AMurdarVehicle*>& Out)
	{
		const UAIQualitySettings* S = UAIQualitySettings::Get();
		if (UAISpatialIndex* Idx = S && S->bSpatialIndex ? UAISpatialIndex::Get(Ctx) : nullptr) { Idx->QueryVehicles(Where, R, Out); return; }
		Out.Reset();
		for (TActorIterator<AMurdarVehicle> It(World); It; ++It) { Out.Add(*It); }
	}
	void NearCharacters(const UObject* Ctx, UWorld* World, const FVector& Where, float R, TArray<ACharacter*>& Out)
	{
		const UAIQualitySettings* S = UAIQualitySettings::Get();
		if (UAISpatialIndex* Idx = S && S->bSpatialIndex ? UAISpatialIndex::Get(Ctx) : nullptr) { Idx->QueryCharacters(Where, R, Out); return; }
		Out.Reset();
		for (TActorIterator<ACharacter> It(World); It; ++It) { if (!It->IsHidden() && !It->GetAttachParentActor()) { Out.Add(*It); } }
	}
}
```
- `FindLeader`: radius `LookAheadCm + 1000.f` round `Here` (it already rejects farther ones).
- `NonTrafficClear`: radius `SirenJunctionCm + 2000.f` round `J.Center`.
- `OppositeClear`: round `PointAt((FromS + ToS) * 0.5f)`, radius `(ToS - FromS) * 0.5f + 4000.f`.
Keep every check inside the loops as it is (they read live positions). Note `OppositeClear`'s character loop only
checked `IsHidden`; the index also drops seated people — correct (a passenger is not in the other lane).

### T2. Who is standing ahead, and what to do about it
Add members (`.h`, private):
```cpp
MurdarAIQ::EObstacleKind LeaderKind = MurdarAIQ::EObstacleKind::Queue;
MurdarAIQ::FWatchdog Watch;
int32 WatchLevel = 0;
float StillSince = -1.f;      // when we stopped (speed < 50 cm/s); -1 moving
float FlippedSince = -1.f;
```
Public: `int32 GetWatchLevel() const { return WatchLevel; }` and
`float GetStillSeconds() const { return StillSince < 0.f || !GetWorld() ? 0.f : GetWorld()->GetTimeSeconds() - StillSince; }`.

In `Think`, replace the line computing `bLeaderIsObstacle` with:
```cpp
// What stands ahead: a queue (a traffic car driving), a car nobody drives / stranded, a person, the player, a body.
LeaderKind = MurdarAIQ::EObstacleKind::Queue;
int32 LeaderLevel = 0;
if (bLeader && Leader)
{
	if (const ACharacter* Ch = Cast<ACharacter>(Leader))
	{
		const AMurdarCharacter* MC = Cast<AMurdarCharacter>(Ch);
		const UHealthComponent* HP = Ch->FindComponentByClass<UHealthComponent>();
		if ((MC && MC->IsRagdoll()) || (HP && HP->IsDead())) { LeaderKind = MurdarAIQ::EObstacleKind::Body; }
		else if (Ch == UGameplayStatics::GetPlayerPawn(this, 0)) { LeaderKind = MurdarAIQ::EObstacleKind::Player; }
		else { LeaderKind = MurdarAIQ::EObstacleKind::Person; }
	}
	else if (!LeaderDriver || !LeaderDriver->IsDriving() || LeaderDriver->IsObstacle()) { LeaderKind = MurdarAIQ::EObstacleKind::Car; }
	else { LeaderLevel = LeaderDriver->GetWatchLevel(); }
}
const UAIQualitySettings* QS = UAIQualitySettings::Get();
const bool bNearJunction = JunctionZone != INDEX_NONE && (bInsideJunction || DistToLineCm < 2500.f);
const float StillFor = LeaderStillSince >= 0.f ? Now - LeaderStillSince : 0.f;
MurdarAIQ::EObstacleAction Action = MurdarAIQ::EObstacleAction::Follow;
if (bLeader)
{
	Action = MurdarAIQ::ObstacleAction(LeaderKind, StillFor, bNearJunction, LeaderLevel, QS ? QS->Obstacles(BypassAfterSeconds) : MurdarAIQ::FObstacleTuning());
	// Switched off: people are never driven round (the old behaviour).
	if (QS && !QS->bDriveRoundPeople && LeaderKind != MurdarAIQ::EObstacleKind::Car) { Action = MurdarAIQ::EObstacleAction::Follow; }
}
const bool bLeaderIsObstacle = bLeader && LeaderKind != MurdarAIQ::EObstacleKind::Queue;
```
`JunctionZone` / `bInsideJunction` / `DistToLineCm` here are from the previous think (`UpdateJunction()` runs just
below and reads the leader fields set above, so leave the order as it is): 0.1 s old, fine for a 25 m test.
Includes: `"AI/Quality/AIQualitySettings.h"`, `"AI/Quality/AIWatchdogSubsystem.h"`, `"Health/HealthComponent.h"`.

Then the bypass start condition (3b) becomes:
```cpp
if (!bBypassing && Action == MurdarAIQ::EObstacleAction::DriveRound && LeaderGapCm < 2000.f && !bInsideJunction)
```
(the rest of 3b unchanged; for a person the obstacle front is `ObstRear + 80.f` instead of the 200 fallback:
`(ObstCar ? 2.f * ObstCar->GetBodyHalfExtents().X : (Cast<ACharacter>(Leader) ? 80.f : 200.f))`).
`S0` keeps using `bLeaderIsObstacle`. `MaybeHonk` already honks at a standing person / car — unchanged.

### T3. Reverse towards the lane, not straight back
`DriveTick`, the `ReverseUntil > Now` branch — replace the steer line:
```cpp
FVector LaneDir;
PointAt(CarS, &LaneDir);
const float HeadingErr = SignedTurnDeg(V->GetActorForwardVector().GetSafeNormal2D(), LaneDir);
SmoothedSteer = FMath::FInterpTo(SmoothedSteer, MurdarAIQ::ReverseSteer(CarLateral, HeadingErr), DeltaTime, 4.f);
```
(Sign check: + steer = right, `CarLateral` + = right of the lane, as in `Project`; reversing with the wheel right
swings the nose left, back towards a lane on our left. Test TRF-06 confirms on the car.)

### T4. Flipped = stranded
`Think`, next to the vertical "stranded" check:
```cpp
const FRotator Rot = V->GetActorRotation();
if (MurdarAIQ::IsFlipped(Rot.Roll, Rot.Pitch)) { if (FlippedSince < 0.f) { FlippedSince = Now; } } else { FlippedSince = -1.f; }
if (FlippedSince >= 0.f && Now - FlippedSince > (QS ? QS->FlippedSeconds : 4.f)) { bStranded = true; return; }
```
(declare `QS` at the top of `Think` instead of inside T2's block).

### T5. A car that stops being traffic releases its junction
`Think` returns early for `bConfront`, `KTurnPhase > 0 || bStranded || bHoldHere` — its junction fields then stay as
they were (`bCommitted = true` for ever). Before both early returns:
```cpp
if (bConfront || bStranded || bHoldHere) { JunctionZone = INDEX_NONE; MovementLane = INDEX_NONE; bCommitted = false; WaitingSince = -1.f; }
```
(`MayEnter` then no longer sees it; if it stands physically on someone's path, their `FindLeader` stops them, and T2
drives round it.)

### T6. The watchdog
New private `void UpdateWatch(float Now);`, called in `TickComponent` right after `Think()` (inside the
`Now >= NextThink` block):
```cpp
void UTrafficDriverComponent::UpdateWatch(float Now)
{
	const UAIQualitySettings* QS = UAIQualitySettings::Get();
	const AMurdarVehicle* V = Vehicle();
	if (!QS || !QS->bWatchdog || !V) { WatchLevel = 0; return; }
	const float Vcms = GetSpeedCms();
	if (FMath::Abs(Vcms) < 50.f) { if (StillSince < 0.f) { StillSince = Now; } } else { StillSince = -1.f; }
	// Wants to move: driving, not held / in a row / stranded. A K-turn wants to move too (it has progress).
	const bool bWants = bDriving && !bHoldHere && !bConfront && !bStranded;
	// Legitimate waits: a red or amber light; a queue whose head is not stuck. A priority wait is NOT legitimate: it
	// is seconds long when the rules work, and when they do not the watchdog is what breaks the tie.
	const bool bRed = JunctionWhy.Contains(TEXT("red")) || JunctionWhy.Contains(TEXT("amber"));
	const bool bQueue = LeaderGapCm >= 0.f && LeaderKind == MurdarAIQ::EObstacleKind::Queue;
	bool bUp = false;
	const FVector L = V->GetActorLocation();
	WatchLevel = Watch.Update(Now, L.X, L.Y, bWants, bRed || bQueue, QS->Watch(), bUp);
	// Level 3 is reported, not acted on here: the watchdog subsystem recycles the car when nobody sees it. It is NOT
	// made stranded - stranded is for good, and a car held up by the player standing in a busy road must drive on
	// the moment he steps aside.
	if (UAIWatchdogSubsystem* WD = UAIWatchdogSubsystem::Get(this))
	{
		const FString Why = LeaderGapCm >= 0.f ? LeaderWho : JunctionWhy;
		WD->Report(GetOwner(), MurdarAIQ::EAgent::Traffic, WatchLevel, bUp, bStranded, Why, IsWaitingAtLine() || bInsideJunction ? JunctionZone : INDEX_NONE);
	}
}
```
Order note: T5 clears `JunctionZone` for stranded cars, so a stranded car reports no junction — gridlock detection
counts the cars *waiting* at a junction, which is what a gridlock is.
Also `StartDriving` must reset the watchdog: `Watch = MurdarAIQ::FWatchdog(); WatchLevel = 0; StillSince = FlippedSince = -1.f;`.
`EndPlay`: `if (UAIWatchdogSubsystem* WD = UAIWatchdogSubsystem::Get(this)) { WD->Forget(GetOwner()); }`.

### T7. Level 2 at a junction line: go when nobody is going through
`UpdateJunction`, after the four `bGo` checks and before `if (bGo)`:
```cpp
if (!bGo && Sig != ETrafficSignal::Red)
{
	const UAIQualitySettings* QS = UAIQualitySettings::Get();
	bool bCrossingMoving = false;
	for (const TWeakObjectPtr<UTrafficDriverComponent>& W : TS->GetDrivers())
	{
		const UTrafficDriverComponent* O = W.Get();
		if (!O || O == this || O->GetJunctionZone() != J->Zone) { continue; }
		const int32 Ov = J->IndexOf(O->GetMovementLane());
		if (Ov != INDEX_NONE && J->Conflicts(Mv, Ov) && O->GetSpeedCms() > 150.f) { bCrossingMoving = true; break; }
	}
	if (QS && QS->bJunctionFixes && MurdarAIQ::ForceJunction(WatchLevel, false, bCrossingMoving) && NonTrafficClear(*J, Mv, Why))
	{
		bGo = true;
		UE_LOG(LogTemp, Log, TEXT("%s: junction %d deadlocked (%s), going"), *GetOwner()->GetName(), J->Zone, *Why);
	}
}
```
(The physical checks still apply: `FindLeader` stops us behind anything actually in the box.)

## Patches — junctions (`AI/TrafficSubsystem.cpp`, `AI/TrafficDriverComponent.cpp`)

### J1. A stopped committed car no longer reserves the box — `UTrafficSubsystem::MayEnter`
Replace
```cpp
if (O->IsCommitted())
{
	OutWhy = FString::Printf(TEXT("%s is going through"), *Name);
	return false;
}
```
with
```cpp
if (O->IsCommitted())
{
	const UAIQualitySettings* QS = UAIQualitySettings::Get();
	MurdarAIQ::FJunctionOther Other;
	Other.bCommitted = true;
	Other.bObstacle = O->IsObstacle();
	Other.SpeedCms = FMath::Abs(O->GetSpeedCms());
	Other.StillSeconds = O->GetStillSeconds();
	Other.WatchLevel = O->GetWatchLevel();
	if (!QS || !QS->bJunctionFixes || MurdarAIQ::CommittedBlocksEntry(Other, QS->CommittedStillSeconds))
	{
		OutWhy = FString::Printf(TEXT("%s is going through"), *Name);
		return false;
	}
	continue; // stopped in the box: the leader check stops us if it is on our path
}
```

### J2. Only a siren that is coming closes the junction — `UTrafficDriverComponent::NonTrafficClear`
Replace `if (const UPoliceEmergencyComponent* E = ...; E && E->IsSirenOn() && ToCentre < SirenJunctionCm)` with
```cpp
if (const UPoliceEmergencyComponent* E = O->FindComponentByClass<UPoliceEmergencyComponent>(); E && E->IsSirenOn())
{
	const UAIQualitySettings* QS = UAIQualitySettings::Get();
	const FVector SV = O->GetVelocity();
	const float Closing = FVector::DotProduct(SV, (J.Center - L).GetSafeNormal2D());
	const bool bHolds = (QS && QS->bJunctionFixes) ? MurdarAIQ::SirenHoldsJunction(ToCentre, SV.Size2D(), Closing, SirenJunctionCm) : ToCentre < SirenJunctionCm;
	if (bHolds)
	{
		OutWhy = FString::Printf(TEXT("siren: %s"), *O->GetName());
		return false;
	}
}
```
(the rest of the loop - cross traffic - unchanged; a parked siren car is then just a standing car: skipped below
`150 cm/s` as before.)

## Patches — on foot (`AI/MurdarNPCAIController.h/.cpp`, `AI/PedestrianComponent.h/.cpp`)

### F1. The foot watchdog
Members (`MurdarNPCAIController.h`, private):
```cpp
MurdarAIQ::FWatchdog FootWatch;
int32 FootLevel = 0;
int32 SidestepAttempt = 0;
float SidestepUntil = -1.f;
FVector SidestepDir = FVector::ZeroVector;
float ForcePathUntil = -1.f;
```
At the end of `Think()` (after the per-state switch, before `SetActorTickEnabled`):
```cpp
if (const UAIQualitySettings* QS = UAIQualitySettings::Get(); QS && QS->bWatchdog)
{
	const AMurdarCharacter* C = Character();
	const bool bWants = bHasMoveGoal && !(C && C->IsRagdoll());
	bool bUp = false;
	FootLevel = FootWatch.Update(Now, MyLoc.X, MyLoc.Y, bWants, false, QS->Watch(), bUp);
	if (bUp)
	{
		switch (MurdarAIQ::FootFix(FootLevel))
		{
		case MurdarAIQ::EFootFix::Sidestep:
		{
			const FVector Fwd = (MoveGoal - MyLoc).GetSafeNormal2D();
			SidestepDir = FVector(-Fwd.Y, Fwd.X, 0.f) * static_cast<float>(MurdarAIQ::SidestepSide(SidestepAttempt++));
			SidestepUntil = Now + 0.8f;
			break;
		}
		case MurdarAIQ::EFootFix::ForcePath:
			ForcePathUntil = Now + 8.f;
			RouteTime = -100.f;
			break;
		case MurdarAIQ::EFootFix::NewGoal:
			if (Ambient) { Ambient->Unstick(); }
			bHasMoveGoal = false;
			HideHops = 0;
			ReleaseCover(); // F4
			break;
		default: break;
		}
	}
	const MurdarAIQ::EAgent Kind = Faction == ENPCFaction::Civilian ? MurdarAIQ::EAgent::Pedestrian : Faction == ENPCFaction::Police ? MurdarAIQ::EAgent::Police : MurdarAIQ::EAgent::Gunman;
	if (UAIWatchdogSubsystem* WD = UAIWatchdogSubsystem::Get(this)) { WD->Report(GetPawn(), Kind, FootLevel, bUp, false, UEnum::GetValueAsString(State)); }
}
```
Report the **pawn**: the recycler destroys the
pawn and its controller.

`UpdateMovement`, right after the "no steering in the air" return:
```cpp
if (SidestepUntil > GetWorld()->GetTimeSeconds()) { Me->AddMovementInput(SidestepDir, 1.f); return; }
```
`RouteTo`: after the 5 Hz line check, `if (ForcePathUntil > Now) { bRouteBlocked = true; }` (walk the navmesh path even
if the sweep said clear — the sweep ignores pawns and thin props; a man leaning on a bollard sees a clear line).

`UPedestrianComponent` — public:
```cpp
/** The controller's watchdog gave up on the current walk: drop it, pick the pavement again, turn round. */
void Unstick()
{
	Sidewalk = INDEX_NONE;
	Dir = -Dir;
	SetMode(EPedestrianMode::Stroll, GetWorld()->GetTimeSeconds());
}
```
(inline in the header, or in the .cpp.) For `WaitCross` stuck on a crossing that never clears, the existing 45 s
turn-back already works; the watchdog does not fire there (the person stands: `bHasMoveGoal` is false).

### F2. Keep right, step round people and the player
`UpdateMovement`, after `Dir` is computed from the route and before the knee probe:
```cpp
if (const UAIQualitySettings* QS = UAIQualitySettings::Get(); QS && QS->bFootPassing && State != ENPCState::Engage)
{
	TArray<ACharacter*> Near;
	if (UAISpatialIndex* Idx = UAISpatialIndex::Get(this)) { Idx->QueryCharacters(Me->GetActorLocation(), 450.f, Near); }
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	float Shift = 0.f;
	for (const ACharacter* O : Near)
	{
		if (O == Me) { continue; }
		const FVector Rel = O->GetActorLocation() - Me->GetActorLocation();
		const FVector OV = O->GetVelocity().GetSafeNormal2D();
		const float OtherDot = OV.IsNearlyZero() ? 0.f : FVector::DotProduct(OV, Dir);
		const bool bYields = Cast<AMurdarNPCAIController>(O->GetController()) != nullptr; // the player does not
		const float S = MurdarAIQ::PassOffset(FVector::DotProduct(Rel, Dir), FVector::DotProduct(Rel, Right), OtherDot, bYields);
		if (FMath::Abs(S) > FMath::Abs(Shift)) { Shift = S; }
	}
	if (Shift != 0.f) { Dir = (Dir * 150.f + Right * Shift).GetSafeNormal2D(); }
}
```
(Option, measured separately: `UCharacterMovementComponent::bUseRVOAvoidance = true` on civilians in `OnPossess`,
`AvoidanceConsiderationRadius = 250`. ADAPT: RVO applies to AI-driven `AddMovementInput` movement in this engine
version — verify with FOOT-02 before keeping it; the rule above does not depend on it.)

### F3. Flee and hide on the navmesh
`FindHidingSpot()` — at its top:
```cpp
if (const UAIQualitySettings* QS = UAIQualitySettings::Get(); QS && QS->bCover)
{
	FVector Spot, Peek;
	bool bNone = false;
	if (UCoverSubsystem* CS = UCoverSubsystem::Get(this); CS && CS->FindSpot(ESpotPurpose::Hide, GetPawn(), Target.Get(), PreferredRange, Spot, Peek, bNone)) { return Spot; }
	if (!bNone) { return FVector::ZeroVector; } // budget spent this second: ask again next think (Hide re-asks while !bHasMoveGoal)
}
// ...the old ring below stays as the fallback
```
`Think`, `case ENPCState::Flee`: replace the straight 15 m with
```cpp
if (T && (!bHasMoveGoal || FVector::Dist2D(MyLoc, MoveGoal) < 200.f))
{
	FVector Spot, Peek;
	bool bNone = false;
	UCoverSubsystem* CS = UCoverSubsystem::Get(this);
	const UAIQualitySettings* QS = UAIQualitySettings::Get();
	if (QS && QS->bCover && CS && CS->FindSpot(ESpotPurpose::Flee, GetPawn(), T, PreferredRange, Spot, Peek, bNone)) { MoveGoal = Spot; bHasMoveGoal = true; }
	else if (!QS || !QS->bCover || bNone)
	{
		const FVector Away = (MyLoc - T->GetActorLocation()).GetSafeNormal2D();
		MoveGoal = MyLoc + Away * 1500.f; bHasMoveGoal = true;
	}
}
```

### F4. Cover in a fight
Members:
```cpp
MurdarAIQ::FCoverCycle CoverCycle;
FVector CoverSpot = FVector::ZeroVector, PeekSpot = FVector::ZeroVector;
bool bHasCover = false;
bool bCoverHold = false;      // hidden: no shooting
float NextCoverSearch = 0.f;
void ReleaseCover();
```
```cpp
void AMurdarNPCAIController::ReleaseCover()
{
	if (bHasCover) { if (UCoverSubsystem* CS = UCoverSubsystem::Get(this)) { CS->Release(this); } }
	bHasCover = false;
	bCoverHold = false;
}
```
`case ENPCState::Engage:` — before the existing `if (!bTargetInSight)` chain:
```cpp
const UAIQualitySettings* QS = UAIQualitySettings::Get();
UCoverSubsystem* CS = UCoverSubsystem::Get(this);
if (QS && QS->bCover && CS && bTargetInSight && !bApprehend)
{
	if (!bHasCover && Now >= NextCoverSearch)
	{
		bool bNone = false;
		if (CS->FindSpot(ESpotPurpose::Cover, GetPawn(), T, PreferredRange, CoverSpot, PeekSpot, bNone))
		{
			bHasCover = true;
			CS->Claim(this, CoverSpot);
			CoverCycle.Start(Now);
		}
		else if (bNone) { NextCoverSearch = Now + 4.f; } // nothing here: strafe as before for a while
	}
	if (bHasCover)
	{
		const UWeaponComponent* W = Weapons();
		const AMurdarWeapon* Gun = W ? W->GetEquippedWeapon() : nullptr;
		const bool bReloading = Gun && Gun->IsReloading();
		const bool bAt = FVector::Dist2D(MyLoc, CoverCycle.Phase == MurdarAIQ::ECoverPhase::Peek ? PeekSpot : CoverSpot) < 120.f;
		const bool bSeen = CoverCycle.Phase == MurdarAIQ::ECoverPhase::Hidden && CS->IsExposed(GetPawn(), T);
		const MurdarAIQ::ECoverPhase Ph = CoverCycle.Update(Now, bAt, bSeen, bReloading,
			FMath::FRandRange(PauseSeconds.X, PauseSeconds.Y), FMath::FRandRange(BurstSeconds.X, BurstSeconds.Y) + 0.4f, QS->MaxStaySeconds);
		if (Ph == MurdarAIQ::ECoverPhase::Leave) { ReleaseCover(); NextCoverSearch = Now; }
		else
		{
			MoveGoal = Ph == MurdarAIQ::ECoverPhase::Peek ? PeekSpot : CoverSpot;
			bHasMoveGoal = FVector::Dist2D(MyLoc, MoveGoal) > 60.f;
			bCoverHold = Ph == MurdarAIQ::ECoverPhase::Hidden;
			break; // cover decides the movement; the strafing below is the no-cover fallback
		}
	}
}
```
(`FRandRange` inside `Update` each think re-rolls the thresholds; that is intended jitter — a hide of 0.5–1.4 s.)
`UpdateFire`: `const bool bCanShoot = bTargetInSight && bReady && Dist < MaxEngageRange && !bCoverHold;`
`UpdateMovement`: the walk scale — at a cover or peek spot within 300 cm, walk (`0.5f`) as it already does in Engage.
`EnterState`: `if (New != ENPCState::Engage && New != ENPCState::Reload) { ReleaseCover(); }`.
`Reload` state: when `bHasCover`, stay at `CoverSpot` (`MoveGoal = CoverSpot`) instead of backing off 3 m.
`EndPlay` / `OnUnPossess`: `ReleaseCover();`.

## Patches — population and police (`AI/PopulationSubsystem.cpp`)

### P1. Disabled police cars are removed; stuck ones too
`CleanupFarActors`, the police guard:
```cpp
if (Cop && Cop->GetState() != EPoliceState::Patrol) { continue; }
```
becomes
```cpp
if (Cop)
{
	const EPoliceState PS = Cop->GetState();
	const bool bFree = PS == EPoliceState::Patrol || PS == EPoliceState::Disabled || PS == EPoliceState::StandDown;
	if (!bFree) { continue; } // part of a chase the player is in
}
```
Without this a wrecked unit far away is kept for ever and counts against `MaxPoliceCars`: after a few chases no new
patrol spawns. (Stuck-but-near recycling is the watchdog subsystem's; nothing else to add here.)

### P2. Police cars report to the watchdog
`AMurdarPoliceAIController::Think`, at its end:
```cpp
if (UAIWatchdogSubsystem* WD = UAIWatchdogSubsystem::Get(this))
{
	const int32 L = State == EPoliceState::Disabled ? 3 : State == EPoliceState::Recovery ? 1 : 0;
	WD->Report(GetPawn(), MurdarAIQ::EAgent::Police, L, L != LastReportedLevel && L > LastReportedLevel, State == EPoliceState::Disabled, PendingReason);
	LastReportedLevel = L;
}
```
(member `int32 LastReportedLevel = 0;`). Police already has its own recovery (Phase 5); this only counts it in the soak
and lets a disabled unit that nobody sees be recycled (`Recycle` never touches a unit in a chase).

## Density (step 6)

`DefaultGame.ini` → `[/Script/Murdar_GameDev.MurdarAISettings]` (Project Settings > Murdar AI). Steps, each with a
soak and 30 s of `stat unit` in the busiest street:

| Step | MaxTrafficCars | MaxPedestrians | MaxParkedCars |
|---|---|---|---|
| now | 8 | 24 | 12 |
| A | 12 | 32 | 16 |
| B | 16 | 40 | 20 |
| C | 20 | 50 | 24 |

Stop at the last step where Game ms stays under the budget (Handoff/AILod) and the soak passes. Write the numbers into
Docs/AI_SOAK_REPORT.md.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/AIQuality/Source/Murdar_GameDev/AI/Quality Handoff/AIQuality/Tests/ai_quality_rules_test.cpp -o aq && ./aq`
→ `101 checks, 0 failed` (also run by `Handoff/Automation/Tools/run_rule_tests.py`).

Soak: `Murdar.AI.Soak 600 tour` BEFORE and AFTER; PASS = **0 gridlocks** and ≤ 2 give-ups (level 3) per kind per
10 minutes. Paste both summary lines into Docs/AI_SOAK_REPORT.md.

| ID | Test | Pass |
|---|---|---|
| TRF-01 | stand in the middle of a lane on foot | the car stops, honks (~1.5 s), drives round you through the other lane after ~8 s when it is clear |
| TRF-02 | kill a pedestrian on the road | cars stop, then drive round the body within ~2 s |
| TRF-03 | a pedestrian stopped in the road (`Murdar.Traffic.Debug 1`) | honk at 2 s, round at 5 s |
| TRF-04 | block a junction exit with your car, hazards on, walk away | queued cars stop; after 10 s near the junction they drive round; nobody waits > 30 s |
| TRF-05 | ram a car so it stops inside a junction (confront) | the crossing movements keep flowing round it (J1, T5) — no gridlock in `Murdar.AI.Stuck` |
| TRF-06 | push a traffic car nose-first into a wall | it backs off **towards** its lane and drives on (T3); no third attempt |
| TRF-07 | flip a traffic car | stranded after 4 s, hazards on; look away → recycled (log "recycled traffic") |
| TRF-08 | police car parked with siren on 30 m from a junction | traffic through that junction keeps moving (J2); a police car *coming* with the siren still clears it |
| TRF-09 | four-way junction without lights, 4 cars arrive together (`Murdar.Traffic.Debug 1`) | resolves within 3 s (existing) or by the level-2 force within 12 s — never stays |
| TRF-10 | `stat unit` with 8 cars / 24 people, index on vs off (`bSpatialIndex`) | Game ms lower with the index on; traffic behaves the same (same honks, same stops) |
| FOOT-01 | walk into a pedestrian head-on on a pavement | he steps to his right before touching you |
| FOOT-02 | two groups of pedestrians crossing on one pavement | they pass without grinding into each other |
| FOOT-03 | push a pedestrian into a corner between a wall and a parked car | sidestep at 4 s, navmesh path at 12 s, gives up (turns round) at 30 s — no one leaning on a wall for minutes |
| FOOT-04 | fire a shot in a street | civilians run along the pavement / into side streets, not into the road or a wall |
| COV-01 | fight 2 armed hostiles in a street with parked cars | each takes cover behind something, peeks to fire bursts, hides to reload — not both behind the same car |
| COV-02 | flank one in cover | he leaves that cover within ~1 s and finds another |
| COV-03 | stand still, keep them pinned | they move cover after ~12 s (the fight moves) |
| COV-04 | fight in an open field (no cover) | they strafe as before (fallback), no errors, search retried every 4 s |
| POP-01 | wreck 3 police cars in a chase, escape, drive 300 m away | the wrecks are removed when unseen; new patrols appear |
| POP-02 | `Murdar.AI.StuckDraw 1` during a soak | markers appear and disappear; red ones near you stay until you look away |
| POP-03 | a stuck traffic car right behind you (not visible) within 15 m | **kept** (never recycled that close) |

## With other handoffs

- **Pooling (26)** integrated: in `UAIWatchdogSubsystem::RecyclePass` release a stuck pedestrian to the pool
  (`UActorPoolSubsystem::ReleasePedestrian`) instead of destroying it (ragdoll / dead ones are still destroyed).
- **AILod (11)**: item 8 (FindLeader O(N²)) is done here by T1. The significance LOD still decides how often each agent
  thinks; the watchdog counts seconds, not thinks, so a slow-thinking far agent escalates on the same clock.
- **Order**: this handoff touches only AI that already exists, so it can go **first** (before 12–27) — it is what the
  player feels most.
