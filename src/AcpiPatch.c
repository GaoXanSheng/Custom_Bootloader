#include "AcpiPatch.h"
#include "Logging.h"

#include "../build/SsdtUnlockDB.hex"

static void SignatureToStr(UINT32 Signature, CHAR16 *Str)
{
    Str[0] = (CHAR16)(Signature & 0xFF);
    Str[1] = (CHAR16)((Signature >> 8) & 0xFF);
    Str[2] = (CHAR16)((Signature >> 16) & 0xFF);
    Str[3] = (CHAR16)((Signature >> 24) & 0xFF);
    Str[4] = 0;
}

static UINTN SearchAndReplace(
    UINT8 *Buffer, 
    UINTN Size, 
    const UINT8 *Search, 
    UINTN SearchSize, 
    const UINT8 *Replace, 
    UINTN ReplaceSize
)
{
    UINTN Count = 0;
    UINTN i, j;

    if (SearchSize != ReplaceSize || Size < SearchSize) {
        return 0;
    }

    for (i = 0; i <= Size - SearchSize; i++) {
        BOOLEAN Match = TRUE;
        for (j = 0; j < SearchSize; j++) {
            if (Buffer[i + j] != Search[j]) {
                Match = FALSE;
                break;
            }
        }
        if (Match) {
            for (j = 0; j < ReplaceSize; j++) {
                Buffer[i + j] = Replace[j];
            }
            Count++;
            i += SearchSize - 1; 
        }
    }
    return Count;
}

static BOOLEAN IsValidAcpiPointer(VOID *Ptr)
{
    UINT64 Addr = (UINT64)Ptr;

    if (Ptr == NULL) return FALSE;

    if ((Addr & 3) != 0) return FALSE;

    if (Addr < 0x100000ULL || Addr > 0x4000000000ULL) return FALSE;

    return TRUE;
}

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

EFI_STATUS PatchSsdt4PowerWall(
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
    BOOLEAN PatchedAny = FALSE;

    static const UINT8 SearchATPP[8]  = { 0x70, 0x0B, 0x68, 0x01, 0x41, 0x54, 0x50, 0x50 };

    static const UINT8 ReplaceATPP[8] = { 0x70, 0x0B, 0xE0, 0x01, 0x41, 0x54, 0x50, 0x50 };

    static const UINT8 SearchATP2[8]  = { 0x70, 0x0B, 0x68, 0x01, 0x41, 0x54, 0x50, 0x32 };

    static const UINT8 ReplaceATP2[8] = { 0x70, 0x0B, 0xE0, 0x01, 0x41, 0x54, 0x50, 0x32 };

    if (Xsdt == NULL || Xsdt->Signature != 0x54445358) { 
        LogToFile(SystemTable, ImageHandle, L"[-] Error: Invalid XSDT signature during power wall scanning.");
        return EFI_NOT_FOUND;
    }

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    LogToFile(SystemTable, ImageHandle, L"[+] Starting in-memory scan for SSDT tables...");

    for (Index = 0; Index < EntryCount; Index++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(EntryPtr[Index]);
        CHAR16 AddrStr[32];
        CHAR16 IndexStr[32];
        CHAR16 LogBuf[256];
        CHAR16 SigStr[5];

        StatusToHex((EFI_STATUS)Index, IndexStr);
        StatusToHex((EFI_STATUS)Table, AddrStr);

        if (!IsValidAcpiPointer(Table)) {

            UefiMemcpy(LogBuf, L"[D] Entry index ", 16 * sizeof(CHAR16));
            UefiMemcpy(LogBuf + 16, IndexStr, 18 * sizeof(CHAR16));
            UefiMemcpy(LogBuf + 34, L" points to invalid address: ", 28 * sizeof(CHAR16));
            UefiMemcpy(LogBuf + 62, AddrStr, 18 * sizeof(CHAR16));
            LogBuf[80] = 0;
            LogToFile(SystemTable, ImageHandle, LogBuf);
            continue;
        }

        SignatureToStr(Table->Signature, SigStr);

        UefiMemcpy(LogBuf, L"[D] Table ", 10 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 10, IndexStr, 18 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 28, L" at ", 4 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 32, AddrStr, 18 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 50, L" (Sig: ", 7 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 57, SigStr, 4 * sizeof(CHAR16));
        UefiMemcpy(LogBuf + 61, L")", 1 * sizeof(CHAR16));
        LogBuf[62] = 0;
        LogToFile(SystemTable, ImageHandle, LogBuf);

        if (Table->Signature == 0x54445353) {

            if (Table->OemTableId == 0x20202020324B4445ULL) {
                EFI_STATUS Status;
                UINTN TableLength = Table->Length;
                UINTN PagesNeeded = (TableLength + 4095) / 4096;
                EFI_PHYSICAL_ADDRESS NewTableAddr = 0;
                EFI_ACPI_SDT_HEADER *NewTable = NULL;
                UINTN RepCount1 = 0;
                UINTN RepCount2 = 0;

                LogToFile(SystemTable, ImageHandle, L"[+] Matches OEM Table ID: EDK2. Allocating writable shadow memory page...");

                Status = BS->AllocatePages(0, 9, PagesNeeded, &NewTableAddr); 
                if (EFI_ERROR(Status)) {
                    LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to allocate memory for SSDT shadow copy.");
                    continue;
                }

                NewTable = (EFI_ACPI_SDT_HEADER *)NewTableAddr;

                UefiMemcpy(NewTable, Table, TableLength);

                RepCount1 = SearchAndReplace((UINT8 *)NewTable, TableLength, SearchATPP, 8, ReplaceATPP, 8);
                RepCount2 = SearchAndReplace((UINT8 *)NewTable, TableLength, SearchATP2, 8, ReplaceATP2, 8);

                if (RepCount1 > 0 || RepCount2 > 0) {
                    CHAR16 Rep1Str[32];
                    CHAR16 Rep2Str[32];
                    CHAR16 NewAddrStr[32];

                    NewTable->Checksum = 0;
                    NewTable->Checksum = CalculateChecksum8((UINT8 *)NewTable, TableLength);

                    EntryPtr[Index] = (UINT64)NewTableAddr;
                    PatchedAny = TRUE;

                    StatusToHex(RepCount1, Rep1Str);
                    StatusToHex(RepCount2, Rep2Str);
                    StatusToHex((EFI_STATUS)NewTableAddr, NewAddrStr);

                    UefiMemcpy(LogBuf, L"[+] Shadow patch success. Redirected pointer to: ", 49 * sizeof(CHAR16));
                    UefiMemcpy(LogBuf + 49, NewAddrStr, 18 * sizeof(CHAR16));
                    UefiMemcpy(LogBuf + 67, L" (ATPP count: ", 14 * sizeof(CHAR16));
                    UefiMemcpy(LogBuf + 81, Rep1Str, 18 * sizeof(CHAR16));
                    UefiMemcpy(LogBuf + 99, L", ATP2 count: ", 14 * sizeof(CHAR16));
                    UefiMemcpy(LogBuf + 113, Rep2Str, 18 * sizeof(CHAR16));
                    UefiMemcpy(LogBuf + 131, L")", 1 * sizeof(CHAR16));
                    LogBuf[132] = 0;
                    LogToFile(SystemTable, ImageHandle, LogBuf);

                } else {
                    LogToFile(SystemTable, ImageHandle, L"[-] Warn: No power wall patterns matched in EDK2 SSDT.");

                    BS->FreePages(NewTableAddr, PagesNeeded);
                }
            }
        }
    }

    if (PatchedAny) {

        Xsdt->Checksum = 0;
        Xsdt->Checksum = CalculateChecksum8((UINT8 *)Xsdt, Xsdt->Length);

        LogToFile(SystemTable, ImageHandle, L"[+] 240W Global Power Wall Unlock shadow-patch applied successfully.");
        return EFI_SUCCESS;
    } else {
        LogToFile(SystemTable, ImageHandle, L"[-] Warn: Target SSDT table with ATPP/ATP2 patterns not found or already shadow-patched.");
        return EFI_SUCCESS;
    }
}
