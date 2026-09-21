#include "AcpiPatch.h"
#include "Logging.h"

// ---------------------------------------------------------------------------
// ACPI 运行时特征搜索与内存变量就地修补（PatchAcpiTables）
//
// 本引导器完全不预埋任何静态 ACPI 表（不包含任何预编译的 .aml / .hex）。
// 运行流程：
//   1. 遍历固件原始 ACPI 结构，找到当前机器真实运行的 FACP 和 DSDT。
//   2. 将原始 DSDT 复制到可写的 ACPI Reclaim 内存页（<4GB）。
//   3. 直接在 DSDT 内存镜像中搜索特征（按 tools/thmp_config.yaml 配置），
//      对目标内存变量进行就地修改。
//   4. 重算校验和，克隆 FACP 并将 DSDT / X_DSDT 指针重定向到修改后的内存副本。
//   5. 克隆并更新 XSDT / RSDT 指针，安装新的 ACPI 2.0 配置表。
//
// 优势：完全脱离固件版本与表头指纹的死板绑定，适应任何 BIOS 小版本更新。
// ---------------------------------------------------------------------------

#define ACPI_SIG_DSDT        0x54445344              // "DSDT"
#define ACPI_SIG_FACP        0x50434146              // "FACP"
#define ACPI_SIG_XSDT        0x54445358              // "XSDT"
#define ACPI_SIG_RSDT        0x54445352              // "RSDT"

// THMP 补丁配置（构建期由 tools/generate_thmp_config.py 从 tools/thmp_config.yaml 生成）
#include "ThmpConfig.h"

// 把宽字符串 src 拷入 dst，返回拷贝的 CHAR16 数（不含结尾 NUL）。
// 用于按位置追踪的日志缓冲拼装，避免硬编码偏移。
static UINTN LogAppend(CHAR16 *Dst, const CHAR16 *Src)
{
    UINTN n = 0;
    while (Src[n]) { Dst[n] = Src[n]; n++; }
    return n;
}

// 将 64 位无符号数以十进制追加到 Dst，返回写入的 CHAR16 字符数。
static UINTN LogAppendU64(CHAR16 *Dst, UINT64 Val)
{
    CHAR16 Tmp[24];
    UINTN i = 0, len;
    if (Val == 0) {
        Dst[0] = L'0';
        return 1;
    }
    while (Val > 0) {
        Tmp[i++] = (CHAR16)(L'0' + (Val % 10));
        Val /= 10;
    }
    len = i;
    while (i > 0) {
        *Dst++ = Tmp[--i];
    }
    return len;
}

static void SignatureToStr(UINT32 Signature, CHAR16 *Str)
{
    Str[0] = (CHAR16)(Signature & 0xFF);
    Str[1] = (CHAR16)((Signature >> 8) & 0xFF);
    Str[2] = (CHAR16)((Signature >> 16) & 0xFF);
    Str[3] = (CHAR16)((Signature >> 24) & 0xFF);
    Str[4] = 0;
}

// 从 FACP 读出 DSDT 物理地址，尊重字段存在性：
//   DSDT  （32 位，偏移 40）要求 Length >= 44
//   X_DSDT（64 位，偏移 140，revision >= 2）要求 Length >= 148
static UINT64 ReadFacpDsdtAddress(const EFI_ACPI_SDT_HEADER *Facp)
{
    if (Facp->Length >= 148) {
        UINT64 Addr = *(UINT64 *)((UINT8 *)Facp + 140);
        if (Addr != 0) {
            return Addr;
        }
    }
    if (Facp->Length >= 44) {
        return *(UINT32 *)((UINT8 *)Facp + 40);
    }
    return 0;
}

// 整表换入后让 ACPI 1.0 RSDT 与 XSDT 保持一致。
// Windows x64 走 XSDT，但任何仍在遍历 RSDT 的回退读取者不应落在
// 过期的原表上。32 位 RSDT 条目只能存 4 GiB 以下地址；更高的不动。
static VOID SyncRsdt(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp,
    const UINT64 *OldAddrs,
    const UINT64 *NewAddrs,
    UINTN SwapCount
)
{
    EFI_ACPI_SDT_HEADER *Rsdt;
    UINT32 *EntryPtr;
    UINTN EntryCount;
    UINTN Index;
    UINTN k;
    UINTN Updated = 0;

    if (Rsdp->RsdtAddress == 0 || SwapCount == 0) {
        return;
    }

    Rsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)Rsdp->RsdtAddress;
    if (!IsValidAcpiPointer(Rsdt) || Rsdt->Signature != ACPI_SIG_RSDT) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: RSDT not found/valid - skipped RSDT sync.");
        return;
    }

    EntryCount = (Rsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT32);
    EntryPtr = (UINT32 *)((UINT8 *)Rsdt + sizeof(EFI_ACPI_SDT_HEADER));

    for (Index = 0; Index < EntryCount; Index++) {
        for (k = 0; k < SwapCount; k++) {
            if ((UINT32)OldAddrs[k] == EntryPtr[Index] && NewAddrs[k] <= 0xFFFFFFFFULL) {
                EntryPtr[Index] = (UINT32)NewAddrs[k];
                Updated++;
            }
        }
    }

    if (Updated > 0) {
        Rsdt->Checksum = 0;
        Rsdt->Checksum = CalculateChecksum8((UINT8 *)Rsdt, Rsdt->Length);
        LogToFile(SystemTable, ImageHandle, L"[+] RSDT entries synced to replacement tables.");
    } else {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: RSDT contained no matching entries to sync.");
    }
}

static EFI_STATUS AllocateAcpiPagesBelow4G(
    EFI_BOOT_SERVICES *BS,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Address
)
{
    *Address = 0xFFFFFFFFULL;
    return BS->AllocatePages(AllocateMaxAddress, 9, Pages, Address); // 9 = EfiACPIReclaimMemory
}

// ---------------------------------------------------------------------------
// THMP 表运行时特征搜索与内存变量就地修补
//
// 在 DSDT AML 字节流中定位 THMP_ANCHOR（如 "THMP" Name 声明），然后在
// THMP 数据区域内按 tools/thmp_config.yaml (ThmpConfig.h) 定义的规则逐条
// 执行特征搜索替换。
//
// 调用者保证传入的 Dsdt 在可写内存上（ACPI Reclaim 克隆）。替换后调用者
// 需要重算 Dsdt->Checksum。
// ---------------------------------------------------------------------------
static UINTN PatchThmpInPlace(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_SDT_HEADER *Dsdt
)
{
    UINT8 *Base = (UINT8 *)Dsdt;
    UINTN Length = Dsdt->Length;
    UINTN ThmpOffset = 0;
    BOOLEAN ThmpFound = FALSE;
    UINTN SearchStart;
    UINTN SearchEnd;
    UINTN i, j, r;
    UINTN TotalReplaced = 0;
    static const char Anchor[] = THMP_ANCHOR;
    UINTN AnchorLen = sizeof(Anchor) - 1;

    if (gThmpRuleCount == 0) {
        LogToFile(SystemTable, ImageHandle,
                  L"[I] No THMP patch rules enabled in ThmpConfig.h.");
        return 0;
    }

    // Step 1: 在 AML 中定位锚点（如 "THMP"）
    if (AnchorLen > 0) {
        if (Length < sizeof(EFI_ACPI_SDT_HEADER) + AnchorLen) {
            LogToFile(SystemTable, ImageHandle,
                      L"[-] ALARM: DSDT too small for THMP anchor scan.");
            return 0;
        }

        for (i = sizeof(EFI_ACPI_SDT_HEADER); i + AnchorLen <= Length; i++) {
            BOOLEAN Match = TRUE;
            for (j = 0; j < AnchorLen; j++) {
                if (Base[i + j] != (UINT8)Anchor[j]) {
                    Match = FALSE;
                    break;
                }
            }
            if (Match) {
                ThmpOffset = i;
                ThmpFound = TRUE;
                break;  // 取第一次出现（= Name 声明）
            }
        }

        if (!ThmpFound) {
            LogToFile(SystemTable, ImageHandle,
                      L"[-] ALARM: THMP anchor marker not found in DSDT - in-place patch skipped.");
            return 0;
        }

        LogU64ToFile(SystemTable, ImageHandle,
                     L"[+] THMP anchor found at DSDT offset: ", ThmpOffset);

        SearchStart = ThmpOffset;
        SearchEnd = ThmpOffset + THMP_SEARCH_WINDOW;
    } else {
        // 无锚点时进行全表搜索
        SearchStart = sizeof(EFI_ACPI_SDT_HEADER);
        SearchEnd = Length;
    }

    // Step 2: 针对每条规则在搜索窗口内执行特征搜索并就地替换
    for (r = 0; r < gThmpRuleCount; r++) {
        const THMP_PATCH_RULE *Rule = &gThmpRules[r];
        UINTN RuleHits = 0;
        UINTN PatLen = Rule->PatternLength;
        UINTN RuleEnd = SearchEnd;

        if (PatLen == 0) {
            continue;
        }

        if (RuleEnd > Length - PatLen) {
            RuleEnd = Length - PatLen;
        }

        for (i = SearchStart; i <= RuleEnd; i++) {
            BOOLEAN Match = TRUE;
            for (j = 0; j < PatLen; j++) {
                if (Base[i + j] != Rule->OldPattern[j]) {
                    Match = FALSE;
                    break;
                }
            }
            if (Match) {
                for (j = 0; j < PatLen; j++) {
                    Base[i + j] = Rule->NewPattern[j];
                }
                RuleHits++;
                TotalReplaced++;
                LogU64ToFile(SystemTable, ImageHandle,
                             L"[+]   patched feature at DSDT offset: ", i);
            }
        }

        // 规则执行结果日志
        {
            CHAR16 Buf[128];
            UINTN p = 0;
            p += LogAppend(Buf + p, L"[+] THMP rule '");
            p += LogAppend(Buf + p, Rule->Name);
            p += LogAppend(Buf + p, L"': ");
            p += LogAppendU64(Buf + p, RuleHits);
            p += LogAppend(Buf + p, L" hit(s)");
            if (Rule->ExpectedCount > 0) {
                p += LogAppend(Buf + p, L" (expected ");
                p += LogAppendU64(Buf + p, Rule->ExpectedCount);
                p += LogAppend(Buf + p, L")");
            }
            Buf[p] = 0;
            LogToFile(SystemTable, ImageHandle, Buf);
        }

        if (Rule->ExpectedCount > 0 && RuleHits != Rule->ExpectedCount) {
            CHAR16 Buf[128];
            UINTN p = 0;
            p += LogAppend(Buf + p, L"[!] ALARM: Rule '");
            p += LogAppend(Buf + p, Rule->Name);
            p += LogAppend(Buf + p, L"' hit count mismatch!");
            Buf[p] = 0;
            LogToFile(SystemTable, ImageHandle, Buf);
        }
    }

    return TotalReplaced;
}

// ---------------------------------------------------------------------------
// ReplaceAcpiTables：直接在内存中搜寻固件真实 DSDT 并克隆修补
// ---------------------------------------------------------------------------
EFI_STATUS ReplaceAcpiTables(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
)
{
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_ACPI_SDT_HEADER *Xsdt = (EFI_ACPI_SDT_HEADER *)(Rsdp->XsdtAddress);
    UINTN EntryCount;
    UINT64 *EntryPtr;
    UINTN Index;
    BOOLEAN DsdtPatched = FALSE;
    UINT64 OldAddrs[8];
    UINT64 NewAddrs[8];
    UINTN SwapCount = 0;

    if (Xsdt == NULL || Xsdt->Signature != ACPI_SIG_XSDT) {
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Invalid XSDT signature during table scan.");
        return EFI_NOT_FOUND;
    }

    // 先将固件 XSDT 克隆到自分配的 ACPI Reclaim 页（保证可写），重定向 RSDP
    {
        UINTN XsdtPages = (Xsdt->Length + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS CloneAddr = 0xFFFFFFFFULL;
        EFI_ACPI_SDT_HEADER *Clone;
        UINT32 RsdpLength;

        if (EFI_ERROR(AllocateAcpiPagesBelow4G(BS, XsdtPages, &CloneAddr))) {
            LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate writable XSDT clone.");
            return EFI_OUT_OF_RESOURCES;
        }

        Clone = (EFI_ACPI_SDT_HEADER *)CloneAddr;
        UefiMemcpy(Clone, Xsdt, Xsdt->Length);

        Rsdp->XsdtAddress = (UINT64)CloneAddr;
        Rsdp->Checksum = 0;
        Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);
        RsdpLength = Rsdp->Length;
        if (RsdpLength < 36) {
            RsdpLength = 36;
        }
        Rsdp->ExtendedChecksum = 0;
        Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);

        Xsdt = Clone;
        LogToFile(SystemTable, ImageHandle, L"[+] Writable XSDT clone in place (no embedded tables).");
    }

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    LogToFile(SystemTable, ImageHandle, L"[+] Scanning ACPI tables in memory to locate FACP & DSDT...");

    for (Index = 0; Index < EntryCount; Index++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(EntryPtr[Index]);
        CHAR16 AddrStr[32];
        CHAR16 IndexStr[32];
        CHAR16 LogBuf[256];
        CHAR16 SigStr[5];

        if (!IsValidAcpiPointer(Table)) {
            continue;
        }

        SignatureToStr(Table->Signature, SigStr);

        {
            UINTN pos = 0;
            StatusToHex((EFI_STATUS)Index, IndexStr);
            StatusToHex((EFI_STATUS)Table, AddrStr);
            pos += LogAppend(LogBuf + pos, L"[D] Table ");
            pos += LogAppend(LogBuf + pos, IndexStr);
            pos += LogAppend(LogBuf + pos, L" at ");
            pos += LogAppend(LogBuf + pos, AddrStr);
            pos += LogAppend(LogBuf + pos, L" (Sig: ");
            pos += LogAppend(LogBuf + pos, SigStr);
            pos += LogAppend(LogBuf + pos, L")");
            LogBuf[pos] = 0;
        }
        LogToFile(SystemTable, ImageHandle, LogBuf);

        if (Table->Signature == ACPI_SIG_FACP) {
            // DSDT 由 FACP 引用，不在 XSDT 里直接挂条目
            EFI_ACPI_SDT_HEADER *NewFacp = NULL;
            EFI_PHYSICAL_ADDRESS NewFacpAddr = 0;
            UINTN FacpPagesNeeded;
            UINT64 DsdtPhysicalAddress;
            BOOLEAN FacpDsdtReplaced = FALSE;

            FacpPagesNeeded = (Table->Length + 4095) / 4096;
            if (EFI_ERROR(AllocateAcpiPagesBelow4G(BS, FacpPagesNeeded, &NewFacpAddr))) {
                LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for FACP shadow copy.");
                continue;
            }

            NewFacp = (EFI_ACPI_SDT_HEADER *)NewFacpAddr;
            UefiMemcpy(NewFacp, Table, Table->Length);

            DsdtPhysicalAddress = ReadFacpDsdtAddress(NewFacp);

            if (DsdtPhysicalAddress != 0) {
                EFI_ACPI_SDT_HEADER *DsdtTable = (EFI_ACPI_SDT_HEADER *)DsdtPhysicalAddress;

                if (IsValidAcpiPointer(DsdtTable) && DsdtTable->Signature == ACPI_SIG_DSDT) {
                    BOOLEAN WroteXDsdt = FALSE;
                    BOOLEAN WroteLegacyDsdt = FALSE;
                    UINTN DsdtPagesNeeded;
                    EFI_PHYSICAL_ADDRESS NewDsdtAddr = 0xFFFFFFFFULL;
                    EFI_ACPI_SDT_HEADER *NewDsdt;
                    UINTN ThmpCount;

                    LogU64ToFile(SystemTable, ImageHandle, L"[D]   Firmware DSDT TabId: ", DsdtTable->OemTableId);
                    LogU64ToFile(SystemTable, ImageHandle, L"[D]   Firmware DSDT Rev:  ", DsdtTable->OemRevision);
                    LogU64ToFile(SystemTable, ImageHandle, L"[D]   Firmware DSDT Len:  ", DsdtTable->Length);

                    // 克隆固件原版 DSDT 到可写 ACPI Reclaim 页
                    DsdtPagesNeeded = (DsdtTable->Length + 4095) / 4096;
                    if (EFI_ERROR(AllocateAcpiPagesBelow4G(BS, DsdtPagesNeeded, &NewDsdtAddr))) {
                        LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for DSDT clone.");
                    } else {
                        NewDsdt = (EFI_ACPI_SDT_HEADER *)NewDsdtAddr;
                        UefiMemcpy(NewDsdt, DsdtTable, DsdtTable->Length);

                        // 直接在原版 DSDT 内存镜像中搜寻特征并就地替换
                        ThmpCount = PatchThmpInPlace(SystemTable, ImageHandle, NewDsdt);
                        {
                            CHAR16 Buf[128];
                            UINTN p = 0;
                            p += LogAppend(Buf + p, L"[+] THMP in-place memory patch done: ");
                            p += LogAppendU64(Buf + p, ThmpCount);
                            p += LogAppend(Buf + p, L" replacement(s) applied.");
                            Buf[p] = 0;
                            LogToFile(SystemTable, ImageHandle, Buf);
                        }

                        // 重算新 DSDT 校验和
                        NewDsdt->Checksum = 0;
                        NewDsdt->Checksum = CalculateChecksum8((UINT8 *)NewDsdt, NewDsdt->Length);

                        // 重定向 FACP 的 DSDT 指针到新副本
                        if (NewFacp->Length >= 148) {
                            *(UINT64 *)((UINT8 *)NewFacp + 140) = (UINT64)NewDsdtAddr;
                            WroteXDsdt = TRUE;
                            LogToFile(SystemTable, ImageHandle, L"[D]   FACP X_DSDT (+140) written.");
                        }
                        if (NewFacp->Length >= 44 && NewDsdtAddr <= 0xFFFFFFFFULL) {
                            *(UINT32 *)((UINT8 *)NewFacp + 40) = (UINT32)NewDsdtAddr;
                            WroteLegacyDsdt = TRUE;
                            LogToFile(SystemTable, ImageHandle, L"[D]   FACP DSDT (+40) written.");
                        }
                        if (!WroteXDsdt && !WroteLegacyDsdt) {
                            LogToFile(SystemTable, ImageHandle,
                                      L"[!] ALARM: FACP too short / DSDT above 4GiB - no DSDT pointer written!");
                        }

                        LogToFile(SystemTable, ImageHandle, L"[+] Successfully patched DSDT in memory.");
                        FacpDsdtReplaced = TRUE;
                        DsdtPatched = TRUE;

                        if (SwapCount < 8) {
                            OldAddrs[SwapCount] = DsdtPhysicalAddress;
                            NewAddrs[SwapCount] = (UINT64)NewDsdtAddr;
                            SwapCount++;
                        }
                        LogToFile(SystemTable, ImageHandle, L"[D] Patched DSDT at:");
                        LogAddrToFile(SystemTable, ImageHandle, (UINT64)NewDsdtAddr);
                    }
                }
            }

            if (FacpDsdtReplaced) {
                NewFacp->Checksum = 0;
                NewFacp->Checksum = CalculateChecksum8((UINT8 *)NewFacp, NewFacp->Length);
                if (SwapCount < 8) {
                    OldAddrs[SwapCount] = EntryPtr[Index];
                    NewAddrs[SwapCount] = (UINT64)NewFacpAddr;
                    SwapCount++;
                }
                EntryPtr[Index] = (UINT64)NewFacpAddr;
                LogToFile(SystemTable, ImageHandle, L"[D] FACP shadow at:");
                LogAddrToFile(SystemTable, ImageHandle, (UINT64)NewFacpAddr);
            } else {
                BS->FreePages(NewFacpAddr, FacpPagesNeeded);
            }
        }
    }

    if (DsdtPatched) {
        Xsdt->Checksum = 0;
        Xsdt->Checksum = CalculateChecksum8((UINT8 *)Xsdt, Xsdt->Length);

        LogToFile(SystemTable, ImageHandle,
                  L"[+] ACPI in-place memory patch pass completed successfully.");

        SyncRsdt(SystemTable, ImageHandle, Rsdp, OldAddrs, NewAddrs, SwapCount);
        BS->InstallConfigurationTable(&gEfiAcpi20TableGuid, (VOID *)Rsdp);
        return EFI_SUCCESS;
    }

    ConsolePrint(SystemTable,
                 L"  [-] ALARM: Could not locate FACP/DSDT in ACPI tables for in-place patching!\r\n",
                 TRUE);
    LogToFile(SystemTable, ImageHandle,
              L"[-] ALARM: FACP or DSDT not found during ACPI table scan.");
    return EFI_NOT_FOUND;
}
