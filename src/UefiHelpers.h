#ifndef UEFI_HELPERS_H
#define UEFI_HELPERS_H

#include <stddef.h>

typedef unsigned short CHAR16;          
typedef unsigned char UINT8;            
typedef unsigned short UINT16;          
typedef unsigned int UINT32;            
typedef unsigned long long UINT64;      
typedef size_t UINTN;                   
typedef short BOOLEAN;                  
typedef void *EFI_HANDLE;               
typedef UINTN EFI_STATUS;               
typedef void VOID;                      
typedef UINT64 EFI_PHYSICAL_ADDRESS;    

#define TRUE 1
#define FALSE 0

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 0x800000000000000EULL
#define EFI_ERROR(status) (((long long)(status)) < 0)

#define EFIAPI __cdecl

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

extern EFI_GUID gEfiAcpi20TableGuid;
extern EFI_GUID gEfiLoadedImageProtocolGuid;
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiDevicePathProtocolGuid;

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void* Reset;
    EFI_TEXT_STRING OutputString; 
};

#pragma pack(push, 1)

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    UINT32 Signature;       
    UINT32 Length;          
    UINT8  Revision;        
    UINT8  Checksum;        
    UINT8  OemId[6];        
    UINT64 OemTableId;      
    UINT32 OemRevision;     
    UINT32 CreatorId;       
    UINT32 CreatorRevision; 
} EFI_ACPI_SDT_HEADER;

typedef struct {
    UINT64 Signature;        
    UINT8  Checksum;         
    UINT8  OemId[6];         
    UINT8  Revision;         
    UINT32 RsdtAddress;      
    UINT32 Length;           
    UINT64 XsdtAddress;      
    UINT8  ExtendedChecksum; 
    UINT8  Reserved[3];      
} EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER;

struct _EFI_DEVICE_PATH_PROTOCOL {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
};

#pragma pack(pop)

typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN) (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE) (
    EFI_FILE_PROTOCOL *This
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE) (
    EFI_FILE_PROTOCOL *This
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE) (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_READ) (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION) (
    EFI_FILE_PROTOCOL *This,
    UINT64 *Position
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION) (
    EFI_FILE_PROTOCOL *This,
    UINT64 Position
);

struct _EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    EFI_FILE_DELETE Delete;
    EFI_FILE_READ Read;
    EFI_FILE_WRITE Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition; 
};

#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_OPEN_VOLUME) (
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    EFI_FILE_PROTOCOL **Root
);
struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_SIMPLE_FILE_SYSTEM_OPEN_VOLUME OpenVolume;
};

typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL) (
    EFI_GUID *Protocol,
    VOID *Registration,
    VOID **Interface
);
typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL) (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface
);
typedef EFI_STATUS (EFIAPI *EFI_IMAGE_LOAD) (
    BOOLEAN BootPolicy,
    EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    VOID *SourceBuffer,
    UINTN SourceSize,
    EFI_HANDLE *ImageHandle
);
typedef EFI_STATUS (EFIAPI *EFI_IMAGE_START) (
    EFI_HANDLE ImageHandle,
    UINTN *ExitDataSize,
    CHAR16 **ExitData
);
typedef EFI_STATUS (EFIAPI *EFI_STALL) (
    UINTN Microseconds
);
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES) (
    UINTN AllocateType,
    UINTN MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
);
typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES) (
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN Pages
);
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL) (
    UINTN PoolType,
    UINTN Size,
    VOID **Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL) (
    VOID *Buffer
);

struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    void* RaiseTPL;
    void* RestoreTPL;
    EFI_ALLOCATE_PAGES AllocatePages; 
    EFI_FREE_PAGES FreePages;
    void* GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    void* CreateEvent;
    void* SetTimer;
    void* WaitForEvent;
    void* SignalEvent;
    void* CloseEvent;
    void* CheckEvent;
    void* InstallProtocolInterface;
    void* ReinstallProtocolInterface;
    void* UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    void* Reserved;
    void* RegisterProtocolNotify;
    void* LocateHandle;
    void* LocateDevicePath;
    void* InstallConfigurationTable;
    EFI_IMAGE_LOAD LoadImage;
    EFI_IMAGE_START StartImage;
    void* Exit;
    void* UnloadImage;
    void* ExitBootServices;
    void* GetNextMonotonicCount;
    EFI_STALL Stall;
    void* SetWatchdogTimer;
    void* ConnectController;
    void* DisconnectController;
    void* OpenProtocol;
    void* CloseProtocol;
    void* OpenProtocolInformation;
    void* ProtocolsPerHandle;
    void* LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
};

typedef struct {
    EFI_GUID VendorGuid;
    VOID *VendorTable;
} EFI_CONFIGURATION_TABLE;

struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void* ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    void* StandardErrorHandle;
    void* StdErr;
    void* RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;                  
    EFI_CONFIGURATION_TABLE *ConfigurationTable; 
};

void *UefiMemcpy(void *dest, const void *src, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
UINT8 CalculateChecksum8(UINT8 *Buffer, UINTN Size);
BOOLEAN CompareGuid(EFI_GUID *g1, EFI_GUID *g2);
UINTN GetDevicePathSize(EFI_DEVICE_PATH_PROTOCOL *DevicePath);
EFI_DEVICE_PATH_PROTOCOL *AppendFileNameToDevicePath(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_DEVICE_PATH_PROTOCOL *PartPath,
    CHAR16 *FilePathStr
);

#endif 
