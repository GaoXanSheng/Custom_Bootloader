@echo off
title SSDT-UnlockDB WMI/ACPI 调试部署工具
color 0B

:: 检查管理员权限
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] 需要管理员权限。正在申请提权...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

:: 强制切换到脚本文件所在目录
cd /d "%~dp0"

:menu
cls
echo ==========================================================
echo           SSDT-UnlockDB WMI/ACPI 调试部署菜单
echo ==========================================================
echo.
echo  [1] 检查 WMI 设备注册状态 (CustomDbUnlock)
echo  [2] 读取当前 DBFS 标志值 (WMI 方法: GetDbfs)
echo  [3] 禁用 Dynamic Boost (WMI 方法: SetDbfs = 0)
echo  [4] 启用 Dynamic Boost (WMI 方法: SetDbfs = 1)
echo  [5] 读取内部版本号 (WMI 方法: GetVersion)
echo  [6] 查看 ESP 日志与版本 (unlock.log / version.txt)
echo  [7] 查询 NVIDIA 显卡当前功耗限制 (nvidia-smi)
echo  [8] 一键部署 BOOTX64.efi 到 ESP 分区 (S:\EFI\BOOT\BOOTX64.efi)
echo  [9] 一键添加 Custom Bootloader 为默认启动项 (bcdedit)
echo  [10] 退出
echo.
echo ==========================================================
set /p choice="请选择操作 (1-10): "

if "%choice%"=="1" goto check_wmi
if "%choice%"=="2" goto read_dbfs
if "%choice%"=="3" goto disable_db
if "%choice%"=="4" goto enable_db
if "%choice%"=="5" goto read_version
if "%choice%"=="6" goto view_log
if "%choice%"=="7" goto gpu_limit
if "%choice%"=="8" goto deploy_efi
if "%choice%"=="9" goto set_boot_order
if "%choice%"=="10" exit /b
goto menu

:check_wmi
echo.
echo [+] 正在查询 CustomDbUnlock 注册状态...
powershell -NoProfile -Command "Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue | Format-List -Property Active, InstanceName, __SERVER"
if %errorlevel% neq 0 (
    echo [-] 错误: 未在 WMI 中找到 CustomDbUnlock 类。
)
echo.
pause
goto menu

:read_dbfs
echo.
echo [+] 正在通过 WMI 调用 GetDbfs 方法...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.GetDbfs(); Write-Host '返回值 (DBFS 状态):' $res.Value } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例' }"
echo.
pause
goto menu

:disable_db
echo.
echo [+] 正在通过 WMI 设置 DBFS 为 0 (禁用 Dynamic Boost)...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.SetDbfs(0); Write-Host 'SetDbfs 返回状态:' $res.Status } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例' }"
echo.
pause
goto menu

:enable_db
echo.
echo [+] 正在通过 WMI 设置 DBFS 为 1 (启用 Dynamic Boost)...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.SetDbfs(1); Write-Host 'SetDbfs 返回状态:' $res.Status } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例' }"
echo.
pause
goto menu

:read_version
echo.
echo [+] 正在通过 WMI 调用 GetVersion 方法...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.GetVersion(); $hex = '0x{0:X8}' -f $res.BuildVersion; Write-Host '内部版本号 (BuildVersion):' $res.BuildVersion ('(' + $hex + ')') } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例' }"
echo.
pause
goto menu

:view_log
echo.
echo [+] 正在释放 S: 盘占位...
mountvol S: /D >nul 2>&1
echo [+] 正在挂载 ESP 分区为 S: 盘...
mountvol S: /S >nul 2>&1
if exist S:\EFI\BOOT\version.txt (
    echo [+] S:\EFI\BOOT\version.txt 内部版本:
    echo -------------------------------------------------------------
    type S:\EFI\BOOT\version.txt
    echo -------------------------------------------------------------
)
if exist S:\EFI\BOOT\unlock.log (
    echo [+] 读取 S:\EFI\BOOT\unlock.log 运行日志:
    echo -------------------------------------------------------------
    type S:\EFI\BOOT\unlock.log
    echo -------------------------------------------------------------
) else (
    echo [-] 未在 ESP 中找到 S:\EFI\BOOT\unlock.log 日志文件
)
echo [+] 正在卸载 S: 盘...
mountvol S: /D >nul 2>&1
echo.
pause
goto menu

:gpu_limit
echo.
echo [+] 正在通过 nvidia-smi 查询显卡功耗上限...
where nvidia-smi >nul 2>&1
if %errorlevel% equ 0 (
    nvidia-smi --query-gpu=power.limit,power.default_limit,power.max_limit --format=csv
) else (
    echo [-] 未在系统 PATH 中找到 nvidia-smi 命令。
)
echo.
pause
goto menu

:deploy_efi
echo.
set "EFI_SRC="
if exist build\BOOTX64.efi (
    set "EFI_SRC=build\BOOTX64.efi"
) else if exist BOOTX64.efi (
    set "EFI_SRC=BOOTX64.efi"
)

if "%EFI_SRC%"=="" (
    echo [-] 错误: 未在 build\ 目录或当前目录找到可用的 BOOTX64.efi 文件！
    echo 请先运行 build.bat 进行编译。
    echo.
    pause
    goto menu
)

echo [+] 正在释放 S: 盘占位...
mountvol S: /D >nul 2>&1
echo [+] 正在挂载 ESP 分区为 S: 盘...
mountvol S: /S >nul 2>&1
if %errorlevel% neq 0 (
    echo [-] 错误: 挂载 ESP 分区失败。
    pause
    goto menu
)

if not exist S:\EFI\BOOT (
    echo [+] 创建目标文件夹 S:\EFI\BOOT...
    md S:\EFI\BOOT >nul 2>&1
)

if exist S:\EFI\BOOT\BOOTX64.efi (
    echo [+] 正在备份旧文件为 BOOTX64.efi.bak...
    copy /Y S:\EFI\BOOT\BOOTX64.efi S:\EFI\BOOT\BOOTX64.efi.bak >nul 2>&1
)

echo [+] 正在复制新的 BOOTX64.efi 到 S:\EFI\BOOT\BOOTX64.efi...
copy /Y %EFI_SRC% S:\EFI\BOOT\BOOTX64.efi >nul 2>&1
if %errorlevel% equ 0 (
    echo [+] 复制 bootloader 成功！
) else (
    echo [-] 错误: 复制 bootloader 失败。
)

if exist version.txt (
    echo [+] 正在复制 version.txt 到 S:\EFI\BOOT\version.txt...
    copy /Y version.txt S:\EFI\BOOT\version.txt >nul 2>&1
)

echo [+] 正在安全卸载 S: 盘...
mountvol S: /D >nul 2>&1
echo.
pause
goto menu

:set_boot_order
cls
echo ==========================================================
echo       设置 Custom Bootloader 为 UEFI 第一启动项
echo ==========================================================
echo [+] 正在挂载 EFI 分区...
mountvol S: /D >nul 2>&1
mountvol S: /S
if not exist S:\EFI (
    echo [-] 挂载 EFI 分区失败！
    pause
    goto menu
)
set EFI_PATH=\EFI\BOOT\BOOTX64.efi
if not exist S:%EFI_PATH% (
    echo [-] 未找到: S:%EFI_PATH%
    echo 请先部署 BOOTX64.efi！
    pause
    goto menu
)
echo [+] 当前 EFI 目录文件:
dir S:\EFI\BOOT
echo.

echo [+] 正在配置 UEFI 启动项...
powershell -NoProfile -Command "$bcd = bcdedit /enum firmware; $guid = ''; foreach ($line in $bcd) { if ($line -match '(?:标识符|identifier)\s+(\{[a-f0-9-]{36}\})') { $curr = $Matches[1] } if ($line -match 'description\s+Custom Bootloader' -and $curr) { $guid = $curr; break } }; if (-not $guid) { Write-Host '[+] 未找到现有启动项，正在创建...'; $out = bcdedit /copy '{bootmgr}' /d 'Custom Bootloader'; if ($out -match '(\{[a-f0-9-]{36}\})') { $guid = $Matches[1]; 'default', 'resumeobject', 'displayorder', 'toolsdisplayorder', 'timeout' | ForEach-Object { bcdedit /deletevalue $guid $_ *>$null } } } else { Write-Host '[+] 找到已存在的 Custom Bootloader 启动项。' }; if ($guid) { Write-Host ('[+] 启动项 GUID: ' + $guid); bcdedit /set $guid device partition=S:; bcdedit /set $guid path '\EFI\BOOT\BOOTX64.efi'; bcdedit /set '{fwbootmgr}' displayorder $guid /addfirst; bcdedit /set '{fwbootmgr}' timeout 3; Write-Host '[+] 成功设置 Custom Bootloader 为第一启动项！' } else { Write-Error '[-] 错误: 无法定位或创建 Custom Bootloader 启动项。' }"

echo.
echo ==========================================================
echo 当前 firmware 启动项顺序:
echo ==========================================================
bcdedit /enum firmware
mountvol S: /D
pause
goto menu