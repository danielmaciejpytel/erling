@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start.ps1" %*
set "ErlingExitCode=%errorlevel%"
if not "%ErlingExitCode%"=="0" pause
exit /b %ErlingExitCode%
