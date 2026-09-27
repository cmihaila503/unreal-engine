# Găștile (Gangs) — handoff for local Claude Code

## Pe scurt (română)

Fiecare gașcă are **străzile ei** (zonele existente, `Zone.*`) și un **respect** față de tine, de la −100 la 100, care
se salvează:
- **Ostili** (sub −40): în străzile lor oamenii lor te caută, înarmați.
- **Cu ochii pe tine** (neutru): câțiva stau pe la colțuri. Când intri în cartier, unul poate veni să-ți ceară
  **taxa** (un dialog: plătești și ești lăsat în pace 3 zile, sau refuzi și îi superi).
- **Prieteni** (peste 40): te lasă în pace.

Respectul scade mult dacă le omori oamenii, crește când plătești taxa sau le faci treburi (un contact de la Jobs
poate fi al lor). Pe zi ce trece se mai uită și binele, și răul. Trecerea dintre stări are histerezis, deci nu
sare înainte-înapoi.

**Nu e încă aici: războaiele dintre găști.** AI-ul actual are doar facțiunile Civilian/Police/Hostile și un ostil te
țintește pe tine, nu pe alt ostil. Pentru bătăi între găști trebuie „echipe” în AI-ul NPC-urilor (un pas separat).

**Verified here:** `GangRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 5 tests / 25 checks pass. **Not
compiled:** the Unreal files. Written against the real source (`UMurdarAILibrary::SpawnNPC`, `ENPCFaction`,
`AMurdarNPCAIController::NotifyAttacked`, `Event.Actor.Died` Magnitude 2 = by the player, zone facts).

**Depends on:** Dialogue (the taxa), TimeOfDay (`Event.Time.NewDay`, the hour). Works with Jobs (a gang contact).

## Paste this prompt into local Claude Code

```
Read Handoff/Gangs/README.md. Dialogue and TimeOfDay must be integrated. One step at a time, building after each
(editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Apply README §Patches 2 first (SetFaction on the NPC controller — read how Faction is used there and tell me what
   must be refreshed when it changes), then copy Handoff/Gangs/Source/Murdar_GameDev/AI/Gangs/* into
   Source/Murdar_GameDev/AI/Gangs/. Build.
3. Check: are zone facts (Zone.*) set while the player is inside a ZoneVolume and cleared on leaving? Tell me.
4. Apply README §Patches 1, 3. Set up one gang (README §Setup) and the taxa dialogue asset.
5. Run README §In-game tests; write Docs/GANGS_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `AI/Gangs/GangRules.h` | pure: respect deeds, drift, stance with hysteresis, taxa, members around — unit-tested |
| `AI/Gangs/GangSettings.h` | Project Settings > Game > Murdar Gangs |
| `AI/Gangs/GangMemberComponent.h` | marks a pawn as a gang's |
| `AI/Gangs/GangSubsystem.h/.cpp` | territory, stance, members, taxa, respect |

## Patches

### 1. Tags (one set per gang)
```ini
+GameplayTagList=(Tag="Gang.Tatarasi",DevComment="")
+GameplayTagList=(Tag="Stat.Respect.Tatarasi",DevComment="Gang respect for the player (saved)")
+GameplayTagList=(Tag="Stat.TaxPaidUntil.Tatarasi",DevComment="Game day the taxa runs out")
+GameplayTagList=(Tag="Event.Gang.TaxPaid",DevComment="Payload = the gang's taxa DialogueTag")
+GameplayTagList=(Tag="Event.Gang.TaxRefused",DevComment="Payload = the gang's taxa DialogueTag")
+GameplayTagList=(Tag="Dialogue.Gang.Tatarasi.Tax",DevComment="")
```

### 2. `AI/MurdarNPCAIController.h/.cpp` — change sides
```cpp
/** A gang man who stops pretending. */
void SetFaction(ENPCFaction NewFaction);
```
```cpp
void AMurdarNPCAIController::SetFaction(ENPCFaction NewFaction)
{
	if (Faction == NewFaction) { return; }
	Faction = NewFaction;
	// ADAPT: re-register with UFactionMemorySubsystem's member lists / perception affiliation if Faction was cached
	// at BeginPlay/OnPossess; give him the gang's weapon if he had none (UWeaponComponent::GiveWeapon).
}
```

### 3. Cheat
```cpp
/** MurdarGangs [gang index, deed 0-3]: show; or apply a deed (0 kill, 1 hurt, 2 taxa, 3 job). */ UFUNCTION(Exec) void MurdarGangs(int32 Gang = -1, int32 Deed = 0);
```
```cpp
#include "AI/Gangs/GangSubsystem.h"
void UDirectorCheats::MurdarGangs(int32 Gang, int32 Deed)
{
	UGangSubsystem* G = UGangSubsystem::Get(GetWorld());
	if (!G) { return; }
	if (Gang >= 0) { G->AddDeed(Gang, MurdarGangs::EDeed(FMath::Clamp(Deed, 0, 3))); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *G->Describe());
}
```

## Setup
- One gang to start: „Băieții din Tătărași” — `Territory` = the Tătărași zone fact(s), stats as above, a pistol.
- `DA_Dialogue_Gang_Tatarasi_Tax` (Handoff/Dialogue): `DialogueTag` `Dialogue.Gang.Tatarasi.Tax`, no start event (the
  gang starts it), `MaxDistanceCm` 800. Node „Ce faci, frate, pe la noi? Aici se plătește.” Choices:
  „Dă-i 200” [`Stat.Money` ≥ 200; AddValue −200; Publish `Event.Gang.TaxPaid`], „Nu dau nimic” [Publish
  `Event.Gang.TaxRefused`], ChoiceTimeout 8, DefaultChoice = refuse.
- A job contact belonging to them: when their jobs succeed, call `AddDeed(G, JobDone)` (Jobs: map contact → gang).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Gangs/Source/Murdar_GameDev/AI/Gangs Handoff/Gangs/Tests/gang_rules_test.cpp -o gg && ./gg` → `25 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| GNG-01 | walk into their streets | 3 men drift in over a few seconds (4 at night) |
| GNG-02 | enter repeatedly (wary, unpaid) | sometimes one comes for the taxa |
| GNG-03 | pay | respect +12; no taxa for 3 game days (`MurdarGangs`) |
| GNG-04 | refuse | respect −16 |
| GNG-05 | kill one of theirs | −35; at ≤ −40 they turn on you, armed |
| GNG-06 | `MurdarGangs 0 2` ×5 | friendly; fewer men, no taxa |
| GNG-07 | leave the territory | their men despawn beyond 150 m |
| GNG-08 | save, load | respect kept |
