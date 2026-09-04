#include "BootIcon.h"
#include "UefiHelpers.h"
#include "Logging.h"
#include "AcpiPatch.h"
#include "Version.h"

EFI_STATUS EFIAPI efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
)
{
    EFI_STATUS Status;
    EFI_BOOT_SERVICES *BS;
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = NULL;
    UINTN Index;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    EFI_DEVICE_PATH_PROTOCOL *FullDevicePath = NULL;
    EFI_HANDLE WinBootHandle = NULL;
    CHAR16 StatusStr[32];
    UINTN i;

    CHAR16 *Candidates[] = {
        L"EFI\\Microsoft\\Boot\\bootmgfw.efi",
        L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
    };
    UINTN NumCandidates = sizeof(Candidates) / sizeof(Candidates[0]);

    BS = SystemTable->BootServices;

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"--------------------------------------------------\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"  JiaoLong 16 PRO - Custom ACPI Injector Bootloader\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"--------------------------------------------------\r\n");

    LogToFile(SystemTable, ImageHandle, L"=== BOOTLOADER STARTED ===");
    LogToFile(SystemTable, ImageHandle, L"[I] Build: ");
    LogToFile(SystemTable, ImageHandle, BUILD_VERSION_STR);

    for (Index = 0; Index < SystemTable->NumberOfTableEntries; Index++) {
        if (CompareGuid(&(SystemTable->ConfigurationTable[Index].VendorGuid), &gEfiAcpi20TableGuid)) {
            Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)SystemTable->ConfigurationTable[Index].VendorTable;
            break;
        }
    }

    if (Rsdp == NULL) {
        LogToFile(SystemTable, ImageHandle, L"[-] ALARM: UNABLE TO LOCATE ACPI RSDP TABLE IN CONFIGURATION TABLE!");
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"  [!] ALARM: UNABLE TO LOCATE ACPI RSDP TABLE!\r\n");
        BS->Stall(10000000); 
        return EFI_NOT_FOUND;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[+] Successfully located ACPI RSDP Table.\r\n");
    LogToFile(SystemTable, ImageHandle, L"[+] Successfully located ACPI RSDP Table.");

    Status = InjectSsdt(SystemTable, ImageHandle, Rsdp);
    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[-] Error: SSDT Injection failed.\r\n");
        BS->Stall(5000000);
        return Status;
    }
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[+] Custom SSDT injected successfully.\r\n");

    Status = PatchTablesInPlace(SystemTable, ImageHandle, Rsdp);
    if (!EFI_ERROR(Status)) {
        ConsolePrint(SystemTable, L"[+] In-place patch pass executed.\r\n", FALSE);
    }

    Status = ReplaceAcpiTables(SystemTable, ImageHandle, Rsdp);
    if (EFI_ERROR(Status)) {
        ConsolePrint(SystemTable,
                     L"  [-] ALARM: ACPI table replacement matched NO target table. "
                     L"Check unlock.log / BIOS version.\r\n",
                     TRUE);
        LogToFile(SystemTable, ImageHandle,
                  L"[-] ALARM: ACPI table replacement matched no target table.");
    } else {
        ConsolePrint(SystemTable,
                     L"[+] ACPI table replacement applied (DSDT/SSDT4).\r\n", FALSE);
    }

#if BOOT_ICON_REPLACE
    // Replace UEFI boot logo via ACPI BGRT hijack and GOP Blt before chainloading.
    // Cosmetic only: failure must be visible but must not block the boot.
    Status = PatchBgrtAndDrawLogo(SystemTable, ImageHandle, Rsdp);
    if (EFI_ERROR(Status)) {
        LogToFile(SystemTable, ImageHandle,
                  L"[-] Boot icon replacement failed (non-fatal).");
    }
#else
    LogToFile(SystemTable, ImageHandle,
              L"[I] Boot icon replacement disabled (BOOT_ICON_REPLACE=0).");
#endif

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[+] Chainloading Windows Boot Manager from ESP...\r\n");
    LogToFile(SystemTable, ImageHandle, L"[+] Chainloading Windows Boot Manager from ESP...");

    Status = BS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == NULL) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[-] Error: Failed to retrieve Loaded Image Protocol.\r\n");
        BS->Stall(5000000);
        return Status;
    }

    Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (EFI_ERROR(Status) || DevicePath == NULL) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[-] Error: Failed to retrieve ESP Partition Device Path.\r\n");
        BS->Stall(5000000);
        return Status;
    }

    for (i = 0; i < NumCandidates; i++) {
        CHAR16 *Path = Candidates[i];

        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[+] Attempting to load path: ");
        SystemTable->ConOut->OutputString(SystemTable->ConOut, Path);
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r\n");

        LogToFile(SystemTable, ImageHandle, L"--------------------------------------");
        LogToFile(SystemTable, ImageHandle, Path);

        FullDevicePath = AppendFileNameToDevicePath(SystemTable, DevicePath, Path);
        if (FullDevicePath == NULL) {
            SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[-] Error: Failed to construct combined Device Path.\r\n");
            continue;
        }

        LogHexToFile(SystemTable, ImageHandle, L"[D] PartPath hex: ", DevicePath, GetDevicePathSize(DevicePath));
        LogHexToFile(SystemTable, ImageHandle, L"[D] FullDevicePath hex: ", FullDevicePath, GetDevicePathSize(FullDevicePath));

        Status = BS->LoadImage(
            FALSE,
            ImageHandle,
            FullDevicePath,
            NULL,
            0,
            &WinBootHandle
        );

        BS->FreePool(FullDevicePath);

        StatusToHex(Status, StatusStr);
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[D] LoadImage returned: ");
        SystemTable->ConOut->OutputString(SystemTable->ConOut, StatusStr);
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r\n");
        LogStatusToFile(SystemTable, ImageHandle, L"[D] LoadImage returned: ", Status);

        if (!EFI_ERROR(Status) && WinBootHandle != NULL) {
            LogToFile(SystemTable, ImageHandle, L"[+] LoadImage succeeded!");
            break;
        }
    }

    if (WinBootHandle != NULL) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[+] Starting Windows Boot Manager...\r\n");
        LogToFile(SystemTable, ImageHandle, L"[+] Starting Windows Boot Manager...");
        Status = BS->StartImage(WinBootHandle, NULL, NULL);
        return Status;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[-] Error: Failed to load Windows Boot Manager on any path.\r\n");
    LogToFile(SystemTable, ImageHandle, L"[-] Error: Failed to load on any path.");
    BS->Stall(10000000);
    return EFI_NOT_FOUND;
}
