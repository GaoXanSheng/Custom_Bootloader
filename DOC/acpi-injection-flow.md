# ACPI 注入与补丁执行流程

本文档详细描述 UEFI bootloader 在引导时的完整 ACPI 注入和补丁执行流程。

---

## 1. 总体执行顺序

```
UEFI 固件 → Boot Manager
  │
  └─→ BOOTX64.efi 加载
        │
        ├───[Phase 1] efimain() 入口
        │     ├─ 输出标识信息到 ConOut
        │     └─ 初始化日志文件 (ESP\EFI\BOOT\unlock.log)
        │
        ├───[Phase 2] 定位 RSDP
        │     └─ 遍历 ConfigurationTable[] → 匹配 gEfiAcpi20TableGuid
        │
        ├───[Phase 3] 注入自定义 SSDT
        │     └─ InjectSsdt(SystemTable, ImageHandle, Rsdp)
        │
        ├───[Phase 4] 执行 ACPI 二进制补丁
        │     └─ PatchSsdt4PowerWall(SystemTable, ImageHandle, Rsdp)
        │
        └───[Phase 5] 链式加载 Windows Boot Manager
              ├─ 获取 ESP 分区 Device Path
              ├─ 尝试候选路径加载 bootmgfw.efi
              └─ StartImage(WinBootHandle) → 控制权移交
```

---

## 2. Phase 2: RSDP 定位

### 2.1 定位算法

```c
for (Index = 0; Index < SystemTable->NumberOfTableEntries; Index++) {
    if (CompareGuid(
        &SystemTable->ConfigurationTable[Index].VendorGuid,
        &gEfiAcpi20TableGuid
    )) {
        Rsdp = SystemTable->ConfigurationTable[Index].VendorTable;
        break;
    }
}
```

### 2.2 ACPI 2.0 GUID

```
gEfiAcpi20TableGuid:
  Data1: 0x8868E871
  Data2: 0xE4F1
  Data3: 0x11D3
  Data4: { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 }
```

### 2.3 RSDP 结构体

```
RSDP (sizeof=36):
  Offset 0:  Signature[8]   "RSD PTR "
  Offset 8:  Checksum       (20 字节校验和)
  Offset 9:  OemId[6]       OEM ID
  Offset 15: Revision       必须为 2
  Offset 16: RsdtAddress    32 位 RSDT 指针
  Offset 20: Length         RSDP 长度 (≥36)
  Offset 24: XsdtAddress    64 位 XSDT 指针 ← 注入过程修改此字段
  Offset 32: ExtendedChecksum  全结构校验和
```

---

## 3. Phase 3: 自定义 SSDT 注入（InjectSsdt）

### 3.1 内存分配

```
AllocatePages(AllocateType=0, MemoryType=9, PagesNeeded, &AllocateAddr)
  AllocateType=0  → AllocateAnyPages (固件任意分配)
  MemoryType=9    → EfiACPIReclaimMemory
  PagesNeeded     → ⌈sizeof(ssdtunlockdb_aml_code) / 4096⌉
```

将 `SsdtUnlockDB.aml`（编译产物，通过 `#include "SsdtUnlockDB.hex"` 嵌入）逐字节拷贝到分配的内存页。

### 3.2 XSDT 重建步骤

```
原 XSDT:
  [SDT Header 36字节]
  [EntryPtr[0]]  ← 指向 SSDT0
  [EntryPtr[1]]  ← 指向 SSDT1
  ...
  [EntryPtr[N]]  ← 指向 SSDT24

新 XSDT:
  [SDT Header 36字节]
  [EntryPtr[0]]
  [EntryPtr[1]]
  ...
  [EntryPtr[N]]
  [EntryPtr[N+1]] ← ✨ 指向自定义 SSDT (DBUL 设备)
```

具体步骤：
1. `NewXsdtLength = OriginalXsdtLength + 8`（追加 8 字节）
2. `AllocatePages(0, 9, XsdtPagesNeeded, &NewXsdtAddr)`
3. `UefiMemcpy(NewXsdt, OldXsdt, OriginalXsdtLength)` — 完整拷贝
4. `NewEntryPtr[0] = (UINT64)InjectTableBuffer` — 追加新条目
5. `NewXsdt->Length = NewXsdtLength`
6. 重算 `NewXsdt->Checksum`

### 3.3 三阶段校验和同步

| 阶段 | 对象 | 范围 | 字段 |
|------|------|------|------|
| 1 | 新 XSDT | `NewXsdt->Length` 字节 | `NewXsdt->Checksum` |
| 2 | RSDP 旧格式 | 前 20 字节 | `Rsdp->Checksum`（兼容 ACPI 1.0） |
| 3 | RSDP 新格式 | `Rsdp->Length` 字节（≥36） | `Rsdp->ExtendedChecksum`（ACPI 2.0+） |

```c
// 阶段 1
NewXsdt->Checksum = 0;
NewXsdt->Checksum = CalculateChecksum8((UINT8 *)NewXsdt, NewXsdt->Length);

// 阶段 2
Rsdp->Checksum = 0;
Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);

// 阶段 3
RsdpLength = Rsdp->Length;
if (RsdpLength < 36) RsdpLength = 36;  // 边界保护
Rsdp->ExtendedChecksum = 0;
Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);
```

---

## 4. Phase 4: ACPI 二进制补丁（PatchSsdt4PowerWall）

### 4.1 扫描流程

```
PatchSsdt4PowerWall(Rsdp)
  │
  ├─ 从 Rsdp->XsdtAddress 获取 XSDT
  ├─ 验证 XSDT 签名 (必须为 'XSDT' = 0x54445358)
  ├─ EntryCount = (Xsdt->Length - 36) / 8
  │
  └─ for (Index = 0; Index < EntryCount; Index++)
       │
       ├─ IsValidAcpiPointer(EntryPtr[Index]) ?
       │   ├─ NULL?         → skip
       │   ├─ 非 4 字节对齐?  → skip
       │   └─ 地址超出 [1MB, 256GB]? → skip
       │
       ├─ 记录日志: Table Index, Address, Signature
       │
       ├─ [分支: SSDT 表]
       │   ├─ Table->Signature == 0x54445353 ('SSDT') ?
       │   ├─ OEM Table ID 匹配 gPatches[] ?
       │   │   ├─ AllocatePages(影子页)
       │   │   ├─ UefiMemcpy(NewTable, Table, TableLength)
       │   │   ├─ 遍历 gPatches[] → SearchAndReplace()
       │   │   ├─ 有匹配 → NewTable->Checksum 重算
       │   │   │         → EntryPtr[Index] = NewTableAddr
       │   │   │         → PatchedAny = TRUE
       │   │   └─ 无匹配 → FreePages(影子页)
       │   └─ 不匹配 → 下一个表
       │
       └─ [分支: FACP 表]
           ├─ Table->Signature == 0x50434146 ('FACP') ?
           ├─ AllocatePages(FACP 影子页)
           ├─ XDsdtPtr = NewTable + 140  → 获取 DSDT 物理地址
           ├─ DsdtPtr  = NewTable + 40
           │
           ├─ DSDT 存在且有效?
           │   ├─ AllocatePages(DSDT 影子页)
           │   ├─ UefiMemcpy(NewDsdt, DsdtTable, DsdtLength)
           │   ├─ 遍历 gPatches[] → SearchAndReplace()
           │   ├─ 有匹配 → NewDsdt->Checksum 重算
           │   │         → *XDsdtPtr = NewDsdtAddr (同时更新 *DsdtPtr)
           │   │         → FACP EntryPtr[Index] = NewTableAddr
           │   │         → PatchedAny = TRUE
           │   └─ 无匹配 → FreePages(DSDT 影子页)
           └─ DSDT 无补丁 → FreePages(FACP 影子页)
       │
       └─ [其他表] → 跳过
  │
  └─ if (PatchedAny)
       ├─ Xsdt->Checksum 重算
       └─ 日志: "All ACPI memory patches applied successfully."
```

### 4.2 SSDT 匹配逻辑

```c
if (Table->Signature == 0x54445353) {  // 'SSDT'
    BOOLEAN TableMatchesPatch = FALSE;
    for (pIdx = 0; pIdx < gPatchCount; pIdx++) {
        if (Table->OemTableId == gPatches[pIdx].OemTableId) {
            TableMatchesPatch = TRUE;
            break;
        }
    }
    if (TableMatchesPatch) {
        // 创建影子页并执行所有补丁
    }
}
```

### 4.3 FACP → DSDT 重定向

FACP（Fixed ACPI Description Table）包含 DSDT 的两个地址：
- **偏移 40**：`DSDT`（32 位物理地址）
- **偏移 140**：`X_DSDT`（64 位物理地址，ACPI 2.0+ 扩展）

补丁通过 FACP 定位 DSDT 表，对 DSDT 也执行写时复制补丁（如 `cpu_clamp_unlock`），并同步更新 FACP 中的两个 DSDT 指针。

---

## 5. Phase 5: 链式加载 Windows Boot Manager

### 5.1 设备路径构建

```
ESP 分区 Device Path (来自 LoadedImage->DeviceHandle)
  │
  └─ AppendFileNameToDevicePath(SystemTable, PartPath, FilePath)
       │
       ├─ 移除原路径的结束节点 (Type=0x7F, SubType=0xFF)
       ├─ 创建文件路径节点 (Type=4, SubType=4)
       │   ├─ Length = sizeof(NODE_HEADER) + FilePathLen * sizeof(CHAR16)
       │   └─ 填充 UTF-16LE 文件路径字符串
       └─ 追加新结束节点
```

### 5.2 候选路径

| # | 路径 | 格式 |
|---|------|------|
| 0 | `EFI\Microsoft\Boot\bootmgfw.efi` | 相对路径 |
| 1 | `\EFI\Microsoft\Boot\bootmgfw.efi` | 绝对路径 |

### 5.3 加载与启动

```c
for (i = 0; i < NumCandidates; i++) {
    FullDevicePath = AppendFileNameToDevicePath(SystemTable, DevicePath, Candidates[i]);
    
    Status = BS->LoadImage(
        FALSE,          // BootPolicy = FALSE
        ImageHandle,    // ParentImageHandle
        FullDevicePath, // 完整设备路径
        NULL, 0,        // 无 SourceBuffer
        &WinBootHandle  // 输出: 映像句柄
    );
    
    BS->FreePool(FullDevicePath);
    
    if (!EFI_ERROR(Status) && WinBootHandle != NULL) {
        break;  // 加载成功
    }
}

if (WinBootHandle != NULL) {
    Status = BS->StartImage(WinBootHandle, NULL, NULL);
    // ← 控制权移交，此后的代码不会执行（除非 StartImage 失败）
}
```

---

## 6. 内存管理总结

| 阶段 | 分配对象 | 内存类型 | 用途 |
|------|---------|---------|------|
| Phase 3 | 自定义 SSDT | EfiACPIReclaimMemory (9) | 存放 SsdtUnlockDB AML 字节码 |
| Phase 3 | 新 XSDT | EfiACPIReclaimMemory (9) | 重建的 XSDT（含新条目） |
| Phase 4 | SSDT 影子页 | EfiACPIReclaimMemory (9) | OEM SSDT 的写时复制副本 |
| Phase 4 | FACP 影子页 | EfiACPIReclaimMemory (9) | FACP 的写时复制副本（更新 DSDT 指针） |
| Phase 4 | DSDT 影子页 | EfiACPIReclaimMemory (9) | DSDT 的写时复制副本 |
| Phase 5 | FullDevicePath | EfiBootServicesData (2) | 临时设备路径拼接 |

> 所有 ACPI NVS 分配（MemoryType=9）在 Windows 引导完成后由操作系统回收。

---

## 7. 完整日志输出示例

引导时日志写入 `ESP\EFI\BOOT\unlock.log`：

```
=== BOOTLOADER STARTED ===
[+] Successfully located ACPI RSDP Table.
[+] Custom SSDT-UnlockDB table injected successfully.
[+] Starting in-memory scan for SSDT/DSDT tables...
[D] Table 0 at 0x00000000BA3E0000 (Sig: SSDT)
[D] Table 1 at 0x00000000BA3E4000 (Sig: SSDT)
...
[D] Table 4 at 0x00000000BA3F0000 (Sig: SSDT)
[+] Matches target OEM ID. Allocating writable shadow memory page...
    [+] SSDT Patch success: atpp_unlock (count: 0x0000000000000003)
    [+] SSDT Patch success: atp2_unlock (count: 0x0000000000000001)
    [+] SSDT Patch success: cpu_clamp_unlock (count: 0x0000000000000002)
[+] Redirection applied. New table address: 0x00000000BE800000
...
[+] All ACPI memory patches applied successfully.
[+] Chainloading Windows Boot Manager from ESP...
--------------------------------------
EFI\Microsoft\Boot\bootmgfw.efi
[D] LoadImage returned: 0x0000000000000000
[+] LoadImage succeeded!
[+] Starting Windows Boot Manager...
```
