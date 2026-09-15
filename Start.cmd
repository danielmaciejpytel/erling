@echo off
set "ErlingEditor=C:\Engine\Unreal\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%ErlingEditor%" (
  echo Otworz ErlingFootball.uproject w Unreal Engine 5.8.2 i wybierz Play.
  pause
  exit /b 1
)
start "Erling Football" "%ErlingEditor%" "%~dp0ErlingFootball.uproject" -game -windowed -ResX=1440 -ResY=900
