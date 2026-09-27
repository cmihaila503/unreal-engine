# Joburi, pager și telefon public (Jobs) — handoff for local Claude Code

## Pe scurt (română)

Bucla de bani. Când ești liber (fără treabă, fără misiune, fără poliție), **pager-ul bipăie**: „Sună-l pe Vali.
Urgent.” Mergi la un **telefon public**, suni, iar contactul îți dă o treabă generată din locul în care ești:
- **Livrare:** iei pachetul de la X și îl duci la Y, cu ceas. Plata depinde de distanță.
- **Contrabandă:** pachetul merge la graniță (Prut, vamă). Plătește de 2,5 ori mai mult, dar dacă pornește o
  urmărire, treaba e ratată.
- **Mașină la comandă:** „Am un client care vrea un Aro.” Furi unul (Handoff/VehicleTheft) și îl aduci. Plata
  scade cu avariile.
- **Recuperare:** un tip datorează bani. Te duci și „Ceri banii”: plătește sau fuge; a doua oară, prins, plătește.

Dacă nu suni la timp, pager-ul tace până la următorul mesaj. O treabă ratată îl supără pe contact pentru o vreme.
Treaba rulează pe același motor ca misiunile de poveste, deci salvatul și somnul sunt blocate cât ține.

**Verified here:** `JobRules.h` — g++ C++17 `-Wall -Wextra -Wshadow -Werror`, 9 tests / 29 checks pass, **including
running the generated jobs on the real `MurdarMission::FMissionRuntime`** from Handoff/Missions. **Not compiled:** the
Unreal files.

**Depends on:** Missions (runtime), Interaction (payphones, the debtor), Economy (pay). CarDelivery uses VehicleTheft
if present (any way of getting the car counts).

## Paste this prompt into local Claude Code

```
Read Handoff/Jobs/README.md. Missions, Interaction and Economy must be integrated. One step at a time, building
after each (editor closed), reporting in Romanian:
1. Run the unit test (README §Tests).
2. Copy Handoff/Jobs/Source/Murdar_GameDev/Director/Jobs/* into Source/Murdar_GameDev/Director/Jobs/. Build.
3. Apply README §Patches 1-2. Fill Project Settings > Murdar Jobs as in README §Setup (config only).
4. Ask me before placing job points and payphones in a map.
5. Run README §In-game tests; write Docs/JOBS_TEST_REPORT.md. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files (new)
| File | What |
|---|---|
| `Director/Jobs/JobRules.h` | pure: point picking, job generation, pay, time limits, job → mission spec, pager, debtor — unit-tested |
| `Director/Jobs/JobSettings.h` | Project Settings > Game > Murdar Jobs (contacts, wanted cars, pager, pay) |
| `Director/Jobs/JobPoint.h/.cpp` | a place for pickups / drops / border drops, with the name the contact says |
| `Director/Jobs/JobSubsystem.h/.cpp` | pager, payphone, running the job, subtitles, pay |

## Patches

### 1. Tags — `Config/DefaultGameplayTags.ini`
```ini
+GameplayTagList=(Tag="Job",DevComment="Payload of Event.Mission.* published for jobs")
+GameplayTagList=(Tag="Event.Job.Paged",DevComment="Magnitude = contact index")
+GameplayTagList=(Tag="Event.Job.Collected",DevComment="The debtor paid")
+GameplayTagList=(Tag="Interact.Payphone",DevComment="Payphones (Interactable)")
+GameplayTagList=(Tag="Interact.Job.Debtor",DevComment="The man owing money")
```

### 2. Cheats — `Director/DirectorCheats.h/.cpp`
```cpp
/** MurdarJob [kind]: status; "page" pages now; 0-3 starts that kind with contact 0. */ UFUNCTION(Exec) void MurdarJob(const FString& Arg);
/** MurdarJobAbort: fail the running job. */                                         UFUNCTION(Exec) void MurdarJobAbort();
```
```cpp
#include "Director/Jobs/JobSubsystem.h"
void UDirectorCheats::MurdarJob(const FString& Arg)
{
	UJobSubsystem* J = UJobSubsystem::Get(GetWorld());
	if (!J) { return; }
	if (Arg == TEXT("page")) { J->PageNow(); }
	else if (!Arg.IsEmpty()) { J->StartJob(0, FCString::Atoi(*Arg)); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *J->Describe());
}
void UDirectorCheats::MurdarJobAbort() { if (UJobSubsystem* J = UJobSubsystem::Get(GetWorld())) { J->AbortJob(TEXT("cheat")); } }
```

## Setup
Project Settings > Murdar Jobs:
- Contacts: „Vali” (Delivery, Collection, ×1), „Nea Costel” (Smuggling, ×1.2, `RequiredFact` = a story fact once
  you've met him), „Marian de la service” (CarDelivery, ×1).
- WantedCars: the vehicle definitions that exist (Dacia, Aro, …).
- PagerBeep: a short 90s pager beep (optional).

Map (with the user): 6–10 `AJobPoint`s with a `PlaceName` („curtea din spatele Gării”, „depozitul de pe Păcurari”),
2 with `bBorder` near the Prut / the customs; a few payphones = any actor with an `Interactable` (Verb „Sună”,
`InteractionTag` `Interact.Payphone`).

## Tests

Unit: `g++ -std=c++17 -Wall -Wextra -Wshadow -I Handoff/Jobs/Source/Murdar_GameDev/Director/Jobs -I Handoff/Missions/Source/Murdar_GameDev/Director/Missions Handoff/Jobs/Tests/job_rules_test.cpp -o jt && ./jt` → `29 checks, 0 failed`.

| ID | Test | Pass |
|---|---|---|
| JOB-01 | `MurdarJob page` | beep, „Pager: Sună-l pe Vali. Urgent.” |
| JOB-02 | E at a payphone | job subtitle with a place name; `MurdarJob` shows RUNNING |
| JOB-03 | deliver in time | „Bravo. Ai N lei.”; money +N |
| JOB-04 | `MurdarJob 1` (smuggling), get chased | fails at the pursuit start |
| JOB-05 | `MurdarJob 2`, steal the wanted model, bring it dented | pay reduced by damage |
| JOB-06 | `MurdarJob 3`, „Cere banii” | pays, or runs; asked again he pays |
| JOB-07 | during a job: pause menu Save / safehouse bed | refused („în mijlocul unei treburi”) |
| JOB-08 | page, don't call for 5 min | page gone; payphone „Nu răspunde nimeni.” |
| JOB-09 | a story mission running / heat > 0 | no pages |

## Design notes
- A job is a generated mission: one runtime, tested once, used by story and side work.
- No markers: the contact names the place. The paper map (Handoff/PaperMap) is where he looks it up.
