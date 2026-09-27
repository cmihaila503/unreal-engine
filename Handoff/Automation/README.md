# Teste automate și build automat (Automation) — handoff for local Claude Code

## Pe scurt (română)

Toate regulile pure din handoff-uri (27 de fișiere de test, peste 700 de verificări) pot rula acum **automat**, în
trei feluri:
1. **`run_rule_tests.py`** — le găsește singur, le compilează cu avertismentele tratate ca erori și le rulează
   (g++, clang sau compilatorul Visual Studio). Aici, în cloud: **27 trec, 0 picate**.
2. **În Unreal** — `make_ue_rule_tests.py` le transformă în teste Unreal („Murdar.Rules.<Sistem>”), pe care le vezi în
   *Session Frontend → Automation* sau le rulezi fără interfață cu `RunRuleTests_UE.bat`. Așa se testează **headerele
   din proiect**, nu copiile din `Handoff/`. Am verificat aici că fișierele generate se compilează și trec (26 de
   teste), și separat, și lipite într-un singur fișier cum face Unreal (unity build). Simularea a găsit o problemă
   reală, pe care am reparat-o.
3. **GitHub Actions** — `github-rule-tests.yml` rulează testele la fiecare push, pe Linux, în ~30 s, fără Unreal.

Plus `Nightly_Murdar.bat`: teste → build editor + teste Unreal → pachet, și se oprește la prima eroare.

## Paste this prompt into local Claude Code

```
Read Handoff/Automation/README.md. One step at a time, reporting in Romanian:
1. Copy Handoff/Automation/Tools/* into Tools/. Run: python Tools/run_rule_tests.py Handoff  -> all pass.
2. Run Tools/RunRuleTests_UE.bat (editor closed). It generates Source/Murdar_GameDev/Tests/Rules/RuleTest_*.cpp for the
   systems already integrated, builds, and runs Murdar.Rules headless. Show me the summary.
3. Ask me before adding .github/workflows/rule-tests.yml (from Handoff/Automation/github-rule-tests.yml) and before
   un-ignoring Handoff/ if it is in .gitignore.
4. Report IMPLEMENTED/TESTED/FAILED/BLOCKED/NEXT.
```

## Files
| File | What |
|---|---|
| `Tools/run_rule_tests.py` | finds, builds (warnings = errors) and runs every `Handoff/*/Tests` test (C++ and Python) |
| `Tools/make_ue_rule_tests.py` | generates `RuleTest_<System>.cpp` automation wrappers for the integrated systems |
| `Tools/RunRuleTests_UE.bat` | regenerate → build editor → `Automation RunTests Murdar.Rules` headless → report |
| `Tools/Nightly_Murdar.bat` | rule tests → engine tests → package |
| `github-rule-tests.yml` | GitHub Actions workflow template (Linux, no Unreal) |

## Notes
- The generated wrappers put each test in its own namespace, rename its `main`, and include the rule headers at
  global scope (a header first seen inside one namespace would be hidden from the next test in a unity build — the
  unity simulation caught exactly that). They are generated files: regenerate, don't edit.
- `-ModelContextProtocolPort=8123` in the .bat avoids the port-8000 clash with an open editor (the cook failure
  from before).
- Next step, when wanted: functional tests in a test map (`AFunctionalTest`): spawn a car, carjack it, respray it,
  check the police description — the systems' seams, which the pure tests can't reach.

## Tests
- `python Tools/run_rule_tests.py Handoff` → `27 test file(s) passed, 0 failed`.
- `Tools\RunRuleTests_UE.bat` → every `Murdar.Rules.*` green in `Saved/Automation/Rules/index.json`.
