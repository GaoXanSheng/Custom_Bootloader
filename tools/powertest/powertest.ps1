[CmdletBinding()]
param(
    [int]$Seconds = 30,
    [int]$Window = 20,
    [string]$LhmDll = '',
    [double]$MinW = 0,
    [string]$FurmarkExe = '',
    [int]$Width = 2560,
    [int]$Height = 1440,
    [int]$Msaa = 0,
    [switch]$GpuOnly,
    [switch]$NoElevate
)

$ErrorActionPreference = 'Stop'

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Host '[-] 需要 PowerShell 7 (pwsh)，请通过 run.bat 启动。' -ForegroundColor Red
    exit 1
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin -and -not $NoElevate) {
    $exe = (Get-Process -Id $PID).Path
    $forward = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'))
    foreach ($kv in $PSBoundParameters.GetEnumerator()) {
        if ($kv.Key -eq 'NoElevate') { continue }
        if ($kv.Value -is [switch]) {
            if ($kv.Value.IsPresent) { $forward += ('-' + $kv.Key) }
        } else {
            $forward += ('-' + $kv.Key)
            $forward += [string]$kv.Value
        }
    }
    Start-Process -FilePath $exe -ArgumentList $forward -Verb RunAs
    exit
}

if (-not $LhmDll) {
    $found = Get-ChildItem 'D:\software' -Recurse -Filter LibreHardwareMonitorLib.dll -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $LhmDll = $found.FullName }
}
if (-not $LhmDll -or -not (Test-Path $LhmDll)) {
    throw '找不到 LibreHardwareMonitorLib.dll，请用 -LhmDll 指定路径'
}
Add-Type -Path $LhmDll

$cpuSrc = Join-Path $PSScriptRoot 'stress.rs'
$cpuExe = Join-Path $PSScriptRoot 'stress.exe'
if ((-not (Test-Path $cpuExe)) -or ((Get-Item $cpuExe).LastWriteTime -lt (Get-Item $cpuSrc).LastWriteTime)) {
    $rustc = Join-Path $env:USERPROFILE '.cargo\bin\rustc.exe'
    if (-not (Test-Path $rustc)) {
        $cmd = Get-Command rustc -ErrorAction SilentlyContinue
        if ($cmd) { $rustc = $cmd.Source }
    }
    if (-not $rustc) { throw '找不到 rustc，无法编译 stress.exe' }
    & $rustc -O -C target-cpu=native $cpuSrc -o $cpuExe
    if ($LASTEXITCODE -ne 0) { throw 'rustc 编译 stress.rs 失败' }
}

$gpuSrc = Join-Path $PSScriptRoot 'gputest.cs'
$gpuExe = Join-Path $PSScriptRoot 'gputest.exe'
if ((-not (Test-Path $gpuExe)) -or ((Get-Item $gpuExe).LastWriteTime -lt (Get-Item $gpuSrc).LastWriteTime)) {
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (-not (Test-Path $csc)) { $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe' }
    if (-not (Test-Path $csc)) { throw '找不到 csc.exe' }
    & $csc -nologo -platform:x64 -optimize+ ("-out:" + $gpuExe) $gpuSrc
    if ($LASTEXITCODE -ne 0) { throw 'csc 编译 gputest.cs 失败' }
}

if (-not $FurmarkExe) {
    $fm = 'D:\software\图吧工具箱\src\Tools\烤鸡工具\FurMark_win64\furmark.exe'
    if (Test-Path $fm) {
        $FurmarkExe = $fm
    } else {
        $foundFm = Get-ChildItem 'D:\software' -Recurse -Filter furmark.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($foundFm) { $FurmarkExe = $foundFm.FullName }
    }
}

$PowerType = [LibreHardwareMonitor.Hardware.SensorType]::Power
$TempType = [LibreHardwareMonitor.Hardware.SensorType]::Temperature

$computer = [LibreHardwareMonitor.Hardware.Computer]::new()
$computer.IsCpuEnabled = $true
$computer.IsGpuEnabled = $true
$computer.Open()

try {
    $cpu = $computer.Hardware | Where-Object { $_.HardwareType -eq [LibreHardwareMonitor.Hardware.HardwareType]::Cpu } | Select-Object -First 1
    if (-not $cpu) { throw 'LHM 未枚举到 CPU' }
    $gpu = $computer.Hardware | Where-Object { $_.HardwareType -eq [LibreHardwareMonitor.Hardware.HardwareType]::GpuNvidia } | Select-Object -First 1

    $smi = Join-Path $env:WINDIR 'System32\nvidia-smi.exe'
    $gpuLimitCur = 0.0
    $gpuLimitDef = 0.0
    if (Test-Path $smi) {
        try {
            $q = & $smi -q -d POWER 2>$null
            foreach ($line in $q) {
                if ($line -match 'Current Power Limit\s*:\s*([\d.]+)\s*W') { $gpuLimitCur = [double]$Matches[1] }
                elseif ($line -match 'Default Power Limit\s*:\s*([\d.]+)\s*W') { $gpuLimitDef = [double]$Matches[1] }
            }
        } catch { }
    }

    Write-Host ('[+] ' + $cpu.Name)
    if ($gpu) { Write-Host ('[+] ' + $gpu.Name) }
    if ($gpuLimitCur -gt 0) {
        Write-Host ('[+] GPU 功耗墙: 当前 ' + $gpuLimitCur + ' W / 默认 ' + $gpuLimitDef + ' W')
    }
    if ($GpuOnly) { Write-Host ('[+] GPU 单烤 ' + $Seconds + ' 秒 ...') }
    else { Write-Host ('[+] 双烤 ' + $Seconds + ' 秒 ...') }

    $cpuProc = $null
    if (-not $GpuOnly) {
        $cpuProc = Start-Process -FilePath $cpuExe -ArgumentList @($Seconds, [Environment]::ProcessorCount) -PassThru -WindowStyle Hidden
    }
    $gpuProc = $null
    $gpuOut = Join-Path $env:TEMP ('gputest_' + $PID + '.log')
    if ($gpu) {
        if ($FurmarkExe) {
            $fmArgs = @('--demo', 'furmark-gl', '--max-time', $Seconds, '--width', $Width, '--height', $Height, '--no-osi', '--hpgfx', '1', '--disable-logfile', '--disable-traces')
            if ($Msaa -gt 0) { $fmArgs += @('--msaa', $Msaa) }
            $gpuProc = Start-Process -FilePath $FurmarkExe -WorkingDirectory (Split-Path -Parent $FurmarkExe) -ArgumentList $fmArgs -PassThru
        } else {
            $gpuProc = Start-Process -FilePath $gpuExe -ArgumentList @($Seconds) -PassThru -WindowStyle Hidden -RedirectStandardOutput $gpuOut
        }
    }

    $samples = @()
    $gpuKilled = $false
    try {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
            $cpu.Update()
            $sensors = $cpu.Sensors

            $cpuPower = 0.0
            $pkg = $sensors | Where-Object { $_.SensorType -eq $PowerType -and $_.Name -eq 'Package' } | Select-Object -First 1
            if ($pkg -and $null -ne $pkg.Value -and [double]$pkg.Value -gt 0) {
                $cpuPower = [double]$pkg.Value
            } else {
                foreach ($c in ($sensors | Where-Object { $_.SensorType -eq $PowerType -and $_.Name -like 'Core #* (SMU)' })) {
                    if ($null -ne $c.Value) { $cpuPower += [double]$c.Value }
                }
            }
            $cpuTemp = 0.0
            $tdie = $sensors | Where-Object { $_.SensorType -eq $TempType -and $_.Name -eq 'Core (Tctl/Tdie)' } | Select-Object -First 1
            if ($tdie -and $null -ne $tdie.Value) { $cpuTemp = [double]$tdie.Value }

            $gpuPower = 0.0
            $gpuTemp = 0.0
            if ($gpu) {
                $gpu.Update()
                $gp = $gpu.Sensors | Where-Object { $_.SensorType -eq $PowerType -and $_.Name -eq 'GPU Package' } | Select-Object -First 1
                if (-not $gp) {
                    $gp = $gpu.Sensors | Where-Object { $_.SensorType -eq $PowerType } | Select-Object -First 1
                }
                if ($gp -and $null -ne $gp.Value) { $gpuPower = [double]$gp.Value }
                $gt = $gpu.Sensors | Where-Object { $_.SensorType -eq $TempType -and $_.Name -eq 'GPU Core' } | Select-Object -First 1
                if ($gt -and $null -ne $gt.Value) { $gpuTemp = [double]$gt.Value }
            }

            $samples += [pscustomobject]@{ t = $sw.Elapsed.TotalSeconds; cpu = $cpuPower; gpu = $gpuPower; cpu_t = $cpuTemp; gpu_t = $gpuTemp }
            Start-Sleep -Milliseconds 1000
        }
    } finally {
        if ($cpuProc -and -not $cpuProc.HasExited) { $cpuProc.Kill() }
        if ($cpuProc) { $cpuProc.WaitForExit() }
        if ($gpuProc -and -not $gpuProc.HasExited) { $gpuProc.Kill(); $gpuKilled = $true }
        if ($gpuProc) { $gpuProc.WaitForExit() }
    }

    $tail = @($samples | Where-Object { $_.t -ge ($Seconds - $Window) })
    if ($tail.Count -eq 0) { $tail = $samples }

    $cpuAvg = [math]::Round((($tail | Measure-Object -Property cpu -Average).Average), 1)
    $gpuAvg = [math]::Round((($tail | Measure-Object -Property gpu -Average).Average), 1)
    $sumAvg = [math]::Round($cpuAvg + $gpuAvg, 1)
    $cpuTmax = [math]::Round((($tail | Measure-Object -Property cpu_t -Maximum).Maximum), 1)
    $gpuTmax = [math]::Round((($tail | Measure-Object -Property gpu_t -Maximum).Maximum), 1)

    Write-Host ''
    Write-Host ('最后 ' + $Window + ' 秒平均: CPU ' + $cpuAvg + ' W | GPU ' + $gpuAvg + ' W | 合计 ' + $sumAvg + ' W') -ForegroundColor Green
    Write-Host ('温度峰值: CPU ' + $cpuTmax + ' C | GPU ' + $gpuTmax + ' C    样本数: ' + $tail.Count)
    if ($gpuLimitCur -gt 0) {
        Write-Host ('GPU 功耗墙: ' + $gpuLimitCur + ' W（默认 ' + $gpuLimitDef + ' W）')
    }

    if ($cpuAvg -le 0) {
        Write-Host '[!] CPU 功耗读数为 0：LHM 驱动未加载（需管理员）。' -ForegroundColor Yellow
    }
    if ($gpu -and $gpuAvg -le 0) {
        Write-Host '[!] GPU 功耗读数为 0：LHM 未读到 NVIDIA 功耗传感器。' -ForegroundColor Yellow
    }
    if ($gpuProc -and -not $gpuKilled -and $gpuProc.ExitCode -ne 0) {
        Write-Host ('[!] GPU 压力程序退出码 ' + $gpuProc.ExitCode + '，日志: ' + $gpuOut) -ForegroundColor Yellow
    }
    if ($MinW -gt 0) {
        if ($sumAvg -ge $MinW) { Write-Host ('[+] 判定: 合计达到 ' + $MinW + ' W 阈值') -ForegroundColor Green }
        else { Write-Host ('[-] 判定: 合计未达 ' + $MinW + ' W 阈值') -ForegroundColor Red }
    }
} finally {
    $computer.Close()
}

if ($isAdmin -and -not $NoElevate) {
    Write-Host ''
    Read-Host '按回车退出' | Out-Null
}
