#ifndef LOGGING_H
#define LOGGING_H

#include "UefiHelpers.h"

void LogToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, const CHAR16 *Message);
void StatusToHex(EFI_STATUS Status, CHAR16 *Buffer);
void LogStatusToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, EFI_STATUS Status);
void LogHexToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, CHAR16 *Prefix, VOID *Buffer, UINTN Size);

// 控制台辅助函数：将 Message 以红色（IsError）或浅灰色打印到 ConOut，
// 然后恢复默认颜色。Message 应自带 CRLF。
void ConsolePrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Message, BOOLEAN IsError);

// 将 64 位值记录为单独的一行 "  Address: 0xXXXXXXXXXXXXXXXX"。
void LogAddrToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, UINT64 Addr);

// 将 "<Prefix>0xXXXXXXXXXXXXXXXX" 记录在同一行（表标识字段等）。
void LogU64ToFile(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, const CHAR16 *Prefix, UINT64 Value);

#endif 
