#include "AcpiPatch.h"
#include "Logging.h"
#include "InplacePatches.h"

#include "../build/SsdtUnlockDB.hex"
#include "../build/ssdt4_patched.hex"
#include "../build/dsdt_patched.hex"

// ---------------------------------------------------------------------------
// ACPI Table Replacement & In-place Patching
//
// 1) Whole-Table Replacement (ReplaceAcpiTables):
//    Patched tables (DSDT, SSDT4) are compiled offline by iASL into compliant AML
//    and injected via XSDT / FACP pointer redirection. When the OS ACPI driver
//    initializes tables from RSDP, it uses the replacement tables.
//
// 2) Optional In-place Byte Patching (PatchTablesInPlace):
//    Applies size-preserving byte patches directly to original table pages as a
//    fallback pass.
// ---------------------------------------------------------------------------

#define OEM_TABLE_ID_EDK2    0x20202020324B4445ULL   // "EDK2    " (little-endian)

#define ACPI_SIG_DSDT        0x54445344              // "DSDT"
#define ACPI_SIG_SSDT        0x54445353              // "SSDT"
#define ACPI_SIG_FACP        0x50434146              // "FACP"

typedef struct {
    UINT32 Signature;          // Target table signature ('SSDT' / 'DSDT')
    UINT64 OemTableId;         // Target OEM Table ID
    UINT32 OemRevision;        // Target OEM Revision
    const UINT8 *Fingerprint;  // Optional content fingerprint (NULL = skip)
    UINTN FingerprintLen;
    const UINT8 *NewTable;     // Pre-built replacement table (with ACPI header)
    UINTN NewTableSize;
    const CHAR16 *Name;
} TABLE_REPLACEMENT;

static const TABLE_REPLACEMENT gTableReplacements[] = {
    {
        ACPI_SIG_SSDT,
        OEM_TABLE_ID_EDK2,
        0x00001000,                          // SSDT4 (NPCF power wall)
        (const UINT8 *)"NPCF", 4,            // content fingerprint, unique to SSDT4
        ssdt4_patched_aml_code,
        sizeof(ssdt4_patched_aml_code),
        L"SSDT4_NPCF_PowerWall"
    },
    {
        ACPI_SIG_DSDT,
        OEM_TABLE_ID_EDK2,
        0x00000002,                          // DSDT (CPU power clamp MSPL/MFPT/FNQS)
        NULL, 0,
        dsdt_patched_aml_code,
        sizeof(dsdt_patched_aml_code),
        L"DSDT_CpuPowerClamp"
    },
};

static const UINTN gTableReplacementCount =
    sizeof(gTableReplacements) / sizeof(gTableReplacements[0]);

static void SignatureToStr(UINT32 Signature, CHAR16 *Str)
{
    Str[0] = (CHAR16)(Signature & 0xFF);
    Str[1] = (CHAR16)((Signature >> 8) & 0xFF);
    Str[2] = (CHAR16)((Signature >> 16) & 0xFF);
    Str[3] = (CHAR16)((Signature >> 24) & 0xFF);
    Str[4] = 0;
}

// Read the DSDT physical address out of a FACP, honoring field presence:
//   DSDT  (32-bit, offset 40) requires Length >= 44
//   X_DSDT (64-bit, offset 140, revision >= 2) requires Length >= 148
// Unconditional reads at +140 (previous code) could walk off a short FACP.
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

// Keep the ACPI 1.0 RSDT in step with the XSDT after whole-table swaps.
// Windows x64 consumes the XSDT, but a fallback reader that still walks the
// RSDT must not end up at the stale originals. 32-bit RSDT entries can only
// hold addresses below 4 GiB; entries above that are left untouched.
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
    if (!IsValidAcpiPointer(Rsdt) || Rsdt->Signature != 0x54445352) { // "RSDT"
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

static BOOLEAN ContainsBytes(
    const UINT8 *Buffer,
    UINTN BufferSize,
    const UINT8 *Pattern,
    UINTN PatternSize
)
{
    UINTN i, j;

    if (Pattern == NULL || PatternSize == 0 || BufferSize < PatternSize) {
        return FALSE;
    }

    for (i = 0; i <= BufferSize - PatternSize; i++) {
        BOOLEAN Match = TRUE;
        for (j = 0; j < PatternSize; j++) {
            if (Buffer[i + j] != Pattern[j]) {
                Match = FALSE;
                break;
            }
        }
        if (Match) {
            return TRUE;
        }
    }
    return FALSE;
}

static const TABLE_REPLACEMENT *FindReplacement(
    UINT32 Signature,
    const EFI_ACPI_SDT_HEADER *Table
)
{
    UINTN i;

    for (i = 0; i < gTableReplacementCount; i++) {
        const TABLE_REPLACEMENT *Spec = &gTableReplacements[i];

        if (Spec->Signature != Signature) {
            continue;
        }
        if (Table->OemTableId != Spec->OemTableId) {
            continue;
        }
        if (Table->OemRevision != Spec->OemRevision) {
            continue;
        }
        if (Spec->Fingerprint != NULL) {
            if (!ContainsBytes((const UINT8 *)Table, Table->Length,
                               Spec->Fingerprint, Spec->FingerprintLen)) {
                continue;
            }
        }
        return Spec;
    }
    return NULL;
}

// Copy the pre-built replacement table into ACPI Reclaim memory and point
// *EntryPtr at it. Returns TRUE on success.
static BOOLEAN ApplyReplacement(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    const TABLE_REPLACEMENT *Spec,
    UINT64 *EntryPtr,
    const CHAR16 *LogTag
)
{
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    UINTN PagesNeeded;
    EFI_PHYSICAL_ADDRESS NewTableAddr = 0;
    EFI_ACPI_SDT_HEADER *NewTable;

    PagesNeeded = (Spec->NewTableSize + 4095) / 4096;
    if (EFI_ERROR(BS->AllocatePages(0, 9, PagesNeeded, &NewTableAddr))) {
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate ACPI Reclaim memory for replacement table.");
        return FALSE;
    }

    NewTable = (EFI_ACPI_SDT_HEADER *)NewTableAddr;
    UefiMemcpy(NewTable, Spec->NewTable, Spec->NewTableSize);

    // Rebuild header checksum
    NewTable->Checksum = 0;
    NewTable->Checksum = CalculateChecksum8((UINT8 *)NewTable, NewTable->Length);

    *EntryPtr = (UINT64)NewTableAddr;

    LogToFile(SystemTable, ImageHandle, LogTag);
    return TRUE;
}

// ---------------------------------------------------------------------------
// PatchTablesInPlace: apply equal-size byte patches to the ORIGINAL tables.
// ---------------------------------------------------------------------------

static UINTN CountOccurrences(
    const UINT8 *Buffer,
    UINTN BufferSize,
    const UINT8 *Pattern,
    UINTN PatternSize
)
{
    UINTN Count = 0;
    UINTN i, j;

    if (Pattern == NULL || PatternSize == 0 || BufferSize < PatternSize) {
        return 0;
    }

    for (i = 0; i <= BufferSize - PatternSize; i++) {
        BOOLEAN Match = TRUE;
        for (j = 0; j < PatternSize; j++) {
            if (Buffer[i + j] != Pattern[j]) {
                Match = FALSE;
                break;
            }
        }
        if (Match) {
            Count++;
            i += PatternSize - 1;
        }
    }
    return Count;
}

static VOID ReplaceAllOccurrences(
    UINT8 *Buffer,
    UINTN BufferSize,
    const UINT8 *Search,
    UINTN SearchLen,
    const UINT8 *Replace,
    UINTN ReplaceLen
)
{
    UINTN i, j;

    if (Search == NULL || Replace == NULL || SearchLen == 0 ||
        ReplaceLen != SearchLen || BufferSize < SearchLen) {
        return;
    }

    for (i = 0; i <= BufferSize - SearchLen; i++) {
        BOOLEAN Match = TRUE;
        for (j = 0; j < SearchLen; j++) {
            if (Buffer[i + j] != Search[j]) {
                Match = FALSE;
                break;
            }
        }
        if (Match) {
            for (j = 0; j < ReplaceLen; j++) {
                Buffer[i + j] = Replace[j];
            }
            i += SearchLen - 1;
        }
    }
}

static VOID LogInplaceResult(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    const CHAR16 *Prefix,
    const CHAR16 *Name,
    UINTN Found,
    UINTN Expected
)
{
    CHAR16 Buf[256];
    CHAR16 NumStr[32];
    UINTN Pos = 0;
    UINTN i;

    for (i = 0; Prefix[i] != 0 && Pos < 240; i++) {
        Buf[Pos++] = Prefix[i];
    }
    for (i = 0; Name[i] != 0 && Pos < 240; i++) {
        Buf[Pos++] = Name[i];
    }
    if (Pos < 240) Buf[Pos++] = L' ';
    if (Pos < 240) Buf[Pos++] = L'(';
    StatusToHex((EFI_STATUS)Found, NumStr);
    for (i = 0; NumStr[i] != 0 && Pos < 240; i++) {
        Buf[Pos++] = NumStr[i];
    }
    if (Pos < 240) Buf[Pos++] = L'/';
    StatusToHex((EFI_STATUS)Expected, NumStr);
    for (i = 0; NumStr[i] != 0 && Pos < 240; i++) {
        Buf[Pos++] = NumStr[i];
    }
    if (Pos < 240) Buf[Pos++] = L')';
    Buf[Pos] = 0;

    LogToFile(SystemTable, ImageHandle, Buf);
}

// Locates the ORIGINAL DSDT (via FACP) and SSDT4 (via XSDT entry match),
// applies every gInplacePatches entry whose occurrence count matches its
// ExpectedCount, and recomputes the modified tables' checksums in place.
// Must run BEFORE ReplaceAcpiTables, while the XSDT entries still point at
// the original tables. The original pages are writable on this platform
// (verified by the earlier in-place injection experiments).
EFI_STATUS PatchTablesInPlace(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
)
{
    EFI_ACPI_SDT_HEADER *Xsdt = (EFI_ACPI_SDT_HEADER *)(Rsdp->XsdtAddress);
    EFI_ACPI_SDT_HEADER *DsdtTable = NULL;
    EFI_ACPI_SDT_HEADER *Ssdt4Table = NULL;
    BOOLEAN DsdtModified = FALSE;
    BOOLEAN Ssdt4Modified = FALSE;
    UINTN EntryCount;
    UINT64 *EntryPtr;
    UINTN Index;
    UINTN i;
    UINTN PatchesApplied = 0;
    UINTN PatchesSkipped = 0;

    if (Xsdt == NULL || Xsdt->Signature != 0x54445358) { // "XSDT"
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Invalid XSDT signature during in-place patch scan.");
        return EFI_NOT_FOUND;
    }

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    LogToFile(SystemTable, ImageHandle, L"[+] Starting in-place patch scan on ORIGINAL firmware tables...");

    for (Index = 0; Index < EntryCount; Index++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(EntryPtr[Index]);

        if (!IsValidAcpiPointer(Table)) {
            continue;
        }

        if (Table->Signature == ACPI_SIG_FACP && DsdtTable == NULL) {
            // DSDT is referenced from FACP, not from the XSDT directly.
            UINT64 Addr = ReadFacpDsdtAddress(Table);
            EFI_ACPI_SDT_HEADER *Dsdt;

            if (Addr == 0) {
                continue;
            }
            Dsdt = (EFI_ACPI_SDT_HEADER *)Addr;
            if (IsValidAcpiPointer(Dsdt) && Dsdt->Signature == ACPI_SIG_DSDT) {
                DsdtTable = Dsdt;
                LogToFile(SystemTable, ImageHandle, L"[+] InPlace: original DSDT located via FACP.");
            }
        }
        else if (Table->Signature == ACPI_SIG_SSDT && Ssdt4Table == NULL) {
            if (Table->OemTableId == OEM_TABLE_ID_EDK2 &&
                Table->OemRevision == 0x00001000 &&
                ContainsBytes((const UINT8 *)Table, Table->Length,
                              (const UINT8 *)"NPCF", 4)) {
                Ssdt4Table = Table;
                LogToFile(SystemTable, ImageHandle, L"[+] InPlace: original SSDT4 (NPCF) located via XSDT.");
            }
        }
    }

    if (DsdtTable == NULL) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: original DSDT not found for in-place patching.");
    }
    if (Ssdt4Table == NULL) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: original SSDT4 not found for in-place patching.");
    }

    for (i = 0; i < gInplacePatchCount; i++) {
        const INPLACE_PATCH *Patch = &gInplacePatches[i];
        EFI_ACPI_SDT_HEADER *Target;
        BOOLEAN *ModifiedFlag;
        UINTN Found;

        if (Patch->TargetTable == INPLACE_TARGET_DSDT) {
            Target = DsdtTable;
            ModifiedFlag = &DsdtModified;
        } else {
            Target = Ssdt4Table;
            ModifiedFlag = &Ssdt4Modified;
        }

        if (Target == NULL) {
            LogToFile(SystemTable, ImageHandle, L"[-] ALARM: InPlace target table not found. Patch SKIPPED: ");
            LogToFile(SystemTable, ImageHandle, Patch->Name);
            PatchesSkipped++;
            continue;
        }

        if (Patch->SearchLen != Patch->ReplaceLen || Patch->SearchLen == 0) {
            LogToFile(SystemTable, ImageHandle, L"[-] ALARM: InPlace patch has unequal search/replace size. Patch SKIPPED: ");
            LogToFile(SystemTable, ImageHandle, Patch->Name);
            PatchesSkipped++;
            continue;
        }

        Found = CountOccurrences((const UINT8 *)Target, Target->Length,
                                 Patch->Search, Patch->SearchLen);
        if (Found == Patch->ExpectedCount) {
            ReplaceAllOccurrences((UINT8 *)Target, Target->Length,
                                  Patch->Search, Patch->SearchLen,
                                  Patch->Replace, Patch->ReplaceLen);
            *ModifiedFlag = TRUE;
            PatchesApplied++;
            LogInplaceResult(SystemTable, ImageHandle, L"[+] InPlace patch applied: ",
                             Patch->Name, Found, Patch->ExpectedCount);
        } else {
            PatchesSkipped++;
            LogInplaceResult(SystemTable, ImageHandle, L"[-] ALARM: InPlace count mismatch, patch SKIPPED: ",
                             Patch->Name, Found, Patch->ExpectedCount);
        }
    }

    if (DsdtModified) {
        DsdtTable->Checksum = 0;
        DsdtTable->Checksum = CalculateChecksum8((UINT8 *)DsdtTable, DsdtTable->Length);
        LogToFile(SystemTable, ImageHandle, L"[+] InPlace: original DSDT checksum recomputed in place.");
    }
    if (Ssdt4Modified) {
        Ssdt4Table->Checksum = 0;
        Ssdt4Table->Checksum = CalculateChecksum8((UINT8 *)Ssdt4Table, Ssdt4Table->Length);
        LogToFile(SystemTable, ImageHandle, L"[+] InPlace: original SSDT4 checksum recomputed in place.");
    }

    if (DsdtTable == NULL && Ssdt4Table == NULL) {
        ConsolePrint(SystemTable,
                     L"  [-] ALARM: in-place patch targets NOT FOUND (neither original DSDT "
                     L"nor SSDT4). BIOS version changed?\r\n",
                     TRUE);
        LogToFile(SystemTable, ImageHandle,
                  L"[-] ALARM: no in-place target tables located (DSDT/SSDT4).");
        return EFI_NOT_FOUND;
    }

    if (PatchesApplied == 0) {
        LogToFile(SystemTable, ImageHandle,
                  L"[I] In-place patch pass: 0 patches applied (relies on table replacement).");
        return EFI_SUCCESS;
    }

    if (PatchesSkipped != 0) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: some in-place patches were skipped.");
    }

    LogToFile(SystemTable, ImageHandle, L"[+] In-place patch pass finished.");
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// InjectSsdt: append the custom DBUL WMI SSDT to the XSDT.
// ---------------------------------------------------------------------------
EFI_STATUS InjectSsdt(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
)
{
    EFI_STATUS Status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_ACPI_SDT_HEADER *Xsdt = (EFI_ACPI_SDT_HEADER *)(Rsdp->XsdtAddress);
    UINTN PagesNeeded;
    EFI_PHYSICAL_ADDRESS AllocateAddr = 0;
    UINT8 *InjectTableBuffer;

    PagesNeeded = (sizeof(ssdtunlockdb_aml_code) + 4095) / 4096;
    Status = BS->AllocatePages(0, 9, PagesNeeded, &AllocateAddr);
    if (EFI_ERROR(Status)) {
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate ACPI Reclaim Memory for custom SSDT.");
        return Status;
    }

    InjectTableBuffer = (UINT8 *)AllocateAddr;
    UefiMemcpy(InjectTableBuffer, ssdtunlockdb_aml_code, sizeof(ssdtunlockdb_aml_code));

    {
        UINTN OriginalXsdtLength = Xsdt->Length;
        UINTN NewXsdtLength = OriginalXsdtLength + sizeof(UINT64);
        UINTN XsdtPagesNeeded = (NewXsdtLength + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS NewXsdtAddr = 0;
        EFI_ACPI_SDT_HEADER *NewXsdt = NULL;
        UINT64 *NewEntryPtr = NULL;
        UINT32 RsdpLength = 0;

        Status = BS->AllocatePages(0, 9, XsdtPagesNeeded, &NewXsdtAddr);
        if (EFI_ERROR(Status)) {
            LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for New XSDT.");
            return Status;
        }

        NewXsdt = (EFI_ACPI_SDT_HEADER *)NewXsdtAddr;

        UefiMemcpy(NewXsdt, Xsdt, OriginalXsdtLength);

        NewEntryPtr = (UINT64 *)((UINT8 *)NewXsdt + OriginalXsdtLength);
        NewEntryPtr[0] = (UINT64)InjectTableBuffer;

        NewXsdt->Length = (UINT32)NewXsdtLength;
        NewXsdt->Checksum = 0;
        NewXsdt->Checksum = CalculateChecksum8((UINT8 *)NewXsdt, NewXsdt->Length);

        Rsdp->XsdtAddress = (UINT64)NewXsdtAddr;

        Rsdp->Checksum = 0;
        Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);

        RsdpLength = Rsdp->Length;
        if (RsdpLength < 36) {
            RsdpLength = 36;
        }
        Rsdp->ExtendedChecksum = 0;
        Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);

        // Best effort: keep ACPI 1.0 RSDT fallback readers aware of the newly
        // injected SSDT. Only possible when the allocation fits in 32 bits.
        if (AllocateAddr <= 0xFFFFFFFFULL && Rsdp->RsdtAddress != 0) {
            EFI_ACPI_SDT_HEADER *OldRsdt =
                (EFI_ACPI_SDT_HEADER *)(UINTN)Rsdp->RsdtAddress;
            if (IsValidAcpiPointer(OldRsdt) && OldRsdt->Signature == 0x54445352) {
                UINTN OldRsdtLength = OldRsdt->Length;
                UINTN NewRsdtLength = OldRsdtLength + sizeof(UINT32);
                UINTN RsdtPagesNeeded = (NewRsdtLength + 4095) / 4096;
                EFI_PHYSICAL_ADDRESS NewRsdtAddr = 0;
                EFI_ACPI_SDT_HEADER *NewRsdt;

                Status = BS->AllocatePages(0, 9, RsdtPagesNeeded, &NewRsdtAddr);
                if (!EFI_ERROR(Status)) {
                    NewRsdt = (EFI_ACPI_SDT_HEADER *)NewRsdtAddr;
                    UefiMemcpy(NewRsdt, OldRsdt, OldRsdtLength);
                    *(UINT32 *)((UINT8 *)NewRsdt + OldRsdtLength) = (UINT32)AllocateAddr;
                    NewRsdt->Length = (UINT32)NewRsdtLength;
                    NewRsdt->Checksum = 0;
                    NewRsdt->Checksum = CalculateChecksum8((UINT8 *)NewRsdt, NewRsdtLength);
                    Rsdp->RsdtAddress = (UINT32)NewRsdtAddr;
                    Rsdp->Checksum = 0;
                    Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);
                    Rsdp->ExtendedChecksum = 0;
                    Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);
                    LogToFile(SystemTable, ImageHandle,
                              L"[+] RSDT extended with injected SSDT entry.");
                } else {
                    LogToFile(SystemTable, ImageHandle,
                              L"[-] Warn: RSDT extend failed (non-fatal).");
                }
            }
        }
    }

    LogToFile(SystemTable, ImageHandle, L"[+] Custom SSDT-UnlockDB table injected successfully.");
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// ReplaceAcpiTables: swap pre-built replacement tables (DSDT + SSDT4) into the
// ACPI namespace via XSDT / FACP pointer redirection.
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
    BOOLEAN ReplacedAny = FALSE;
    UINT64 OldAddrs[8];
    UINT64 NewAddrs[8];
    UINTN SwapCount = 0;

    if (Xsdt == NULL || Xsdt->Signature != 0x54445358) { // "XSDT"
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Invalid XSDT signature during table replacement scan.");
        return EFI_NOT_FOUND;
    }

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    LogToFile(SystemTable, ImageHandle, L"[+] Starting in-memory scan for tables to replace...");

    for (Index = 0; Index < EntryCount; Index++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(EntryPtr[Index]);
        CHAR16 AddrStr[32];
        CHAR16 IndexStr[32];
        CHAR16 LogBuf[256];
        CHAR16 SigStr[5];
        const TABLE_REPLACEMENT *Spec;

        if (!IsValidAcpiPointer(Table)) {
            continue;
        }

        SignatureToStr(Table->Signature, SigStr);

        UefiMemcpy(LogBuf, L"[D] Table ", 10 * sizeof(CHAR16));
        StatusToHex((EFI_STATUS)Index, IndexStr);
        UefiMemcpy(LogBuf + 10, IndexStr, 18 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 28, L" at ", 4 * sizeof(CHAR16));
        StatusToHex((EFI_STATUS)Table, AddrStr);
        UefiMemcpy(LogBuf + 32, AddrStr, 18 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 50, L" (Sig: ", 7 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 57, SigStr, 4 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 61, L")", 1 * sizeof(CHAR16));
        LogBuf[62] = 0;
        LogToFile(SystemTable, ImageHandle, LogBuf);

        if (Table->Signature == ACPI_SIG_SSDT) {
            UINT64 OldEntry = EntryPtr[Index];

            Spec = FindReplacement(ACPI_SIG_SSDT, Table);
            if (Spec == NULL) {
                continue;
            }

            LogToFile(SystemTable, ImageHandle, L"[+] Matches SSDT replacement target. Swapping table...");

            if (ApplyReplacement(SystemTable, ImageHandle, Spec,
                                 &EntryPtr[Index], Spec->Name)) {
                if (SwapCount < 8) {
                    OldAddrs[SwapCount] = OldEntry;
                    NewAddrs[SwapCount] = EntryPtr[Index];
                    SwapCount++;
                }
                ReplacedAny = TRUE;
                LogToFile(SystemTable, ImageHandle, L"[D] SSDT4 new table at:");
                LogAddrToFile(SystemTable, ImageHandle, EntryPtr[Index]);
            }
        }
        else if (Table->Signature == ACPI_SIG_FACP) {
            // DSDT is referenced from FACP, not from the XSDT directly.
            EFI_ACPI_SDT_HEADER *NewFacp = NULL;
            EFI_PHYSICAL_ADDRESS NewFacpAddr = 0;
            UINTN FacpPagesNeeded;
            UINT64 DsdtPhysicalAddress;
            BOOLEAN DsdtReplaced = FALSE;

            FacpPagesNeeded = (Table->Length + 4095) / 4096;
            if (EFI_ERROR(BS->AllocatePages(0, 9, FacpPagesNeeded, &NewFacpAddr))) {
                LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for FACP shadow copy.");
                continue;
            }

            NewFacp = (EFI_ACPI_SDT_HEADER *)NewFacpAddr;
            UefiMemcpy(NewFacp, Table, Table->Length);

            DsdtPhysicalAddress = ReadFacpDsdtAddress(NewFacp);

            if (DsdtPhysicalAddress != 0) {
                EFI_ACPI_SDT_HEADER *DsdtTable = (EFI_ACPI_SDT_HEADER *)DsdtPhysicalAddress;

                if (IsValidAcpiPointer(DsdtTable) && DsdtTable->Signature == ACPI_SIG_DSDT) {
                    Spec = FindReplacement(ACPI_SIG_DSDT, DsdtTable);
                    if (Spec != NULL) {
                        UINTN DsdtPagesNeeded;
                        EFI_PHYSICAL_ADDRESS NewDsdtAddr = 0;
                        EFI_ACPI_SDT_HEADER *NewDsdt;

                        LogToFile(SystemTable, ImageHandle, L"[+] Matches DSDT replacement target. Swapping table...");

                        DsdtPagesNeeded = (Spec->NewTableSize + 4095) / 4096;
                        if (EFI_ERROR(BS->AllocatePages(0, 9, DsdtPagesNeeded, &NewDsdtAddr))) {
                            LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for new DSDT.");
                        } else {
                            NewDsdt = (EFI_ACPI_SDT_HEADER *)NewDsdtAddr;
                            UefiMemcpy(NewDsdt, Spec->NewTable, Spec->NewTableSize);

                            NewDsdt->Checksum = 0;
                            NewDsdt->Checksum = CalculateChecksum8((UINT8 *)NewDsdt, NewDsdt->Length);

                            // Redirect only the fields the FACP actually carries.
                            if (NewFacp->Length >= 148) {
                                *(UINT64 *)((UINT8 *)NewFacp + 140) = (UINT64)NewDsdtAddr;
                            }
                            if (NewFacp->Length >= 44 && NewDsdtAddr <= 0xFFFFFFFFULL) {
                                *(UINT32 *)((UINT8 *)NewFacp + 40) = (UINT32)NewDsdtAddr;
                            }

                            LogToFile(SystemTable, ImageHandle, Spec->Name);
                            DsdtReplaced = TRUE;
                            if (SwapCount < 8) {
                                OldAddrs[SwapCount] = DsdtPhysicalAddress;
                                NewAddrs[SwapCount] = (UINT64)NewDsdtAddr;
                                SwapCount++;
                            }
                            ReplacedAny = TRUE;
                            LogToFile(SystemTable, ImageHandle, L"[D] New DSDT at:");
                            LogAddrToFile(SystemTable, ImageHandle, (UINT64)NewDsdtAddr);
                        }
                    }
                }
            }

            if (DsdtReplaced) {
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

    if (ReplacedAny) {
        Xsdt->Checksum = 0;
        Xsdt->Checksum = CalculateChecksum8((UINT8 *)Xsdt, Xsdt->Length);
        LogToFile(SystemTable, ImageHandle, L"[+] All ACPI table replacements applied successfully.");
        SyncRsdt(SystemTable, ImageHandle, Rsdp, OldAddrs, NewAddrs, SwapCount);
        return EFI_SUCCESS;
    }

    ConsolePrint(SystemTable,
                 L"  [-] ALARM: ACPI table replacement matched NO target table "
                 L"(DSDT/SSDT4). BIOS OEM ID/Revision changed?\r\n",
                 TRUE);
    LogToFile(SystemTable, ImageHandle,
              L"[-] ALARM: no target tables matched for replacement.");
    return EFI_NOT_FOUND;
}
