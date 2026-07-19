# WMI 接口规范：CustomDbUnlock

本文档完整记录自定义 WMI 接口 `root\wmi\CustomDbUnlock` 的设计与实现。

---

## 1. 概述

| 属性 | 值 |
|------|-----|
| 命名空间 | `root\wmi` |
| 类名 | `CustomDbUnlock` |
| WMI GUID | `{850C1B62-A3D9-4E65-B8D5-DE3C74C9B38D}` |
| ACPI 设备路径 | `\_SB.PCI0.LPC0.DBUL` |
| ACPI 设备 HID | `PNP0C14`（标准 WMI 映射标识） |
| ACPI 设备 UID | `0xAA` |
| 用途 | 运行时读写 DBFS 标志，控制 NVIDIA Dynamic Boost 开关 |

---

## 2. MOF 类定义

文件：`src/acpi/SsdtUnlockDB.mof`

```c
[WMI,
 Dynamic,
 Provider("WmiProv"),
 GUID("{850C1B62-A3D9-4E65-B8D5-DE3C74C9B38D}"),
 locale("MS\\0x409"),
 Description("Custom DBUnlock WMI Interface")]
class CustomDbUnlock {
    [key, read] string InstanceName;
    [read] boolean Active;

    [WmiMethodId(1), Implemented, Description("Set DBFS Flag")]
    void SetDbfs([in] uint32 Value, [out] uint32 Status);

    [WmiMethodId(2), Implemented, Description("Read DBFS Flag")]
    void ReadDbfs([out] uint32 Value);
};
```

### 2.1 属性说明

| 属性 | 类型 | 修饰符 | 说明 |
|------|------|--------|------|
| `InstanceName` | `string` | `key, read` | WMI 实例名称（用作主键） |
| `Active` | `boolean` | `read` | 设备激活状态（由 _STA 返回 0x0F，始终为 TRUE） |

### 2.2 方法说明

| 方法 | Method ID | 输入参数 | 输出参数 | 功能 |
|------|-----------|---------|---------|------|
| `SetDbfs` | 1 | `[in] uint32 Value` | `[out] uint32 Status` | 写入 DBFS 标志：0=禁用，1=启用 |
| `ReadDbfs` | 2 | 无 | `[out] uint32 Value` | 读取当前 DBFS 标志值 |

---

## 3. ASL 设备定义

文件：`src/acpi/SsdtUnlockDB.asl`

### 3.1 设备声明

```asl
DefinitionBlock ("", "SSDT", 2, "CUSTOM", "DBUNLOCK", 0x00001000)
{
    External (\DBFS, IntObj)
    External (\_SB.PCI0.LPC0, DeviceObj)
    External (\_SB.PCI0.LPC0.H_EC.COMM, MethodObj)

    Scope (\_SB.PCI0.LPC0)
    {
        Device (DBUL)
        {
            Name (_HID, EisaId ("PNP0C14"))
            Name (_UID, 0xAA)
            ...
        }
    }
}
```

| 属性 | 值 | 说明 |
|------|-----|------|
| `_HID` | `PNP0C14`（`EisaId("PNP0C14")`） | 标准 WMI 设备硬件 ID |
| `_UID` | `0xAA` | 设备唯一标识符 |
| `_STA` | `0x0F`（固定返回） | bit0=存在, bit1=启用, bit2=显示, bit3=功能正常 |
| SSDT OEM ID | `"CUSTOM"` | 自定义标识，与 OEM 表明确区分 |
| SSDT OEM Table ID | `"DBUNLOCK"` | 自定义表标识 |
| OEM Revision | `0x00001000` | 版本号 |

### 3.2 WDG 数据块（WMI Data Block）

```asl
Name (_WDG, Buffer (0x28)  // 40 字节，包含两个 WDG 条目
{
    // 条目 1: WMTF — WMI 方法入口 (20 字节)
    0x62, 0x1B, 0x0C, 0x85,  // GUID Byte 0-3
    0xD9, 0xA3, 0x65, 0x4E,  // GUID Byte 4-7
    0xB8, 0xD5, 0xDE, 0x3C,  // GUID Byte 8-11
    0x74, 0xC9, 0xB3, 0x8D,  // GUID Byte 12-15
    0x54, 0x46,              // Object ID "TF" (0x54='T', 0x46='F')
    0x01,                    // Flags = 0x01
    0x02,                    // WMI Type = 0x02 (Method)

    // 条目 2: WQBA — 二进制 MOF 数据入口 (20 字节)
    0x21, 0x12, 0x90, 0x05,  // GUID Byte 0-3
    0x66, 0xD5, 0xD1, 0x11,  // GUID Byte 4-7
    0xB2, 0xF0, 0x00, 0xA0,  // GUID Byte 8-11
    0xC9, 0x06, 0x29, 0x10,  // GUID Byte 12-15
    0x42, 0x41,              // Object ID "BA" (0x42='B', 0x41='A')
    0x01,                    // Flags = 0x01
    0x00                     // WMI Type = 0x00 (Binary MOF Data)
})
```

#### WDG 条目详解

| 条目 | Object ID | GUID | WMI 类型 | Flags | 说明 |
|------|-----------|------|----------|-------|------|
| WMTF | `TF`（2 字节 ASCII） | `{850C1B62-A3D9-4E65-B8D5-DE3C74C9B38D}` | `0x02` | `0x01` | WMI 方法提供者，实现 SetDbfs 和 ReadDbfs |
| WQBA | `BA`（2 字节 ASCII） | `{05901221-D566-11D1-B2F0-00A0C9062910}` | `0x00` | `0x01` | WMI 二进制 MOF 数据（标准 WMI MOF GUID） |

> `{05901221-D566-11D1-B2F0-00A0C9062910}` 是 Microsoft 保留的标准 WMI 二进制 MOF GUID，Windows 内核将其识别为"此处有 MOF 类定义"。

### 3.3 WMTF 方法实现

```asl
Method (WMTF, 3, Serialized)  // Serialized: 全局互斥，防止并发调用
{
    Name (RBUF, Buffer (4) {0, 0, 0, 0})  // 返回缓冲区
    CreateDWordField (RBUF, Zero, RVAL)    // 将 RBUF 映射为 32 位字段

    If (LEqual (Arg0, Zero)) {}           // Arg0: WMI 数据块索引（未使用）

    // --- 子命令 1: SetDbfs (MethodId=1) ---
    If (LEqual (Arg1, One))
    {
        If (LGreaterEqual (SizeOf (Arg2), One))  // Arg2 至少 1 字节
        {
            Store (DerefOf (Index (Arg2, Zero)), Local0)  // 取首字节
            If (CondRefOf (\DBFS))
            {
                Store (Local0, \DBFS)                  // 写入全局 DBFS
            }
            If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM))
            {
                \_SB.PCI0.LPC0.H_EC.COMM ()           // 通知 EC
            }
        }
        Store (Zero, RVAL)
        Return (RBUF)
    }

    // --- 子命令 2: ReadDbfs (MethodId=2) ---
    If (LEqual (Arg1, 2))
    {
        If (CondRefOf (\DBFS))
        {
            Store (\DBFS, RVAL)                       // 读取当前 DBFS
        }
        Else
        {
            Store (0xFFFFFFFF, RVAL)                  // DBFS 不存在 = 读取失败
        }
        Return (RBUF)
    }

    // --- 未知 MethodId ---
    Store (0xFFFFFFFF, RVAL)
    Return (RBUF)
}
```

### 3.4 _INI 初始化方法

```asl
Method (_INI, 0, NotSerialized)
{
    If (CondRefOf (\DBFS))
    {
        Store (Zero, \DBFS)                            // 默认禁用 Dynamic Boost
    }
    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM))
    {
        \_SB.PCI0.LPC0.H_EC.COMM ()                   // 立即提交 EC 设置
    }
}
```

> **设计原则：** 每次引导默认将 DBFS 置零（禁用 Dynamic Boost），确保系统始终从已知的安全状态启动。用户需通过 WMI 接口显式启用。

---

## 4. 调用接口

### 4.1 PowerShell 示例

```powershell
# 获取 WMI 实例
$ins = Get-WmiObject -Namespace root\wmi -Class CustomDbUnlock -Authentication PacketPrivacy

# 启用 Dynamic Boost
$result = $ins.SetDbfs(1)
Write-Host "Status: $($result.Status)"  # 0 = 成功

# 读取当前状态
$result = $ins.ReadDbfs()
Write-Host "DBFS Value: $($result.Value)"  # 0 = 禁用, 1 = 启用
```

### 4.2 WMIC 示例

```batch
wmic /namespace:\\root\wmi path CustomDbUnlock call SetDbfs 1
wmic /namespace:\\root\wmi path CustomDbUnlock call ReadDbfs
```

### 4.3 返回值编码

| 返回值 | 含义 |
|--------|------|
| `0` | 成功 |
| `0xFFFFFFFF`（`4294967295`） | 失败（DBFS 不存在或不支持的 MethodId） |

---

## 5. 编译流水线

WMI 接口的构建涉及多步转换：

```
SsdtUnlockDB.mof（MOF 源码）
  │  mofcomp.exe -B:build\SsdtUnlockDB.bmf
  ▼
SsdtUnlockDB.bmf（二进制 MOF）
  │  PowerShell: ReadAllBytes → hex array
  ▼
BmfData.asl（ASL Name(WQBA, Buffer(){...})）
  │  #include 到 SsdtUnlockDB.asl
  ▼
SsdtUnlockDB.asl（完整 ASL 源码）
  │  iasl.exe -p SsdtUnlockDB -tc
  ▼
SsdtUnlockDB.aml（编译后的 AML 字节码）
SsdtUnlockDB.hex（C 头文件格式的 AML 字节码）
  │  #include 到 AcpiPatch.c
  ▼
嵌入到 BOOTX64.efi → 引导时通过 AllocatePages 注入 ACPI NVS 内存
```

---

## 6. 运行时数据流

```
用户态 (PowerShell/WMIC)
  │  WMI 调用
  ▼
Windows WMI 服务 (winmgmt)
  │  解析 MOF 契约
  ▼
ACPI.sys (内核 WMI 映射驱动)
  │  PNP0C14 → WMI 方法转发
  ▼
AML 解释器 (acpi.sys)
  │  执行 WMTF 方法
  ▼
\DBFS 全局变量 ←→ EC COMM 方法
  │
  ▼
EC 固件 (嵌入式控制器)
  │  → 调整 Dynamic Boost 策略
  ▼
NVIDIA GPU Boost 控制器
```
