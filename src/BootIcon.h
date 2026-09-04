#ifndef BOOT_ICON_H
#define BOOT_ICON_H

#include "UefiHelpers.h"

// ---------------------------------------------------------------------------
// Boot Icon Replacement (ACPI BGRT Table Hijack + UEFI GOP Blt)
//
// In modern UEFI systems, Windows Boot Manager uses the ACPI BGRT table
// (Boot Graphics Resource Table) to display the boot logo seamlessly on screen.
// Writing files to ESP (\EFI\Microsoft\Boot\bootmgr_bmp.bmp) is ignored by
// Windows under normal UEFI graphics boot.
//
// This module:
//   1. Parses the embedded BMP (BootIcon_data.h) to extract width, height, pixels.
//   2. Queries UEFI GOP (Graphics Output Protocol) to get current screen resolution.
//   3. Locates ACPI BGRT table in XSDT/RSDT.
//   4. Allocates ACPI Reclaim memory for the BMP image, copies it, and updates
//      BGRT->ImageAddress, ImageOffsetX, ImageOffsetY, and recomputes checksum.
//   5. Draws (Blt) the new BMP onto the screen at (OffsetX, OffsetY) to replace
//      the vendor firmware logo before Windows Boot Manager starts.
// ---------------------------------------------------------------------------

#include "BootIcon_data.h"

#define BOOT_ICON_REPLACE  0

// Patch ACPI BGRT table and draw the logo on screen via GOP.
EFI_STATUS PatchBgrtAndDrawLogo(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

#endif // BOOT_ICON_H
