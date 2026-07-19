#ifndef LOGGING_H
#define LOGGING_H

#include "UefiHelpers.h"

void LogToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Message);
void StatusToHex(EFI_STATUS Status, CHAR16 *Buffer);
void LogStatusToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, EFI_STATUS Status);
void LogHexToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, VOID *Buffer, UINTN Size);

#endif 
