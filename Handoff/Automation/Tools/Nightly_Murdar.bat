@echo off
rem The whole chain, for a nightly / before a playtest: rule tests (g++ or cl) -> editor build + Murdar.Rules -> package.
rem Stops at the first failure. Editor must be closed.
setlocal
echo === 1. Pure rule tests ===
python "%~dp0run_rule_tests.py" "%~dp0..\Handoff" || exit /b 1
echo === 2. Engine automation (Murdar.Rules) ===
call "%~dp0RunRuleTests_UE.bat" || exit /b 1
echo === 3. Package (Tools\Package_Murdar.bat) ===
call "%~dp0Package_Murdar.bat" || exit /b 1
echo === All green ===
