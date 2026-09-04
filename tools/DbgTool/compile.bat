@echo off
setlocal
cd /d "%~dp0"

set "CSC64=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
set "CSC32=%SystemRoot%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if exist "%CSC64%" set "CSC=%CSC64%"
if not exist "%CSC64%" set "CSC=%CSC32%"

if not exist "%CSC%" goto no_csc

echo [+] Compiler: %CSC%
"%CSC%" /nologo /codepage:65001 /target:exe /platform:anycpu ^
  /out:DbgTool.exe ^
  /r:System.dll /r:System.Core.dll /r:System.Management.dll ^
  DbgTool.cs
if errorlevel 1 goto fail

echo [+] OK: %~dp0DbgTool.exe
exit /b 0

:no_csc
echo [-] csc.exe not found - .NET Framework 4.x required.
exit /b 1

:fail
echo [-] Compile failed.
exit /b 1
