# Paranoia în retrovizoare — design

Ideea utilizatorului (26 sep 2026), adaptată pe codul existent. Fără UI: jucătorul află totul din oglindă.

## Ce simte jucătorul

Noaptea, pe un drum gol, în oglindă apar două faruri. Stau la aceeași distanță. Nu știi cine e: un om care merge acasă,
un echipaj sub acoperire sau o mașină de interlopi. Ai mai multe moduri să afli, și fiecare te poate trăda și pe tine:

| Test | Civilul | Polițistul sub acoperire | Interlopul |
|---|---|---|---|
| **Frână scurtă** (brake check) | aproape: claxonează; mai departe: dă flash de două ori | se lasă calm în spate, fără claxon, fără flash | trage lângă tine și stă acolo |
| **Viraj brusc** (25+ km/h, pe o lăturalnică) | își vede de drum | uneori te urmează, uneori merge înainte | te urmează mereu |
| **Întoarcere (U-turn)** | își vede de drum | aproape niciodată nu se întoarce: trece pe lângă tine (îl vezi la față) | se întoarce și el |
| **Turul cvartalului** (dreapta-dreapta-dreapta) | niciodată | un profesionist se desprinde la ultimul colț | te urmează — și e deconspirat |
| **Tragi pe dreapta** și aștepți | trece pe lângă tine | începătorul oprește la 40 m în spate; profesionistul trece, parchează mai în față cu farurile stinse și iese după tine când treci | oprește în spate, cu farurile aprinse |
| **Stingi farurile** pe întuneric | nu-i pasă | aprinde faza lungă și accelerează să te găsească | la fel |

**Stopurile te trădează.** Cu farurile stinse, dacă frânezi cu pedala, stopurile se aprind și urmăritorul te vede de la
~90 m. Dacă încetinești cu frâna de mână, rămâi în întuneric — trucul vechi funcționează.

**Nimeni nu reacționează instant.** Fiecare urmăritor răspunde după 0,4–1,3 s (mai repede cei pricepuți), deci doi
urmăritori nu fac niciodată același lucru în aceeași clipă.

Fiecare test pe care urmăritorul „îl pică” îl face să simtă că a fost văzut. Când simte asta:
- **polițistul** renunță și dispare (dacă poliția te bănuia deja, între timp a transmis prin stație unde ești);
- **interlopul** nu se mai preface: vine lângă tine, apoi te lovește.

Civilii sunt partea importantă: sunt oameni obișnuiți din trafic, care uneori merg exact în aceeași direcție cu tine.
Paranoia funcționează doar dacă uneori te sperii degeaba. Noaptea drumurile sunt goale, așa că sistemul pune
intenționat, din când în când, **o mașină obișnuită** în spatele tău (la fel de des ca pe un urmăritor, implicit):
merge pe drumul ei, claxonează dacă frânezi brusc, nu te urmărește.

## Oglinda

Ții apăsat un buton: imaginea trece în oglinda din habitaclu, îngustă, cu margini întunecate, iar motorul se aude
încet ca să auzi mașina din spate. Dai drumul: revii la camera normală.

## Când apare un urmăritor

Doar noaptea, doar când conduci (peste 30 km/h), niciodată în timpul unei urmăriri (atunci e treaba poliției cu
girofar), cel mult unul odată, cu pauză între ei (4 minute implicit).
- **Sub acoperire:** mai des când poliția te bănuiește (wanted = Stop sau au descrierea mașinii tale).
- **Interlopi:** mai des când povestea o cere (faptul `Fact.Gang.Hunting`, setat de un capitol sau trigger).
- Apare în spatele tău, pe banda ta, unde nu-l vezi apărând; arată ca orice mașină din trafic.

## Ce vede urmăritorul

Doar ce vede cu adevărat: cu farurile aprinse, noaptea, te vede de la ~120 m; cu farurile stinse, doar de la ~18 m
(cu faza lungă, ~33 m). Ziua farurile nu contează. Când te pierde, merge spre unde ar trebui să fii, nu spre unde ești
(aceeași regulă ca la poliție: nu știe mai mult decât a văzut).

## Ce e verificat și ce nu

Regulile (frâna, virajul, întoarcerea, turul cvartalului, vizibilitatea cu stopuri, „am fost văzut”, ce face la
oprire) sunt scrise separat de Unreal și testate aici: 46 de verificări trec. Testele au prins două greșeli reale,
reparate: o frână de urgență până la oprire era luată drept brake check; un viraj brusc părea lent pentru că se
măsura și drumul drept dinaintea lui. Partea de Unreal e scrisă pe codul real al proiectului, dar nu e compilată.

## Review (26 sep, după prima versiune)

| # | Problema în v1 | Rezolvare în v1.1 |
|---|---|---|
| 1 | Orice viraj obișnuit (~30°/s la 20 km/h) era „viraj de inspecție” — fără semnalizare, fiecare intersecție era un test | doar virajele bruște (45+°/s, 25+ km/h); testele puternice sunt acum U-turn și turul cvartalului |
| 2 | Noaptea, singura mașină din spate era aproape mereu urmăritorul → nicio paranoia | civili-momeală puși intenționat în spate |
| 3 | Frâna nu spunea nimic peste 25 m (civilul claxona doar aproape, urmăritorul stă la 40 m) | civilul mai îndepărtat dă flash de două ori |
| 4 | Camera oglinzii era în habitaclu, privind prin propria caroserie | pusă chiar în spatele mașinii, din dimensiunile ei |
| 5 | Lipsea testul cel mai simplu: oprești | testul de oprire (începătorul oprește în spate; profesionistul parchează în față, dark) |
| 6 | Mașina jucătorului nu avea stopuri | stopurile se aprind la pedală și te trădează în întuneric; frâna de mână nu |
| 7 | Toți reacționau în aceeași clipă | întârziere umană, după pricepere |
| — | Urmăritorii trec pe roșu ca să țină pasul | lăsat intenționat: e un semn real de urmărire |

## Ce vine după (v2)

- Semnalizare pentru jucător (virajul semnalizat nu mai e test).
- Polițistul care „merge înainte” te așteaptă la capătul lăturalnicei.
- Felinarele: o mașină fără faruri sub un felinar se vede.
- Interlopii coboară din mașină (lupta pe jos).
- Sunet: urmăritorul care accelerează se aude înainte să-l vezi.
