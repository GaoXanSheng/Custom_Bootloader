#ifndef ACPI_PATCH_H
#define ACPI_PATCH_H

#include "UefiHelpers.h"

// 运行时内存特征搜索与就地修改（按 tools/thmp_config.yaml 配置规则）
// 直接在固件真实 DSDT 镜像中修改变量，完全不预埋静态 ACPI 表，适应不同 BIOS 版本。
EFI_STATUS ReplaceAcpiTables(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

#endif
