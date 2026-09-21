# probe_boost_floor.ps1 —— GPU 增压期间 CPU 40W 钳制定位探针
#
# 背景（2026-09-21 实测）：运行时 DSDT 已确认是补丁版（verify_runtime 19 项
# 全 PASS），_Q81/_Q82 每次 DB 事件都会经 FNQS/COMM 把 110W 下限重推 SMU，
# 但 GPU 一满载 CPU 仍被压到 ~40W —— 钳制发生在 AML 层以下。本探针用注入
# DBUL 的 WMI（root\wmi CustomDbUnlock）在 GPU 满载期间逐秒采样 EC，判定
# 40W 到底是谁写的。
#
# 用法（必须管理员 PowerShell）：
#   第 1 步（观察基线，跑 GPU 压力时执行）：
#     powershell -ExecutionPolicy Bypass -File tools\probe_boost_floor.ps1 -Seconds 60
#   第 2 步（可选，验证运行时推送能否顶开钳制）：
#     powershell -ExecutionPolicy Bypass -File tools\probe_boost_floor.ps1 -Seconds 60 -Push
#
# 判定规则（看结尾自动结论）：
#   A. 采样中 CSPL(EC 0xF6) 出现 < 80W 的值 → 增压期间有角色把低值写进了
#      EC（EC 固件直写或驱动路径）→ 顺藤摸瓜找写入者。
#   B. CSPL 全程 >= 80W（甚至保持 110W）而 CPU 实测仍 ~40W → 钳制在 SMU
#      内部动态增压表（NVPCF fun#3 阶梯只是它的镜像副本）。此时看 -Push
#      结果：推送后 CPU 恢复 → 可用"增压期间持续重推"对策；仍 40W →
#      SMU 固件硬锁，AML 层无解，只能从 NVIDIA 驱动侧或 SMU 邮箱另想。
#
# 采样项（EC 偏移来自 SsdtUnlockDB.asl Method 8/17 白名单）：
#   DBFS(Method 2)、ITSM 0xE4、DGPU 0xF2、CPUT 0xF3、STSM 0xF4、
#   CSPL 0xF6（CPU PL1，W）、FPPT 0xF7（CPU PL2，W）、CTCL 0xF8

param(
    [int]$Seconds = 60,
    [switch]$Push,                 # 每 5 秒调一次 ApplyPowerLimits(110, 120)
    [switch]$Diff,                 # 全量 EC 差分模式：空闲基线 vs 增压钳制中
    [string]$OutCsv = "build\boost_floor.csv"
)

$ErrorActionPreference = "Stop"
$ns = "root\wmi"
$cls = "CustomDbUnlock"

# ---- 前置检查：WMI 类可访问（需要管理员） ----
try {
    $inst = Get-CimInstance -Namespace $ns -ClassName $cls -ErrorAction Stop
} catch {
    Write-Host "[-] 无法访问 root\wmi\$cls ：$($_.Exception.Message)" -ForegroundColor Red
    Write-Host "    请用【管理员】PowerShell 运行本脚本（WMI 固件类需要提权）。"
    exit 2
}
$obj = $inst[0]
Write-Host "[+] CustomDbUnlock 在线 (Instance: $($obj.InstanceName))"
# 类实例存在本身即证明注入 SSDT 已被 Windows 加载（aa_0 = DBUL _UID 0xAA）；
# CustomDbUnlock 是动态实例类，方法必须经 -InputObject 在实例上调用，
# 用 -ClassName 类级调用会报 0x8004102f 无效的方法参数。

# 探活：GetVersion 返回构建 epoch（仅输出参数，不能带 -Arguments）
$ver = Invoke-CimMethod -InputObject $obj -MethodName GetVersion -ErrorAction SilentlyContinue
if ($ver -and $ver.BuildVersion) {
    $epoch = [DateTimeOffset]::FromUnixTimeSeconds([long]$ver.BuildVersion).LocalDateTime
    Write-Host "[+] GetVersion = $($ver.BuildVersion) ($($epoch.ToString('yyyy-MM-dd HH:mm:ss')))"
} else {
    Write-Host "[!] GetVersion 无返回（不影响采样；实例存在已证明表在）" -ForegroundColor Yellow
}

# ---- -Diff 模式：全量 EC 差分（找增压钳制期间变化的寄存器） ----
# 已证明钳制不在 AML/EC 限值层（CSPL 全程 110W）。若增压时仍有别的 EC 寄存器
# 变化（尤其值变成 0x28=40），那就是 SMU 档位在 EC 的镜像/旋钮；若零差异，
# 调度完全在 SMU 内部，AML/EC 层无解，剩余杠杆见脚本头注释。
$script:FailCount = 0
function Get-Ec([uint32]$Off) {
    try {
        $r = Invoke-CimMethod -InputObject $script:obj -MethodName GetEcByte -Arguments @{ RegisterOffset = $Off }
        $script:FailCount = 0
        return [int]$r.ReadVal
    } catch {
        $script:FailCount++
        if ($script:FailCount -ge 3) { throw }
        return -1
    }
}


$ecNames = @{
0x10 = 'EVMR'
    0x11 = 'EVMN'
    0x12 = 'EVT1'
    0x13 = 'EVT2'
    0x14 = 'HTKS'
    0x15 = 'HTKE'
    0x17 = 'TSR1'
    0x18 = 'TSR2'
    0x19 = 'TSR3'
    0x1A = 'TSR4'
    0x1B = 'TSR5'
    0x1C = 'TSR6'
    0x1D = 'TSR7'
    0x1E = 'TSR8'
    0x1F = 'TSR9'
    0x23 = 'GSTS'
    0x24 = 'HKST'
    0x26 = 'AST1'
    0x28 = 'SMPR'
    0x29 = 'SMST'
    0x2A = 'SMAD'
    0x2B = 'SMCD'
    0x4C = 'SMCN'
    0x56 = 'BS56'
    0x5F = 'FASP'
    0x60 = 'ECWR'
    0x61 = 'PAWT'
    0x7C = 'BRSC'
    0x7D = 'MIDL'
    0x7E = 'MIDH'
    0x7F = 'HIDL'
    0x80 = 'HIDH'
    0x81 = 'FWVL'
    0x82 = 'FWVH'
    0x83 = 'DAVL'
    0x84 = 'DAVH'
    0x8E = 'AWHG'
    0x8F = 'AWLW'
    0x96 = 'BATM'
    0x97 = 'BBHL'
    0x98 = 'BBLP'
    0x99 = 'BBHM'
    0x9A = 'KBNL'
    0x9B = 'F1HI'
    0x9C = 'F1LO'
    0x9D = 'F2HI'
    0x9E = 'F2LO'
    0x9F = 'PABD'
    0xD0 = 'TFLG'
    0xD1 = 'GFLG'
    0xD2 = 'GPMD'
    0xE1 = 'SSDK'
    0xE3 = 'BPWM'
    0xE4 = 'ITSM'
    0xE7 = 'ECTP'
    0xE8 = 'LEDM'
    0xE9 = 'RGBR'
    0xEA = 'RGBG'
    0xEB = 'RGBB'
    0xF2 = 'DGPU'
    0xF3 = 'CPUT'
    0xF4 = 'STSM'
    0xF6 = 'CSPL'
    0xF7 = 'FPPT'
    0xF8 = 'CTCL'
}

function Scan-AllEc {
    $b = New-Object byte[] 256
    for ($o = 0; $o -lt 256; $o++) {
        $v = Get-Ec ([uint32]$o)
        if ($v -ge 0) { $b[$o] = [byte]$v }
        Write-Progress -Activity "EC 全量扫描" -Status "$o/256" -PercentComplete ([int]($o / 256 * 100))
    }
    Write-Progress -Activity "EC 全量扫描" -Completed
    return $b
}

if ($Diff) {
    Write-Host "[*] 第 1 遍：空闲基线扫描（现在不要跑 GPU 压力；256 字节约需 20-60 秒）..."
    $base = Scan-AllEc
    Write-Host "[+] 基线完成。现在启动 GPU 压力，等 CPU 掉到 ~40W 后回到本窗口按回车。"
    Read-Host "按回车继续" | Out-Null
    Write-Host "[*] 第 2 遍：增压钳制中扫描..."
    $load = Scan-AllEc
    Write-Host ""
    Write-Host "========== EC 差分（空闲 vs 增压钳制中）=========="
    $n = 0
    for ($o = 0; $o -lt 256; $o++) {
        if ($base[$o] -ne $load[$o]) {
            $n++
            $name = $ecNames[[int]$o]
            if (-not $name) { $name = "-" }
            $mark = ""
            if ($load[$o] -eq 0x28) { $mark = "   <== 40 (0x28)!" }
            Write-Host ("  0x{0:X2} {1,-6} : 0x{2:X2}({3}) -> 0x{4:X2}({5}){6}" -f $o, $name, $base[$o], $base[$o], $load[$o], $load[$o], $mark)
        }
    }
    if ($n -eq 0) {
        Write-Host "  （零差异：增压调度完全不经过 EC RAM —— 钳制在 SMU 固件内部，AML/EC 层无解）" -ForegroundColor Yellow
    } else {
        Write-Host "共 $n 个寄存器变化。重点：值变成 0x28(40) 的、或瓦数语义字段（CSPL/FPPT/CTCL/TFLG...）。" -ForegroundColor Yellow
        Write-Host "对照 wmi_varmap.py 的语义表判断哪个是档位旋钮；在白名单内的偏移可用 Method 8 (SetEcRegister) 试写对抗。"
    }
    exit 0
}


New-Item -ItemType Directory -Force -Path (Split-Path $OutCsv) | Out-Null
"time,dbfs,itsm,dgpu,cput,stsm,cspl_w,fppt_w,ctcl,cusl,cuct,t6c,t6v,gpu_w,gpu_cap,pushed" | Set-Content -Path $OutCsv -Encoding UTF8

Write-Host "`n[*] 开始采样 ${Seconds}s（现在去跑 GPU 压力/CPU 压力并观察 CPU 功率何时掉 40W）..."
if ($Push) { Write-Host "[*] -Push 已开启：每 5 秒 ApplyPowerLimits(110, 120) 强推一次" -ForegroundColor Cyan }

$minCspl = 999
$minRow = ""
$script:PrevCspl = $null
$end = (Get-Date).AddSeconds($Seconds)
$i = 0
while ((Get-Date) -lt $end) {
    $i++
    $dbfs = $null
    try { $dbfs = (Invoke-CimMethod -InputObject $obj -MethodName GetDbfs).Value } catch { $dbfs = $null }
    $itsm = Get-Ec 0xE4; $dgpu = Get-Ec 0xF2; $cput = Get-Ec 0xF3; $stsm = Get-Ec 0xF4
    $cspl = Get-Ec 0xF6; $fppt = Get-Ec 0xF7; $ctcl = Get-Ec 0xF8

    # NVPCF 驱动调用追踪（Method 21）：CUSL/CUCT=驱动经 fun#5 下发的最后值，
    # T6C/T6V=fun#6(INC6=1) 调用计数与最后下发的 NCHP(CPU W)
    $cusl = $cuct = $t6c = $t6v = -1
    try {
        $tr = Invoke-CimMethod -InputObject $obj -MethodName GetNpcfTrace
        $cusl = [int]$tr.Cusl; $cuct = [int]$tr.Cuct
        $t6c = [int]$tr.Fun6Count; $t6v = [int]$tr.LastNchp
    } catch {}

    # GPU 实际功率与驱动侧功率上限（钳制时刻 GPU 到底多少瓦、cap 是 121 还是 140）
    $gpuW = ""; $gpuCap = ""
    try {
        $q = (& nvidia-smi --query-gpu=power.draw,power.limit --format=csv,noheader,nounits) -split ","
        $gpuW = $q[0].Trim(); $gpuCap = $q[1].Trim()
    } catch {}

    $pushed = ""
    if ($Push -and ($i % 5 -eq 1)) {
        $r = Invoke-CimMethod -InputObject $obj -MethodName ApplyPowerLimits -Arguments @{ CsplWatts = 110; FpptWatts = 120 }
        $pushed = "PUSH(status=$($r.Status))"
    }

    $t = Get-Date -Format "HH:mm:ss"
    "{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15}" -f $t, $dbfs, $itsm, $dgpu, $cput, $stsm, $cspl, $fppt, $ctcl, $cusl, $cuct, $t6c, $t6v, $gpuW, $gpuCap, $pushed |
        Add-Content -Path $OutCsv -Encoding UTF8
    $chg = ""
    if ($script:PrevCspl -ne $null -and $cspl -ne $script:PrevCspl) { $chg = " <== CSPL $($_)change $($script:PrevCspl)->$cspl" }
    $script:PrevCspl = $cspl
    Write-Host ("{0}  DBFS={1} ITSM={2} DGPU=0x{3:X2}  GPU={4,3}W/cap{5,3}  CSPL={6,3} FPPT={7,3}  fun5:{8}/{9} fun6:n={10},last={11}(0x{12:X2}){13}  {14}" -f `
            $t, $dbfs, $itsm, $dgpu, $gpuW, $gpuCap, $cspl, $fppt, $cusl, $cuct, $t6c, $t6v, $t6v, $chg, $pushed)

    if ($cspl -lt $minCspl) { $minCspl = $cspl; $minRow = "$t DBFS=$dbfs DGPU=0x$($dgpu.ToString('X2'))" }
    Start-Sleep -Milliseconds 1000
}

Write-Host "`n========== 结论 =========="
Write-Host "采样期间 CSPL 最小值: ${minCspl}W （出现在 $minRow）"
if ($minCspl -lt 80) {
    Write-Host "[A] 增压期间有角色把 CSPL 写到了 ${minCspl}W（< 80W 下限）→ 40W 是被写进 EC 的；对照 CSV 时间轴找写入者（EC 固件直写最可能）。" -ForegroundColor Yellow
} else {
    Write-Host "[B] CSPL 全程 >= ${minCspl}W：EC/AML 层从未低于下限，而 CPU 实测 ~40W → 钳制在 SMU 内部动态增压表。" -ForegroundColor Yellow
    if ($Push) {
        Write-Host "    -Push 期间若 CPU 功率随每次 PUSH 回升后又跌回 → SMU 接受推送但会被增压调度重新压回（可做持续重推对策）；"
        Write-Host "    若 CPU 始终 ~40W → SMU 固件硬锁，AML 层无解。"
    } else {
        Write-Host "    建议再做第二步：加 -Push 重跑，验证运行时推送能否顶开钳制。"
    }
}
Write-Host "---------- NVPCF 追踪判读 ----------"
if ($t6c -gt 0) {
    Write-Host "fun#6 被驱动调用过 $t6c 次，最后下发的 CPU 功率值 = $t6v W (0x$($t6v.ToString('X2')))" -ForegroundColor Cyan
    if ($t6v -gt 0 -and $t6v -le 60) {
        Write-Host "  ==> 实锤：原厂驱动正在通过 NVPCF fun#6 把 CPU 压到 ${t6v}W（请求侧即 N 卡 DB 通信）。" -ForegroundColor Yellow
        Write-Host "      对策方向：改 SSDT4 的 fun2 字段（MAGA/MIGA/DROP/PC02）或 fun1 能力位，让驱动读到不同的预算；或采用解锁 DB 驱动方案。"
    }
} else {
    Write-Host "fun#6 从未被调用 (T6C=0)、CUSL/CUCT=$cusl/$cuct —— 驱动不经 NVPCF 下发 CPU 限值，40W 与 ACPI 通信无关。"
}
Write-Host "CSV 已保存: $OutCsv"
