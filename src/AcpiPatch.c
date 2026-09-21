#include "AcpiPatch.h"
#include "Logging.h"

// ---------------------------------------------------------------------------
// ACPI 运行时特征搜索与内存变量就地修补（ReplaceAcpiTables）
//
// 本引导器完全不预埋任何静态 ACPI 表（不包含任何预编译的 .aml / .hex）。
// 运行流程：
//   1. 遍历固件原始 ACPI 结构，找到当前机器真实运行的 FACP、DSDT 与 SSDT。
//   2. 在内存中按 tools/thmp_config.yaml 配置搜索特征字节模式，
//      支持锚点窗口搜索与全表回退搜索双重机制，执行就地变量替换。
//   3. 仅当表内命中并修补了特征时，才分配可写的 EfiACPIReclaimMemory 页（<4GB）克隆该表。
//   4. 替换完成后安全跳过模式长度，重算校验和并更新重定向指针。
//   5. 克隆 XSDT 与 RSDP，并安全克隆 RSDT，通过 InstallConfigurationTable 注入。
// ---------------------------------------------------------------------------

#define ACPI_SIG_DSDT        0x54445344              // "DSDT"
#define ACPI_SIG_SSDT        0x54445353              // "SSDT"
#define ACPI_SIG_FACP        0x50434146              // "FACP"
#define ACPI_SIG_XSDT        0x54445358              // "XSDT"
#define ACPI_SIG_RSDT        0x54445352              // "RSDT"

// THMP 补丁配置（构建期由 tools/generate_thmp_config.py 从 tools/thmp_config.yaml 生成）
#include "ThmpConfig.h"

static UINTN LogAppend(CHAR16 *Dst, const CHAR16 *Src)
{
    UINTN n = 0;
    while (Src[n]) { Dst[n] = Src[n]; n++; }
    return n;
}

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
// 同步 RSDT（通过安全克隆新 RSDT，杜绝直接写只读固件内存的保护错误）
// ---------------------------------------------------------------------------
static VOID SyncRsdt(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp,
    const UINT64 *OldAddrs,
    const UINT64 *NewAddrs,
    UINTN SwapCount
)
{
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_ACPI_SDT_HEADER *OldRsdt;
    EFI_ACPI_SDT_HEADER *NewRsdt;
    EFI_PHYSICAL_ADDRESS NewRsdtAddr = 0xFFFFFFFFULL;
    UINT32 *EntryPtr;
    UINTN EntryCount;
    UINTN Index;
    UINTN k;
    UINTN Updated = 0;
    UINTN RsdtPages;

    if (Rsdp->RsdtAddress == 0 || SwapCount == 0) {
        return;
    }

    OldRsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)Rsdp->RsdtAddress;
    if (!IsValidAcpiPointer(OldRsdt) || OldRsdt->Signature != ACPI_SIG_RSDT) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: RSDT not found/valid - skipped RSDT sync.");
        return;
    }

    RsdtPages = (OldRsdt->Length + 4095) / 4096;
    if (EFI_ERROR(AllocateAcpiPagesBelow4G(BS, RsdtPages, &NewRsdtAddr))) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: Failed to allocate RSDT clone page.");
        return;
    }

    NewRsdt = (EFI_ACPI_SDT_HEADER *)NewRsdtAddr;
    UefiMemcpy(NewRsdt, OldRsdt, OldRsdt->Length);

    EntryCount = (NewRsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT32);
    EntryPtr = (UINT32 *)((UINT8 *)NewRsdt + sizeof(EFI_ACPI_SDT_HEADER));

    for (Index = 0; Index < EntryCount; Index++) {
        for (k = 0; k < SwapCount; k++) {
            if ((UINT32)OldAddrs[k] == EntryPtr[Index] && NewAddrs[k] <= 0xFFFFFFFFULL) {
                EntryPtr[Index] = (UINT32)NewAddrs[k];
                Updated++;
            }
        }
    }

    if (Updated > 0) {
        UINT32 RsdpLength;

        NewRsdt->Checksum = 0;
        NewRsdt->Checksum = CalculateChecksum8((UINT8 *)NewRsdt, NewRsdt->Length);

        Rsdp->RsdtAddress = (UINT32)NewRsdtAddr;
        Rsdp->Checksum = 0;
        Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);
        RsdpLength = (Rsdp->Length >= 36) ? Rsdp->Length : 36;
        Rsdp->ExtendedChecksum = 0;
        Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);

        LogToFile(SystemTable, ImageHandle, L"[+] RSDT safely cloned and synced to replacement tables.");
    } else {
        BS->FreePages(NewRsdtAddr, RsdtPages);
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: RSDT contained no matching entries to sync.");
    }
}

// ---------------------------------------------------------------------------
// 在指定 ACPI 表的 AML 内存镜像中搜寻特征并就地替换
// 具备双重搜索策略：
//   1. 若指定了锚点（THMP_ANCHOR），优先在锚点附近搜索窗口内匹配。
//   2. 若锚点未定位或窗口内匹配数为 0，自动回退为全表扫描，确保永不漏搜。
// 匹配成功后向前跳过模式长度，避免重叠或递归误改。
// ---------------------------------------------------------------------------
static UINTN PatchTableFeatures(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_SDT_HEADER *Table,
    const CHAR16 *TableName
)
{
    UINT8 *Base = (UINT8 *)Table;
    UINTN Length = Table->Length;
    UINTN HeaderSize = sizeof(EFI_ACPI_SDT_HEADER);
    UINTN r;
    UINTN TotalReplaced = 0;
    static const char Anchor[] = THMP_ANCHOR;
    UINTN AnchorLen = sizeof(Anchor) - 1;

    if (gThmpRuleCount == 0) {
        return 0;
    }

    if (Length <= HeaderSize) {
        return 0;
    }

    for (r = 0; r < gThmpRuleCount; r++) {
        const THMP_PATCH_RULE *Rule = &gThmpRules[r];
        UINTN RuleHits = 0;
        UINTN PatLen = Rule->PatternLength;
        UINTN SearchStart = HeaderSize;
        UINTN SearchEnd = Length;
        BOOLEAN UsedAnchor = FALSE;

        if (PatLen == 0 || Length < HeaderSize + PatLen) {
            continue;
        }

        // 策略 1：如果配置了锚点，优先在锚点定位窗口内搜索
        if (AnchorLen > 0 && Length >= HeaderSize + AnchorLen) {
            UINTN a;
            for (a = HeaderSize; a + AnchorLen <= Length; a++) {
                BOOLEAN AMatch = TRUE;
                UINTN k;
                for (k = 0; k < AnchorLen; k++) {
                    if (Base[a + k] != (UINT8)Anchor[k]) {
                        AMatch = FALSE;
                        break;
                    }
                }
                if (AMatch) {
                    SearchStart = a;
                    SearchEnd = a + THMP_SEARCH_WINDOW;
                    if (SearchEnd > Length) {
                        SearchEnd = Length;
                    }
                    UsedAnchor = TRUE;
                    LogU64ToFile(SystemTable, ImageHandle, L"[+] Anchor found at offset: ", a);
                    break;
                }
            }
        }

        // 执行特征搜索与就地替换
        if (SearchEnd >= PatLen) {
            UINTN limit = SearchEnd - PatLen;
            UINTN i;
            for (i = SearchStart; i <= limit; i++) {
                BOOLEAN Match = TRUE;
                UINTN j;
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
                    LogU64ToFile(SystemTable, ImageHandle, L"[+]   patched feature at offset: ", i);
                    i += PatLen - 1; // 安全跳过已替换字节
                }
            }
        }

        // 策略 2（回退保护）：若指定了锚点但窗口内命中数为 0，自动触发全表扫描
        if (UsedAnchor && RuleHits == 0 && Length >= HeaderSize + PatLen) {
            UINTN limit = Length - PatLen;
            UINTN i;
            LogToFile(SystemTable, ImageHandle,
                      L"[*] Anchor window had 0 hits; running full table fallback scan...");
            for (i = HeaderSize; i <= limit; i++) {
                BOOLEAN Match = TRUE;
                UINTN j;
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
                    LogU64ToFile(SystemTable, ImageHandle, L"[+]   patched feature (fallback) at offset: ", i);
                    i += PatLen - 1; // 安全跳过已替换字节
                }
            }
        }

        // 规则执行结果日志（仅在该表有命中或非预期时记录）
        if (RuleHits > 0) {
            CHAR16 Buf[128];
            UINTN p = 0;
            p += LogAppend(Buf + p, L"[+] [");
            p += LogAppend(Buf + p, TableName);
            p += LogAppend(Buf + p, L"] Rule '");
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

        if (Rule->ExpectedCount > 0 && RuleHits != 0 && RuleHits != Rule->ExpectedCount) {
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
// ReplaceAcpiTables：动态搜索内存并克隆修补
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
    BOOLEAN AnyTablePatched = FALSE;
    UINT64 OldAddrs[16];
    UINT64 NewAddrs[16];
    UINTN SwapCount = 0;
    EFI_PHYSICAL_ADDRESS XsdtCloneAddr = 0xFFFFFFFFULL;
    EFI_PHYSICAL_ADDRESS RsdpCloneAddr = 0xFFFFFFFFULL;
    UINTN XsdtPages;

    if (Xsdt == NULL || Xsdt->Signature != ACPI_SIG_XSDT) {
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Invalid XSDT signature during table scan.");
        return EFI_NOT_FOUND;
    }

    // 1. 克隆 XSDT 到可写 ACPI Reclaim 页
    XsdtPages = (Xsdt->Length + 4095) / 4096;
    if (EFI_ERROR(AllocateAcpiPagesBelow4G(BS, XsdtPages, &XsdtCloneAddr))) {
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate writable XSDT clone.");
        return EFI_OUT_OF_RESOURCES;
    }

    {
        EFI_ACPI_SDT_HEADER *Clone = (EFI_ACPI_SDT_HEADER *)XsdtCloneAddr;
        UefiMemcpy(Clone, Xsdt, Xsdt->Length);
        Xsdt = Clone;
        LogToFile(SystemTable, ImageHandle, L"[+] Writable XSDT clone in place.");
    }

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    LogToFile(SystemTable, ImageHandle, L"[+] Scanning ACPI tables in memory (DSDT & SSDTs)...");

    // 2. 遍历 XSDT 条目，检查 FACP (引向 DSDT) 与所有 SSDT
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

        // --- 途径 A: FACP -> DSDT ---
        if (Table->Signature == ACPI_SIG_FACP) {
            EFI_PHYSICAL_ADDRESS NewFacpAddr = 0;
            UINTN FacpPagesNeeded = (Table->Length + 4095) / 4096;
            UINT64 DsdtPhysicalAddress;

            if (EFI_ERROR(AllocateAcpiPagesBelow4G(BS, FacpPagesNeeded, &NewFacpAddr))) {
                LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for FACP shadow copy.");
                continue;
            }

            EFI_ACPI_SDT_HEADER *NewFacp = (EFI_ACPI_SDT_HEADER *)NewFacpAddr;
            UefiMemcpy(NewFacp, Table, Table->Length);
            DsdtPhysicalAddress = ReadFacpDsdtAddress(NewFacp);

            BOOLEAN FacpDsdtReplaced = FALSE;

            if (DsdtPhysicalAddress != 0) {
                EFI_ACPI_SDT_HEADER *DsdtTable = (EFI_ACPI_SDT_HEADER *)DsdtPhysicalAddress;

                if (IsValidAcpiPointer(DsdtTable) && DsdtTable->Signature == ACPI_SIG_DSDT) {
                    UINTN DsdtPagesNeeded = (DsdtTable->Length + 4095) / 4096;
                    EFI_PHYSICAL_ADDRESS NewDsdtAddr = 0xFFFFFFFFULL;

                    LogU64ToFile(SystemTable, ImageHandle, L"[D]   Firmware DSDT TabId: ", DsdtTable->OemTableId);
                    LogU64ToFile(SystemTable, ImageHandle, L"[D]   Firmware DSDT Rev:  ", DsdtTable->OemRevision);
                    LogU64ToFile(SystemTable, ImageHandle, L"[D]   Firmware DSDT Len:  ", DsdtTable->Length);

                    if (!EFI_ERROR(AllocateAcpiPagesBelow4G(BS, DsdtPagesNeeded, &NewDsdtAddr))) {
                        EFI_ACPI_SDT_HEADER *NewDsdt = (EFI_ACPI_SDT_HEADER *)NewDsdtAddr;
                        UefiMemcpy(NewDsdt, DsdtTable, DsdtTable->Length);

                        UINTN ReplacedCount = PatchTableFeatures(SystemTable, ImageHandle, NewDsdt, L"DSDT");
                        if (ReplacedCount > 0) {
                            CHAR16 Buf[128];
                            UINTN p = 0;
                            p += LogAppend(Buf + p, L"[+] DSDT in-place patch complete: ");
                            p += LogAppendU64(Buf + p, ReplacedCount);
                            p += LogAppend(Buf + p, L" replacement(s) applied.");
                            Buf[p] = 0;
                            LogToFile(SystemTable, ImageHandle, Buf);

                            // 重算新 DSDT 校验和
                            NewDsdt->Checksum = 0;
                            NewDsdt->Checksum = CalculateChecksum8((UINT8 *)NewDsdt, NewDsdt->Length);

                            // 重定向 FACP 的 DSDT 指针
                            BOOLEAN WroteXDsdt = FALSE;
                            BOOLEAN WroteLegacyDsdt = FALSE;
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

                            FacpDsdtReplaced = TRUE;
                            AnyTablePatched = TRUE;

                            if (SwapCount < 16) {
                                OldAddrs[SwapCount] = DsdtPhysicalAddress;
                                NewAddrs[SwapCount] = (UINT64)NewDsdtAddr;
                                SwapCount++;
                            }
                            LogToFile(SystemTable, ImageHandle, L"[D] Patched DSDT at:");
                            LogAddrToFile(SystemTable, ImageHandle, (UINT64)NewDsdtAddr);
                        } else {
                            // 0 次替换：释放临时分配的 DSDT 克隆
                            BS->FreePages(NewDsdtAddr, DsdtPagesNeeded);
                            LogToFile(SystemTable, ImageHandle, L"[-] DSDT had 0 pattern matches (skipped).");
                        }
                    } else {
                        LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for DSDT clone.");
                    }
                }
            }

            if (FacpDsdtReplaced) {
                NewFacp->Checksum = 0;
                NewFacp->Checksum = CalculateChecksum8((UINT8 *)NewFacp, NewFacp->Length);
                if (SwapCount < 16) {
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
        // --- 途径 B: XSDT 直接挂载的 SSDT ---
        else if (Table->Signature == ACPI_SIG_SSDT) {
            UINTN SsdtPagesNeeded = (Table->Length + 4095) / 4096;
            EFI_PHYSICAL_ADDRESS NewSsdtAddr = 0xFFFFFFFFULL;

            if (!EFI_ERROR(AllocateAcpiPagesBelow4G(BS, SsdtPagesNeeded, &NewSsdtAddr))) {
                EFI_ACPI_SDT_HEADER *NewSsdt = (EFI_ACPI_SDT_HEADER *)NewSsdtAddr;
                UefiMemcpy(NewSsdt, Table, Table->Length);

                UINTN ReplacedCount = PatchTableFeatures(SystemTable, ImageHandle, NewSsdt, L"SSDT");
                if (ReplacedCount > 0) {
                    CHAR16 Buf[128];
                    UINTN p = 0;
                    p += LogAppend(Buf + p, L"[+] SSDT in-place patch complete: ");
                    p += LogAppendU64(Buf + p, ReplacedCount);
                    p += LogAppend(Buf + p, L" replacement(s) applied.");
                    Buf[p] = 0;
                    LogToFile(SystemTable, ImageHandle, Buf);

                    NewSsdt->Checksum = 0;
                    NewSsdt->Checksum = CalculateChecksum8((UINT8 *)NewSsdt, NewSsdt->Length);

                    if (SwapCount < 16) {
                        OldAddrs[SwapCount] = EntryPtr[Index];
                        NewAddrs[SwapCount] = (UINT64)NewSsdtAddr;
                        SwapCount++;
                    }
                    EntryPtr[Index] = (UINT64)NewSsdtAddr;
                    AnyTablePatched = TRUE;

                    LogToFile(SystemTable, ImageHandle, L"[D] Patched SSDT at:");
                    LogAddrToFile(SystemTable, ImageHandle, (UINT64)NewSsdtAddr);
                } else {
                    BS->FreePages(NewSsdtAddr, SsdtPagesNeeded);
                }
            }
        }
    }

    if (AnyTablePatched) {
        EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *NewRsdp = NULL;
        UINT32 RsdpLength;

        // 3. 重算克隆 XSDT 校验和
        Xsdt->Checksum = 0;
        Xsdt->Checksum = CalculateChecksum8((UINT8 *)Xsdt, Xsdt->Length);

        // 4. 安全克隆 RSDP 到可写页，避免原固件页写保护触发硬件异常
        if (!EFI_ERROR(AllocateAcpiPagesBelow4G(BS, 1, &RsdpCloneAddr))) {
            NewRsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)RsdpCloneAddr;
            RsdpLength = (Rsdp->Length >= 36) ? Rsdp->Length : 36;
            UefiMemcpy(NewRsdp, Rsdp, RsdpLength);

            NewRsdp->XsdtAddress = (UINT64)XsdtCloneAddr;
            NewRsdp->Checksum = 0;
            NewRsdp->Checksum = CalculateChecksum8((UINT8 *)NewRsdp, 20);
            NewRsdp->ExtendedChecksum = 0;
            NewRsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)NewRsdp, RsdpLength);

            SyncRsdt(SystemTable, ImageHandle, NewRsdp, OldAddrs, NewAddrs, SwapCount);
            BS->InstallConfigurationTable(&gEfiAcpi20TableGuid, (VOID *)NewRsdp);
            LogToFile(SystemTable, ImageHandle, L"[+] Installed new ACPI 2.0 ConfigurationTable.");
        } else {
            // 回退直接修改原 RSDP
            Rsdp->XsdtAddress = (UINT64)XsdtCloneAddr;
            Rsdp->Checksum = 0;
            Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);
            RsdpLength = (Rsdp->Length >= 36) ? Rsdp->Length : 36;
            Rsdp->ExtendedChecksum = 0;
            Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);

            SyncRsdt(SystemTable, ImageHandle, Rsdp, OldAddrs, NewAddrs, SwapCount);
            BS->InstallConfigurationTable(&gEfiAcpi20TableGuid, (VOID *)Rsdp);
        }

        LogToFile(SystemTable, ImageHandle, L"[+] ACPI in-place memory patch completed successfully.");
        return EFI_SUCCESS;
    }

    // 未修补任何表时，释放 XSDT 克隆，明确报警
    BS->FreePages(XsdtCloneAddr, XsdtPages);

    ConsolePrint(SystemTable,
                 L"  [-] ALARM: No target features matched in DSDT or SSDTs!\r\n",
                 TRUE);
    LogToFile(SystemTable, ImageHandle,
              L"[-] ALARM: 0 feature patterns matched in memory during ACPI scan.");
    return EFI_NOT_FOUND;
}
