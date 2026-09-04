#include "BootIcon.h"
#include "Logging.h"
#include "UefiHelpers.h"
#include "BootIcon_data.h"

#define ACPI_SIG_BGRT  0x54524742  // "BGRT" (little-endian)
#define ACPI_SIG_XSDT  0x54445358  // "XSDT" (little-endian)
#define ACPI_SIG_RSDT  0x54445352  // "RSDT" (little-endian)

#pragma pack(push, 1)
typedef struct {
    UINT16 Type;
    UINT32 Size;
    UINT16 Reserved1;
    UINT16 Reserved2;
    UINT32 OffBits;
    UINT32 SizeHeader;
    UINT32 Width;
    UINT32 Height;
    UINT16 Planes;
    UINT16 BitCount;
    UINT32 Compression;
    UINT32 SizeImage;
    UINT32 XPelsPerMeter;
    UINT32 YPelsPerMeter;
    UINT32 ClrUsed;
    UINT32 ClrImportant;
} BMP_HEADER;
#pragma pack(pop)

static VOID UefiZeroMem(VOID *Dest, UINTN Size)
{
    UINT8 *D = (UINT8 *)Dest;
    while (Size--) {
        *D++ = 0;
    }
}

/**
 * Locate existing ACPI BGRT table in XSDT, or dynamically create and inject
 * a new BGRT table into XSDT (HackBGRT method).
 */
static EFI_STATUS CreateOrUpdateBgrt(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp,
    EFI_PHYSICAL_ADDRESS BmpAddress,
    UINT32 OffsetX,
    UINT32 OffsetY,
    EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE **OutBgrt
)
{
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_ACPI_SDT_HEADER *Xsdt;
    UINTN EntryCount;
    UINT64 *EntryPtr;
    UINTN i;
    EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *Bgrt = NULL;
    EFI_STATUS Status;

    if (Rsdp == NULL || Rsdp->XsdtAddress == 0) {
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Invalid RSDP or missing XSDT address.");
        return EFI_INVALID_PARAMETER;
    }

    Xsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)(Rsdp->XsdtAddress);
    if (!IsValidAcpiPointer(Xsdt) || Xsdt->Signature != ACPI_SIG_XSDT) {
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Invalid XSDT pointer or signature.");
        return EFI_NOT_FOUND;
    }

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    // 1. Search for existing BGRT in XSDT
    for (i = 0; i < EntryCount; i++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(UINTN)(EntryPtr[i]);
        if (IsValidAcpiPointer(Table) && Table->Signature == ACPI_SIG_BGRT) {
            Bgrt = (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *)Table;
            break;
        }
    }

    if (Bgrt != NULL) {
        // Case A: Existing BGRT table found -> update in place
        Bgrt->Version = 1;
        Bgrt->Status = 1;       // Bit 0 = 1: Image is displayed on screen
        Bgrt->ImageType = 0;    // 0 = Bitmap
        Bgrt->ImageAddress = (UINT64)BmpAddress;
        Bgrt->ImageOffsetX = OffsetX;
        Bgrt->ImageOffsetY = OffsetY;

        Bgrt->Header.Checksum = 0;
        Bgrt->Header.Checksum = CalculateChecksum8((UINT8 *)Bgrt, Bgrt->Header.Length);

        *OutBgrt = Bgrt;
        LogToFile(SystemTable, ImageHandle, L"[+] BGRT: Existing ACPI BGRT table updated with new logo.");
        return EFI_SUCCESS;
    }

    // Case B: No BGRT table present -> allocate new BGRT and inject into XSDT (HackBGRT)
    LogToFile(SystemTable, ImageHandle, L"[+] BGRT: No existing BGRT table in XSDT. Creating new BGRT table...");

    EFI_PHYSICAL_ADDRESS NewBgrtAddr = 0;
    Status = BS->AllocatePages(0, 9, 1, &NewBgrtAddr); // 9 = EfiACPIReclaimMemory
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Failed to allocate memory for BGRT table: ", Status);
        return Status;
    }

    Bgrt = (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *)(UINTN)NewBgrtAddr;
    UefiZeroMem(Bgrt, sizeof(EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE));

    // Fill ACPI SDT Header
    Bgrt->Header.Signature = ACPI_SIG_BGRT;
    Bgrt->Header.Length = sizeof(EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE);
    Bgrt->Header.Revision = 1;
    Bgrt->Header.Checksum = 0;
    UefiMemcpy(Bgrt->Header.OemId, "Mtblx*", 6);
    UefiMemcpy((VOID *)&Bgrt->Header.OemTableId, "HackBGRT", 8);
    Bgrt->Header.OemRevision = 1;
    Bgrt->Header.CreatorId = 0x454E4F4E; // "NONE"
    Bgrt->Header.CreatorRevision = 1;

    // Fill BGRT fields
    Bgrt->Version = 1;
    Bgrt->Status = 1;       // Bit 0 = 1: Image is displayed on screen
    Bgrt->ImageType = 0;    // 0 = Bitmap
    Bgrt->ImageAddress = (UINT64)BmpAddress;
    Bgrt->ImageOffsetX = OffsetX;
    Bgrt->ImageOffsetY = OffsetY;

    // Checksum
    Bgrt->Header.Checksum = CalculateChecksum8((UINT8 *)Bgrt, Bgrt->Header.Length);

    // Expand XSDT to append the new BGRT pointer
    UINTN OldXsdtLength = Xsdt->Length;
    UINTN NewXsdtLength = OldXsdtLength + sizeof(UINT64);
    UINTN XsdtPagesNeeded = (NewXsdtLength + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS NewXsdtAddr = 0;

    Status = BS->AllocatePages(0, 9, XsdtPagesNeeded, &NewXsdtAddr);
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Failed to allocate memory for expanded XSDT: ", Status);
        return Status;
    }

    EFI_ACPI_SDT_HEADER *NewXsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)NewXsdtAddr;
    UefiMemcpy(NewXsdt, Xsdt, OldXsdtLength);

    // Append new BGRT pointer at the end of the entry array
    *(UINT64 *)((UINT8 *)NewXsdt + OldXsdtLength) = (UINT64)NewBgrtAddr;

    NewXsdt->Length = (UINT32)NewXsdtLength;
    NewXsdt->Checksum = 0;
    NewXsdt->Checksum = CalculateChecksum8((UINT8 *)NewXsdt, NewXsdt->Length);

    // Update RSDP XSDT address and recalculate checksums
    Rsdp->XsdtAddress = (UINT64)NewXsdtAddr;

    Rsdp->Checksum = 0;
    Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);

    UINT32 RsdpLength = (Rsdp->Length >= 36) ? Rsdp->Length : 36;
    Rsdp->ExtendedChecksum = 0;
    Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);

    // Sync RSDT if within 32-bit range
    if (NewBgrtAddr <= 0xFFFFFFFFULL && Rsdp->RsdtAddress != 0) {
        EFI_ACPI_SDT_HEADER *OldRsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)Rsdp->RsdtAddress;
        if (IsValidAcpiPointer(OldRsdt) && OldRsdt->Signature == ACPI_SIG_RSDT) {
            UINTN OldRsdtLength = OldRsdt->Length;
            UINTN NewRsdtLength = OldRsdtLength + sizeof(UINT32);
            UINTN RsdtPages = (NewRsdtLength + 4095) / 4096;
            EFI_PHYSICAL_ADDRESS NewRsdtAddr = 0;

            if (!EFI_ERROR(BS->AllocatePages(0, 9, RsdtPages, &NewRsdtAddr))) {
                EFI_ACPI_SDT_HEADER *NewRsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)NewRsdtAddr;
                UefiMemcpy(NewRsdt, OldRsdt, OldRsdtLength);
                *(UINT32 *)((UINT8 *)NewRsdt + OldRsdtLength) = (UINT32)NewBgrtAddr;
                NewRsdt->Length = (UINT32)NewRsdtLength;
                NewRsdt->Checksum = 0;
                NewRsdt->Checksum = CalculateChecksum8((UINT8 *)NewRsdt, NewRsdtLength);
                Rsdp->RsdtAddress = (UINT32)NewRsdtAddr;
                Rsdp->Checksum = 0;
                Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);
                Rsdp->ExtendedChecksum = 0;
                Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);
                LogToFile(SystemTable, ImageHandle, L"[+] BGRT: RSDT extended with new BGRT entry.");
            }
        }
    }

    *OutBgrt = Bgrt;
    LogToFile(SystemTable, ImageHandle, L"[+] BGRT: Successfully injected new ACPI BGRT table into XSDT.");
    return EFI_SUCCESS;
}

EFI_STATUS PatchBgrtAndDrawLogo(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
)
{
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_STATUS Status;
    BMP_HEADER *BmpHdr;
    UINT32 BmpWidth, BmpHeight;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    UINT32 ScreenW, ScreenH;
    UINT32 OffsetX, OffsetY;
    EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *Bgrt = NULL;
    EFI_PHYSICAL_ADDRESS NewBmpAddr = 0;
    UINTN PagesNeeded;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuf = NULL;
    UINTN Row, Col;
    UINT8 *PixelData;
    UINTN RowSize;

    if (gBootIconData == NULL || gBootIconSize < sizeof(BMP_HEADER)) {
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Invalid or missing embedded BMP data.");
        return EFI_NOT_FOUND;
    }

    BmpHdr = (BMP_HEADER *)gBootIconData;
    if (BmpHdr->Type != 0x4D42) { // "BM"
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Embedded data is not a valid BMP.");
        return EFI_UNSUPPORTED;
    }

    BmpWidth = BmpHdr->Width;
    BmpHeight = BmpHdr->Height;

    LogToFile(SystemTable, ImageHandle, L"[+] BGRT: Starting BGRT hijack and GOP display...");

    // 1. Locate GOP
    Status = BS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
    if (EFI_ERROR(Status) || Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Locate GOP failed: ", Status);
        return Status;
    }

    ScreenW = Gop->Mode->Info->HorizontalResolution;
    ScreenH = Gop->Mode->Info->VerticalResolution;

    // Calculate center coordinates
    if (ScreenW > BmpWidth) {
        OffsetX = (ScreenW - BmpWidth) / 2;
    } else {
        OffsetX = 0;
    }
    if (ScreenH > BmpHeight) {
        OffsetY = (ScreenH - BmpHeight) / 2;
    } else {
        OffsetY = 0;
    }

    // 2. Allocate ACPI Reclaim Memory for the BMP file so Windows bootmgr can read it
    PagesNeeded = (gBootIconSize + 4095) / 4096;
    Status = BS->AllocatePages(0, 9, PagesNeeded, &NewBmpAddr); // 9 = EfiACPIReclaimMemory
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Failed to allocate ACPI Reclaim memory: ", Status);
        return Status;
    }

    UefiMemcpy((VOID *)(UINTN)NewBmpAddr, gBootIconData, gBootIconSize);

    // 3. Locate or inject ACPI BGRT table
    Status = CreateOrUpdateBgrt(SystemTable, ImageHandle, Rsdp, NewBmpAddr, OffsetX, OffsetY, &Bgrt);
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: CreateOrUpdateBgrt failed: ", Status);
    }

    // 4. Clear screen to wipe OEM vendor logo and earlier console text
    SystemTable->ConOut->EnableCursor(SystemTable->ConOut, FALSE);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    // 5. Draw BMP on screen via GOP Blt to replace the vendor logo on display
    // Support 24-bit uncompressed BMP (tools/logo.bmp)
    if (BmpHdr->BitCount == 24 && BmpHdr->Compression == 0) {
        Status = BS->AllocatePool(2, BmpWidth * BmpHeight * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL), (VOID **)&BltBuf);
        if (!EFI_ERROR(Status) && BltBuf != NULL) {
            RowSize = ((BmpWidth * 3 + 3) / 4) * 4;
            PixelData = (UINT8 *)gBootIconData + BmpHdr->OffBits;

            // BMP stores pixels bottom-to-top
            for (Row = 0; Row < BmpHeight; Row++) {
                UINT8 *SrcRow = PixelData + (BmpHeight - 1 - Row) * RowSize;
                EFI_GRAPHICS_OUTPUT_BLT_PIXEL *DstRow = BltBuf + Row * BmpWidth;
                for (Col = 0; Col < BmpWidth; Col++) {
                    DstRow[Col].Blue = SrcRow[Col * 3 + 0];
                    DstRow[Col].Green = SrcRow[Col * 3 + 1];
                    DstRow[Col].Red = SrcRow[Col * 3 + 2];
                    DstRow[Col].Reserved = 0;
                }
            }

            // Blt buffer to video screen
            Status = Gop->Blt(
                Gop,
                BltBuf,
                EfiBltBufferToVideo,
                0, 0,
                OffsetX, OffsetY,
                BmpWidth, BmpHeight,
                0
            );

            BS->FreePool(BltBuf);

            if (!EFI_ERROR(Status)) {
                LogToFile(SystemTable, ImageHandle, L"[+] BGRT: GOP Blt drawn successfully to screen.");
            } else {
                LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: GOP Blt failed: ", Status);
                if (Bgrt != NULL) {
                    Bgrt->Status = 0; // Windows bootmgr fallback display
                    Bgrt->Header.Checksum = 0;
                    Bgrt->Header.Checksum = CalculateChecksum8((UINT8 *)Bgrt, Bgrt->Header.Length);
                }
            }
        }
    } else {
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Only 24-bit uncompressed BMP is supported for GOP Blt.");
    }

    return EFI_SUCCESS;
}
