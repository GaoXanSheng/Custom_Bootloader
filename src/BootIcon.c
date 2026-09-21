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
 * 在 XSDT 中定位现有 ACPI BGRT 表；找不到则动态创建并注入一张新 BGRT
 * （HackBGRT 方式）。两种情况都不直接改写固件内存，统一走副本+重定向。
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

    // 1. 在 XSDT 中查找现有 BGRT
    for (i = 0; i < EntryCount; i++) {
        EFI_ACPI_SDT_HEADER *Table = (EFI_ACPI_SDT_HEADER *)(UINTN)(EntryPtr[i]);
        if (IsValidAcpiPointer(Table) && Table->Signature == ACPI_SIG_BGRT) {
            Bgrt = (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *)Table;
            break;
        }
    }

    if (Bgrt != NULL) {
        // Case A: 固件已有 BGRT 表。不改写固件内存里的表（部分 OEM 的 ACPI
        // 页面只读，直接写会静默失败）——复制到自分配的 ACPI Reclaim 页，
        // 修改副本后把 XSDT 条目重定向过去，与整表替换（AcpiPatch.c）同风格。
        // 此时 XSDT 已是 ReplaceAcpiTables 分配的可写副本，条目可安全改写。
        EFI_PHYSICAL_ADDRESS NewBgrtAddr = 0xFFFFFFFFULL;
        UINTN BgrtPages = (Bgrt->Header.Length + 4095) / 4096;
        EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *NewBgrt;

        Status = BS->AllocatePages(AllocateMaxAddress, 9, BgrtPages, &NewBgrtAddr);
        if (EFI_ERROR(Status)) {
            LogStatusToFile(SystemTable, ImageHandle,
                            L"[-] BGRT: Failed to allocate shadow copy of existing BGRT: ", Status);
            return Status;
        }

        NewBgrt = (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *)(UINTN)NewBgrtAddr;
        UefiMemcpy(NewBgrt, Bgrt, Bgrt->Header.Length);

        NewBgrt->Version = 1;
        NewBgrt->Status = 1;       // Bit 0 = 1: 图像已显示在屏幕上
        NewBgrt->ImageType = 0;    // 0 = 位图
        NewBgrt->ImageAddress = (UINT64)BmpAddress;
        NewBgrt->ImageOffsetX = OffsetX;
        NewBgrt->ImageOffsetY = OffsetY;

        NewBgrt->Header.Checksum = 0;
        NewBgrt->Header.Checksum = CalculateChecksum8((UINT8 *)NewBgrt, NewBgrt->Header.Length);

        // XSDT 条目重定向到副本
        EntryPtr[i] = (UINT64)NewBgrtAddr;
        Xsdt->Checksum = 0;
        Xsdt->Checksum = CalculateChecksum8((UINT8 *)Xsdt, Xsdt->Length);

        // RSDT 中指向旧 BGRT 的条目同步换新（与 AcpiPatch.c 的 SyncRsdt 同思路）
        if (NewBgrtAddr <= 0xFFFFFFFFULL && Rsdp->RsdtAddress != 0) {
            EFI_ACPI_SDT_HEADER *Rsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)Rsdp->RsdtAddress;
            if (IsValidAcpiPointer(Rsdt) && Rsdt->Signature == ACPI_SIG_RSDT) {
                UINTN RsdtEntries = (Rsdt->Length - sizeof(EFI_ACPI_SDT_HEADER)) / sizeof(UINT32);
                UINT32 *RsdtEntry = (UINT32 *)((UINT8 *)Rsdt + sizeof(EFI_ACPI_SDT_HEADER));
                UINTN k;
                for (k = 0; k < RsdtEntries; k++) {
                    if (RsdtEntry[k] == (UINT32)(UINTN)Bgrt) {
                        RsdtEntry[k] = (UINT32)NewBgrtAddr;
                        Rsdt->Checksum = 0;
                        Rsdt->Checksum = CalculateChecksum8((UINT8 *)Rsdt, Rsdt->Length);
                    }
                }
            }
        }

        BS->InstallConfigurationTable(&gEfiAcpi20TableGuid, (VOID *)Rsdp);

        *OutBgrt = NewBgrt;
        LogToFile(SystemTable, ImageHandle,
                  L"[+] BGRT: Existing BGRT shadow-copied and redirected with new logo.");
        return EFI_SUCCESS;
    }

    // Case B: 没有 BGRT 表 —— 分配新 BGRT 并注入 XSDT（HackBGRT）
    LogToFile(SystemTable, ImageHandle, L"[+] BGRT: No existing BGRT table in XSDT. Creating new BGRT table...");

    EFI_PHYSICAL_ADDRESS NewBgrtAddr = 0xFFFFFFFFULL;
    Status = BS->AllocatePages(AllocateMaxAddress, 9, 1, &NewBgrtAddr); // 9 = EfiACPIReclaimMemory
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Failed to allocate memory for BGRT table: ", Status);
        return Status;
    }

    Bgrt = (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *)(UINTN)NewBgrtAddr;
    UefiZeroMem(Bgrt, sizeof(EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE));

    // 填充 ACPI SDT 表头
    Bgrt->Header.Signature = ACPI_SIG_BGRT;
    Bgrt->Header.Length = sizeof(EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE);
    Bgrt->Header.Revision = 1;
    Bgrt->Header.Checksum = 0;
    UefiMemcpy(Bgrt->Header.OemId, "Mtblx*", 6);
    UefiMemcpy((VOID *)&Bgrt->Header.OemTableId, "HackBGRT", 8);
    Bgrt->Header.OemRevision = 1;
    Bgrt->Header.CreatorId = 0x454E4F4E; // "NONE"
    Bgrt->Header.CreatorRevision = 1;

    // 填充 BGRT 字段
    Bgrt->Version = 1;
    Bgrt->Status = 1;       // Bit 0 = 1: 图像已显示在屏幕上
    Bgrt->ImageType = 0;    // 0 = 位图
    Bgrt->ImageAddress = (UINT64)BmpAddress;
    Bgrt->ImageOffsetX = OffsetX;
    Bgrt->ImageOffsetY = OffsetY;

    // 校验和
    Bgrt->Header.Checksum = CalculateChecksum8((UINT8 *)Bgrt, Bgrt->Header.Length);

    // 扩容 XSDT 以追加新 BGRT 指针
    UINTN OldXsdtLength = Xsdt->Length;
    UINTN NewXsdtLength = OldXsdtLength + sizeof(UINT64);
    UINTN XsdtPagesNeeded = (NewXsdtLength + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS NewXsdtAddr = 0xFFFFFFFFULL;

    Status = BS->AllocatePages(AllocateMaxAddress, 9, XsdtPagesNeeded, &NewXsdtAddr);
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Failed to allocate memory for expanded XSDT: ", Status);
        return Status;
    }

    EFI_ACPI_SDT_HEADER *NewXsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)NewXsdtAddr;
    UefiMemcpy(NewXsdt, Xsdt, OldXsdtLength);

    // 在条目数组末尾追加新 BGRT 指针
    *(UINT64 *)((UINT8 *)NewXsdt + OldXsdtLength) = (UINT64)NewBgrtAddr;

    NewXsdt->Length = (UINT32)NewXsdtLength;
    NewXsdt->Checksum = 0;
    NewXsdt->Checksum = CalculateChecksum8((UINT8 *)NewXsdt, NewXsdt->Length);

    // 更新 RSDP 的 XSDT 地址并重算校验和
    Rsdp->XsdtAddress = (UINT64)NewXsdtAddr;

    Rsdp->Checksum = 0;
    Rsdp->Checksum = CalculateChecksum8((UINT8 *)Rsdp, 20);

    UINT32 RsdpLength = (Rsdp->Length >= 36) ? Rsdp->Length : 36;
    Rsdp->ExtendedChecksum = 0;
    Rsdp->ExtendedChecksum = CalculateChecksum8((UINT8 *)Rsdp, RsdpLength);

    // 地址在 32 位范围内时同步 RSDT
    if (NewBgrtAddr <= 0xFFFFFFFFULL && Rsdp->RsdtAddress != 0) {
        EFI_ACPI_SDT_HEADER *OldRsdt = (EFI_ACPI_SDT_HEADER *)(UINTN)Rsdp->RsdtAddress;
        if (IsValidAcpiPointer(OldRsdt) && OldRsdt->Signature == ACPI_SIG_RSDT) {
            UINTN OldRsdtLength = OldRsdt->Length;
            UINTN NewRsdtLength = OldRsdtLength + sizeof(UINT32);
            UINTN RsdtPages = (NewRsdtLength + 4095) / 4096;
            EFI_PHYSICAL_ADDRESS NewRsdtAddr = 0xFFFFFFFFULL;

            if (!EFI_ERROR(BS->AllocatePages(AllocateMaxAddress, 9, RsdtPages, &NewRsdtAddr))) {
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

    BS->InstallConfigurationTable(&gEfiAcpi20TableGuid, (VOID *)Rsdp);

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

    // 1. 定位 GOP
    Status = BS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
    if (EFI_ERROR(Status) || Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Locate GOP failed: ", Status);
        return Status;
    }

    ScreenW = Gop->Mode->Info->HorizontalResolution;
    ScreenH = Gop->Mode->Info->VerticalResolution;

    // 计算居中坐标
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

    // 2. 为 BMP 文件分配 ACPI Reclaim 内存，让 Windows bootmgr 可读
    PagesNeeded = (gBootIconSize + 4095) / 4096;
    NewBmpAddr = 0xFFFFFFFFULL;
    Status = BS->AllocatePages(AllocateMaxAddress, 9, PagesNeeded, &NewBmpAddr); // 9 = EfiACPIReclaimMemory
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: Failed to allocate ACPI Reclaim memory: ", Status);
        return Status;
    }

    UefiMemcpy((VOID *)(UINTN)NewBmpAddr, gBootIconData, gBootIconSize);

    // 3. 定位或注入 ACPI BGRT 表
    Status = CreateOrUpdateBgrt(SystemTable, ImageHandle, Rsdp, NewBmpAddr, OffsetX, OffsetY, &Bgrt);
    if (EFI_ERROR(Status)) {
        LogStatusToFile(SystemTable, ImageHandle, L"[-] BGRT: CreateOrUpdateBgrt failed: ", Status);
    }

    // 4. 清屏擦掉 OEM 厂标与之前的控制台文本
    SystemTable->ConOut->EnableCursor(SystemTable->ConOut, FALSE);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    // 5. 经 GOP Blt 把 BMP 画上屏，替换显示中的厂标
    // 仅支持 24 位无压缩 BMP（tools/logo.bmp）
    if (BmpHdr->BitCount == 24 && BmpHdr->Compression == 0) {
        Status = BS->AllocatePool(2, BmpWidth * BmpHeight * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL), (VOID **)&BltBuf);
        if (!EFI_ERROR(Status) && BltBuf != NULL) {
            RowSize = ((BmpWidth * 3 + 3) / 4) * 4;
            PixelData = (UINT8 *)gBootIconData + BmpHdr->OffBits;

            // BMP 像素自底向上存储
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

            // 把缓冲 Blt 到显存
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
                    Bgrt->Status = 0; // 交回 Windows bootmgr 兜底显示
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
