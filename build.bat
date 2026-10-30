@echo off
setlocal
set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
echo ===================================================
echo   Building Custom Bootloader (Cargo)
echo ===================================================
echo.

python tools\generate_version.py
if errorlevel 1 goto fail

python tools\build_custom_tables.py --out-dir build\Custom
if errorlevel 1 goto fail

cargo build --release
if errorlevel 1 goto fail

if not exist build mkdir build
copy /Y target\x86_64-unknown-uefi\release\BOOTX64.efi build\BOOTX64.efi >nul
if errorlevel 1 goto fail
copy /Y version.txt build\version.txt >nul

pushd tools\DbgTool
call compile.bat
set "DBGTOOL_ERR=%errorlevel%"
popd
if not "%DBGTOOL_ERR%"=="0" goto fail

echo.
echo [+] SUCCESS! All artifacts generated:
echo     - build\BOOTX64.efi
echo     - build\Custom\dsdt_mod.aml
echo     - build\DbgTool.exe
exit /b 0

:fail
echo.
echo [-] Error: Build failed.
pause
exit /b 1
