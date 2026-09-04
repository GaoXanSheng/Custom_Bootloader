#include "AcpiPatch.h"
#include "Logging.h"
#include "InplacePatches.h"

#include "../build/SsdtUnlockDB.hex"
#include "../build/ssdt4_patched.hex"
#include "../build/dsdt_patched.hex"

// ---------------------------------------------------------------------------
// Dual-channel ACPI patching
//
// 1) Table Replacement (整表替换, ReplaceAcpiTables): target tables are
//    patched at the ASL source level, compiled offline by iASL into legal AML
//    (src/acpi/patched/*.dsl -> build/*.hex), and swapped in wholesale via
//    XSDT/FACP pointer redirection. Consumers that re-read the tables from
//    RSDP at boot (Windows ACPICA) see the new tables; the firmware AML is
//    never modified, so WMI device registration (e.g. \_SB.PCI0.WMID /
//    MICommonInterface) can no longer be broken by malformed patch bytes.
//
// 2) In-place patching (原地补丁, PatchTablesInPlace): firmware-side
//    consumers (Insyde SMM / EC arbitration) bound themselves to the ORIGINAL
//    DSDT/SSDT4 physical pages at POST and never follow pointer redirection,
//    so replacement alone leaves the power/thermal control logic running
//    from the old pages. Equal-size byte patches (src/InplacePatches.h,
//    occurrence counts verified) are therefore applied directly to the
//    original tables, whose checksums are recomputed in place. Windows
//    follows the new pointers (channel 1) and the firmware sees the patched
//    original bytes (channel 2); both deliver the same logic changes.
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


static BOOLEAN IsValidAcpiPointer(VOID *Ptr)
{
    UINT64 Addr = (UINT64)Ptr;

    if (Ptr == NULL) return FALSE;

    if ((Addr & 3) != 0) return FALSE;

    if (Addr < 0x100000ULL || Addr > 0x4000000000ULL) return FALSE;

    return TRUE;
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
            UINT64 *XDsdtPtr = (UINT64 *)((UINT8 *)Table + 140);
            UINT32 *DsdtPtr = (UINT32 *)((UINT8 *)Table + 40);
            UINT64 Addr = *XDsdtPtr;
            EFI_ACPI_SDT_HEADER *Dsdt;

            if (Addr == 0) {
                Addr = *DsdtPtr;
            }
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

    if (PatchesApplied == 0 && PatchesSkipped == 0) {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: no in-place patches evaluated.");
        return EFI_NOT_FOUND;
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
            Spec = FindReplacement(ACPI_SIG_SSDT, Table);
            if (Spec == NULL) {
                continue;
            }

            LogToFile(SystemTable, ImageHandle, L"[+] Matches SSDT replacement target. Swapping table...");

            if (ApplyReplacement(SystemTable, ImageHandle, Spec,
                                 &EntryPtr[Index], Spec->Name)) {
                ReplacedAny = TRUE;
            }
        }
        else if (Table->Signature == ACPI_SIG_FACP) {
            // DSDT is referenced from FACP, not from the XSDT directly.
            EFI_ACPI_SDT_HEADER *NewFacp = NULL;
            EFI_PHYSICAL_ADDRESS NewFacpAddr = 0;
            UINTN FacpPagesNeeded;
            UINT64 *XDsdtPtr;
            UINT32 *DsdtPtr;
            UINT64 DsdtPhysicalAddress;
            BOOLEAN DsdtReplaced = FALSE;

            FacpPagesNeeded = (Table->Length + 4095) / 4096;
            if (EFI_ERROR(BS->AllocatePages(0, 9, FacpPagesNeeded, &NewFacpAddr))) {
                LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for FACP shadow copy.");
                continue;
            }

            NewFacp = (EFI_ACPI_SDT_HEADER *)NewFacpAddr;
            UefiMemcpy(NewFacp, Table, Table->Length);

            XDsdtPtr = (UINT64 *)((UINT8 *)NewFacp + 140);
            DsdtPtr = (UINT32 *)((UINT8 *)NewFacp + 40);
            DsdtPhysicalAddress = *XDsdtPtr;
            if (DsdtPhysicalAddress == 0) {
                DsdtPhysicalAddress = *DsdtPtr;
            }

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

                            *XDsdtPtr = (UINT64)NewDsdtAddr;
                            if (DsdtPhysicalAddress <= 0xFFFFFFFF) {
                                *DsdtPtr = (UINT32)NewDsdtAddr;
                            }

                            LogToFile(SystemTable, ImageHandle, Spec->Name);
                            DsdtReplaced = TRUE;
                            ReplacedAny = TRUE;
                        }
                    }
                }
            }

            if (DsdtReplaced) {
                NewFacp->Checksum = 0;
                NewFacp->Checksum = CalculateChecksum8((UINT8 *)NewFacp, NewFacp->Length);
                EntryPtr[Index] = (UINT64)NewFacpAddr;
            } else {
                BS->FreePages(NewFacpAddr, FacpPagesNeeded);
            }
        }
    }

    if (ReplacedAny) {
        Xsdt->Checksum = 0;
        Xsdt->Checksum = CalculateChecksum8((UINT8 *)Xsdt, Xsdt->Length);
        LogToFile(SystemTable, ImageHandle, L"[+] All ACPI table replacements applied successfully.");
        return EFI_SUCCESS;
    }

    LogToFile(SystemTable, ImageHandle, L"[-] Warn: No target tables matched for replacement.");
    return EFI_SUCCESS;
}
