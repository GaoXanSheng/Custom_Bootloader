#ifndef LOGGING_H
#define LOGGING_H

#include "UefiHelpers.h"

void LogToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, const CHAR16 *Message);
void StatusToHex(EFI_STATUS Status, CHAR16 *Buffer);
void LogStatusToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, EFI_STATUS Status);
void LogHexToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, VOID *Buffer, UINTN Size);

// Console helper: print Message to ConOut in red (IsError) or light gray,
// then restore the default color. Message should carry its own CRLF.
void ConsolePrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Message, BOOLEAN IsError);

// Log a 64-bit value as a separate "  Address: 0xXXXXXXXXXXXXXXXX" line.
void LogAddrToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, UINT64 Addr);

#endif 
