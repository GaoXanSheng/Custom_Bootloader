# Custom_Bootloader

蛟龙 16 PRO 用的 UEFI 引导器。开机时替换 ACPI 表、替换开机 logo、可选执行 NVIDIA 张量解锁，然后链式引导 Windows Boot Manager。

## 功能

| 模块 | 行为 |
| --- | --- |
| ACPI | 扫描 `\EFI\Custom\*.aml`，DSDT 改写 FACP 的 `DSDT`/`X_DSDT` 指针，SSDT 按 `OemTableId` + `OemRevision` + `NPCF` 指纹替换或注入 XSDT，并回读验证指针是否写成功 |
| 开机 logo | 劫持 ACPI BGRT 表，把内嵌 BMP 复制到 ACPI Reclaim 内存，再用 GOP Blt 画到屏幕 |
| NVIDIA 解锁 | 内置 nv-uefi-unlocker v2.1.5 的 TX 链，在链式引导前执行；需要 ESP 根目录的固件资产，缺少资产时自动跳过 |
| 链式引导 | 从 ESP 加载 `\EFI\Microsoft\Boot\bootmgfw.efi`，按相对路径和绝对路径各试一次 |
| 日志 | 全部阶段写到 `\EFI\BOOT\unlock.log`，持有文件句柄、每 16 行和关键行 flush |

ACPI 补丁、开机 logo、链式引导是默认行为。NVIDIA TX 链由 Cargo 的 `tx` feature 控制，默认开启；`cargo build --release --no-default-features` 编译纯 ACPI 版本。

## 构建

依赖：

- Rust stable + `x86_64-unknown-uefi` target（`rust-toolchain.toml` 会自动安装）
- Python 3
- .NET Framework 4.x 的 `csc.exe`（编译 DbgTool）
- `iasl`：放在 `tools/isal/iasl.exe`，或加入 PATH

```bat
build.bat
```

产物：

```
build\BOOTX64.efi
build\Custom\dsdt_mod.aml
build\Custom\ssdt4_mod.aml
build\DbgTool.exe
build\version.txt
```

`tools/acpi/`（原厂表）和 `tools/isal/`（编译器）是本地文件，不进仓库。

## 部署

引导器没有签名，Secure Boot 需要关闭（或把自签证书加进 db）。

管理员运行 `build\DbgTool.exe`，或直接用命令：

```bat
DbgTool.exe deploy      :: 复制 BOOTX64.efi 到 ESP 的 \EFI\BOOT，ACPI 表到 \EFI\Custom
DbgTool.exe boot        :: 把 Custom Bootloader 设为固件第一启动项
DbgTool.exe log         :: 查看 \EFI\BOOT\unlock.log 和 version.txt
DbgTool.exe acpi        :: 对比 Windows 当前 ACPI 表与 build\Custom 补丁表
DbgTool.exe nvassets    :: 部署 NVIDIA 解锁资产到 ESP 根目录
DbgTool.exe uninstall   :: 清理启动项
```

`v67-ad102-four-plm.bin` 放在 ESP 根目录时是解锁模式；只放其余资产是回放模式（跑完整条链，但不写解锁值）。

## NVIDIA 解锁资产

资产由 nv-uefi-unlocker 的 `make-usbroot.ps1 -Family ad102` 生成，把下面 7 个文件放到 ESP 根目录（不是 `\EFI\Custom`）：

```
booter_load_ad102.bin
gsp_boot_desc_ad102.bin
gsp_boot_ad102.bin
fwimage_ad102.bin
fwsignature_ad102.bin
scrubber_ad10x.bin
v67-ad102-four-plm.bin
```

`fwimage_ad102.bin` 约 80 MB。ESP 剩下的空间不够时，`nvassets` 会先删 `\EFI\BOOT\BOOTX64.efi.bak` 和旧日志。

资产路径以 `\` 开头，由 `boot::get_image_file_system` 解析到引导卷根目录。缺少资产时日志出现 `TX: assets missing for family ... skipping`，引导继续。

TX 链没有确认环节，资产在位时每次启动都会执行。解锁是易失的：重启、休眠、D3cold 后消失。

验证：进 Windows 后管理员运行 nv-uefi-unlocker release 里的 `ssdiag.exe`，看 SS0/SS1 和 PLM 判定。

## 功耗测试

`tools\powertest\run.bat` 一键双烤：FurMark 加 32 线程 CPU 压力，默认 30 秒，输出最后 20 秒的平均 CPU / GPU / 合计功耗。

- 需要 PowerShell 7、FurMark（默认在 `D:\software` 下查找）、LibreHardwareMonitorLib.dll
- CPU 功耗需要管理员权限（脚本会自提权）
- `-GpuOnly` 只烤 GPU，`-Width`/`-Height`/`-Msaa` 调渲染负载，`-MinW` 设合计阈值
- GPU 限值直接读 `nvidia-smi`，默认与当前功耗墙都会打印

## 目录

```
src/
  main.rs        入口、ACPI/BGRT 调用、bootmgfw 链式引导
  acpi.rs        ACPI 表替换与注入、回读验证
  booticon.rs    BMP 解析、BGRT 替换、GOP 绘制
  filelog.rs     \EFI\BOOT\unlock.log
  nv.rs          NVIDIA TX 入口（tx_phase / tx_one_card）
  pci.rs         PCI 枚举与 PciIo 封装
  tx*.rs         nv-uefi-unlocker v2.1.5 TX 链
tools/
  DbgTool/       Windows 侧部署、ACPI 比对、日志查看、资产部署
  acpi_patched/  打补丁的 DSDT/SSDT 源文件
  build_custom_tables.py
  powertest/     双烤功耗测试
DOC/
  ACPI_PATCH_DETAILS.md
```

## 已知限制

- ACPI 表替换直接改固件内存里的 FACP/XSDT/RSDP。如果固件把这些页映射为只读，写入不会生效；日志里的 `[Acpi] VERIFY ...` 会显示 `FAILED`。
- Windows 只在启动时解析 ACPI，运行中改表不会重建命名空间，测试必须重启。开着快速启动时，“关机再开”是休眠恢复，不会经过引导器；用“重启”。
- NVIDIA TX 链在这台机器上还没有完成实机验证，先跑 `ssdiag` 确认。
- ESP 只有约 117 MB，放入 80 MB 的 `fwimage` 后剩余空间很少，测完建议删掉资产。

## 许可

仓库包含 nv-uefi-unlocker（GPL-2.0-only）的源码，整体按 GPL-2.0 分发。见 `LICENSE.nv-uefi-unlocker` 和 `NOTICE.nv-uefi-unlocker`。
