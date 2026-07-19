#include "UefiHelpers.h"

EFI_GUID gEfiAcpi20TableGuid = { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } };
EFI_GUID gEfiLoadedImageProtocolGuid = { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiSimpleFileSystemProtocolGuid = { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiDevicePathProtocolGuid = { 0x09576E91, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };

void *UefiMemcpy(void *dest, const void *src, size_t count)
{
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

#pragma function(memcpy)
void *memcpy(void *dest, const void *src, size_t count)
{
    return UefiMemcpy(dest, src, count);
}

UINT8 CalculateChecksum8(UINT8 *Buffer, UINTN Size)
{
    UINT8 Sum = 0;
    UINTN i;
    for (i = 0; i < Size; i++) {
        Sum += Buffer[i];
    }
    return (UINT8)(0 - Sum);
}

BOOLEAN CompareGuid(EFI_GUID *g1, EFI_GUID *g2)
{
    int i;
    if (g1->Data1 != g2->Data1) return FALSE;
    if (g1->Data2 != g2->Data2) return FALSE;
    if (g1->Data3 != g2->Data3) return FALSE;
    for (i = 0; i < 8; i++) {
        if (g1->Data4[i] != g2->Data4[i]) return FALSE;
    }
    return TRUE;
}

UINTN GetDevicePathSize(EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    UINTN Size = 0;
    while (DevicePath != NULL) {
        UINTN NodeSize = (DevicePath->Length[1] << 8) | DevicePath->Length[0];
        if (NodeSize < 4) {
            break;
        }
        Size += NodeSize;
        if (DevicePath->Type == 0x7F && DevicePath->SubType == 0xFF) {
            break;
        }
        DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)DevicePath + NodeSize);
    }
    return Size;
}

EFI_DEVICE_PATH_PROTOCOL *AppendFileNameToDevicePath(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_DEVICE_PATH_PROTOCOL *PartPath,
    CHAR16 *FilePathStr
)
{
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    UINTN PartPathSize;
    UINTN PartPathSizeNoEnd;
    UINTN NewPathSize;
    UINTN FilePathLen = 0;
    UINTN FilePathSize;
    UINT8 *NewPathBuffer = NULL;
    EFI_STATUS Status;

    #pragma pack(push, 1)
    typedef struct {
        UINT8 Type;
        UINT8 SubType;
        UINT16 Length;
    } NODE_HEADER;
    #pragma pack(pop)

    NODE_HEADER FileNodeHeader;
    NODE_HEADER EndNodeHeader;

    while (FilePathStr[FilePathLen] != 0 && FilePathLen < 256) {
        FilePathLen++;
    }
    FilePathLen++; 

    FilePathSize = FilePathLen * sizeof(CHAR16);

    FileNodeHeader.Type = 4;
    FileNodeHeader.SubType = 4;
    FileNodeHeader.Length = (UINT16)(sizeof(NODE_HEADER) + FilePathSize);

    EndNodeHeader.Type = 0x7F;
    EndNodeHeader.SubType = 0xFF;
    EndNodeHeader.Length = (UINT16)sizeof(NODE_HEADER);

    PartPathSize = GetDevicePathSize(PartPath);
    if (PartPathSize < 4) return NULL;

    PartPathSizeNoEnd = PartPathSize - 4;
    NewPathSize = PartPathSizeNoEnd + FileNodeHeader.Length + sizeof(NODE_HEADER);

    Status = BS->AllocatePool(2, NewPathSize, (VOID **)&NewPathBuffer); 
    if (EFI_ERROR(Status) || NewPathBuffer == NULL) {
        return NULL;
    }

    UefiMemcpy(NewPathBuffer, PartPath, PartPathSizeNoEnd);
    UefiMemcpy(NewPathBuffer + PartPathSizeNoEnd, &FileNodeHeader, sizeof(NODE_HEADER));
    UefiMemcpy(NewPathBuffer + PartPathSizeNoEnd + sizeof(NODE_HEADER), FilePathStr, FilePathSize);
    UefiMemcpy(NewPathBuffer + PartPathSizeNoEnd + FileNodeHeader.Length, &EndNodeHeader, sizeof(NODE_HEADER));

    return (EFI_DEVICE_PATH_PROTOCOL *)NewPathBuffer;
}
