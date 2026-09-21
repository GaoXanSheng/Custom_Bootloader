@echo off
setlocal
echo ===================================================
echo   Compiling Custom ACPI Injector Bootloader (CMake)
echo ===================================================
echo.

:: 配置（默认 VS 生成器 + x64，CMake 自动定位 MSVC，无需 vcvars）
cmake -S . -B build\cmake -A x64
if %errorlevel% neq 0 (
    echo [-] Error: CMake configure failed.
    pause
    exit /b %errorlevel%
)

:: 全量构建（生成链 + BOOTX64.efi + DbgTool.exe，产物在 build\）
cmake --build build\cmake --config Release
if %errorlevel% neq 0 (
    echo [-] Error: Build failed.
    pause
    exit /b %errorlevel%
)

echo.
echo [+] SUCCESS! build\BOOTX64.efi generated successfully.
