# Heat — design (v2, 2026-09-26)

## Pe scurt (română)

Heat-ul rămâne **un singur număr global** (0–100), dar se comportă mai credibil:

- **Nu mai scade imediat.** După ce poliția te-a văzut ultima dată sau după o crimă raportată, 10 s nu scade deloc
  (încă te caută activ).
- **Crimele grave se uită greu.** După o crimă (80) heat-ul scade de ~4× mai încet decât după o depășire de viteză,
  timp de 5 minute. Înainte, o crimă dispărea complet în ~2 minute.
- **Căutarea încetinește scăderea.** Cât timp o unitate e în Searching, heat-ul scade la jumătate din viteză.
- **Aceeași abatere repetată contează tot mai puțin** (100 %, 50 %, 25 % … minim 10 %) într-un minut: nu mai
  sare heat-ul din viteză continuă.
- **Nivelul de wanted nu mai pâlpâie** la prag: urci la 40, cobori abia sub 35.
- **Martorii civili raportează cu întârziere.** Crima văzută de un civil intră în heat abia când martorul ajunge
  la poliție: câteva secunde dacă e un polițist aproape, altfel 30–90 s (cabină telefonică, secție — anii '90,
  fără mobile). Până atunci poți fugi, sau îl poți opri. Un martor mort nu mai raportează. Poliția primește **locul
  și ora crimei**, nu unde ești acum. Mai mulți martori ai aceleiași fapte nu dublează heat-ul — doar îl confirmă
  (+25 % pentru al doilea și al treilea).
- **Salvarea nu șterge martorii**: la save, rapoartele în așteptare sunt livrate.
- Poliția care vede fapta direct reacționează **instant**, ca acum.

## 1. Model

One scalar `Heat` 0..100, global, owned by `UFactionMemorySubsystem` (saved as `Stat.Heat`). The arithmetic is
`MurdarHeat::FModel` (`AI/Heat/MurdarHeatModel.h`, pure C++, unit-tested). Tunables: `UMurdarHeatSettings`.
The crime → heat table and the thresholds 12/40/75 stay in the memory.

| Rule | Detail | Setting (default) |
|---|---|---|
| Gain | crime heat from the table × repeat factor | — |
| Repeats | n-th repeat of the same crime type within the window: × `RepeatFactor^n`, ≥ `RepeatFloor` | 60 s, 0.5, 0.1 |
| Decay | none while police see him; none for `DecayDelaySeconds` after the last sighting/crime; then `DecayPerSecond × severity scale × search scale` | 0.6/s, 10 s |
| Severity scale | lerp(1, `SevereCrimeDecayScale`, worst recent crime heat / max table heat) for `SeverityMemorySeconds` | 0.25, 300 s |
| Search scale | `SearchDecayScale` while any unit is Searching | 0.5 |
| Wanted | up at the threshold, down below threshold − `WantedHysteresisHeat` | 5 |
| Cheat / bribe / load | `SetHeat` — wanted snaps, no hysteresis | — |

Decay times after the delay, from heat 80 to 0 (numbers from the model, not measured in game):

| Worst recent crime | Before | After |
|---|---|---|
| none / speeding (6) | 133 s | ~140 s |
| shots fired (35) | 133 s | ~200 s (search: ~400 s) |
| murder (80) | 133 s | ~360 s (−45 in the first 300 s at ×0.25, the last 35 at the base rate) |

## 2. Why one scalar (decision 2026-09-26)

The user chose the single global scalar over "heat + long-term record" and over per-sector heat. Everything above
improves how the one number moves; nothing adds a second number. Per-sector bribes stay as they are.

## 3. Witness reports

`UWitnessReportSubsystem` (`AI/Heat/`) + `MurdarHeat::FReportQueue` (pure, tested).

- A civilian who sees a crime → `WitnessCrime(Witness, Crime, Where)`: the crime becomes an **incident** (same type
  within 15 m and 5 s = the same incident), and a report is queued with a due time: `NearbyPoliceReportSeconds`
  (3 s) if a police member is within 30 m, else random in [30, 90] s.
- When it's due: the first report of the incident → the memory adds the crime's heat and a radio fix **at the
  crime's place and time**; later reports of the same incident → `CorroborationFraction` (25 %) of its heat, at most
  `MaxCorroborations` (2).
- Police who saw it themselves → heat at once (existing path) and the incident is marked known: civilian reports of
  it only corroborate.
- The witness dies / is destroyed / `SilenceWitness` → the report never arrives. `WitnessReachedPolice` → arrives
  now (hook for the foot AI walking to a phone booth — civilian spec Phase 5).
- A witness with a pending report is pinned in the civilian population (never despawned).
- Save → `FlushForSave` delivers everything first.

## 4. Tests

Unit (here, no engine): `Handoff/Heat/Tests/heat_model_test.cpp` — 15 cases, 42 checks, all pass
(`g++ -std=c++17 -Wall -Wextra -Wshadow`).

In game (local, after integration):

| ID | Test | Pass |
|---|---|---|
| HEAT-T01 | shots fired with only police watching | heat +35 at once, Pursuit at once (unchanged) |
| HEAT-T02 | run over a pedestrian with one civilian watching, no police | heat unchanged until the report (30–90 s), then +30; police search the crash site, not the player's position |
| HEAT-T03 | same, witness killed before reporting | heat never rises |
| HEAT-T04 | same, a patrol 20 m from the witness | report within ~3 s |
| HEAT-T05 | three civilians see one shooting | 35 + 2 × 8.75 heat, not 105 |
| HEAT-T06 | 60 s continuous speeding in sight of police | heat far below 6 × reports; check `MurdarHeatLog` |
| HEAT-T07 | heat 41, hide | wanted stays Pursuit until heat < 35 |
| HEAT-T08 | murder, then hide 120 s | still ≥ Pursuit (before the change: back to None) |
| HEAT-T09 | witnessed crime, autosave at a checkpoint, reload | the crime's heat is there |
| HEAT-T10 | Police AI regression: Phase 7–9 ghost chases | unchanged within noise (heat is set by cheat there) |

## 5. What changes for the police AI

Only through `GetWantedLevel()` (now with hysteresis) and the slower decay: pursuits and searches last longer after
serious crimes. Handoff 07 (graded aggression) reads the same heat — no change needed there.
