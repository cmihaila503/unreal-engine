@echo off
rem Builds the editor and runs the Murdar.Rules automation tests headless. Editor must be closed.
rem Usage: Tools\RunRuleTests_UE.bat            (from the project root)
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8
set PROJECT=%~dp0..\Murdar_GameDev.uproject

echo === Regenerating the automation wrappers from Handoff\*\Tests ===
python "%~dp0make_ue_rule_tests.py" "%~dp0..\Handoff" "%~dp0..\Source\Murdar_GameDev" "%~dp0..\Source\Murdar_GameDev\Tests\Rules" || exit /b 1

echo === Building Murdar_GameDevEditor ===
call "%UE%\Engine\Build\BatchFiles\Build.bat" Murdar_GameDevEditor Win64 Development -Project="%PROJECT%" -WaitMutex || exit /b 1

echo === Running Murdar.Rules ===
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -ExecCmds="Automation RunTests Murdar.Rules; Quit" ^
  -unattended -nullrhi -nosplash -nosound -log -ReportExportPath="%~dp0..\Saved\Automation\Rules" -ModelContextProtocolPort=8123
set RESULT=%ERRORLEVEL%
echo Report: Saved\Automation\Rules\index.json  (exit %RESULT%)
exit /b %RESULT%
