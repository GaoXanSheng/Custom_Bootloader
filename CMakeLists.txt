cmake_minimum_required(VERSION 3.24)

# =============================================================================
# Custom_Bootloader —— 全量 CMake 构建
#
# 产物（全部落在仓库 build\ 目录）：
#   build\BOOTX64.efi   UEFI 引导器（MSVC freestanding，纯内存特征修补）
#   build\DbgTool.exe   Windows 侧部署与调试工具（csc 编译）
# =============================================================================

project(CustomBootloader C)

# ---------------------------------------------------------------------------
# 工具定位
# ---------------------------------------------------------------------------
find_program(PYTHON_EXE NAMES python python3 REQUIRED)

# 生成物与最终产物目录（gitignored）
set(GEN_DIR "${CMAKE_SOURCE_DIR}/build")

# ---------------------------------------------------------------------------
# 阶段 1：版本头（build\Version.h、version.txt）
# ---------------------------------------------------------------------------
add_custom_target(version_gen
    COMMAND "${PYTHON_EXE}" tools/generate_version.py --out-dir "${GEN_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "[version] 生成 Version.h / version.txt"
    VERBATIM)

# ---------------------------------------------------------------------------
# 阶段 2：THMP 补丁配置头（build\ThmpConfig.h，源于 tools\thmp_config.yaml）
# ---------------------------------------------------------------------------
add_custom_target(thmp_config_gen
    COMMAND "${PYTHON_EXE}" tools/generate_thmp_config.py --out-dir "${GEN_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "[thmp_config] 生成 ThmpConfig.h（源自 tools/thmp_config.yaml）"
    VERBATIM)

# ---------------------------------------------------------------------------
# EFI 引导器 BOOTX64.efi（freestanding：无 CRT、无默认库）
# ---------------------------------------------------------------------------
# 抹掉 CMake 为 MSVC 注入的默认编译/链接选项（运行时库、/EHsc、增量链接
# 等），EFI 镜像需要完全可控的裸命令行。
set(CMAKE_C_FLAGS "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
set(CMAKE_C_FLAGS_MINSIZEREL "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
set(CMAKE_CXX_FLAGS_MINSIZEREL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "")
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "")

add_executable(BOOTX64
    src/main.c
    src/UefiHelpers.c
    src/Logging.c
    src/AcpiPatch.c
    src/BootIcon.c)

target_include_directories(BOOTX64 PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${GEN_DIR}")

target_compile_options(BOOTX64 PRIVATE /GS- /W4 /O2 /utf-8)

# /dll + /subsystem:efi_application 是 freestanding EFI PE 的标准组合：
# 不拉 CRT 启动对象，入口点由 /entry 指定。/MANIFEST:NO 关掉 CMake/VS 默认
# 注入的 UAC 清单（与 EFI subsystem 不兼容，LNK1295）。
set_target_properties(BOOTX64 PROPERTIES
    OUTPUT_NAME "BOOTX64"
    SUFFIX ".efi"
    LINK_FLAGS "/subsystem:efi_application /entry:efi_main /nodefaultlib /dll /MACHINE:X64 /MANIFEST:NO /INCREMENTAL:NO"
    RUNTIME_OUTPUT_DIRECTORY "${GEN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${GEN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${GEN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${GEN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${GEN_DIR}")

# 编译 BOOTX64 之前，其引用的生成头（Version.h, ThmpConfig.h）必须就位
add_dependencies(BOOTX64 version_gen thmp_config_gen)

# ---------------------------------------------------------------------------
# DbgTool（Windows 侧部署与引导管理工具，csc 编译；compile.bat 自带版本戳生成）
# ---------------------------------------------------------------------------
add_custom_target(DbgTool ALL
    COMMAND cmd /c compile.bat
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/DbgTool"
    COMMENT "[dbgtool] 编译 DbgTool.exe"
    VERBATIM)
