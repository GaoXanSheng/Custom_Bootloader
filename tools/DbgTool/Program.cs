// ============================================================================
// DbgTool — SSDT-UnlockDB WMI/ACPI 调试部署工具（主入口与菜单路由）
// ============================================================================
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Principal;
using DbgTool.Common;
using DbgTool.Core;

namespace DbgTool
{
    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            ConsoleUI.Init();
            Console.Title = "SSDT-UnlockDB 调试部署工具";

            // 解析 CLI 参数
            bool noElevate = false;
            string command = null;
            var tail = new System.Collections.Generic.List<string>();

            for (int i = 0; i < args.Length; i++)
            {
                string a = args[i];
                if (string.Equals(a, "--no-elevate", StringComparison.OrdinalIgnoreCase))
                {
                    noElevate = true;
                }
                else if (string.Equals(a, "--help", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(a, "-h", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(a, "/?", StringComparison.OrdinalIgnoreCase))
                {
                    PrintUsage();
                    return 0;
                }
                else if (command == null && !a.StartsWith("-"))
                {
                    command = a;
                }
                else if (command != null && !a.StartsWith("-"))
                {
                    tail.Add(a);   // 命令参数（如 dbc 10 的瓦数）
                }
            }

            // 权限检查与提权
            if (!IsAdministrator() && !noElevate)
            {
                Console.WriteLine("[!] 需要管理员权限，正在请求提权...");
                try
                {
                    string exePath = Assembly.GetExecutingAssembly().Location;
                    var psi = new ProcessStartInfo
                    {
                        FileName = exePath,
                        Verb = "runas",
                        UseShellExecute = true,
                        WorkingDirectory = PathResolver.AppDir
                    };
                    if (!string.IsNullOrEmpty(command))
                    {
                        psi.Arguments = string.Join(" ", new[] { command }.Concat(tail));
                    }
                    Process.Start(psi);
                    return 0;
                }
                catch (Exception ex)
                {
                    ConsoleUI.Error("提权失败或被用户取消: " + ex.Message);
                    return 1;
                }
            }

            // CLI 命令行模式：静默执行，绝不 Pause，返回准确 ExitCode
            if (!string.IsNullOrEmpty(command))
            {
                return RunCliCommand(command, tail.ToArray());
            }

            // 交互式菜单模式
            MenuLoop();
            return 0;
        }

        private static void PrintUsage()
        {
            Console.WriteLine("SSDT-UnlockDB 调试部署工具 (构建: " + BuildInfo.BuildStamp + ")");
            Console.WriteLine("用法: DbgTool.exe [选项] [命令]");
            Console.WriteLine();
            Console.WriteLine("选项:");
            Console.WriteLine("  --no-elevate    跳过管理员提权请求（仅用于只读调试或受限测试）");
            Console.WriteLine("  --help, -h      显示本帮助信息");
            Console.WriteLine();
            Console.WriteLine("支持的命令:");
            Console.WriteLine("  wmi         检查 CustomDbUnlock WMI 设备注册状态");
            Console.WriteLine("  dbfs        读取当前 Dynamic Boost (DBFS) 状态");
            Console.WriteLine("  db0         禁用 Dynamic Boost (SetDbfs = 0，GPU 锁 115W，CPU 满血)");
            Console.WriteLine("  db1         启用 Dynamic Boost (SetDbfs = 1，GPU 借电冲 140W)");
            Console.WriteLine("  dbc <瓦>    设置 DB 借电上限 0..100 (SetDbBoostCap，如 dbc 10)");
            Console.WriteLine("  gput <瓦>   设置 GPU 功耗预算 35..140 (SetGpuBudget，如 gput 100)");
            Console.WriteLine("  dbstatus    DB 全景 (DBFS/CSPL/FPPT/当前借电上限)");
            Console.WriteLine("  version     读取内部版本号及构建时间");
            Console.WriteLine("  status      一键功耗状态 (GPMD/ITSM/CSPL/FPPT/DBFS/适配器功率)");
            Console.WriteLine("  ecram       全量 EC RAM 转储 (0x00-0xFF)");
            Console.WriteLine("  mifs        固件原生 WMI (Bitland MIFS / AMD AOD)");
            Console.WriteLine("  ecdiff      EC 快照采集与差异对比 (变量推断)");
            Console.WriteLine("  verify      重定向 ACPI 表生效验证");
            Console.WriteLine("  npcftrace   NVPCF 驱动调用追踪");
            Console.WriteLine("  log         查看 ESP 日志与版本 (unlock.log / version.txt)");
            Console.WriteLine("  deploy      部署 BOOTX64.efi 到 ESP 分区");
            Console.WriteLine("  boot        添加 Custom Bootloader 为 UEFI 第一启动项");
            Console.WriteLine("  uninstall   卸载 Custom Bootloader (清理 BCD 项并还原 ESP)");
        }

        private static int RunCliCommand(string cmd, string[] cmdArgs)
        {
            switch (cmd.ToLowerInvariant())
            {
                case "wmi":
                    return WmiClient.CheckRegistration() ? 0 : 1;
                case "dbfs":
                    uint dbfs;
                    if (WmiClient.GetDbfs(out dbfs))
                    {
                        ConsoleUI.Info("DBFS: " + dbfs + (dbfs != 0 ? " (已启用)" : " (已禁用)"));
                        return 0;
                    }
                    return 1;
                case "db0":
                    uint st0;
                    return (WmiClient.SetDbfs(0, out st0) && st0 == 0) ? 0 : 1;
                case "db1":
                    uint st1;
                    return (WmiClient.SetDbfs(1, out st1) && st1 == 0) ? 0 : 1;
                case "dbc":
                    if (cmdArgs == null || cmdArgs.Length == 0)
                    {
                        ConsoleUI.Error("用法: DbgTool.exe dbc <瓦>   (0..100，100=原厂)");
                        return 1;
                    }
                    uint watts;
                    if (!uint.TryParse(cmdArgs[0], out watts) || watts > 100)
                    {
                        ConsoleUI.Error("无效瓦数: " + cmdArgs[0] + " （允许 0..100）");
                        return 1;
                    }
                    uint stc;
                    bool okc = WmiClient.SetDbBoostCap(watts, out stc);
                    if (okc && stc == 0)
                    {
                        ConsoleUI.Info(string.Format("DB 借电上限已设为 {0} W（GPU 顶约 {1} W；0=锁115W）。已 Notify 驱动即时生效。",
                            watts, 115 + Math.Min(watts, 25)));
                        return 0;
                    }
                    ConsoleUI.Error("SetDbBoostCap 失败 (状态码: " + stc + "；0xFFFFFFFF=注入表未加载/旧固件)");
                    return 1;
                case "dbstatus":
                    uint d2, c2, f2, cap2;
                    if (WmiClient.GetDynamicBoost(out d2, out c2, out f2, out cap2))
                    {
                        ConsoleUI.Info(string.Format("DBFS={0} CSPL={1}W FPPT={2}W DB上限={3}W",
                            d2, c2, f2, cap2 == 0xFF ? "不可读" : cap2.ToString()));
                        return 0;
                    }
                    return 1;
                case "gput":
                    if (cmdArgs == null || cmdArgs.Length == 0)
                    {
                        ConsoleUI.Error("用法: DbgTool.exe gput <瓦>   (35..140，140=原厂)");
                        return 1;
                    }
                    uint gw;
                    if (!uint.TryParse(cmdArgs[0], out gw) || gw < 35 || gw > 140)
                    {
                        ConsoleUI.Error("无效瓦数: " + cmdArgs[0] + " （允许 35..140）");
                        return 1;
                    }
                    uint stg;
                    bool okg = WmiClient.SetGpuBudget(gw, out stg);
                    if (okg && stg == 0)
                    {
                        ConsoleUI.Info(string.Format("GPU 功耗预算已设为 {0} W（仅性能档生效，已 Notify 驱动即时生效）。", gw));
                        return 0;
                    }
                    ConsoleUI.Error("SetGpuBudget 失败 (状态码: " + stg + "；0xFFFFFFFF=注入表未加载/旧固件)");
                    return 1;
                case "version":
                    uint ver;
                    string dt;
                    if (WmiClient.GetVersion(out ver, out dt))
                    {
                        ConsoleUI.Info(string.Format("BuildVersion: 0x{0:X8} ({1}) 构建时间: {2}", ver, ver, dt));
                        return 0;
                    }
                    return 1;
                case "status":
                    WmiClient.DumpPowerState();
                    return 0;
                case "ecram":
                    WmiClient.DumpEcRamConsole();
                    return 0;
                case "mifs":
                    FirmwareWmi.Menu();
                    return 0;
                case "ecdiff":
                    EcSnapshot.Menu();
                    return 0;
                case "verify":
                    WmiClient.VerifyTables();
                    return 0;
                case "npcftrace":
                    WmiClient.NpcfTrace();
                    return 0;
                case "log":
                    EspManager.ViewLog();
                    return 0;
                case "deploy":
                    return EspManager.Deploy() ? 0 : 1;
                case "boot":
                    return BcdManager.SetFirstBoot() ? 0 : 1;
                case "uninstall":
                    return BcdManager.Uninstall() ? 0 : 1;
                default:
                    ConsoleUI.Error("未知命令: " + cmd);
                    PrintUsage();
                    return 1;
            }
        }

        private static void MenuLoop()
        {
            while (true)
            {
                ConsoleUI.SafeClear();
                PrintBanner();
                Console.WriteLine();
                Console.WriteLine("  [1] 检查 WMI 设备注册状态 (CustomDbUnlock)");
                Console.WriteLine("  [2] 读取 DB 全景 (DBFS/CSPL/FPPT/借电上限)");
                Console.WriteLine("  [3] 禁用 Dynamic Boost (SetDbfs = 0，GPU 锁 115W，CPU 满血)");
                Console.WriteLine("  [4] 启用 Dynamic Boost (SetDbfs = 1，GPU 借电冲 140W)");
                Console.WriteLine("  [B] 设置 DB 借电上限 (SetDbBoostCap 0..100W)");
                Console.WriteLine("  [G] 设置 GPU 功耗预算 (SetGpuBudget 35..140W)");
                Console.WriteLine("  [5] 读取内部版本号 (GetVersion)");
                Console.WriteLine("  [6] 查看 ESP 日志与版本 (unlock.log / version.txt)");
                Console.WriteLine("  [7] 部署 BOOTX64.efi 到 ESP 分区");
                Console.WriteLine("  [8] 添加 Custom Bootloader 为 UEFI 第一启动项");
                Console.WriteLine("  [U] 卸载 Custom Bootloader (BCD 项 + ESP 文件)");
                Console.WriteLine("  [9] 更多 WMI 调试方法 (电池/EC/功耗推送/固件WMI)");
                Console.WriteLine("  [S] 一键功耗状态 (GPMD/ITSM/CSPL/FPPT/DBFS)");
                Console.WriteLine("  [E] 全量 EC RAM 转储 (0x00-0xFF)");
                Console.WriteLine("  [F] 固件原生 WMI (Bitland MIFS / AMD AOD)");
                Console.WriteLine("  [D] EC 快照对比 (变量推断)");
                Console.WriteLine("  [V] 重定向表生效验证");
                Console.WriteLine("  [T] NVPCF 驱动调用追踪 (DB钳制入口定位)");
                Console.WriteLine("  [0] 退出");
                Console.WriteLine();
                Console.Write("  请选择操作: ");

                string choice = Console.ReadLine();
                if (choice == null || choice == "0" || choice.Equals("q", StringComparison.OrdinalIgnoreCase))
                {
                    return;
                }

                switch (choice.ToUpperInvariant())
                {
                    case "1": WmiClient.CheckRegistration(); break;
                    case "2":
                        uint d1, c1, f1, cap1;
                        if (WmiClient.GetDynamicBoost(out d1, out c1, out f1, out cap1))
                        {
                            ConsoleUI.Info(string.Format("DBFS={0} ({1})  CSPL={2}W  FPPT={3}W  DB借电上限={4}W",
                                d1, d1 != 0 ? "启用" : "禁用", c1, f1,
                                cap1 == 0xFF ? "不可读(旧固件)" : cap1.ToString()));
                        }
                        break;
                    case "3":
                        uint st0;
                        WmiClient.SetDbfs(0, out st0);
                        ConsoleUI.Info("SetDbfs(0) 结果: " + (st0 == 0 ? "成功（GPU 锁 115W，CPU 满血）" : "失败 (状态码: " + st0 + ")"));
                        break;
                    case "4":
                        uint st1;
                        WmiClient.SetDbfs(1, out st1);
                        ConsoleUI.Info("SetDbfs(1) 结果: " + (st1 == 0 ? "成功（GPU 可借电冲 140W）" : "失败 (状态码: " + st1 + ")"));
                        break;
                    case "B":
                    case "b":
                        Console.Write("  输入 DB 借电上限 (0..100W，100=原厂，直接回车取消): ");
                        string capIn = Console.ReadLine();
                        uint capW;
                        if (!string.IsNullOrEmpty(capIn) && uint.TryParse(capIn, out capW) && capW <= 100)
                        {
                            uint stc2;
                            bool okc2 = WmiClient.SetDbBoostCap(capW, out stc2);
                            if (okc2 && stc2 == 0)
                                ConsoleUI.Info(string.Format("DB 借电上限已设为 {0} W（GPU 顶约 {1} W）。",
                                    capW, 115 + Math.Min(capW, 25)));
                            else
                                ConsoleUI.Error("SetDbBoostCap 失败 (状态码: " + stc2 + ")");
                        }
                        else
                        {
                            ConsoleUI.Warn("已取消（输入无效或为空）。");
                        }
                        break;
                    case "G":
                    case "g":
                        Console.Write("  输入 GPU 功耗预算 (35..140W，140=原厂，直接回车取消): ");
                        string gpuIn = Console.ReadLine();
                        uint gpuW;
                        if (!string.IsNullOrEmpty(gpuIn) && uint.TryParse(gpuIn, out gpuW) && gpuW >= 35 && gpuW <= 140)
                        {
                            uint stg2;
                            bool okg2 = WmiClient.SetGpuBudget(gpuW, out stg2);
                            if (okg2 && stg2 == 0)
                                ConsoleUI.Info(string.Format("GPU 功耗预算已设为 {0} W（仅性能档生效，已即时生效）。", gpuW));
                            else
                                ConsoleUI.Error("SetGpuBudget 失败 (状态码: " + stg2 + ")");
                        }
                        else
                        {
                            ConsoleUI.Warn("已取消（输入无效或为空）。");
                        }
                        break;
                    case "5":
                        uint ver;
                        string dt;
                        if (WmiClient.GetVersion(out ver, out dt))
                        {
                            ConsoleUI.Info(string.Format("内部版本 (BuildVersion): 0x{0:X8} ({1})", ver, ver));
                            ConsoleUI.Info("构建时间: " + dt);
                        }
                        break;
                    case "6": EspManager.ViewLog(); break;
                    case "7": EspManager.Deploy(); break;
                    case "8": BcdManager.SetFirstBoot(); break;
                    case "U": BcdManager.Uninstall(); break;
                    case "9": ExtraWmiMenu(); break;
                    case "S": WmiClient.DumpPowerState(); break;
                    case "E": WmiClient.DumpEcRamConsole(); break;
                    case "F": FirmwareWmi.Menu(); break;
                    case "D": EcSnapshot.Menu(); break;
                    case "V": WmiClient.VerifyTables(); break;
                    case "T": WmiClient.NpcfTrace(); break;
                }

                ConsoleUI.Pause();
            }
        }

        private static void ExtraWmiMenu()
        {
            while (true)
            {
                ConsoleUI.SafeClear();
                ConsoleUI.Header("更多 WMI 调试方法 (CustomDbUnlock)");
                Console.WriteLine("  [1] 读取电池充电阈值    GetBatteryChargeThreshold");
                Console.WriteLine("  [2] 设置电池充电阈值    SetBatteryChargeThreshold(0-100)");
                Console.WriteLine("  [3] 读取热响应阈值      GetThresholds (ACBT/DCBT/AMAT/AMIT)");
                Console.WriteLine("  [4] 写入热响应阈值      SetThresholds");
                Console.WriteLine("  [5] 读取 EC 寄存器      GetEcRegister(offset 0-255)");
                Console.WriteLine("  [6] 写入 EC 寄存器      SetEcRegister(白名单寄存器)");
                Console.WriteLine("  [7] 推送功耗限值        ApplyPowerLimits(CSPL, FPPT) → SMU");
                Console.WriteLine("  [8] AMD ALIB 直写       SetAmdAlibDirect(sub, data)");
                Console.WriteLine("  [0] 返回主菜单");
                Console.WriteLine();
                Console.Write("  请选择操作: ");

                string c = Console.ReadLine();
                if (c == null || c == "0") return;

                switch (c)
                {
                    case "1":
                        uint limit;
                        if (WmiClient.GetBatteryChargeThreshold(out limit))
                            ConsoleUI.Info("当前电池充电阈值: " + limit + "%");
                        break;
                    case "2":
                        int newLimit = ConsoleUI.ReadIntSafely("充电阈值百分比", 80, 0, 100);
                        uint setSt;
                        WmiClient.SetBatteryChargeThreshold((uint)newLimit, out setSt);
                        ConsoleUI.Info("设置阈值结果: " + (setSt == 0 ? "成功" : "失败 (状态码: " + setSt + ")"));
                        break;
                    case "3":
                        uint acbt, dcbt, amat, amit;
                        if (WmiClient.GetThresholds(out acbt, out dcbt, out amat, out amit))
                            ConsoleUI.Info(string.Format("ACBT={0}, DCBT={1}, AMAT={2}, AMIT={3}", acbt, dcbt, amat, amit));
                        break;
                    case "4":
                        uint newAcbt = ConsoleUI.ReadHexSafely("ACBT", 0, uint.MaxValue);
                        uint newDcbt = ConsoleUI.ReadHexSafely("DCBT", 0, uint.MaxValue);
                        uint newAmat = ConsoleUI.ReadHexSafely("AMAT", 0, uint.MaxValue);
                        uint newAmit = ConsoleUI.ReadHexSafely("AMIT", 0, uint.MaxValue);
                        uint thSt;
                        WmiClient.SetThresholds(newAcbt, newDcbt, newAmat, newAmit, out thSt);
                        ConsoleUI.Info("设置热阈值结果: " + (thSt == 0 ? "成功" : "失败 (状态码: " + thSt + ")"));
                        break;
                    case "5":
                        uint regOff = ConsoleUI.ReadHexSafely("EC 寄存器偏移", 0, 255);
                        uint regVal;
                        if (WmiClient.GetEcRegister(regOff, out regVal))
                            ConsoleUI.Info(string.Format("EC[0x{0:X2}] = 0x{1:X2}", regOff, regVal));
                        break;
                    case "6":
                        uint setOff = ConsoleUI.ReadHexSafely("EC 寄存器偏移", 0, 255);
                        uint setVal = ConsoleUI.ReadHexSafely("写入值", 0, 255);
                        uint ecSt;
                        WmiClient.SetEcRegister(setOff, setVal, out ecSt);
                        ConsoleUI.Info(string.Format("EC[0x{0:X2}] = 0x{1:X2} 写入结果: {2}",
                            setOff, setVal, (ecSt == 0 ? "成功" : "失败 (状态: " + ecSt + ")")));
                        break;
                    case "7":
                        ConsoleUI.Warn("CSPL/FPPT 单位为瓦 (W)，例如 85 / 110。");
                        int cspl = ConsoleUI.ReadIntSafely("CSPL (PL1 稳态, W)", 85, 0, 255);
                        int fppt = ConsoleUI.ReadIntSafely("FPPT (PL2 爆发, W)", 110, 0, 255);
                        uint pwrSt;
                        WmiClient.ApplyPowerLimits((uint)cspl, (uint)fppt, out pwrSt);
                        ConsoleUI.Info(string.Format("推送功耗限值结果: {0}", pwrSt == 0 ? "成功 (已推入 SMU)" : "失败 (状态: " + pwrSt + ")"));
                        break;
                    case "8":
                        int sub = ConsoleUI.ReadIntSafely("SubFunction (0-255)", 0, 0, 255);
                        uint data = ConsoleUI.ReadHexSafely("DataParam", 0, uint.MaxValue);
                        uint alibSt;
                        WmiClient.SetAmdAlibDirect((uint)sub, data, out alibSt);
                        ConsoleUI.Info(string.Format("AMD ALIB 直写结果: {0}", alibSt == 0 ? "成功" : "失败 (状态: " + alibSt + ")"));
                        break;
                }
                ConsoleUI.Pause();
            }
        }

        private static void PrintBanner()
        {
            ConsoleUI.WriteColored("==========================================================", ConsoleColor.Cyan);
            ConsoleUI.WriteColored("        SSDT-UnlockDB WMI/ACPI 调试部署工具", ConsoleColor.Cyan);
            ConsoleUI.WriteColored("        构建版本时间戳: " + BuildInfo.BuildStamp, ConsoleColor.Cyan);
            ConsoleUI.WriteColored("==========================================================", ConsoleColor.Cyan);
        }

        private static bool IsAdministrator()
        {
            try
            {
                using (WindowsIdentity id = WindowsIdentity.GetCurrent())
                {
                    var p = new WindowsPrincipal(id);
                    return p.IsInRole(WindowsBuiltInRole.Administrator);
                }
            }
            catch
            {
                return false;
            }
        }
    }
}
