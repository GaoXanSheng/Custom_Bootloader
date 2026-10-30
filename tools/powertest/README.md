# 功耗测试

FurMark 加 CPU 全核压力，默认跑 30 秒，只输出最后 20 秒的平均 CPU / GPU / 合计功耗。

## 依赖

- PowerShell 7
- 管理员权限：读 CPU 包功耗需要 LHM 装内核驱动
- NVIDIA 驱动：GPU 功耗走 NVAPI，限值来自 `nvidia-smi`

依赖文件放在 `bin/`，脚本优先用相对路径，找不到才去 `D:\software` 搜，也可以用参数指定。

```
bin/
  FurMark/                        FurMark_win64 整个目录
  LibreHardwareMonitorLib.dll
  HidSharp.dll
  DiskInfoToolkit.dll
  RAMSPDToolkit-NDD.dll
  Mono.Posix.NETStandard.dll
  System.IO.Ports.dll
  System.Management.dll
  System.Security.AccessControl.dll
  System.Security.Principal.Windows.dll
  System.Threading.AccessControl.dll
```

来源：图吧工具箱的 `src/` 和 `src/Tools/烤鸡工具/FurMark_win64/`。`bin/` 约 54 MB，不进仓库。

## 用法

```
run.bat
```

自动提权，FurMark 加 32 线程 CPU 压力，30 秒后输出最后 20 秒平均。

| 参数 | 作用 |
| --- | --- |
| `-GpuOnly` | 只烤 GPU，不启动 CPU 压力 |
| `-Seconds` / `-Window` | 总时长 / 取平均的尾段，默认 30 / 20 |
| `-Width` / `-Height` / `-Msaa` | FurMark 渲染尺寸和抗锯齿，默认 2560x1440 |
| `-MinW` | 合计功耗阈值，达到就打印判定 |
| `-LhmDll` / `-FurmarkExe` | 覆盖依赖路径 |
| `-NoElevate` | 不自动提权，调试用（CPU 功耗会读成 0） |

## 输出

```
[+] LHM: ...\bin\LibreHardwareMonitorLib.dll
[+] FurMark: ...\bin\FurMark\furmark.exe
[+] GPU 功耗墙: 当前 140 W / 默认 80 W
最后 20 秒平均: CPU 122.6 W | GPU 114.1 W | 合计 236.7 W
温度峰值: CPU 97.9 C | GPU 71 C    样本数: 15
```

- CPU 包功耗读不到时先确认提权，再看内存完整性（HVCI）是否拦了 LHM 驱动。
- GPU 功耗和限值走 NVAPI / `nvidia-smi`，不需要管理员。
- 双烤时 Dynamic Boost 会把 GPU 限值从 140 W 下调到 120 W 左右，把预算让给 CPU，属正常行为；GPU 单烤才能顶满 140 W。
- `stress.exe` 用 rustc 现场编译，FurMark 缺失时回退到 `gputest.exe`（OpenCL）。

## FP16 GEMM

```
python gemm_fp16.py
```

需要 PyTorch CUDA。输出 4096 / 8192 的 TFLOPS、主频、功耗峰值。

同型号 4060 Laptop（28a0）的参考值：原厂 26.37 TFLOPS，解锁后 50.29 TFLOPS。本机解锁后实测 8192 约 53.8 TFLOPS。

对照组要用原厂状态跑同一条命令：重启时选普通 Windows Boot Manager，不要走 Custom Bootloader，这样 TX 链不会执行。
