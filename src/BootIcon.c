#include "BootIcon.h"
#include "Logging.h"
#include "UefiHelpers.h"
#include "BootIcon_data.h"

#define ACPI_SIG_BGRT  0x54524742  // "BGRT" (little-endian)

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

static EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *FindBgrt(
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
)
{
    EFI_ACPI_SDT_HEADER *Xsdt;
    UINTN EntryCount;
    UINT64 *EntryPtr;
    UINTN i;

    if (Rsdp == NULL || Rsdp->XsdtAddress == 0) return NULL;
    Xsdt = (EFI_ACPI_SDT_HEADER *)(Rsdp->XsdtAddress);
    if (Xsdt->Signature != 0x54445358) return NULL; // "XSDT"

    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
    EntryPtr = (UINT64 *)((UINT8 *)Xsdt + sizeof(EFI_ACPI_SDT_HEADER));

    for (i = 0; i < EntryCount; i++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(EntryPtr[i]);
        if (IsValidAcpiPointer(Table) && Table->Signature == ACPI_SIG_BGRT) {
            return (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *)Table;
        }
    }
    return NULL;
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

    // 3. Locate and update ACPI BGRT table
    Bgrt = FindBgrt(Rsdp);
    if (Bgrt != NULL) {
        Bgrt->Version = 1;
        Bgrt->Status = 1;       // Bit 0 = 1: Image is displayed on screen
        Bgrt->ImageType = 0;    // 0 = Bitmap
        Bgrt->ImageAddress = (UINT64)NewBmpAddr;
        Bgrt->ImageOffsetX = OffsetX;
        Bgrt->ImageOffsetY = OffsetY;

        // Recompute BGRT checksum
        Bgrt->Header.Checksum = 0;
        Bgrt->Header.Checksum = CalculateChecksum8((UINT8 *)Bgrt, Bgrt->Header.Length);

        LogToFile(SystemTable, ImageHandle, L"[+] BGRT: Successfully patched ACPI BGRT table with new logo.");
    } else {
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Warning: ACPI BGRT table not found in XSDT.");
    }

    // 4. Draw BMP on screen via GOP Blt to replace the vendor logo on display
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
                ConsolePrint(SystemTable, L"[+] Boot logo replaced and displayed on screen.\r\n", FALSE);
            } else {
                LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: GOP Blt failed: ", Status);
            }
        }
    } else {
        LogToFile(SystemTable, ImageHandle, L"[-] BGRT: Only 24-bit uncompressed BMP is supported for GOP Blt.");
    }

    return EFI_SUCCESS;
}
