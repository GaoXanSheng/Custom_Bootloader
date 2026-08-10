#include "AcpiPatch.h"
#include "Logging.h"

#include "../build/SsdtUnlockDB.hex"
#include "../build/ssdt4_patched.hex"
#include "../build/dsdt_patched.hex"

// ---------------------------------------------------------------------------
// Table Replacement (整表替换)
//
// Replaces the old runtime byte SearchAndReplace patch engine (patches/*.txt).
// Target tables are now patched at the ASL source level, compiled offline by
// iASL into legal AML (src/acpi/patched/*.dsl -> build/*.hex), and swapped in
// wholesale via XSDT/FACP pointer redirection. The firmware AML is never
// modified in place, so WMI device registration (e.g. \_SB.PCI0.WMID /
// MICommonInterface) can no longer be broken by malformed patch bytes.
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
