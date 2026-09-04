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

echo [+] Generating version header...
python tools\generate_version.py
if %errorlevel% neq 0 (
    echo [-] Error: Version header generation failed.
    pause
    exit /b %errorlevel%
)

echo [+] Building patched ACPI tables...
python tools\build_patched_tables.py
if %errorlevel% neq 0 (
    echo [-] Error: Patched ACPI table build failed.
    pause
    exit /b %errorlevel%
)

echo [+] Compiling WMI MOF to BMF...
"%SystemRoot%\System32\wbem\mofcomp.exe" -B:build\SsdtUnlockDB.bmf src\acpi\SsdtUnlockDB.mof
if %errorlevel% neq 0 (
    echo [-] Error: MOF compilation failed.
    pause
    exit /b %errorlevel%
)

echo [+] Converting BMF to ASL buffer...
python -c "b=open('build/SsdtUnlockDB.bmf','rb').read(); open('build/BmfData.asl','w').write('// Auto-generated. Do not edit.\nName (WQBA, Buffer ()\n{\n    ' + ', '.join('0x{:02X}'.format(x) for x in b) + '\n})\n')"
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
cl.exe /GS- /W4 /O2 /utf-8 /TC /c /Fobuild\obj\ src\main.c src\UefiHelpers.c src\Logging.c src\AcpiPatch.c src\BootIcon.c
if %errorlevel% neq 0 (
    echo.
    echo [-] Error: Compilation failed.
    pause
    exit /b %errorlevel%
)

:: Link to BOOTX64.efi
echo [+] Linking...
link.exe /subsystem:efi_application /entry:efi_main /nodefaultlib /dll /out:build\BOOTX64.efi build\obj\main.obj build\obj\UefiHelpers.obj build\obj\Logging.obj build\obj\AcpiPatch.obj build\obj\BootIcon.obj
if %errorlevel% neq 0 (
    echo.
    echo [-] Error: Linking failed.
    pause
    exit /b %errorlevel%
)

echo.
echo [+] SUCCESS! build\BOOTX64.efi generated successfully.
pause
