# CustomDbUnlock WMI 接口文档

## 概述

**命名空间:** `root\wmi`

**类名:** `CustomDbUnlock`

**GUID:** `{850C1B62-A3D9-4E65-B8D5-DE3C74C9B38D}`

**描述:** 自定义 Dynamic Boost 解锁 WMI 接口。通过 ACPI WMI 设备 (`\_SB.PCI0.LPC0.DBUL`) 暴露，允许用户态程序读写 DBFS 标志位以控制 NVIDIA Dynamic Boost 的开关状态。

---

## 属性

| 属性名 | 类型 | 访问 | 说明 |
|--------|------|------|------|
| `InstanceName` | `string` | 只读 (Key) | WMI 实例名称 |
| `Active` | `boolean` | 只读 | WMI 设备激活状态 |

---

## 方法

### 1. SetDbfs — 设置 DBFS 标志

**Method ID:** `1`

**描述:** 写入 DBFS 标志值，控制 Dynamic Boost 开关。写入后自动触发 EC COMM 方法使设置生效。

#### 参数

| 参数名 | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `Value` | 输入 | `uint32` | DBFS 目标值：`0` = 禁用 Dynamic Boost，`1` = 启用 Dynamic Boost |
| `Status` | 输出 | `uint32` | 操作状态码：`0` = 成功，`0xFFFFFFFF` = 失败 |

#### PowerShell 调用示例

```powershell
# 禁用 Dynamic Boost
$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy
$result = $ins.SetDbfs(0)
Write-Host "Status: $($result.Status)"

# 启用 Dynamic Boost
$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy
$result = $ins.SetDbfs(1)
Write-Host "Status: $($result.Status)"
```

---

### 2. ReadDbfs — 读取 DBFS 标志

**Method ID:** `2`

**描述:** 读取当前 DBFS 标志值，用于查询 Dynamic Boost 当前状态。

#### 参数

| 参数名 | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `Value` | 输出 | `uint32` | 当前 DBFS 值：`0` = 禁用，`1` = 启用，`0xFFFFFFFF` = 读取失败（DBFS 不存在） |

#### PowerShell 调用示例

```powershell
$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy
$result = $ins.ReadDbfs()
Write-Host "DBFS Value: $($result.Value)"
```

---

## 技术细节

### ACPI 设备路径

```
\_SB.PCI0.LPC0.DBUL
```

- `_HID`: `PNP0C14` (WMI 设备标准 HID)
- `_UID`: `0xAA`
- `_STA`: `0x0F` (设备存在、启用、可用)

### WDG 数据结构

| 条目 | Object ID | GUID | 类型 |
|------|-----------|------|------|
| WMTF (方法) | `TF` | `{850C1B62-A3D9-4E65-B8D5-DE3C74C9B38D}` | WMI 方法 (0x02) |
| WQBA (数据) | `BA` | `{05901221-D566-11D1-B2F0-00A0C9062910}` | 二进制 MOF (0x00) |

### 自动化初始化

`_INI` 方法在设备加载时自动执行：
1. 将 `\DBFS` 初始化为 `0`（默认禁用 Dynamic Boost）
2. 调用 `\_SB.PCI0.LPC0.H_EC.COMM()` 提交 EC 设置

### 返回值编码

所有 WMI 方法通过 4 字节 Buffer (`RBUF`) 返回 `uint32` 值，与 WMI MOF 定义的 `[out] uint32` 参数映射对齐。
