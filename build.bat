@echo off
setlocal enabledelayedexpansion
echo ===================================================
echo   Compiling Custom ACPI Injector Bootloader...
echo ===================================================
echo.

:: Initialize MSVC 64-bit build environment
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Create output directories
if not exist build\obj mkdir build\obj

echo [+] Compiling WMI MOF to BMF...
"%SystemRoot%\System32\wbem\mofcomp.exe" -B:build\SsdtUnlockDB.bmf src\acpi\SsdtUnlockDB.mof
if %errorlevel% neq 0 (
    echo [-] Error: MOF compilation failed.
    pause
    exit /b %errorlevel%
)

echo [+] Converting BMF to ASL buffer...
powershell -NoProfile -Command "$b=[System.IO.File]::ReadAllBytes('build\SsdtUnlockDB.bmf'); $h=($b | ForEach-Object { '0x{0:X2}' -f $_ }) -join ', '; $c=\"// Auto-generated. Do not edit.`r`nName (WQBA, Buffer ()`r`n{`r`n    $h`r`n})\" ; [System.IO.File]::WriteAllText('build\BmfData.asl', $c)"
if %errorlevel% neq 0 (
    echo [-] Error: BMF to ASL conversion failed.
    pause
    exit /b %errorlevel%
)

:: Compile ASL (run iasl inside build\ so outputs land there)
echo [+] Compiling ASL with iASL...
copy /Y build\BmfData.asl src\acpi\BmfData.asl >nul
pushd build
..\tools\isal\iasl.exe -p SsdtUnlockDB -tc ..\src\acpi\SsdtUnlockDB.asl
set IASL_ERROR=%errorlevel%
popd
del src\acpi\BmfData.asl
if %IASL_ERROR% neq 0 (
    echo [-] Error: iASL compilation failed.
    pause
    exit /b %IASL_ERROR%
)

echo [+] Compiling C sources...
cl.exe /GS- /W4 /O2 /utf-8 /TC /c /Fobuild\obj\ src\main.c src\UefiHelpers.c src\Logging.c src\AcpiPatch.c
if %errorlevel% neq 0 (
    echo.
    echo [-] Error: Compilation failed.
    pause
    exit /b %errorlevel%
)

:: Link to BOOTX64.efi
echo [+] Linking...
link.exe /subsystem:efi_application /entry:efi_main /nodefaultlib /dll /out:build\BOOTX64.efi build\obj\main.obj build\obj\UefiHelpers.obj build\obj\Logging.obj build\obj\AcpiPatch.obj
if %errorlevel% neq 0 (
    echo.
    echo [-] Error: Linking failed.
    pause
    exit /b %errorlevel%
)

echo.
echo [+] SUCCESS! build\BOOTX64.efi generated successfully.
pause
