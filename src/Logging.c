#include "Logging.h"

void LogToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Message)
{
    EFI_STATUS Status;
    EFI_BOOT_SERVICES *BS;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume = NULL;
    EFI_FILE_PROTOCOL *RootDir = NULL;
    EFI_FILE_PROTOCOL *LogFile = NULL;
    char AsciiBuffer[256];
    int i;
    UINTN WriteSize;
    static BOOLEAN LogInitialized = FALSE;

    BS = SystemTable->BootServices;

    Status = BS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == NULL) return;

    Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Volume);
    if (EFI_ERROR(Status) || Volume == NULL) return;

    Status = Volume->OpenVolume(Volume, &RootDir);
    if (EFI_ERROR(Status) || RootDir == NULL) return;

    // On the very first write during boot, delete the old log file to truncate it
    if (!LogInitialized) {
        Status = RootDir->Open(RootDir, &LogFile, L"\\EFI\\BOOT\\unlock.log", 
                               EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
        if (!EFI_ERROR(Status) && LogFile != NULL) {
            LogFile->Delete(LogFile); // This deletes the file and automatically closes the handle
            LogFile = NULL;
        }
        LogInitialized = TRUE;
    }

    Status = RootDir->Open(RootDir, &LogFile, L"\\EFI\\BOOT\\unlock.log", 
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);

    if (!EFI_ERROR(Status) && LogFile != NULL) {
        LogFile->SetPosition(LogFile, 0xFFFFFFFFFFFFFFFFULL);

        i = 0;
        while (Message[i] != 0 && i < 250) {
            AsciiBuffer[i] = (char)Message[i];
            i++;
        }
        AsciiBuffer[i++] = '\r';
        AsciiBuffer[i++] = '\n';
        AsciiBuffer[i] = 0;

        WriteSize = i;
        LogFile->Write(LogFile, &WriteSize, AsciiBuffer); 
        LogFile->Close(LogFile);                          
    }
    RootDir->Close(RootDir);                              
}

void StatusToHex(EFI_STATUS Status, CHAR16 *Buffer)
{
    CHAR16 HexChars[] = L"0123456789ABCDEF";
    int i;
    Buffer[0] = L'0';
    Buffer[1] = L'x';
    for (i = 0; i < 16; i++) {
        Buffer[17 - i] = HexChars[(Status >> (i * 4)) & 0xF];
    }
    Buffer[18] = 0;
}

void LogStatusToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, EFI_STATUS Status)
{
    CHAR16 StatusStr[32];
    CHAR16 LogMsg[128];
    int i = 0, j = 0;

    StatusToHex(Status, StatusStr);

    while (Prefix[i] != 0 && i < 80) {
        LogMsg[i] = Prefix[i];
        i++;
    }
    while (StatusStr[j] != 0 && i < 120) {
        LogMsg[i] = StatusStr[j];
        i++;
        j++;
    }
    LogMsg[i] = 0;

    LogToFile(SystemTable, ImageHandle, LogMsg);
}

void LogHexToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, VOID *Buffer, UINTN Size)
{
    UINT8 *Bytes = (UINT8 *)Buffer;
    CHAR16 HexChars[] = L"0123456789ABCDEF";
    CHAR16 LogMsg[256];
    int idx = 0;
    int i = 0;
    UINTN k;

    while (Prefix[i] != 0 && idx < 100) {
        LogMsg[idx++] = Prefix[i++];
    }

    for (k = 0; k < Size && idx < 250; k++) {
        LogMsg[idx++] = HexChars[(Bytes[k] >> 4) & 0xF];
        LogMsg[idx++] = HexChars[Bytes[k] & 0xF];
        LogMsg[idx++] = L' ';
    }
    LogMsg[idx] = 0;

    LogToFile(SystemTable, ImageHandle, LogMsg);
}
