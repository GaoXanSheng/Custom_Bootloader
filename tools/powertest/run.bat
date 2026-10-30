@echo off
setlocal
set "PS=C:\Program Files\PowerShell\7\pwsh.exe"
if not exist "%PS%" set "PS=pwsh"
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0powertest.ps1" %*
if errorlevel 1 pause
