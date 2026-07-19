# 功率墙机制深度分析

> 目标表：`tools/acpi/ssdt4.dat`（SSDT4，OEM Table ID `"EDK2    "`）  
> 目标设备：`\_SB.NPCF`（NVIDIA Platform Controller Framework，_HID = `"NVDA0820"`）

---

## 1. 功率墙概述

蛟龙 16 PRO 的固件在 SSDT4 中通过 AML 运行时条件逻辑实现了全平台功率限制。该限制针对配备 **独立 GPU + Ryzen 7 处理器** 的配置组合生效，将总平台功耗上限压低 40W。

### 1.1 核心数值

| 参数 | 含义 | 默认值（未触发条件） | 触发条件后的限制值 | 解锁目标值 |
|------|------|---------------------|-------------------|-----------|
| **ATPP** | Total Processor Power | `0x01B8`（reg=440, 实际 220W） | `0x0168`（reg=360, 实际 180W） | `0x01E0`（reg=480, 实际 240W） |
| **ATP2** | Adaptive Thermal Power 2 | `0x0208`（reg=520, 实际 260W） | `0x0168`（reg=360, 实际 180W） | `0x01E0`（reg=480, 实际 240W） |

> **功耗编码倍率：寄存器值 × 0.5 = 实际物理功率（瓦）。**  
> 解锁目标 `0x01E0`（实际 240W）比厂商未触发条件下的 ATPP 默认值 `0x01B8`（实际 220W）高出 20W。

---

## 2. SSDT4 中的 NPCF 设备定义

### 2.1 初始参数声明（第 1141-1165 行）

```asl
Scope (\_SB)
{
    Device (NPCF)
    {
        Name (ACBT, 0x50)        // AC Boost Threshold
        Name (DCBT, 0x28)        // DC Boost Threshold
        Name (DBAC, Zero)        // DB AC flag
        Name (DBDC, Zero)        // DB DC flag
        Name (AMAT, 0x78)        // AMA Threshold
        Name (AMIT, 0xFF88)      // AMI Threshold (signed -120)
        Name (ATPP, 0x01B8)      // ⭐ Total Processor Power
        Name (ATP2, 0x0208)      // ⭐ Adaptive Thermal Power 2
        Name (DTPP, 0xF0)        // DC TPP
        Name (TPPL, Zero)        // TPP Limit
        Name (DROS, Zero)        // DROS
        Name (HPCT, 0x02)        // HP Count
        Name (IOBS, Zero)        // IO Boost
        Name (CMPL, 0x33)        // CMPL (51)
        Name (CNPL, 0x10)        // CNPL (16)
        Name (CDIS, Zero)        // Custom Disable
        Name (CUSL, Zero)        // CUSL
        Name (CUCT, Zero)        // CUCT
```

### 2.2 _HID 与 _UID

```asl
Method (_HID, 0, NotSerialized)
{
    CDIS = Zero
    Return ("NVDA0820")  // NVIDIA Platform Controller Framework
}

Name (_UID, "NPCF")
```

---

## 3. 功率墙触发逻辑（完整还原）

功率墙发生在 `NPCF._DSM` 中 `NIGS == 0`（NVIDIA Integrated Graphics State = 不存在）的条件分支中，即在独立 GPU 模式下。

### 3.1 触发条件分支树

```
NPCF._DSM (Arg0=UUID, Arg1=0x0200, Arg2=2, Arg3)
  │
  ├─ NIGS = CreateField(Arg3, 0x28, 0x02)
  │
  ├─ [分支 A] NIGS == 0（独立 GPU 模式）
  │   │
  │   ├── [子分支 A1] EC.DGPU == 1（独立 GPU 存在）
  │   │   ├── [A1a] EC.CPUT == 0x07（Ryzen 7）
  │   │   │   └── ⚠ ATPP = 0x0168（实际 180W）
  │   │   ├── [A1b] EC.ITSM == 1（性能模式）
  │   │   │   └── TPPA = ATPP（当前 ATPP 值）
  │   │   ├── [A1c] EC.ITSM == 0（非性能模式）
  │   │   │   └── TPPA = ATPP（当前 ATPP 值）
  │   │   └── [A1d] else（其他情况）
  │   │       └── ⚠ TPPA = 0x0168（实际 180W，硬编码）
  │   │
  │   └── [子分支 A2] EC.DGPU == 0（独立 GPU 不存在）
  │       ├── [A2a] EC.CPUT == 0x07（Ryzen 7）
  │       │   └── ⚠ ATPP = 0x0168（实际 180W）
  │       │   └── ⚠ ATP2 = 0x0168（实际 180W）
  │       ├── [A2b] EC.ITSM == 1
  │       │   └── TPPA = ATP2（当前 ATP2 值）
  │       ├── [A2c] EC.ITSM == 0
  │       │   └── TPPA = ATPP（当前 ATPP 值）
  │       └── [A2d] else
  │           └── ⚠ TPPA = 0x0168（实际 180W，硬编码）
  │
  └─ [分支 B] NIGS == 1（混合 GPU 模式）
      └── 不触发功率限制（保留原值）
```

### 3.2 AML 源码精确还原

来自 `ssdt4.dsl` 第 1252-1317 行：

```asl
If ((ToInteger (NIGS) == Zero))
{
    If ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.DGPU)) == One))
    {
        If ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CPUT)) == 0x07))
        {
            ATPP = 0x0168          // ⚠ 功率墙：180W ← 此处被补丁
        }

        If ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.ITSM)) == One))
        {
            DBAC = Zero
            TGPA = 0x0118
            MIGA = Zero
            MAGA = 0xC8
            TPPA = ATPP /* \_SB_.NPCF.ATPP */
        }
        ElseIf ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.ITSM)) == Zero))
        {
            DBAC = Zero
            TGPA = 0x78
            MIGA = Zero
            MAGA = 0xC8
            TPPA = ATPP /* \_SB_.NPCF.ATPP */
        }
        Else
        {
            DBAC = One
            TPPA = 0x0168          // ⚠ 功率墙：180W ← 此处被补丁
        }
    }
    Else
    {
        If ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CPUT)) == 0x07))
        {
            ATPP = 0x0168          // ⚠ 功率墙：180W ← 此处被补丁
            ATP2 = 0x0168          // ⚠ 功率墙：180W ← 此处被补丁
        }

        If ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.ITSM)) == One))
        {
            DBAC = Zero
            TGPA = 0x0118
            MIGA = Zero
            MAGA = 0xC8
            TPPA = ATP2 /* \_SB_.NPCF.ATP2 */
        }
        ElseIf ((\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.ITSM)) == Zero))
        {
            DBAC = Zero
            TGPA = 0x78
            MIGA = Zero
            MAGA = 0xC8
            TPPA = ATPP /* \_SB_.NPCF.ATPP */
        }
        Else
        {
            DBAC = One
            TPPA = 0x0168          // ⚠ 功率墙：180W ← 此处被补丁
        }
    }
}
```

---

## 4. 功率分配链：双考场景还原

### 4.1 你的观测数据

| 指标 | 观测值 |
|------|--------|
| CPU 功耗 | 45W |
| GPU 功耗 | 140W |
| 合计 | 185W |

### 4.2 完整因果链（DGPU=1, CPUT=0x07, ITSM=1）

```
EC 寄存器状态
  ├─ DGPU = 1        （独立显卡存在）
  ├─ CPUT = 0x07     （Ryzen 7 处理器）
  └─ ITSM = 1        （性能模式）
        │
        ▼
NPCF._DSM (Arg2=2)  — NVIDIA 驱动通过 ACPI 调用
        │
        ├── ATPP = 0x0168     ← 寄存器 360, ×0.5 = 180W
        │                        平台总功率预算
        │
        ├── TGPA = 0x0118     ← 寄存器 280, ×0.5 = 140W  ★ 这就是你 GPU 140W 的来源
        │                        GPU 功率目标（Target GPU Power Actual）
        │
        ├── MAGA = 0xC8       ← 寄存器 200, ×0.5 = 100W
        │                        GPU 最大功率下限
        │
        ├── MIGA = 0          ← GPU 最小功率（无下限）
        │
        └── TPPA = ATPP       ← 继承 ATPP = 180W
                                 平台实际总功率（传给 NVIDIA 驱动）
        │
        ▼
PBD2 buffer 返回给 NVIDIA 驱动
  ├─ TPPA = 180W  →  Dynamic Boost 约束: GPU ≤ 180W - EstimatedCPU
  ├─ TGPA = 140W  →  GPU 目标功率
  └─ MAGA = 100W  →  GPU 功率地板
        │
        ├──→ NVIDIA GPU: 拉到 TGPA = 140W
        │    (NVIDIA 内部估算 CPU ≈ 40W, 180-40=140 ≥ TGPA, 所以 GPU 可以满 140W)
        │
        └──→ AMD CPU: 独立受 SMU 控制, STAPM/PPT 限制 ~45W
             (通过 DSDT 的 THMD(profile) → ALIB(0x0C, XX11) 设置)
```

### 4.3 关键结论

**GPU 的 140W 不是从 `TPPA - CPU_power` 算出来的，而是 TGPA 直接指定的。** TGPA 是 GPU 的硬目标值，Dynamic Boost 的约束 `TPPA - CPU_power` 只是上限——只要 `TGPA ≤ TPPA - EstimatedCPU`，GPU 就能跑满 TGPA。

你看到 185W > 180W 的这个 5W 偏差，是因为 NVIDIA 驱动的 **内部 CPU 功耗估算值**（约 40W）比你从 HWiNFO 等工具读到的 **CPU Package Power**（45W）偏低约 5W。这是两个不同来源的读数——NVIDIA 用的是经过滤波/平滑的估值，不一定等于瞬时 Package Power。

### 4.4 CPU 的 45W 从哪来

CPU 功率不由 NPCF 控制。AMD 平台的 CPU 功耗限制走的是另一条路径：

```
DSDT._SB.PCI0.LPC0.H_EC.FNQS(ITSM)
  → 根据 DBFS/DGPU/CPUT/ITSM 选 THMD profile
    → THMD(0x15) 或 THMD(0x16) 等
      → 构造 XX11 buffer { SMUF, SMUD }
        → ALIB(0x0C, XX11)  ← AMD SMU 接口
          → 设置 STAPM / PPT 限制 → CPU 被限制在 ~45W
```

CPU 和 GPU 的功率限制是**两个独立的控制平面**：
- GPU 功率 = NPCF → NVIDIA 驱动 (TGPA/TPPA)
- CPU 功率 = FNQS → THMD → ALIB → AMD SMU (STAPM/PPT)

两者加起来可能超过 TPPA，这就是你观察到 185W > 180W 的原因——NVIDIA 的 Dynamic Boost 用的是估算值而非实际 CPU 功耗。

### 4.5 解锁后的预期变化

| 参数 | 解锁前 (ATPP=0x0168) | 解锁后 (ATPP=0x01E0) | 变化 |
|------|---------------------|---------------------|------|
| ATPP → TPPA | 180W | 240W | +60W |
| TGPA（不变） | 140W | 140W | — |
| Dynamic Boost 余量 | 180 - 40 = 140W | 240 - 40 = 200W | +60W |
| GPU 能否跑满 TGPA | ✅ 刚好（140 ≤ 140） | ✅ 充裕（140 ≤ 200） | 从刚好到充裕 |
| CPU 高负载时 GPU 余量 | 180 - 90 = 90W | 240 - 90 = 150W | +60W |

> **注：** 解锁前 GPU 在双考时刚好能跑满 140W 是因为 CPU 只有 45W。如果 CPU 负载更高（如 AVX 负载到 90W），Dynamic Boost 会把 GPU 压到 180-90=90W。解锁到 240W 后，即使 CPU 跑到 90W，GPU 仍有 150W 余量——这才是 60W 增益真正发挥作用的时候。

---

## 5. 补丁策略

### 5.1 二进制模式

ATPP/ATP2 的 `Store(0x0168, ATPP)` 操作在 AML 字节码中编译为：

```
0x70        ← Store 操作码
  0x0B      ← ATPP 的 AML NameString 表索引
  0x68 0x01 ← 立即数 0x0168（小端序，reg=360）
  0x41 0x54 0x50 0x50 ← "ATPP"（ASCII，操作数名称引用）
```

补丁将 `0x68 0x01`（reg=360）改为 `0xE0 0x01`（reg=480），即实际功率从 180W → 240W。

### 5.2 补丁详细规格

参见 [patch-system.md](patch-system.md) 中 `atpp_unlock` 和 `atp2_unlock` 补丁的完整规格。

---

## 6. 关联机制：DBFS 热管理门控

除了 ATPP/ATP2 的静态功率墙，笔记本固件还通过 `\DBFS`（Dynamic Boost Flag）全局变量实现了 Dynamic Boost 的运行时开关。

### 6.1 DBFS 定义（DSDT）

```asl
Scope (\)
{
    Name (DBFS, Zero)   // 全局变量，默认为 0（禁用）
}
```

### 6.2 EC 查询事件触发

```
_Q81:  EC 查询 0x81 → DBFS = 0（禁用 Dynamic Boost）
_Q82:  EC 查询 0x82 → DBFS = 1（启用 Dynamic Boost）
```

两者均在设置 DBFS 后调用 `FNQS(ITSM)` → `THMD(profile)` → `ALIB(0x0C, ...)` 更新 AMD SMU 热参数，最后调用 `COMM()` 提交 EC 设置。

### 6.3 DBFS 对热管理的影响

DNFS 方法根据 DBFS 状态选择不同的 THMD profile：

| DBFS | DGPU 存在 | CPU 类型 | 条件 | THMD Profile |
|------|----------|---------|------|-------------|
| 0（禁用） | 是 | Ryzen 7（0x07） | — | `0x15` |
| 0（禁用） | 是 | Ryzen 9（0x09） | — | `0x13` |
| 1（启用） | 是 | Ryzen 7（0x07） | — | `0x16`（更高性能） |
| 1（启用） | 是 | Ryzen 9（0x09） | — | `0x14`（更高性能） |
| — | 否 | Ryzen 7（0x07） | — | `0x09` |
| — | 否 | Ryzen 9（0x09） | — | `0x03` |

`cpu_clamp_unlock` 补丁的目标是将 DSDT 中所有 `DBFS == 1` 的条件检查改为 `DBFS == 0xFF`（永假），从而永远不触发 DBFS=1 时的限制性 codepath。
