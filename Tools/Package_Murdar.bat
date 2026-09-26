@echo off
rem Packages MURDAR (Win64 Development) with the same options the editor's Package button used on 2026-09-26,
rem plus one: the cook gets its own MCP port, so it doesn't collide with the open editor on port 8000
rem (that collision logged an Error and made UAT report "Cook failed", ExitCode=25).
rem Works with the editor open. Output: H:\Package Murdar\Development 0.1

set UE=C:\Program Files\Epic Games\UE_5.8
set PROJECT=E:\Unreal Engine\Murdar_GameDev\Murdar_GameDev.uproject
set ARCHIVE=H:\Package Murdar\Development 0.1

call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%PROJECT%" -target=Murdar_GameDev -platform=Win64 -clientconfig=Development ^
  -unrealexe="%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  -nop4 -utf8output -nocompileeditor -skipbuildeditor -installed -nocompile -nocompileuat ^
  -build -cook -zenstore -stage -pak -iostore -compressed -prereqs -package ^
  -archive -archivedirectory="%ARCHIVE%" ^
  -AdditionalCookerOptions="-ModelContextProtocolPort=8123"

echo.
if %ERRORLEVEL% neq 0 (
  echo PACKAGE FAILED - ExitCode %ERRORLEVEL%. Log: %APPDATA%\Unreal Engine\AutomationTool\Logs
) else (
  echo PACKAGE OK - %ARCHIVE%
)
pause
