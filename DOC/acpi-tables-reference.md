# OEM ACPI 表完整清单

本文档列出蛟龙 16 PRO（固件供应商 INSYDE）平台上所有原始 ACPI 表。

---

## 概述

| 属性 | 值 |
|------|-----|
| 设备型号 | 蛟龙 16 PRO |
| 固件供应商 | INSYDE（OEM ID 均为 `"INSYDE"`） |
| 表总数 | DSDT × 1 + SSDT × 24 = **25 张表** |
| 目标 OEM Table ID | `"EDK2    "`（DSDT 和多数 SSDT 共用） |
| iASL 版本 | 20260408 |

---

## ACPI SDT 通用头结构

所有 ACPI 表共享 36 字节的标准描述表头：

| 偏移 | 字段 | 大小 | 说明 |
|------|------|------|------|
| 0 | `Signature` | 4 字节 | 表签名，如 `DSDT`（0x54445344）、`SSDT`（0x54445353） |
| 4 | `Length` | 4 字节 | 表总长度（含头） |
| 8 | `Revision` | 1 字节 | 表修订号（DSDT 为 2，多数 SSDT 为 1） |
| 9 | `Checksum` | 1 字节 | 8 位模 256 校验和，全表字节和被清零 |
| 10 | `OemId[6]` | 6 字节 | OEM 标识，本平台均为 `"INSYDE"` |
| 16 | `OemTableId[8]` | 8 字节 | OEM 表标识，本平台多为 `"EDK2    "`（含尾部空格） |
| 24 | `OemRevision` | 4 字节 | OEM 修订号 |
| 28 | `CreatorId` | 4 字节 | 创建者 ID |
| 32 | `CreatorRevision` | 4 字节 | 创建者修订号 |

---

## 表清单

### DSDT — Differentiated System Description Table（差异化系统描述表）

DSDT 是平台的主 ACPI 表，定义了根命名空间（`\`）下的所有主要设备和系统范围对象。

| 属性 | 值 |
|------|-----|
| 文件 | `dsdt.dat` / `dsdt.dsl` |
| 签名 | `DSDT`（`0x54445344`） |
| 长度 | `0x7F95`（32,661 字节） |
| 修订号 | 2 |
| OEM ID | `"INSYDE"` |
| OEM Table ID | `"EDK2    "` |
| OEM Revision | `0x00000002` |
| 编译器 ID | `"ACPI"` |
| 编译器版本 | `0x00040000`（262,144） |

**关键内容：**
- `\_SB.PCI0.WMID` — WMI 数据块设备（_HID = PNP0C14）
- `\_SB.PCI0.LPC0.H_EC` — 嵌入式控制器（EC），包含 CPU 类型（CPUT）、DGPU 存在标志（DGPU）、ITSM 等字段
- `\DBFS` — Dynamic Boost 标志（全局变量，由 EC 查询 _Q81/_Q82 控制）
- `\_SB.PCI0.LPC0.H_EC.COMM` — EC 提交方法
- `\_SB.NPCF` — NVIDIA Platform Controller Framework（_HID = "NVDA0820"）
- FNQS 方法 — 风扇/热管理调度（根据 DBFS 状态选择不同的 THMD profile）
- THMD 方法 — 热管理执行方法（通过 ALIB 接口与 AMD SMU 通信）

---

### SSDT — Secondary System Description Tables（二级系统描述表）

全部 24 张 SSDT 表均由 iASL 版本 20260408 编译，OEM ID 均为 `"INSYDE"`。

| 文件 | 签名 | 长度（字节） | OEM Table ID | OEM Revision | ACPI Rev | 备注 |
|------|------|-------------|-------------|-------------|----------|------|
| `ssdt.dat` | SSDT | `0x12FF`（4,863） | `"EDK2    "` | `0x00000001` | 1 | 主 SSDT（SSDT0） |
| `ssdt1.dat` | SSDT | `0x8457`（33,879） | `"EDK2    "` | `0x00000002` | 2 | 大型表，仅次于 ssdt6/ssdt8 |
| `ssdt2.dat` | SSDT | `0x044B`（1,099） | `"EDK2    "` | `0x00001000`（4,096） | 1 | — |
| `ssdt3.dat` | SSDT | `0x2236`（8,758） | `"EDK2    "` | `0x00003000`（12,288） | 1 | OEM Rev 唯一特殊值 |
| `ssdt4.dat` | SSDT | `0x245A`（9,306） | `"EDK2    "` | `0x00001000`（4,096） | 1 | ⭐ **功率墙所在表**（NPCF 设备） |
| `ssdt5.dat` | SSDT | `0x00F8`（248） | `"EDK2    "` | `0x00001000`（4,096） | 1 | 最小表之一 |
| `ssdt6.dat` | SSDT | `0x9BAE`（39,854） | `"EDK2    "` | `0x00000001` | 2 | **最大表** |
| `ssdt7.dat` | SSDT | `0x0478`（1,144） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt8.dat` | SSDT | `0x95FD`（38,397） | `"EDK2    "` | `0x00000001` | 2 | 第二大表 |
| `ssdt9.dat` | SSDT | `0x22AD`（8,877） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt10.dat` | SSDT | `0x0FE1`（4,065） | `"EDK2    "` | `0x00000001` | 2 | 与 ssdt11 **完全相同**（克隆表） |
| `ssdt11.dat` | SSDT | `0x0FE1`（4,065） | `"EDK2    "` | `0x00000001` | 2 | 与 ssdt10 **完全相同**（克隆表） |
| `ssdt12.dat` | SSDT | `0x1D19`（7,449） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt13.dat` | SSDT | `0x06DA`（1,754） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt14.dat` | SSDT | `0x0946`（2,374） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt15.dat` | SSDT | `0x008D`（141） | `"EDK2    "` | `0x00000001` | 2 | **最小表** |
| `ssdt16.dat` | SSDT | `0x07B3`（1,971） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt17.dat` | SSDT | `0x066F`（1,647） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt18.dat` | SSDT | `0x0460`（1,120） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt19.dat` | SSDT | `0x0868`（2,152） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt20.dat` | SSDT | `0x0BA0`（2,976） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt21.dat` | SSDT | `0x0761`（1,889） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt22.dat` | SSDT | `0x0464`（1,124） | `"EDK2    "` | `0x00000001` | 2 | — |
| `ssdt23.dat` | SSDT | `0x102E`（4,142） | `"EDK2    "` | `0x00000001` | 2 | — |

> **注：** 全部 24 张 SSDT 的 OEM ID 均为 `"INSYDE"`，OEM Table ID 均为 `"EDK2    "`（8 字节，含尾部空格）。ACPI Rev 列表示 DefinitionBlock 的 ACPI 规范版本号。ssdt10 与 ssdt11 的长度和所有表头字段完全相同，可能是固件中的冗余/克隆表。

---

## 提取工具链

ACPI 表的提取和反编译流程：

```
固件运行时内存
  │  acpidump.exe (tools/isal/)
  ▼
.dat 文件（AML 二进制）
  │  iasl.exe -d (tools/isal/)
  ▼
.dsl 文件（ASL/ASL+ 反编译源码）
```

### 工具列表（tools/isal/）

| 工具 | 功能 |
|------|------|
| `acpidump.exe` | 从固件中提取所有 ACPI 表 |
| `acpixtract.exe` | 从 acpidump 输出中提取单个表 |
| `iasl.exe` | AML ↔ ASL 编译器/反编译器（iASL 20260408） |
| `acpibin.exe` | ACPI 二进制表实用工具 |
| `acpiexec.exe` | AML 调试器/执行引擎 |
| `acpihelp.exe` | ACPI 命名空间帮助工具 |
| `acpisrc.exe` | ACPI 源码转换工具 |
