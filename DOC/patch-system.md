# 二进制补丁系统详解

本文档详细说明 4 个 ACPI 二进制补丁的 AML 字节码级规格和执行机制。

---

## 1. 补丁系统架构

### 1.1 补丁规格文件

每个补丁由 `patches/` 目录下的纯文本文件定义，格式为：

```
table=<OEM Table ID>
find=<搜索十六进制串>
replace=<替换十六进制串>
```

### 1.2 代码生成

`tools/generate_patches.py` 扫描所有 `.txt` 补丁文件并生成 `src/GeneratedPatches.h`：

```
patches/*.txt  →  generate_patches.py  →  src/GeneratedPatches.h
```

生成的头文件包含：
- 每个补丁的静态 `Find[]` 和 `Replace[]` 字节数组
- 每个补丁的宽字符名称数组（用于日志输出）
- 全局 `gPatches[]` 数组和 `gPatchCount` 计数

### 1.3 运行时执行

`AcpiPatch.c` 中的 `PatchSsdt4PowerWall()` 函数在引导时遍历 XSDT，对每个 OEM Table ID 匹配 `"EDK2    "` 的表执行所有补丁。

---

## 2. 补丁清单

| # | 补丁名称 | 目标表 | 搜索长度 | 替换长度 | 目标变量 | 功能 |
|---|---------|--------|---------|---------|---------|------|
| 1 | `atpp_unlock` | `EDK2   ` | 8 | 8 | ATPP | 提升 ATPP 功率上限 |
| 2 | `atp2_unlock` | `EDK2   ` | 8 | 8 | ATP2 | 提升 ATP2 功率上限 |
| 3 | `cpu_clamp_unlock` | `EDK2   ` | 6 | 6 | DBFS | 解除 DBFS 条件分支钳位 |
| 4 | `cpu_limit_65w` | `EDK2   ` | 3 | 3 | (未知) | 解除 65W 限制 |

---

## 3. 补丁详细规格

### 3.1 atpp_unlock — ATPP 功率解锁

**文件：** `patches/atpp_unlock.txt`

```
table=EDK2
find=700B680141545050
replace=700BE00141545050
```

#### 字节码解码

| 偏移 | 字节 | AML 语义 | 说明 |
|------|------|---------|------|
| 0 | `0x70` | `Store` 操作码 | 赋值操作 |
| 1 | `0x0B` | NameString 前缀 | 目标对象索引 |
| 2-3 | `0x68 0x01` | `WordConst 0x0168` | ⚠ 原始值：reg=360, 实际 180W |
| 4-7 | `0x41 0x54 0x50 0x50` | `"ATPP"` | 目标对象名称（ASCII） |

→ 替换为：`0x70 0x0B 0xE0 0x01 0x41 0x54 0x50 0x50`

| 偏移 | 字节 | 变化 |
|------|------|------|
| 2-3 | `0xE0 0x01` | `WordConst 0x01E0` — reg=480, 实际 **240W**（+60W） |

#### SSDT4 中的匹配位置

该模式在 SSDT4 的 AML 字节码中匹配 **3 处** `Store(0x0168, ATPP)`：

1. **第 1263 行**：`ATPP = 0x0168`（DGPU 存在 + CPUT=0x07）
2. **第 1285 行**：`TPPA = 0x0168`（DGPU 存在 + ITSM 其他情况）→ 此为 TPPA 赋值，但包含 `ATPP` 字符串的字节模式不会被 `0x70 0x0B ... ATPP` 匹配（因为操作数名称不同）
3. **第 1292 行**：`ATPP = 0x0168`（DGPU 不存在 + CPUT=0x07）

> ⚠ **注意：** `TPPA = 0x0168`（第 1285/1315 行）的 AML 字节码使用 NameString `TPPA` 而非 `ATPP`，因此不会被 `atpp_unlock` 补丁匹配。第 1285 行的 `TPPA = 0x0168` 模式需要在 `.dat` 二进制中搜索确认。

---

### 3.2 atp2_unlock — ATP2 功率解锁

**文件：** `patches/atp2_unlock.txt`

```
table=EDK2
find=700B680141545032
replace=700BE00141545032
```

#### 字节码解码

| 偏移 | 字节 | AML 语义 | 说明 |
|------|------|---------|------|
| 0 | `0x70` | `Store` 操作码 | 赋值操作 |
| 1 | `0x0B` | NameString 前缀 | 目标对象索引 |
| 2-3 | `0x68 0x01` | `WordConst 0x0168` | ⚠ 原始值：reg=360, 实际 180W |
| 4-7 | `0x41 0x54 0x50 0x32` | `"ATP2"` | 目标对象名称（ASCII，`2` = 0x32） |

→ 替换为：`0x70 0x0B 0xE0 0x01 0x41 0x54 0x50 0x32`

| 偏移 | 字节 | 变化 |
|------|------|------|
| 2-3 | `0xE0 0x01` | `WordConst 0x01E0` — reg=480, 实际 **240W**（+60W） |

#### SSDT4 中的匹配位置

匹配 **1 处**：

1. **第 1293 行**：`ATP2 = 0x0168`（DGPU 不存在 + CPUT=0x07）

---

### 3.3 cpu_clamp_unlock — CPU DBFS 条件钳位解锁

**文件：** `patches/cpu_clamp_unlock.txt`

```
table=EDK2
find=934442465301
replace=9344424653FF
```

#### 字节码解码

| 偏移 | 字节 | AML 语义 | 说明 |
|------|------|---------|------|
| 0 | `0x93` | `LEqual` 操作码 | 等于比较 |
| 1-4 | `0x44 0x42 0x46 0x53` | `"DBFS"` | NameString（ASCII） |
| 5 | `0x01` | `One` | ⚠ 原始比较值：1 |

→ 替换为：`0x93 0x44 0x42 0x46 0x53 0xFF`

| 偏移 | 字节 | 变化 |
|------|------|------|
| 5 | `0xFF` | `ByteConst 0xFF` — 永假条件（DBFS 只会是 0 或 1） |

#### 语义分析

原始 ASL 代码（DSDT 中）：
```asl
If ((DBFS == One))
{
    // DBFS=1 时的限制性热管理分支
    THMD (0x16)  // 或 0x14, 0x06 等 profile
}
```

补丁后变为：
```asl
If ((DBFS == 0xFF))  // ← 永假，因为 \DBFS ∈ {0, 1}
{
    // 此代码路径永不执行
}
```

**效果：** 阻止 DBFS=1 时执行的限制性热管理策略执行，使系统在 Dynamic Boost 启用状态下不受 OEM 热管理 profile 的额外约束。

#### DSDT 中的匹配位置

该模式在 DSDT 的 AML 字节码中匹配 **2 处**（FNQS 方法内）：

1. **第 5106 行**：`If ((DBFS == One))`（FNQS Arg0=0 分支内，DGPU 存在 + 特定 CPUT）
2. **第 5140 行**：`If ((DBFS == One))`（FNQS Arg0=1 分支内，DGPU 存在 + 特定 CPUT）

---

### 3.4 cpu_limit_65w — 65W 限制解锁

**文件：** 待创建 `patches/cpu_limit_65w.txt`（当前仅存在于 GeneratedPatches.h 中）

```
table=EDK2
find=0BC8AF
replace=0BFFFF
```

#### 字节码解码

| 偏移 | 字节 | AML 语义 | 说明 |
|------|------|---------|------|
| 0 | `0x0B` | `WordPrefix` | 后续 2 字节为 16 位立即数 |
| 1-2 | `0xC8 0xAF` | `WordConst 0xAFC8`（小端） | ⚠ 原始值：45,000（含义待确认） |

→ 替换为：`0x0B 0xFF 0xFF`

| 偏移 | 字节 | 变化 |
|------|------|------|
| 1-2 | `0xFF 0xFF` | `WordConst 0xFFFF` — 65,535（最大值） |

> ⚠ **注意：** `cpu_limit_65w` 补丁文件尚未创建，当前仅定义了 `GeneratedPatches.h` 中的数组。该补丁的精确匹配位置和语义需要在 DSDT/SSDT 的 AML 二进制中反向搜索 `0x0B 0xC8 0xAF` 序列来确定。

---

## 4. 搜索替换算法

### 4.1 核心实现

```c
static UINTN SearchAndReplace(
    UINT8 *Buffer, UINTN Size,
    const UINT8 *Search, UINTN SearchSize,
    const UINT8 *Replace, UINTN ReplaceSize
)
```

| 特性 | 说明 |
|------|------|
| 前置条件 | `SearchSize == ReplaceSize`（必须等长，否则返回 0） |
| 搜索策略 | 滑动窗口，逐字节比较 |
| 搜索范围 | `Buffer[0]` 到 `Buffer[Size - SearchSize]` |
| 跳步策略 | 匹配后 `i += SearchSize - 1`，防止重叠匹配 |
| 时间复杂度 | O(Size × SearchSize) |
| 安全性 | 在写时复制的影子页上执行，不修改原始 OEM 表 |

### 4.2 写时复制方案

```
OEM 原始 SSDT（固件保留内存，可能只读）
  │
  ├─ AllocatePages(AllocateType=0, MemoryType=9, PagesNeeded)
  │   └─ 分配 EfiACPIReclaimMemory 内存页
  │
  ├─ UefiMemcpy(NewTable, OriginalTable, TableLength)
  │   └─ 完整拷贝 SSDT 到影子页
  │
  ├─ SearchAndReplace(NewTable, ...)
  │   └─ 在影子页上执行所有补丁
  │
  ├─ NewTable->Checksum = CalculateChecksum8(...)
  │   └─ 重算校验和
  │
  └─ EntryPtr[Index] = NewTableAddr
      └─ 重定向 XSDT 指针到影子表
```

### 4.3 校验和重算顺序

修改表内容后必须执行以下顺序的校验和更新：

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | `NewTable->Checksum = 0; NewTable->Checksum = CalculateChecksum8(...)` | 更新影子表校验和 |
| 2 | `EntryPtr[Index] = NewTableAddr` | 重定向 XSDT 指针 |
| 3 | `Xsdt->Checksum = 0; Xsdt->Checksum = CalculateChecksum8(Xsdt, Xsdt->Length)` | 更新 XSDT 自身校验和 |

---

## 5. OEM Table ID 匹配

所有补丁的目标 `OemTableId = 0x20202020324B4445ULL`，即 ASCII 字符串 `"EDK2    "`（8 字节，尾部 4 个空格）。

该 OEM Table ID 同时匹配：
- **DSDT**（OEM Table ID: `"EDK2    "`）
- **SSDT4**（OEM Table ID: `"EDK2    "`）

运行时扫描逻辑：
1. 遍历 XSDT 所有 EntryPtr
2. 对每个 `Signature == 'SSDT'` 的表，检查 `OemTableId == 0x20202020324B4445`
3. 对 FACP 表，额外通过 `X_DsdtPtr`/`DsdtPtr` 定位 DSDT 并对其执行补丁
4. 匹配成功 → 分配影子页 → 执行所有 4 个补丁

---

## 6. 补丁边界保护

| 保护措施 | 实现 |
|---------|------|
| 等长检查 | `SearchSize != ReplaceSize → return 0` |
| 长度下溢保护 | `Size < SearchSize → return 0` |
| 地址合法性 | `IsValidAcpiPointer()`：非空 + 4 字节对齐 + 地址范围 [1MB, 256GB] |
| 内存类型 | `EfiACPIReclaimMemory`（类型 9），OS 引导后可回收 |
| 失败回退 | 补丁未匹配时释放影子页，保持原始指针不变 |
