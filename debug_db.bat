@echo off
title SSDT-UnlockDB WMI/ACPI 调试与部署工具
color 0B

:: 检查管理员权限
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] 需要管理员权限。正在自动请求提权...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

:: 强制切换工作目录到脚本所在文件夹
cd /d "%~dp0"

:menu
cls
echo ==========================================================
echo           SSDT-UnlockDB WMI/ACPI 调试与部署菜单
echo ==========================================================
echo.
echo  [1] 检测 WMI 类注册状态 (CustomDbUnlock)
echo  [2] 读取当前 DBFS 开关值 (WMI 方法: ReadDbfs)
echo  [3] 禁用 Dynamic Boost (WMI 方法: SetDbfs = 0)
echo  [4] 启用 Dynamic Boost (WMI 方法: SetDbfs = 1)
echo  [5] 挂载 ESP 分区并打印引导日志 (unlock.log)
echo  [6] 查询 NVIDIA 显卡当前功耗限制 (nvidia-smi)
echo  [7] 一键部署 BOOTX64.efi 到 ESP 分区 (S:\EFI\BOOT\BOOTX64.efi)
echo  [8] 一键设置 Custom Bootloader 为默认开机启动项 (bcdedit)
echo  [9] 退出
echo.
echo ==========================================================
set /p choice="请输入选项 (1-9): "

if "%choice%"=="1" goto check_wmi
if "%choice%"=="2" goto read_dbfs
if "%choice%"=="3" goto disable_db
if "%choice%"=="4" goto enable_db
if "%choice%"=="5" goto view_log
if "%choice%"=="6" goto gpu_limit
if "%choice%"=="7" goto deploy_efi
if "%choice%"=="8" goto set_boot_order
if "%choice%"=="9" exit /b
goto menu

:check_wmi
echo.
echo [+] 正在查询 CustomDbUnlock 注册状态...
powershell -NoProfile -Command "Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue | Format-List -Property Active, InstanceName, __SERVER"
if %errorlevel% neq 0 (
    echo [-] 错误: 未能在 WMI 中找到 CustomDbUnlock 类。
)
echo.
pause
goto menu

:read_dbfs
echo.
echo [+] 正在通过 COM 加密调用 ReadDbfs 方法...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.ReadDbfs(); Write-Host '方法返回值 (DBFS 状态):' $res.Value } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例。' }"
echo.
pause
goto menu

:disable_db
echo.
echo [+] 正在通过 COM 加密设置 DBFS 为 0 (禁用 Dynamic Boost)...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.SetDbfs(0); Write-Host 'SetDbfs 方法返回值:' $res.Status } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例。' }"
echo.
pause
goto menu

:enable_db
echo.
echo [+] 正在通过 COM 加密设置 DBFS 为 1 (启用 Dynamic Boost)...
powershell -NoProfile -Command "$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy -ErrorAction SilentlyContinue; if ($ins) { $res = $ins.SetDbfs(1); Write-Host 'SetDbfs 方法返回值:' $res.Status } else { Write-Host '[-] 错误: 未找到 CustomDbUnlock 实例。' }"
echo.
pause
goto menu

:view_log
echo.
echo [+] 正在清理并释放 S: 盘占位...
mountvol S: /D >nul 2>&1
echo [+] 正在重新将 ESP 分区挂载为 S: 盘...
mountvol S: /S >nul 2>&1
if exist S:\EFI\BOOT\unlock.log (
    echo [+] 成功读取 S:\EFI\BOOT\unlock.log 日志内容:
    echo -------------------------------------------------------------
    type S:\EFI\BOOT\unlock.log
    echo -------------------------------------------------------------
) else (
    echo [-] 未在 ESP 分区中找到 S:\EFI\BOOT\unlock.log 日志文件。
)
echo [+] 正在卸载 S: 盘...
mountvol S: /D >nul 2>&1
echo.
pause
goto menu

:gpu_limit
echo.
echo [+] 正在通过 nvidia-smi 查询显卡功耗配置...
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
if not exist build\BOOTX64.efi (
    echo [-] 错误: 在当前目录下未找到编译好的 build\BOOTX64.efi 文件！
    echo 请先运行 build.bat 进行编译生成。
    echo.
    pause
    goto menu
)

echo [+] 正在清理并释放 S: 盘占位...
mountvol S: /D >nul 2>&1
echo [+] 正在将 ESP 分区挂载为 S: 盘...
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
    echo [+] 正在备份旧的引导文件为 BOOTX64.efi.bak...
    copy /Y S:\EFI\BOOT\BOOTX64.efi S:\EFI\BOOT\BOOTX64.efi.bak >nul 2>&1
)

echo [+] 正在将最新的 BOOTX64.efi 部署到 S:\EFI\BOOT\BOOTX64.efi...
copy /Y build\BOOTX64.efi S:\EFI\BOOT\BOOTX64.efi >nul 2>&1
if %errorlevel% equ 0 (
    echo [+] 恭喜！最新引导文件已成功部署到 ESP 分区！
) else (
    echo [-] 错误: 文件拷贝失败。
)

echo [+] 正在安全卸载 S: 盘...
mountvol S: /D >nul 2>&1
echo.
pause
goto menu

:set_boot_order
cls
echo ==========================================================
echo       创建 Custom Bootloader UEFI 启动项
echo ==========================================================
echo [+] 检查 EFI 分区...
mountvol S: /D >nul 2>&1
mountvol S: /S
if not exist S:\EFI (
    echo [-] EFI 分区挂载失败
    pause
    goto menu
)
set EFI_PATH=\EFI\BOOT\BOOTX64.efi
if not exist S:%EFI_PATH% (
    echo [-] 未找到:
    echo S:%EFI_PATH%
    echo 请先部署 BOOTX64.efi
    pause
    goto menu
)
echo [+] 当前 EFI:
dir S:\EFI\BOOT
echo.

echo [+] 正在配置 UEFI 启动项...
:: 使用克隆 {bootmgr} 并清理冗余属性的方案来绕过系统限制
powershell -NoProfile -Command "$bcd = bcdedit /enum firmware; $guid = ''; foreach ($line in $bcd) { if ($line -match '(?:标识符|identifier)\s+(\{[a-f0-9-]{36}\})') { $curr = $Matches[1] } if ($line -match 'description\s+Custom Bootloader' -and $curr) { $guid = $curr; break } }; if (-not $guid) { Write-Host '[+] 未找到已有启动项，正在克隆并创建新项...'; $out = bcdedit /copy '{bootmgr}' /d 'Custom Bootloader'; if ($out -match '(\{[a-f0-9-]{36}\})') { $guid = $Matches[1]; 'default', 'resumeobject', 'displayorder', 'toolsdisplayorder', 'timeout' | ForEach-Object { bcdedit /deletevalue $guid $_ *>$null } } } else { Write-Host '[+] 找到已存在的 Custom Bootloader 启动项。' }; if ($guid) { Write-Host ('[+] 正在配置启动项: ' + $guid); bcdedit /set $guid device partition=S:; bcdedit /set $guid path '\EFI\BOOT\BOOTX64.efi'; bcdedit /set '{fwbootmgr}' displayorder $guid /addfirst; bcdedit /set '{fwbootmgr}' timeout 3; Write-Host '[+] 成功设置 Custom Bootloader 为第一启动项！' } else { Write-Error '[-] 错误: 无法定位或创建 Custom Bootloader 引导项。' }"

echo.
echo ==========================================================
echo 操作结束
echo ==========================================================
bcdedit /enum firmware
mountvol S: /D
pause
goto menu