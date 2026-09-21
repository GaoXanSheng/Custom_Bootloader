# check_wmi_suite.ps1 —— 一次判定：固件版本 / 方法存在性 / 调用状态
$ErrorActionPreference = "Continue"
$ns = "root\wmi"
try {
    $inst = Get-CimInstance -Namespace $ns -ClassName CustomDbUnlock -ErrorAction Stop
} catch {
    Write-Host "[-] 无法访问 WMI（需管理员）: $($_.Exception.Message)" -ForegroundColor Red
    exit 2
}
$obj = $inst[0]
Write-Host "[+] 实例: $($obj.InstanceName)"

# 1) 固件版本（判定部署的是哪个构建）
$v = Invoke-CimMethod -InputObject $obj -MethodName GetVersion
if ($v -and $v.BuildVersion) {
    $t = [DateTimeOffset]::FromUnixTimeSeconds([long]$v.BuildVersion).LocalDateTime
    Write-Host ("[1] 固件构建时间: {0}  (0x{1:X})" -f $t, $v.BuildVersion) -ForegroundColor Cyan
    Write-Host "    对照: 04:50=045047(全功能+260W预算) | 04:30=043010(有20/22) | 04:10=041011(有20无22) | 更早=无20/22"
} else {
    Write-Host "[1] GetVersion 无返回 —— 注入表未加载！" -ForegroundColor Red
}

# 2) GetDynamicBoost 四出参（判定 MOF 是否 4-out 新签名）
try {
    $g = Invoke-CimMethod -InputObject $obj -MethodName GetDynamicBoost -Arguments @{ Reserved = 0 }
    Write-Host ("[2] GetDynamicBoost: Dbfs={0} Cspl={1} Fppt={2} DbCapW={3}" -f $g.Dbfs, $g.Cspl, $g.Fppt, $g.DbCapW) -ForegroundColor Cyan
    if ($null -eq $g.DbCapW) { Write-Host "    DbCapW 为空 -> MOF 是旧 1-out 签名（旧固件）" -ForegroundColor Yellow }
} catch {
    Write-Host "[2] GetDynamicBoost 调用失败: $($_.Exception.Message)" -ForegroundColor Yellow
}

# 3) SetDbBoostCap(100)（判定 Method 20 存在性）
try {
    $r = Invoke-CimMethod -InputObject $obj -MethodName SetDbBoostCap -Arguments @{ Watts = 100 }
    Write-Host ("[3] SetDbBoostCap(100): Status={0}  (0=成功, 4294967295=表未加载/旧固件)" -f $r.Status) -ForegroundColor Cyan
} catch {
    Write-Host "[3] SetDbBoostCap 方法不存在（旧固件）: $($_.Exception.Message)" -ForegroundColor Yellow
}

# 4) SetGpuBudget(140)（判定 Method 22 存在性）
try {
    $r2 = Invoke-CimMethod -InputObject $obj -MethodName SetGpuBudget -Arguments @{ Watts = 140 }
    Write-Host ("[4] SetGpuBudget(140): Status={0}" -f $r2.Status) -ForegroundColor Cyan
} catch {
    Write-Host "[4] SetGpuBudget 方法不存在（固件早于 04:30 构建）" -ForegroundColor Yellow
}
