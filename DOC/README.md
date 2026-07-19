# ACPI 深度分析文档

本目录包含蛟龙 16 PRO 平台上 ACPI 表的完整逆向工程分析文档，以及基于 UEFI bootloader 的运行时补丁系统技术说明。

---

## 文档索引

| 文档 | 说明 |
|------|------|
| [acpi-tables-reference.md](acpi-tables-reference.md) | OEM ACPI 表完整清单（DSDT + 24 SSDT）及其详细属性 |
| [power-wall-analysis.md](power-wall-analysis.md) | SSDT4 中 ATPP/ATP2 功率墙机制的深度逆向分析 |
| [dbunlock-wmi-interface.md](dbunlock-wmi-interface.md) | 自定义 WMI 接口（CustomDbUnlock）的完整规范 |
| [patch-system.md](patch-system.md) | 4 个二进制补丁的 AML 字节码级详细说明 |
| [acpi-injection-flow.md](acpi-injection-flow.md) | 引导时 ACPI 注入与补丁执行的完整流程图 |

---

## 项目架构速览

```
tools/acpi/                    ← 本目录：OEM 原始 ACPI 表（反编译产物）
  ├── dsdt.dat / dsdt.dsl      ← DSDT（Differentiated System Description Table）
  ├── ssdt.dat / ssdt.dsl      ← SSDT0（主 SSDT）
  ├── ssdt1.dat / ssdt1.dsl    ← SSDT1
  ├── ...                      ← SSDT2 ~ SSDT23
  └── ssdt24.dat / ssdt24.dsl  ← SSDT24

src/acpi/                      ← 自定义 ACPI/WMI 源码
  ├── SsdtUnlockDB.asl         ← 自定义 WMI 设备定义（PNP0C14）
  └── SsdtUnlockDB.mof         ← WMI MOF 类契约

patches/                       ← 二进制补丁规格文件
  ├── atpp_unlock.txt          ← ATPP 功率解锁补丁
  ├── atp2_unlock.txt          ← ATP2 功率解锁补丁
  └── cpu_clamp_unlock.txt     ← CPU DBFS 条件钳位解锁补丁

src/GeneratedPatches.h         ← 自动生成的补丁数据头文件（由 generate_patches.py 生成）
```

---

## 核心概念

### 功率墙（Power Wall）

蛟龙 16 PRO 的 OEM 固件（INSYDE）在 SSDT4 的 `\_SB.NPCF` 设备中硬编码了全平台功率限制。当检测到独立 GPU 且 CPU 为 Ryzen 7 时：

- **ATPP（Total Processor Power）** 被强制设为 `0x0168`（寄存器值360，实际180W）
- **ATP2（Adaptive Thermal Power 2）** 被强制设为 `0x0168`（寄存器值360，实际180W）

厂商未触发该条件时的默认值为 `0x01B8`（寄存器值440，实际220W），形成了一道 **-40W 的功率墙**。

### 功耗倍率

该平台功耗寄存器采用 **×0.5 倍率编码**：寄存器值 × 0.5 = 实际瓦数。

### 解锁方案

通过 UEFI bootloader 在 Windows Boot Manager 加载前执行：
1. **注入** 自定义 SSDT（WMI 设备 `\_SB.PCI0.LPC0.DBUL`）
2. **二进制补丁** OEM SSDT4/DSDT 中的功率限制参数
3. **链式加载** `bootmgfw.efi`，对 Windows 完全透明

### WMI 运行时控制

通过 `root\wmi\CustomDbUnlock` 类暴露 `SetDbfs(0/1)` 和 `ReadDbfs()` 方法，实现 Dynamic Boost 的运行时开关控制。

---

## 关键数据源

| 数据 | 来源 |
|------|------|
| OEM ACPI 表 | `tools/isal/acpidump.exe` 从运行固件中提取 |
| AML 反编译 | `tools/isal/iasl.exe` (Intel ACPI Component Architecture) |
| 二进制补丁模式 | 对 `ssdt4.dsl` 和 `dsdt.dsl` 的手工逆向分析 |
| WMI 类定义 | 自定义 MOF/ASL 源码（`src/acpi/`） |
