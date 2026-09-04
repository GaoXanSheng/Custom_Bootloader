#ifndef ACPI_PATCH_H
#define ACPI_PATCH_H

#include "UefiHelpers.h"

// Inject custom WMI SSDT (SsdtUnlockDB) by extending XSDT/RSDT.
EFI_STATUS InjectSsdt(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

// Optional in-place fallback byte patch pass on original table pages.
EFI_STATUS PatchTablesInPlace(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

// Replace target ACPI tables (DSDT + SSDT4) via XSDT / FACP pointer redirection.
EFI_STATUS ReplaceAcpiTables(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

#endif
