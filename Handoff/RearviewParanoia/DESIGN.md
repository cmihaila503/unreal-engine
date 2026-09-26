# Paranoia în retrovizoare — design

Ideea utilizatorului (26 sep 2026), adaptată pe codul existent. Fără UI: jucătorul află totul din oglindă.

## Ce simte jucătorul

Noaptea, pe un drum gol, în oglindă apar două faruri. Stau la aceeași distanță. Nu știi cine e: un om care merge acasă,
un echipaj sub acoperire sau o mașină de interlopi. Ai trei moduri să afli, și fiecare te poate trăda și pe tine:

| Test | Civilul | Polițistul sub acoperire | Interlopul |
|---|---|---|---|
| **Frână scurtă** (brake check) | claxonează | se lasă calm în spate, fără claxon | trage lângă tine și stă acolo |
| **Virajul de inspecție** (brusc, fără semnalizare, pe o lăturalnică) | își vede de drum | uneori te urmează, uneori merge înainte (un profesionist nu mușcă momeala) | te urmează mereu |
| **Stingi farurile** pe întuneric | nu-i pasă | aprinde faza lungă și accelerează să te găsească | la fel |

Fiecare test pe care urmăritorul „îl pică” îl face să simtă că a fost văzut. Când simte asta:
- **polițistul** renunță și dispare (dacă poliția te bănuia deja, între timp a transmis prin stație unde ești);
- **interlopul** nu se mai preface: vine lângă tine, apoi te lovește.

Civilii sunt partea importantă: sunt oameni obișnuiți din trafic, care uneori merg exact în aceeași direcție cu tine.
Paranoia funcționează doar dacă uneori te sperii degeaba.

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

Regulile (frâna, virajul, vizibilitatea, „am fost văzut”) sunt scrise separat de Unreal și testate aici: 27 de
verificări trec. Testele au prins o greșeală reală (o frână de urgență până la oprire era luată drept brake check) —
reparată. Partea de Unreal e scrisă pe codul real al proiectului, dar nu e compilată (nu există Unreal aici).

## Ce vine după (v2)

- Semnalizare pentru jucător (virajul semnalizat nu mai e test).
- Polițistul care „merge înainte” te așteaptă la capătul lăturalnicei.
- Felinarele: o mașină fără faruri sub un felinar se vede.
- Interlopii coboară din mașină (lupta pe jos).
- Sunet: urmăritorul care accelerează se aude înainte să-l vezi.
