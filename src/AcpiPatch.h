#ifndef ACPI_PATCH_H
#define ACPI_PATCH_H

#include "UefiHelpers.h"

EFI_STATUS InjectSsdt(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

// ReplaceAcpiTables: swap pre-built replacement tables (DSDT + SSDT4) into the
// ACPI namespace via XSDT / FACP pointer redirection. Replaces the old
// PatchSsdt4PowerWall byte-patch engine.
EFI_STATUS ReplaceAcpiTables(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

#endif
