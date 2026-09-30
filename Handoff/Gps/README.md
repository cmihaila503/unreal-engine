# GPS, minimap și harta din meniu (Gps) — handoff for local Claude Code

## Pe scurt (română)

Ce ai cerut, ca în GTA:

- **Minimap** în stânga jos, rotit după direcția ta (mergi mereu „în sus”). Se micșorează cu viteza: 90 m în jurul
  tău când stai, 250 m la 110 km/h, ca să vezi intersecția care vine.
- **Harta mare mutată în meniul de pauză.** Tasta **M** deschide direct meniul pe hartă, iar Esc te întoarce. Vechea
  hartă mare din `BP_MapManager` se oprește.
- **Pini pe hartă.** Pe harta din meniu apeși **Enter / click dreapta / A pe controller** și pui **punctul tău**.
  Dacă apeși lângă el, îl scoți. Dispare singur când ajungi.
- **Doi pini cu rute în același timp:**
  - **misiunea (galben)**: obiectivul misiunii din poveste sau etapa jobului care rulează;
  - **punctul tău (mov)**.

  Fiecare are **ruta lui desenată pe drumuri**, pe minimap și pe harta mare. Ruta se recalculează când o părăsești,
  când pinul se mută sau din 20 în 20 de secunde.
- **Rutele merg pe benzile de trafic reale** (routerul pe care îl folosește deja poliția), deci GPS-ul nu te trimite
  pe contrasens sau prin câmp. Pe jos: navmesh-ul, altfel linie dreaptă.
- **Pinul care iese de pe minimap** rămâne pe marginea lui și arată în ce direcție e.

Minimapul e desenat direct (nu e o a doua cameră care filmează de sus), deci **nu costă FPS**. Folosește desenul
hărții din handoff-ul PaperMap: o singură hartă, două vederi. Pentru varianta „hardcore”, minimapul se poate opri
din setări, iar harta cu pini din meniu rămâne.

**Atenție:** asta anulează regula „fără minimap în joc” din handoff-ul PaperMap. Ai cerut-o explicit, așa că
setarea implicită e **minimap pornit**.

**Verified here:** `GpsRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 5 grupuri / **41 checks pass**
(pini, proiecția pe rută, recalcularea, rotirea minimapului, marginea, partea vizibilă a rutei). **Not compiled:**
fișierele Unreal. Scrise pe codul real: `MurdarRoad::RouteAlongLanes`, `MurdarNav::FindPath`, `UGameEventSubsystem`
(FHandle = int32), `SPaperMap` / `UPaperMapSettings` (handoff 17), `UJobSubsystem::GetTargetLocation` (handoff 16),
`UMissionSubsystem::PublishMission` (handoff Missions). Nesigurele sunt marcate `// ADAPT:`.

**Needs first:** PaperMap (17) and the pause menu (Menus, 10). Works better with Jobs (16) and Missions.

## Paste this prompt into local Claude Code

```
Read Handoff/Gps/README.md. The editor is connected through MCP (§Assets rule in Handoff/00_README_LOCAL_CLAUDE.md).
One step at a time, reporting in Romanian:

1. Unit test (README §Tests).
2. Look at BP_MapManager through MCP (do not change it yet): what draws its minimap (UMG? a SceneCapture2D render
   target?), where the big map opens, which key/input opens it, what else it does (markers? zones?). Report it to me
   and wait: I decide whether it is switched off or kept for something.
3. Copy Handoff/Gps/Source/Murdar_GameDev/UI/Gps/* into Source/Murdar_GameDev/UI/Gps/. Apply §Patches 1 (Missions),
   2 (SPaperMap), 3 (PaperMapLibrary), 4 (pause menu + M key). Build (editor closed).
4. After my OK on step 2: §Patches 5 (switch off BP_MapManager's minimap and big map - checkpoint first, change only
   that, read back). Set the map texture and world corners in Project Settings > Murdar Paper Map if still unset
   (§Setup).
5. Run README §In-game tests. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files

| File | What |
|---|---|
| `UI/Gps/GpsRules.h` | pure: pins (mission + waypoint, toggle, arrival), route projection / progress / re-route rules, minimap radius by speed, world→minimap heading-up, edge clamp, visible route part |
| `UI/Gps/GpsSettings.h` | Project Settings > Game > Murdar GPS: minimap on/off, route on/off, size, place, zoom, colours, pin radii, routing |
| `UI/Gps/GpsSubsystem.h/.cpp` | pins, two routes, re-planning (one A* per think, alternating), job pin, mission pin from the bus, the minimap on screen, `Murdar.Gps [clear]` |
| `UI/Gps/SMurdarMinimap.h/.cpp` | the minimap: rotated map region, both routes, pins (on the edge when outside), your arrow, N |

## Patches

### 1. Missions put their objective's place on the bus — `Director/Missions/MissionSubsystem.cpp`
`PublishMission("Event.Mission.Objective", ...)` publishes no location today. For a reach objective, add its place
and a label:
```cpp
// PublishMission: an optional location
void UMissionSubsystem::PublishMission(const TCHAR* TagName, float Magnitude, const FVector& Where /*= FVector::ZeroVector*/) const
	...
		E.Location = Where;
// where the Objective event is published: the current objective's ReachLocation when it has one
	PublishMission(TEXT("Event.Mission.Objective"), float(S.Objective), CurrentReachLocation()); // ADAPT: from the runtime's objective
```
`UGpsSubsystem` listens to `Event.Mission.Objective` (with a location → mission pin) and to `Event.Mission.Succeeded /
Failed` (clear). A mission can also call `UGpsSubsystem::SetMissionPin(Where, Label)` directly (Blueprint too).

### 2. The big map places pins and draws routes — `UI/PaperMap/SPaperMap.h/.cpp`
```cpp
// .h — args and members
		SLATE_ARGUMENT(TWeakObjectPtr<class UGpsSubsystem>, Gps)
...
	TWeakObjectPtr<UGpsSubsystem> Gps;
	FVector2D Cursor01 = FVector2D(0.5, 0.5); // where a pin goes: the mouse, or the centre crosshair (keys / pad)
	bool bMouseAim = false;
	FVector2D WorldAt(const FVector2D& Viewport01) const; // viewport 0..1 -> world X/Y (inverse of UVToViewport + UVToWorld)
	FReply PlacePin();
```
```cpp
// .cpp — Construct:  Gps = InArgs._Gps;
FVector2D SPaperMap::WorldAt(const FVector2D& V01) const
{
	const MurdarMap::FView V = MurdarMap::Clamp(View, LastAspect, Limits);
	const float A = FMath::Max(LastAspect, 1e-3f);
	const float SpanX = (A >= 1.f ? A : 1.f) / V.Zoom, SpanY = (A >= 1.f ? 1.f : 1.f / A) / V.Zoom;
	const MurdarMap::FV2 UV{ V.Center.X + (float(V01.X) - 0.5f) * SpanX, V.Center.Y + (float(V01.Y) - 0.5f) * SpanY };
	const UPaperMapSettings* S = GetDefault<UPaperMapSettings>();
	const MurdarMap::FBounds B{ { float(S->WorldAtUV0.X), float(S->WorldAtUV0.Y) }, { float(S->WorldAtUV1.X), float(S->WorldAtUV1.Y) } };
	const MurdarMap::FV2 W = MurdarMap::UVToWorld(UV, B);
	return FVector2D(W.X, W.Y);
}

FReply SPaperMap::PlacePin()
{
	if (UGpsSubsystem* G = Gps.Get())
	{
		const FVector2D W = WorldAt(bMouseAim ? Cursor01 : FVector2D(0.5, 0.5));
		G->ToggleWaypoint(FVector(W.X, W.Y, 0.0)); // Z 0 = find the ground there
	}
	return FReply::Handled();
}
```
Input:
- `OnKeyDown`: `EKeys::Enter`, `EKeys::SpaceBar`, `EKeys::Gamepad_FaceButton_Bottom` → `bMouseAim = false; return PlacePin();`.
- `OnMouseButtonDown`: `RightMouseButton` → `bMouseAim = true; Cursor01 = G.AbsoluteToLocal(Mouse.GetScreenSpacePosition()) / G.GetLocalSize(); return PlacePin();` (the left button keeps dragging).
- `OnMouseMove`: always update `Cursor01` and set `bMouseAim = true` (before the `if (!bDragging)` early return).
- Any pan by keys / stick: `bMouseAim = false` (the crosshair in the middle aims).

`OnPaint`, after the pencil marks (same `ToLocal(MurdarMap::UVToViewport(...))` path as the marks):
1. **routes** — for `Waypoint` then `Mission`: `Gps->GetRoute(K)` world points → `MurdarMap::WorldToUV` → viewport →
   local, `MakeLines` in the pin colour (`UGpsSettings`), thickness 4;
2. **pins** — `Gps->GetPin(K, ...)`: a diamond in the colour + its label;
3. **crosshair** at the centre when `!bMouseAim` (two short lines), none with the mouse (the cursor is the aim);
4. a **legend** at the bottom: `Enter / click dreapta: pune sau scoate punctul · Esc: înapoi` (LOCTEXT).

### 3. `UI/PaperMap/PaperMapLibrary.cpp` — `Build` passes the GPS
`SNew(SPaperMap) ... .Gps(UGpsSubsystem::Get(World))`. The job's red circle already drawn there stays (it is the same
place as the yellow mission pin; drop the circle if it doubles).

### 4. M opens the pause menu on the map — `UI/Menus/PauseMenuSubsystem.h/.cpp` + the player's input
```cpp
// PauseMenuSubsystem: open straight on the map page (GTA's M)
void UPauseMenuSubsystem::OpenOnMap() { if (!IsOpen()) { Open(); } OpenMap(); }
```
Where the game reads keys today (the same place `Input_Interact` is handled, or the HUD tick with
`WasInputKeyJustPressed`, as the other handoffs do): **M** → `UPauseMenuSubsystem::Get(this)->OpenOnMap()`. In the map,
M already goes back (`SPaperMap::OnKeyDown`). ADAPT: if BP_MapManager binds M through an Input Action, rebind that action
to this instead of adding a second binding (report which).

### 5. BP_MapManager (after the user's OK)
Checkpoint first. Switch off only its **minimap widget** and its **big map** (a bool / removing its widget creation
node / disabling the actor's tick - whatever is smallest), keep anything else it does. Read back, report what changed.
If its minimap used a SceneCapture2D, removing it also frees a whole second scene render - note the FPS before/after.

## Setup (with the user)
- **Project Settings > Murdar Paper Map:** the map drawing (MapTexture) and its world corners (WorldAtUV0 / UV1). Read
  two landmarks' world positions and where they sit on the drawing. Until the Iași map exists, a top-down screenshot
  of the test map works (High-res screenshot from an orthographic top view, corners = the camera's bounds).
- **Project Settings > Murdar GPS:** defaults are GTA-like. Colours: mission yellow, waypoint purple.

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Gps/Source/Murdar_GameDev/UI/Gps Handoff/Gps/Tests/gps_rules_test.cpp -o gps && ./gps`
→ `41 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| MM-01 | drive around | minimap bottom-left, the map under it moves with you, your arrow always up |
| MM-02 | turn left 90° | the map turns **right** under the arrow (heading-up). Wrong way → flip the sign in SMurdarMinimap (ADAPT) |
| MM-03 | speed up to 110 km/h | the minimap zooms out smoothly (90 → 250 m), back in when you stop |
| MM-04 | pause menu / cutscene | no minimap over them |
| MAP-01 | press M in play | the pause menu opens directly on the map; M or Esc closes it back to the game |
| MAP-02 | right-click on a street on the big map | a purple pin; a purple route from you to it appears on the big map and on the minimap |
| MAP-03 | right-click near the pin | it disappears with its route |
| MAP-04 | gamepad: move the view, press A | the pin goes under the centre crosshair |
| GPS-01 | start a job (or a story mission with a reach objective) **and** place your pin | **both** routes visible at once, yellow and purple, each to its own pin |
| GPS-02 | drive off your route (wrong turn) | after ~1.5 s off, the route re-plans from where you are |
| GPS-03 | follow the route | it follows the lanes (no wrong way down a one-way road, no field crossing) |
| GPS-04 | arrive at your pin | the pin clears itself (within 25 m) |
| GPS-05 | pin far outside the minimap | the diamond sits on the minimap's edge, on the right side |
| GPS-06 | on foot, pin 100 m away in a park | the route is a navmesh path (or a straight line off the navmesh), not along the road |
| GPS-07 | `stat unit` driving with two routes | no hitch when a route re-plans (one A* per think, alternating) |
| GPS-08 | job ends / mission succeeds or fails | the yellow pin and route go; your purple one stays |
