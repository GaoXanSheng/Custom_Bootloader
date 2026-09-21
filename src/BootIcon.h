#ifndef BOOT_ICON_H
#define BOOT_ICON_H

#include "UefiHelpers.h"

// ---------------------------------------------------------------------------
// 启动图标替换（ACPI BGRT 表劫持 + UEFI GOP Blt）
//
// 在现代 UEFI 系统中，Windows Boot Manager 使用 ACPI BGRT 表
// （Boot Graphics Resource Table）在屏幕上无缝显示启动 Logo。
// 在正常的 UEFI 图形启动下，向 ESP 写入文件
// （\EFI\Microsoft\Boot\bootmgr_bmp.bmp）会被 Windows 忽略。
//
// 本模块：
//   1. 解析内嵌的 BMP（BootIcon_data.h）以提取宽度、高度、像素。
//   2. 查询 UEFI GOP（Graphics Output Protocol）以获取当前屏幕分辨率。
//   3. 在 XSDT/RSDT 中定位 ACPI BGRT 表。
//   4. 为 BMP 图像分配 ACPI Reclaim 内存，复制图像，并更新
//      BGRT->ImageAddress、ImageOffsetX、ImageOffsetY，然后重新计算校验和。
//   5. 在 Windows Boot Manager 启动之前，将新 BMP 绘制（Blt）到屏幕上的
//      (OffsetX, OffsetY) 处，以替换厂商固件 Logo。
// ---------------------------------------------------------------------------

#include "BootIcon_data.h"

#define BOOT_ICON_REPLACE  1

// 修补 ACPI BGRT 表，并通过 GOP 将 Logo 绘制到屏幕上。
EFI_STATUS PatchBgrtAndDrawLogo(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
);

#endif // BOOT_ICON_H
