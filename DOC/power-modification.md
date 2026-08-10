# 功耗修改说明（Power Modification Guide）

适用于：蛟龙16 PRO（Mechrevo / INSYDE 固件，AMD Ryzen R7/R9 + NVIDIA）
目标：解锁 CPU 功耗墙（PL1/PL2 ≈ CSPL/FPPT）与平台/GPU 功率墙（NPCF）

---

## 1. 架构总览

本项目的功耗解锁**不再使用运行时字节补丁**（`patches/*.txt` 已删除），改为**整表替换**：

```
离线（修改在 ASL 源码层，可编译校验）：
  固件表 .dsl ──generate_patched_tables.py──▶ src/acpi/patched/*.dsl（ASL 修改）
                    ──build_patched_tables.py──▶ build/*.hex（iASL 重编译的合法 AML）

运行时 bootloader（build\BOOTX64.efi）：
  InjectSsdt        → 注入 DBUL WMI 设备（15 个方法）
  ReplaceAcpiTables → 整表替换 SSDT4（NPCF 功率墙）+ DSDT（CPU 功耗钳位）
                      （XSDT/FACP 指针重定向，固件表一个字节不碰）
```

关键收益：固件 `\_SB.PCI0.WMID`（`MICommonInterface`）的 AML 结构完全不受影响，
Windows WMI 注册不会再因补丁字节被破坏。

---

## 2. 修改点明细

### 2.1 SSDT4 —— NPCF 平台/GPU 功率墙

文件：`src/acpi/patched/ssdt4_patched.dsl`（由 `tools/acpi/ssdt4.dsl` 生成）

| 修改项 | 原始值 | 修改后 | 说明 |
|---|---|---|---|
| `ATPP = 0x0168`（2 处） | 180W | `0x01E0` 240W | DGPU 直连功耗墙 |
| `ATP2 = 0x0168`（1 处） | 180W | `0x01E0` 240W | DGPU 直连短时功耗墙 |
| `TPPA = 0x0168`（2 处） | 180W | `0x0230` 280W | 混合模式平台功耗墙 |
| `MAGA = 0xC8`（4 处） | 100W | `Zero` | 混合模式独立 GPU 功耗 |
| `Name (CMPL, 0x33)` | 51W | `0x50` 80W | CPU 电流限制 |
| `Name (CNPL, 0x10)` | 16W | `0x36` 54W | CPU 负电流限制 |
| `CPUC = NCHP`（删除） | 执行 | 删除 | 跳过 NVIO I/O 写，避免固件重新钳位平台功耗 |

### 2.2 DSDT —— CPU 功耗钳位（PL1/PL2）

文件：`src/acpi/patched/dsdt_patched.dsl`（由 `tools/acpi/dsdt.dsl` 生成）

| 修改项 | 原始值 | 修改后 | 说明 |
|---|---|---|---|
| `MSPL()` 钳位 `Local0 = 0x2D`（CPUT==0x07 / ==0x05） | 45W | `0x3C` 60W | CSPL（≈PL1）钳位值 |
| `MSPL()` 钳位 `Local0 = 0x37`（CPUT==0x09） | 55W | `0x3C` 60W | CSPL（≈PL1，R9） |
| `MFPT()` 钳位 `Local0 = 0x41`（CPUT==0x07 / ==0x05） | 65W | `0x50` 80W | FPPT（≈PL2）钳位值 |
| `MFPT()` 钳位 `Local0 = 0x4B`（CPUT==0x09） | 75W | `0x50` 80W | FPPT（≈PL2，R9） |
| `If (DBFS == One)`（MSPL/MFPT 各 1 处） | 执行钳位 | `If (Zero)` 恒假 | DB=1 时也不回钳（原 `cpu_clamp_DBFS_unlock`） |
| `If (DBFS == Zero)`（FNQS 2 处） | 随 DBFS | `If (One)` 恒真 | 恒走 DBFS=0 的 THMD profile（原 `dbfs_profile_high`） |

> 注：`MSPL()` 写 3 个寄存器（`MODP 0x05/0x07/0x13`），`MFPT()` 写 `MODP 0x06`，
> 最终经 `MODP → ALIB(0x0C, XX11) → AMD SMU` 生效。

---

## 3. 原理

### 3.1 CPU 功耗（PL1/PL2）应用链路

```
厂商 MI 驱动 / 工具
  → \_SB.PCI0.WMID.WMAA (0x1700, FUN3=2/3)      [ssdt3.dsl]
      FUN3==2: ECWT(→CSPL); MSPL()    ← PL1
      FUN3==3: ECWT(→FPPT); MFPT()    ← PL2
  → DSDT H_EC: MSPL()/MFPT()/MODP()   [dsdt.dsl]
      MODP(0x05/0x07/0x13, CSPL×1000)
      MODP(0x06, FPPT×1000)
      → ALIB(0x0C, XX11) → AMD SMU 生效
```

固件 `MSPL()/MFPT()` 内的钳位条件是 `If (DBFS == One)`：
DB=1（Dynamic Boost 开启）时把 CSPL/FPPT 钳回 45W/65W（R7）。
本修改把钳位条件改为恒假 + 钳位值提升，使 DB 开启时 CPU 也不回钳。

### 3.2 平台/GPU 功率墙（NPCF）为什么必须改表

`NPCF._DSM` 是 NVIDIA 驱动调用的方法，**每次调用都会重新执行**
`ATPP = 0x0168; ATP2 = 0x0168; TPPA = 0x0168; MAGA = 0xC8; CPUC = NCHP`。
因此运行时通过 WMI 写 NPCF 变量会被下一次 `_DSM` 调用覆盖，**动态修改无法持久**，
必须在表内改掉这些赋值（或删除 `CPUC = NCHP`）。

### 3.3 为什么 SsdtUnlockDB 里删除了功耗方法

固件原生 WMI（`MICommonInterface`，WMAA `0x1700`）本身已支持设置
CSPL/FPPT/CTCL、性能模式、风扇、RGB。SsdtUnlockDB 中重复实现这些的 WMI 方法
（`SetAmdCpuLimits`、`SetNpcfPowerLimits`、`SetFanAndPerformanceMode` 等）已被删除，
保留 15 个方法：DBFS 控制、版本、电池充电阈值、ACBT/DCBT 热阈值、EC 读写、
WMOV 覆盖锁、软件 SMI、AMD ALIB 直写。

---

## 4. 运行时调整功耗

| 方式 | 方法 | 说明 |
|---|---|---|
| 固件原生 WMI | `MICommonInterface` 0x1700 FUN3=2/3/4 | 设置 CSPL/FPPT/CTCL（经 MSPL/MFPT，钳位已解锁） |
| DBUL WMI | `SetAmdAlibDirect(SubFunction, DataParam)` | 直写 AMD ALIB 邮箱（绕过固件，调试用） |
| 改表（永久） | 修改 `src/acpi/patched/*.dsl` | 重新构建后生效，见下节 |

---

## 5. 修改功耗值的方法（永久改）

1. 编辑 `src/acpi/patched/dsdt_patched.dsl`（CPU CSPL/FPPT 钳位值）
   或 `src/acpi/patched/ssdt4_patched.dsl`（NPCF ATPP/ATP2/TPPA/MAGA/CMPL/CNPL）
   - 值单位：`Local0 = 0x3C` = 60W（十六进制瓦数）
   - 若从原始表重新生成（`generate_patched_tables.py`），需同步更新该脚本里的
     修改规格，否则生成结果会覆盖你的手改。
2. 运行 `build.bat`（自动：版本刷新 → patched 表重编译 → MOF → iASL → MSVC 链接）
3. 部署 `build\BOOTX64.efi`

---

## 6. 验证

- 构建期：两个 patched 表 iASL 编译必须 0 Errors；
  可用 `python -c` 对 `build/*.aml` 与 `tools/acpi/*.dat` 做字节对比确认修改点。
- 实机：Windows 下用 HWiNFO / `GetEcRegister` 读 CSPL/FPPT，或厂商控制中心
  查看功耗墙；烤机确认 CPU 能稳定跑到目标 PL1/PL2。
- WMI 注册：确认 `root\wmi` 下 `MICommonInterface` 与 `CustomDbUnlock` 类均正常注册
  （这是整表替换相比字节补丁的核心收益）。

---

## 7. 相关文件

| 文件 | 作用 |
|---|---|
| `src/acpi/patched/dsdt_patched.dsl` | DSDT 功耗钳位修改源（ASL） |
| `src/acpi/patched/ssdt4_patched.dsl` | SSDT4 NPCF 功率墙修改源（ASL） |
| `tools/generate_patched_tables.py` | 从固件表生成 patched dsl（记录修改规格） |
| `tools/build_patched_tables.py` | iASL 编译 patched dsl → `build/*.hex` |
| `src/AcpiPatch.c` | `ReplaceAcpiTables()` 整表替换实现 |
| `src/acpi/SsdtUnlockDB.asl` / `.mof` | 注入的 DBUL WMI 设备（15 方法） |
