// ============================================================================
// DbgTool.cs — SSDT-UnlockDB WMI/ACPI 调试部署工具
//
// 编译: 运行 compile.bat（需要 .NET Framework 4.x，Windows 10/11 自带）
// 产物: DbgTool.exe（需管理员权限，程序启动时自动请求提权）
// 用法: DbgTool.exe [wmi|dbfs|db0|db1|version|log|deploy|boot]
// 测试: DbgTool.exe --no-elevate <命令>  （跳过提权，仅供调试）
// ============================================================================
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Management;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text;
using System.Text.RegularExpressions;

namespace DbgTool
{
    internal static class Program
    {
        internal const string WmiNs = @"\\.\root\wmi";
        internal const string WmiCls = "CustomDbUnlock";

        [STAThread]
        private static int Main(string[] args)
        {
            Console.Title = "SSDT-UnlockDB 调试部署工具";
            bool noElevate = args.Length > 0 && args[0] == "--no-elevate";

            if (!IsAdministrator() && !noElevate)
            {
                Console.WriteLine("[!] 需要管理员权限，正在请求提权...");
                try
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = Assembly.GetExecutingAssembly().Location,
                        Verb = "runas",
                        UseShellExecute = true,
                        WorkingDirectory = BaseDir
                    });
                }
                catch (Exception ex)
                {
                    Error("提权失败或被取消: " + ex.Message);
                }
                return 0;
            }

            if (args.Length > 0 && !noElevate)
            {
                return RunCommand(args[0]);
            }
            if (noElevate && args.Length > 1)
            {
                return RunCommand(args[1]);
            }

            MenuLoop();
            return 0;
        }

        private static int RunCommand(string cmd)
        {
            switch (cmd.ToLowerInvariant())
            {
                case "wmi": WmiHelper.CheckRegistration(); return 0;
                case "dbfs": WmiHelper.GetDbfs(); return 0;
                case "db0": WmiHelper.SetDbfs(0); return 0;
                case "db1": WmiHelper.SetDbfs(1); return 0;
                case "version": WmiHelper.GetVersion(); return 0;
                case "status": WmiHelper.DumpPowerState(); return 0;
                case "ecram": WmiHelper.DumpEcRam(); return 0;
                case "log": EspHelper.ViewLog(); return 0;
                case "deploy": EfiHelper.Deploy(); return 0;
                case "boot": BcdHelper.SetFirstBoot(); return 0;
                default:
                    Console.WriteLine("用法: DbgTool.exe [wmi|dbfs|db0|db1|version|status|ecram|log|deploy|boot]");
                    return 1;
            }
        }

        // ------------------------------------------------------------------
        // 菜单
        // ------------------------------------------------------------------
        private static void MenuLoop()
        {
            while (true)
            {
                SafeClear();
                PrintBanner();
                Console.WriteLine();
                Console.WriteLine("  [1] 检查 WMI 设备注册状态 (CustomDbUnlock)");
                Console.WriteLine("  [2] 读取当前 DBFS 标志值 (GetDbfs)");
                Console.WriteLine("  [3] 禁用 Dynamic Boost (SetDbfs = 0)");
                Console.WriteLine("  [4] 启用 Dynamic Boost (SetDbfs = 1)");
                Console.WriteLine("  [5] 读取内部版本号 (GetVersion)");
                Console.WriteLine("  [6] 查看 ESP 日志与版本 (unlock.log / version.txt)");
                Console.WriteLine("  [7] 部署 BOOTX64.efi 到 ESP 分区");
                Console.WriteLine("  [8] 添加 Custom Bootloader 为 UEFI 第一启动项");
                Console.WriteLine("  [9] 更多 WMI 调试方法 (电池/EC/SMI/ALIB)");
                Console.WriteLine("  [S] 一键功耗状态 (GPMD/ITSM/CSPL/FPPT/DBFS)");
                Console.WriteLine("  [E] 全量 EC RAM 转储 (0x00-0xFF)");
                Console.WriteLine("  [0] 退出");
                Console.WriteLine();
                Console.Write("  请选择操作: ");
                string choice = Console.ReadLine();
                if (choice == null) return;   // stdin EOF（重定向）时退出

                switch (choice)
                {
                    case "1": WmiHelper.CheckRegistration(); break;
                    case "2": WmiHelper.GetDbfs(); break;
                    case "3": WmiHelper.SetDbfs(0); break;
                    case "4": WmiHelper.SetDbfs(1); break;
                    case "5": WmiHelper.GetVersion(); break;
                    case "6": EspHelper.ViewLog(); break;
                    case "7": EfiHelper.Deploy(); break;
                    case "8": BcdHelper.SetFirstBoot(); break;
                    case "9": WmiHelper.ExtraMenu(); break;
                    case "s":
                    case "S": WmiHelper.DumpPowerState(); break;
                    case "E":
                    case "e": WmiHelper.DumpEcRam(); break;
                    case "0":
                    case "q":
                    case "Q":
                        return;
                }
            }
        }

        private static void PrintBanner()
        {
            WriteColored("==========================================================", ConsoleColor.Cyan);
            WriteColored("        SSDT-UnlockDB WMI/ACPI 调试部署工具", ConsoleColor.Cyan);
            WriteColored("==========================================================", ConsoleColor.Cyan);
        }

        internal static void SafeClear()
        {
            try { Console.Clear(); }
            catch (IOException) { }
            catch (PlatformNotSupportedException) { }
        }

        // ------------------------------------------------------------------
        // 通用辅助
        // ------------------------------------------------------------------
        internal static string BaseDir
        {
            get { return Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) ?? "."; }
        }

        internal static void Info(string msg) { WriteColored("[+] " + msg, ConsoleColor.Green); }
        internal static void Warn(string msg) { WriteColored("[!] " + msg, ConsoleColor.Yellow); }
        internal static void Error(string msg) { WriteColored("[-] " + msg, ConsoleColor.Red); }

        internal static void WriteColored(string msg, ConsoleColor color)
        {
            ConsoleColor old = Console.ForegroundColor;
            Console.ForegroundColor = color;
            Console.WriteLine(msg);
            Console.ForegroundColor = old;
        }

        internal static void Pause()
        {
            Console.WriteLine();
            try
            {
                Console.Write("  按任意键返回菜单...");
                Console.ReadKey(true);
            }
            catch (InvalidOperationException)
            {
                // stdin 被重定向（脚本/管道调用）时跳过等待
            }
        }

        private static bool IsAdministrator()
        {
            try
            {
                using (WindowsIdentity id = WindowsIdentity.GetCurrent())
                {
                    WindowsPrincipal p = new WindowsPrincipal(id);
                    return p.IsInRole(WindowsBuiltInRole.Administrator);
                }
            }
            catch
            {
                return false;
            }
        }
    }

    // ======================================================================
    // WMI 访问 (CustomDbUnlock, root\wmi)
    // ======================================================================
    internal static class WmiHelper
    {
        private delegate void InstanceAction(ManagementObject mo);

        private static void WithInstance(InstanceAction action, string what)
        {
            try
            {
                using (ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(Program.WmiNs, "SELECT * FROM " + Program.WmiCls))
                {
                    bool found = false;
                    foreach (ManagementObject mo in searcher.Get())
                    {
                        using (mo)
                        {
                            found = true;
                            try { action(mo); }
                            catch (Exception ex) { Program.Error(what + " 调用失败: " + ex.Message); }
                        }
                    }
                    if (!found)
                        Program.Error("未找到 " + Program.WmiCls + " 实例，请确认已部署并注册 ACPI/WMI 设备。");
                }
            }
            catch (Exception ex)
            {
                Program.Error("WMI 访问失败: " + ex.Message);
            }
        }

        private static void CallGet(string method, Action<ManagementBaseObject> show)
        {
            WithInstance(delegate(ManagementObject mo)
            {
                ManagementBaseObject outp = mo.InvokeMethod(method, (ManagementBaseObject)null, null);
                show(outp);
            }, method);
        }

        private static void CallGetIn(string method, Func<ManagementObject, ManagementBaseObject> inBuilder,
            Action<ManagementBaseObject> show)
        {
            WithInstance(delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = inBuilder(mo);
                ManagementBaseObject outp = mo.InvokeMethod(method, inp, null);
                show(outp);
            }, method);
        }

        private static void CallSet(string method, Func<ManagementObject, ManagementBaseObject> inBuilder,
            Action<ManagementBaseObject> show)
        {
            WithInstance(delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = inBuilder(mo);
                ManagementBaseObject outp = mo.InvokeMethod(method, inp, null);
                show(outp);
            }, method);
        }

        private static uint ReadUInt(string label, uint def, uint max)
        {
            while (true)
            {
                string range = max == uint.MaxValue ? "0-0xFFFFFFFF" : "0-" + max;
                Console.Write("  " + label + " [" + range + "] (默认 " + def + "): ");
                string s = Console.ReadLine();
                if (s != null && s.Trim().Length == 0) return def;
                uint v;
                bool hex = s.Trim().StartsWith("0x", StringComparison.OrdinalIgnoreCase);
                bool ok = hex
                    ? uint.TryParse(s.Trim().Substring(2), NumberStyles.AllowHexSpecifier, null, out v)
                    : uint.TryParse(s, out v);
                if (ok && v <= max) return v;
                Console.WriteLine("    输入无效，请重新输入。");
            }
        }

        // EC 寄存器读写的专用输入：一律按 HEX 解释（可带 0x 也可不带），
        // 例如 F6 / 0xF6 / 64(即0x64=100) 都合法。EC 的值全是十六进制，
        // 用十进制提示只会让人把 64 当十进制输入。
        private static uint ReadUIntHex(string label, uint def, uint max)
        {
            while (true)
            {
                Console.Write("  " + label + " [hex, 0x可选] (默认 0x" + def.ToString("X2") + "): ");
                string s = Console.ReadLine();
                if (s != null && s.Trim().Length == 0) return def;
                string t = s.Trim();
                if (t.StartsWith("0x", StringComparison.OrdinalIgnoreCase)) t = t.Substring(2);
                uint v;
                if (uint.TryParse(t, NumberStyles.AllowHexSpecifier, null, out v) && v <= max)
                    return v;
                Console.WriteLine("    输入无效（hex，如 F6 或 0xF6）。");
            }
        }

        // --------------------------------------------------------------
        // 主菜单方法
        // --------------------------------------------------------------
        public static void CheckRegistration()
        {
            Console.WriteLine();
            WithInstance(delegate(ManagementObject mo)
            {
                Console.WriteLine("  InstanceName : " + Convert.ToString(mo["InstanceName"]));
                Console.WriteLine("  Active       : " + Convert.ToString(mo["Active"]));
                Console.WriteLine("  __SERVER     : " + Convert.ToString(mo["__SERVER"]));
            }, "WMI 查询");
            Program.Pause();
        }

        public static void GetDbfs()
        {
            Console.WriteLine();
            CallGet("GetDbfs", delegate(ManagementBaseObject o)
            {
                uint value = Convert.ToUInt32(o["Value"]);
                Program.Info("DBFS 状态: " + value +
                    (value != 0 ? " (Dynamic Boost 已启用)" : " (Dynamic Boost 已禁用)"));
            });
            Program.Pause();
        }

        public static void SetDbfs(uint val)
        {
            Console.WriteLine();
            CallSet("SetDbfs", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetDbfs");
                inp["Value"] = val;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("SetDbfs(" + val + ") 返回状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
            Program.Pause();
        }

        public static void GetVersion()
        {
            Console.WriteLine();
            CallGet("GetVersion", delegate(ManagementBaseObject o)
            {
                uint v = Convert.ToUInt32(o["BuildVersion"]);
                Program.Info("内部版本号 (BuildVersion): 0x" + v.ToString("X8") + " (" + v + ")");
                Program.Info("构建时间: " + DecodeBuildTime(v));
            });
            Program.Pause();
        }

        // Decode the BuildVersion DWORD. New builds store the build time as
        // UTC Unix epoch seconds (second precision). Legacy builds stored a
        // BCD date 0x20YYMMDD — epoch values of 2020s builds start with
        // 0x5E..0x6A while BCD dates start with 0x20, so they never collide.
        private static string DecodeBuildTime(uint v)
        {
            try
            {
                if ((v >> 24) == 0x20)
                {
                    int y = (int)(((v >> 28) & 0xF) * 1000 + ((v >> 24) & 0xF) * 100 +
                                  ((v >> 20) & 0xF) * 10 + ((v >> 16) & 0xF));
                    int mo = (int)(((v >> 12) & 0xF) * 10 + ((v >> 8) & 0xF));
                    int d = (int)(((v >> 4) & 0xF) * 10 + (v & 0xF));
                    if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31)
                        return string.Format("(旧版BCD日期) {0:D4}-{1:D2}-{2:D2}", y, mo, d);
                }
                DateTime t = DateTimeOffset.FromUnixTimeSeconds(v).LocalDateTime;
                return t.ToString("yyyy-MM-dd HH:mm:ss") + " (本地时间)";
            }
            catch (Exception)
            {
                return "(无法解码)";
            }
        }

        // --------------------------------------------------------------
        // 一键功耗状态：一次读取所有关键 EC 寄存器 + DBFS
        // --------------------------------------------------------------
        public static void DumpPowerState()
        {
            Console.WriteLine();
            Program.WriteColored("========== 功耗状态 (CustomDbUnlock) ==========", ConsoleColor.Cyan);
            ReadEc(0xD2, "GPMD  GPU 功耗模式 (1=满血)");
            ReadEc(0xD1, "GFLG  GPU 模式标志 (0x55=满血)");
            ReadEc(0xE4, "ITSM  显卡模式 (0=混合 2=直连)");
            ReadEc(0xF2, "DGPU  独显在位");
            ReadEc(0xF3, "CPUT  CPU 型号 (7=R7 9=R9)");
            ReadEc(0xF6, "CSPL  CPU PL1 (W)");
            ReadEc(0xF7, "FPPT  CPU PL2 (W)");
            ReadEc(0xF8, "CTCL  CPU 温度墙");
            CallGet("GetDbfs", delegate(ManagementBaseObject o)
            {
                uint value = Convert.ToUInt32(o["Value"]);
                Program.Info("DBFS  Dynamic Boost 开关 = " + value);
            });
            Program.Pause();
        }

        private static void ReadEc(uint off, string label)
        {
            CallGetIn("GetEcRegister", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("GetEcRegister");
                inp["RegisterOffset"] = off;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint v = Convert.ToUInt32(o["ReadVal"]);
                string extra = (v == 0xFF) ? "  (未映射/旧版)" : "";
                Program.Info("EC[" + off.ToString("X2") + "] " + label + " = 0x" + v.ToString("X2") + extra);
            });
        }

        // --------------------------------------------------------------
        // 全量 EC RAM 转储 (GetEcByte, Method 17): 打印 0x00-0xFF 全部字节，
        // 高亮功率相关值 (0xC3=97.5W/2, 0xD1=104.5W/2, 0xCE=103W/2 等)
        // --------------------------------------------------------------
        public static void DumpEcRam()
        {
            Console.WriteLine();
            Program.WriteColored("========== EC RAM 全量转储 (0x00-0xFF) ==========", ConsoleColor.Cyan);
            byte[] buf = new byte[256];
            for (int off = 0; off < 256; off++)
            {
                int captured = off;
                CallGetIn("GetEcByte", delegate(ManagementObject mo)
                {
                    ManagementBaseObject inp = mo.GetMethodParameters("GetEcByte");
                    inp["RegisterOffset"] = (uint)captured;
                    return inp;
                }, delegate(ManagementBaseObject o)
                {
                    buf[captured] = Convert.ToByte(o["ReadVal"]);
                });
            }
            for (int row = 0; row < 16; row++)
            {
                var sb = new System.Text.StringBuilder();
                sb.Append("EC[" + (row * 16).ToString("X2") + "]: ");
                for (int col = 0; col < 16; col++)
                {
                    int idx = row * 16 + col;
                    byte v = buf[idx];
                    bool hot = v == 0xC3 || v == 0xD1 || v == 0xD3 || v == 0xD5 || v == 0xCE || v == 0x61;
                    sb.Append(hot ? ">>" + v.ToString("X2") + "<< " : v.ToString("X2") + " ");
                }
                Console.WriteLine(sb.ToString());
            }
            Program.Info("注意: 0xC3=97.5W/2, 0xD1=104.5W/2, 0xCE=103W/2, 0x61=48.5W/2 (半瓦)");
            Program.Pause();
        }

        // --------------------------------------------------------------
        // 扩展子菜单
        // --------------------------------------------------------------
        public static void ExtraMenu()
        {
            while (true)
            {
                Program.SafeClear();
                Program.WriteColored("========== 更多 WMI 调试方法 (CustomDbUnlock) ==========", ConsoleColor.Cyan);
                Console.WriteLine("  [1]  读取电池充电阈值    GetBatteryChargeThreshold");
                Console.WriteLine("  [2]  设置电池充电阈值    SetBatteryChargeThreshold(0-100)");
                Console.WriteLine("  [3]  读取热响应阈值      GetThresholds (ACBT/DCBT/AMAT/AMIT)");
                Console.WriteLine("  [4]  设置热响应阈值      SetThresholds");
                Console.WriteLine("  [5]  读取 EC 寄存器      GetEcRegister(offset 0-255)");
                Console.WriteLine("  [6]  写入 EC 寄存器      SetEcRegister(offset, value)");
                Console.WriteLine("  [7]  读取覆盖锁掩码      GetOverrideLock");
                Console.WriteLine("  [8]  设置覆盖锁掩码      SetOverrideLock");
                Console.WriteLine("  [9]  触发软件 SMI        SetSwSmi(cmd, arg)");
                Console.WriteLine("  [10] 读取 SMI 状态       GetSwSmiStatus");
                Console.WriteLine("  [11] AMD ALIB 直写       SetAmdAlibDirect(sub, data)");
                Console.WriteLine("  [12] AMD ALIB 读取       GetAmdAlibDirect(sub)");
                Console.WriteLine("  [0]  返回主菜单");
                Console.WriteLine();
                Console.Write("  请选择: ");
                string c = Console.ReadLine();
                if (c == null) return;   // stdin EOF（重定向）时退出
                switch (c)
                {
                    case "1": GetBatteryThreshold(); break;
                    case "2": SetBatteryThreshold(); break;
                    case "3": GetThresholds(); break;
                    case "4": SetThresholds(); break;
                    case "5": GetEcRegister(); break;
                    case "6": SetEcRegister(); break;
                    case "7": GetOverrideLock(); break;
                    case "8": SetOverrideLock(); break;
                    case "9": SetSwSmi(); break;
                    case "10": GetSwSmiStatus(); break;
                    case "11": SetAmdAlib(); break;
                    case "12": GetAmdAlib(); break;
                    case "0": return;
                }
                Program.Pause();
            }
        }

        // 电池充电阈值
        private static void GetBatteryThreshold()
        {
            Console.WriteLine();
            CallGet("GetBatteryChargeThreshold", delegate(ManagementBaseObject o)
            {
                Program.Info("电池充电阈值: " + Convert.ToUInt32(o["LimitPercent"]) + "%");
            });
        }

        private static void SetBatteryThreshold()
        {
            Console.WriteLine();
            uint v = ReadUInt("充电阈值百分比", 80, 100);
            CallSet("SetBatteryChargeThreshold", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetBatteryChargeThreshold");
                inp["LimitPercent"] = v;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("SetBatteryChargeThreshold(" + v + ") 状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
        }

        // 热响应阈值
        private static void GetThresholds()
        {
            Console.WriteLine();
            CallGet("GetThresholds", delegate(ManagementBaseObject o)
            {
                Program.Info("ACBT=" + Convert.ToUInt32(o["Acbt"]) +
                             "  DCBT=" + Convert.ToUInt32(o["Dcbt"]) +
                             "  AMAT=" + Convert.ToUInt32(o["Amat"]) +
                             "  AMIT=" + Convert.ToUInt32(o["Amit"]));
            });
        }

        private static void SetThresholds()
        {
            Console.WriteLine();
            uint acbt = ReadUInt("ACBT", 0, uint.MaxValue);
            uint dcbt = ReadUInt("DCBT", 0, uint.MaxValue);
            uint amat = ReadUInt("AMAT", 0, uint.MaxValue);
            uint amit = ReadUInt("AMIT", 0, uint.MaxValue);
            CallSet("SetThresholds", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetThresholds");
                inp["Acbt"] = acbt;
                inp["Dcbt"] = dcbt;
                inp["Amat"] = amat;
                inp["Amit"] = amit;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("SetThresholds 状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
        }

        // EC 寄存器
        private static void GetEcRegister()
        {
            Console.WriteLine();
            uint off = ReadUIntHex("EC 寄存器偏移", 0, 255);
            CallGetIn("GetEcRegister", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("GetEcRegister");
                inp["RegisterOffset"] = off;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                Program.Info("EC[0x" + off.ToString("X2") + "] = 0x" + Convert.ToUInt32(o["ReadVal"]).ToString("X2"));
            });
        }

        private static void SetEcRegister()
        {
            Console.WriteLine();
            uint off = ReadUIntHex("EC 寄存器偏移", 0, 255);
            uint val = ReadUIntHex("写入值", 0, 255);
            CallSet("SetEcRegister", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetEcRegister");
                inp["RegisterOffset"] = off;
                inp["WriteVal"] = val;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("EC[0x" + off.ToString("X2") + "] = 0x" + val.ToString("X2") +
                             "  状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
        }

        // 覆盖锁掩码
        private static void GetOverrideLock()
        {
            Console.WriteLine();
            CallGet("GetOverrideLock", delegate(ManagementBaseObject o)
            {
                Program.Info("覆盖锁掩码 (LockMask): 0x" + Convert.ToUInt32(o["LockMask"]).ToString("X8"));
            });
        }

        private static void SetOverrideLock()
        {
            Console.WriteLine();
            uint v = ReadUInt("LockMask", 0, uint.MaxValue);
            CallSet("SetOverrideLock", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetOverrideLock");
                inp["LockMask"] = v;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("SetOverrideLock(0x" + v.ToString("X8") + ") 状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
        }

        // 软件 SMI
        private static void SetSwSmi()
        {
            Console.WriteLine();
            uint cmd = ReadUInt("SmiCmd", 0, 255);
            uint arg = ReadUInt("SmiArg", 0, 255);
            CallSet("SetSwSmi", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetSwSmi");
                inp["SmiCmd"] = cmd;
                inp["SmiArg"] = arg;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("SetSwSmi(0x" + cmd.ToString("X2") + ", 0x" + arg.ToString("X2") +
                             ") 状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
        }

        private static void GetSwSmiStatus()
        {
            Console.WriteLine();
            CallGet("GetSwSmiStatus", delegate(ManagementBaseObject o)
            {
                Program.Info("SMI 上次执行状态: " + Convert.ToUInt32(o["Status"]));
            });
        }

        // AMD ALIB
        private static void SetAmdAlib()
        {
            Console.WriteLine();
            uint sub = ReadUInt("SubFunction", 0, 255);
            uint data = ReadUInt("DataParam", 0, uint.MaxValue);
            CallSet("SetAmdAlibDirect", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("SetAmdAlibDirect");
                inp["SubFunction"] = sub;
                inp["DataParam"] = data;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                uint st = Convert.ToUInt32(o["Status"]);
                Program.Info("SetAmdAlibDirect(" + sub + ", " + data + ") 状态: " + st + (st == 0 ? " (成功)" : " (失败)"));
            });
        }

        private static void GetAmdAlib()
        {
            Console.WriteLine();
            uint sub = ReadUInt("SubFunction", 0, 255);
            CallGetIn("GetAmdAlibDirect", delegate(ManagementObject mo)
            {
                ManagementBaseObject inp = mo.GetMethodParameters("GetAmdAlibDirect");
                inp["SubFunction"] = sub;
                return inp;
            }, delegate(ManagementBaseObject o)
            {
                Program.Info("ALIB(" + sub + ") 返回: " + Convert.ToUInt32(o["ReturnVal"]));
            });
        }
    }

    // ======================================================================
    // ESP 分区查找 / 挂载 / 卸载
    // ======================================================================
    internal static class EspHelper
    {
        public class MountInfo
        {
            public string MountPoint;  // 形如 "X:\"
            public bool SelfMounted;   // 是否由本工具挂载（可安全卸载）
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr FindFirstVolume(StringBuilder lpszVolumeName, int cchBufferLength);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool FindNextVolume(IntPtr hFindVolume, StringBuilder lpszVolumeName, int cchBufferLength);

        [DllImport("kernel32.dll")]
        private static extern bool FindVolumeClose(IntPtr hFindVolume);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool GetVolumeInformation(string rootPathName, StringBuilder volumeNameBuffer,
            int volumeNameSize, out uint volumeSerialNumber, out uint maximumComponentLength,
            out uint fileSystemFlags, StringBuilder fileSystemNameBuffer, int fileSystemNameSize);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool GetVolumePathNamesForVolumeName(string volumeName, StringBuilder volumePathNames,
            uint bufferLength, out uint returnLength);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool SetVolumeMountPoint(string mountPoint, string volumeName);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool DeleteVolumeMountPoint(string mountPoint);

        [DllImport("kernel32.dll")]
        private static extern uint GetLogicalDrives();

        private static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);

        /// <summary>
        /// 挂载 ESP 并返回挂载信息；失败返回 null。
        /// 策略: 已挂载的 ESP 盘符 &gt; 无盘符 FAT 卷中带 EFI 目录者 &gt; 其它无盘符 FAT 卷(警告)。
        /// </summary>
        public static MountInfo Mount()
        {
            // 1) 已有盘符挂载且带 EFI 目录的卷直接复用（如用户已手动 mountvol S: /S）
            string existing = FindMountedEsp();
            if (existing != null)
            {
                Program.Info("复用已挂载的 ESP: " + existing);
                return new MountInfo { MountPoint = existing, SelfMounted = false };
            }

            // 2) 枚举候选 ESP 卷（FAT 文件系统、当前无挂载点）
            List<string> vols = EnumerateFatVolumesWithoutMount();
            if (vols.Count == 0)
            {
                Program.Error("未找到无盘符的 FAT/FAT32 卷，无法自动挂载 ESP。");
                Program.Warn("可先手动挂载: mountvol S: /S，再重试本操作。");
                return null;
            }

            // 3) 逐个挂载到空闲盘符，优先返回带 EFI 目录的卷
            string firstMounted = null;
            foreach (string vol in vols)
            {
                string mp = TryMount(vol);
                if (mp == null) continue;
                if (firstMounted == null) firstMounted = mp;
                if (Directory.Exists(mp + @"EFI"))
                {
                    Program.Info("已挂载 ESP: " + mp);
                    return new MountInfo { MountPoint = mp, SelfMounted = true };
                }
            }

            if (firstMounted != null)
            {
                Program.Warn("未找到含 EFI 目录的卷；已挂载第一个 FAT 候选卷: " + firstMounted);
                Program.Warn("请确认该卷确实是 ESP（若为空 ESP，后续操作会提示找不到 EFI 文件）。");
                return new MountInfo { MountPoint = firstMounted, SelfMounted = true };
            }

            Program.Error("所有候选卷挂载失败。");
            return null;
        }

        public static void Unmount(string mountPoint)
        {
            if (string.IsNullOrEmpty(mountPoint)) return;
            if (!DeleteVolumeMountPoint(mountPoint))
            {
                int err = Marshal.GetLastWin32Error();
                Program.Warn("卸载 " + mountPoint + " 失败: " + new Win32Exception(err).Message +
                             "（可稍后手动 mountvol " + mountPoint.TrimEnd('\\') + ": /D）");
            }
            else
            {
                Program.Info("已卸载 " + mountPoint);
            }
        }

        public static void ViewLog()
        {
            Console.WriteLine();
            MountInfo mi = Mount();
            if (mi == null) { Program.Pause(); return; }
            try
            {
                string ver = mi.MountPoint + @"EFI\BOOT\version.txt";
                string log = mi.MountPoint + @"EFI\BOOT\unlock.log";
                if (File.Exists(ver))
                {
                    Console.WriteLine();
                    Console.WriteLine("  --- version.txt (" + ver + ") ---");
                    Console.WriteLine(File.ReadAllText(ver).Trim());
                }
                else
                {
                    Program.Warn("ESP 中未找到 EFI\\BOOT\\version.txt");
                }

                if (File.Exists(log))
                {
                    Console.WriteLine();
                    Console.WriteLine("  --- unlock.log (" + log + ") ---");
                    Console.WriteLine(File.ReadAllText(log).Trim());
                }
                else
                {
                    Program.Warn("ESP 中未找到 EFI\\BOOT\\unlock.log");
                }
            }
            catch (Exception ex)
            {
                Program.Error("读取日志失败: " + ex.Message);
            }
            finally
            {
                if (mi.SelfMounted) Unmount(mi.MountPoint);
            }
            Program.Pause();
        }

        // --------------------------------------------------------------
        private static string FindMountedEsp()
        {
            for (char c = 'C'; c <= 'Z'; c++)
            {
                string mp = c + ":\\";
                try
                {
                    if (Directory.Exists(mp + "EFI") && !Directory.Exists(mp + "Windows"))
                        return mp;
                }
                catch { }
            }
            return null;
        }

        private static List<string> EnumerateFatVolumesWithoutMount()
        {
            List<string> result = new List<string>();

            // 优先用 WMI（信息完整）；失败时回退到卷枚举 API
            try
            {
                using (ManagementObjectSearcher searcher = new ManagementObjectSearcher(@"\\.\root\cimv2",
                    "SELECT DeviceID, DriveLetter, FileSystem FROM Win32_Volume"))
                {
                    foreach (ManagementObject v in searcher.Get())
                    {
                        using (v)
                        {
                            string fs = Convert.ToString(v["FileSystem"] ?? "");
                            string dl = Convert.ToString(v["DriveLetter"] ?? "");
                            if (fs.StartsWith("FAT", StringComparison.OrdinalIgnoreCase) && dl.Length == 0)
                                result.Add(Convert.ToString(v["DeviceID"]));
                        }
                    }
                }
                if (result.Count > 0) return result;
            }
            catch { /* fall through */ }

            StringBuilder buf = new StringBuilder(261);
            IntPtr h = FindFirstVolume(buf, buf.Capacity);
            if (h == INVALID_HANDLE_VALUE) return result;
            do
            {
                string vol = buf.ToString();
                StringBuilder volName = new StringBuilder(64);
                StringBuilder fs = new StringBuilder(32);
                uint serial, maxLen, flags;
                if (GetVolumeInformation(vol, volName, volName.Capacity, out serial, out maxLen, out flags,
                    fs, fs.Capacity))
                {
                    if (fs.ToString().StartsWith("FAT", StringComparison.OrdinalIgnoreCase))
                    {
                        StringBuilder paths = new StringBuilder(1024);
                        uint ret = 0;
                        if (GetVolumePathNamesForVolumeName(vol, paths, (uint)paths.Capacity, out ret) &&
                            paths.Length == 0)
                            result.Add(vol);
                    }
                }
            } while (FindNextVolume(h, buf, buf.Capacity));
            FindVolumeClose(h);
            return result;
        }

        private static string TryMount(string volumeDeviceId)
        {
            char d = FindFreeDriveLetter();
            if (d == '\0')
            {
                Program.Error("没有空闲盘符可用，无法挂载 ESP。");
                return null;
            }
            string mp = d + ":\\";
            if (!SetVolumeMountPoint(mp, volumeDeviceId))
            {
                int err = Marshal.GetLastWin32Error();
                Program.Error("挂载 " + volumeDeviceId + " 到 " + mp + " 失败: " + new Win32Exception(err).Message);
                return null;
            }
            return mp;
        }

        private static char FindFreeDriveLetter()
        {
            uint mask = GetLogicalDrives();
            for (char c = 'Z'; c >= 'C'; c--)
            {
                if ((mask & (1u << (c - 'A'))) == 0) return c;
            }
            return '\0';
        }
    }

    // ======================================================================
    // BOOTX64.efi 搜索与部署
    // ======================================================================
    internal static class EfiHelper
    {
        public static void Deploy()
        {
            Console.WriteLine();
            // 1) 搜索候选
            string[] candidates = FindCandidates();
            if (candidates.Length == 0)
            {
                Program.Error("未找到 BOOTX64.efi。请先运行 build.bat 编译，或手动输入路径。");
                Console.Write("  手动输入 EFI 文件完整路径（留空取消）: ");
                string manual = Console.ReadLine();
                if (manual != null && manual.Trim().Length > 0 && File.Exists(manual.Trim()))
                    candidates = new[] { Path.GetFullPath(manual.Trim()) };
                else
                {
                    Program.Pause();
                    return;
                }
            }

            // 2) 选择候选
            string src = Choose(candidates);
            FileInfo fi = new FileInfo(src);
            if (fi.Length == 0)
            {
                Program.Error("源文件大小为 0 字节，可能已损坏: " + src);
                Program.Pause();
                return;
            }
            Program.Info("源文件: " + src + "  (" + fi.Length + " 字节, " + fi.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss") + ")");

            // 3) 挂载 ESP
            EspHelper.MountInfo mi = EspHelper.Mount();
            if (mi == null) { Program.Pause(); return; }
            try
            {
                string dir = mi.MountPoint + @"EFI\BOOT";
                if (!Directory.Exists(dir))
                {
                    Directory.CreateDirectory(dir);
                    Program.Info("已创建 " + dir);
                }

                string dst = dir + @"\BOOTX64.efi";
                if (File.Exists(dst))
                {
                    string bak = dir + @"\BOOTX64.efi.bak";
                    File.Copy(dst, bak, true);
                    Program.Info("已备份旧文件为 " + bak);
                }

                File.Copy(src, dst, true);
                if (new FileInfo(dst).Length != fi.Length)
                    Program.Error("复制后大小不一致，部署可能失败！");
                else
                    Program.Info("部署成功: " + dst);

                // 顺带同步 version.txt
                string verSrc = Path.Combine(Program.BaseDir, "version.txt");
                if (File.Exists(verSrc))
                {
                    File.Copy(verSrc, dir + @"\version.txt", true);
                    Program.Info("已同步 version.txt 到 ESP");
                }
            }
            catch (Exception ex)
            {
                Program.Error("部署失败: " + ex.Message);
            }
            finally
            {
                if (mi.SelfMounted) EspHelper.Unmount(mi.MountPoint);
            }
            Program.Pause();
        }

        // --------------------------------------------------------------
        private static string[] FindCandidates()
        {
            List<string> list = new List<string>();
            AddIfExists(list, Path.Combine(Program.BaseDir, "build", "BOOTX64.efi"));
            AddIfExists(list, Path.Combine(Program.BaseDir, "BOOTX64.efi"));
            AddIfExists(list, Path.Combine(Environment.CurrentDirectory, "BOOTX64.efi"));
            CollectRecursive(list, Program.BaseDir, 0, 3);

            // 去重（保留首次出现顺序）
            HashSet<string> seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            List<string> result = new List<string>();
            foreach (string p in list)
                if (seen.Add(p)) result.Add(p);
            return result.ToArray();
        }

        private static void AddIfExists(List<string> list, string path)
        {
            try
            {
                if (File.Exists(path)) list.Add(Path.GetFullPath(path));
            }
            catch { }
        }

        private static void CollectRecursive(List<string> list, string root, int depth, int maxDepth)
        {
            if (depth > maxDepth) return;
            string[] dirs;
            try { dirs = Directory.GetDirectories(root); }
            catch { return; }
            foreach (string d in dirs)
            {
                if (d.IndexOf("\\.git", StringComparison.OrdinalIgnoreCase) >= 0) continue;
                AddIfExists(list, Path.Combine(d, "BOOTX64.efi"));
                CollectRecursive(list, d, depth + 1, maxDepth);
            }
        }

        private static string Choose(string[] candidates)
        {
            Console.WriteLine("  找到以下 BOOTX64.efi 候选:");
            for (int i = 0; i < candidates.Length; i++)
            {
                FileInfo f = new FileInfo(candidates[i]);
                Console.WriteLine("   [{0}] {1}  ({2} 字节, {3})", i + 1, candidates[i], f.Length,
                    f.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss"));
            }
            Console.Write("  选择序号部署 (直接回车用第 1 个): ");
            string s = Console.ReadLine();
            int idx;
            if (int.TryParse(s, out idx) && idx >= 1 && idx <= candidates.Length)
                return candidates[idx - 1];
            return candidates[0];
        }
    }

    // ======================================================================
    // bcdedit 启动项管理
    // ======================================================================
    internal static class BcdHelper
    {
        private const string BootEntryName = "Custom Bootloader";
        private const string EfiPath = @"\EFI\BOOT\BOOTX64.efi";

        public static void SetFirstBoot()
        {
            Console.WriteLine();
            EspHelper.MountInfo mi = EspHelper.Mount();
            if (mi == null) { Program.Pause(); return; }
            try
            {
                string efiFile = mi.MountPoint + @"EFI\BOOT\BOOTX64.efi";
                if (!File.Exists(efiFile))
                {
                    Program.Error("ESP 中未找到 " + efiFile + "，请先执行 [7] 部署。");
                    Program.Pause();
                    return;
                }

                Program.Info("ESP 中 EFI\\BOOT 内容:");
                foreach (string f in Directory.GetFiles(mi.MountPoint + @"EFI\BOOT"))
                    Console.WriteLine("   " + Path.GetFileName(f));
                Console.WriteLine();

                string guid = FindEntryGuid();
                if (guid == null)
                {
                    Program.Info("未找到现有 '" + BootEntryName + "' 启动项，正在创建...");
                    guid = CreateEntry();
                }
                else
                {
                    Program.Info("找到现有 '" + BootEntryName + "' 启动项: " + guid);
                }

                if (guid == null)
                {
                    Program.Error("无法定位或创建启动项。");
                    Program.Pause();
                    return;
                }

                string dev = "partition=" + mi.MountPoint.TrimEnd('\\');
                Proc.Run("bcdedit", "/set " + guid + " device " + dev);
                Proc.Run("bcdedit", "/set " + guid + " path " + EfiPath);
                Proc.Run("bcdedit", "/set {fwbootmgr} displayorder " + guid + " /addfirst");
                Proc.Run("bcdedit", "/set {fwbootmgr} timeout 3");
                Program.Info("已设置 '" + BootEntryName + "' (" + guid + ") 为 UEFI 第一启动项。");

                Console.WriteLine();
                Console.WriteLine("  === 当前 firmware 启动项 ===");
                Console.WriteLine(Proc.Run("bcdedit", "/enum firmware"));
            }
            finally
            {
                if (mi.SelfMounted) EspHelper.Unmount(mi.MountPoint);
            }
            Program.Pause();
        }

        // --------------------------------------------------------------
        private static string FindEntryGuid()
        {
            string output = Proc.Run("bcdedit", "/enum firmware");
            if (output == null) return null;
            string current = null;
            foreach (string raw in output.Split('\n'))
            {
                string line = raw.Trim();
                Match mId = Regex.Match(line, @"^(?:identifier|标识符)\s+(\{[0-9a-fA-F-]{36}\})",
                    RegexOptions.IgnoreCase);
                if (mId.Success)
                {
                    current = mId.Groups[1].Value;
                    continue;
                }
                Match mDesc = Regex.Match(line, @"^(?:description|描述)\s+(.+)$", RegexOptions.IgnoreCase);
                if (mDesc.Success && current != null &&
                    mDesc.Groups[1].Value.IndexOf(BootEntryName, StringComparison.OrdinalIgnoreCase) >= 0)
                    return current;
            }
            return null;
        }

        private static string CreateEntry()
        {
            string output = Proc.Run("bcdedit", "/copy {bootmgr} /d \"" + BootEntryName + "\"");
            if (output == null) return null;
            Match m = Regex.Match(output, @"(\{[0-9a-fA-F-]{36}\})");
            if (!m.Success)
            {
                Program.Error("bcdedit /copy 输出解析失败: " + output);
                return null;
            }
            string guid = m.Groups[1].Value;
            // 清理从 {bootmgr} 继承的无关值
            foreach (string val in new[] { "default", "resumeobject", "displayorder", "toolsdisplayorder", "timeout" })
                Proc.Run("bcdedit", "/deletevalue " + guid + " " + val);
            return guid;
        }
    }

    // ======================================================================
    // 外部进程执行辅助
    // ======================================================================
    internal static class Proc
    {
        public static string Run(string file, string args)
        {
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo(file, args)
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                    StandardOutputEncoding = Encoding.Default,
                    StandardErrorEncoding = Encoding.Default
                };
                using (Process p = Process.Start(psi))
                {
                    string o = p.StandardOutput.ReadToEnd();
                    string e = p.StandardError.ReadToEnd();
                    p.WaitForExit();
                    return o + e;
                }
            }
            catch (Exception ex)
            {
                Program.Error("执行 " + file + " 失败: " + ex.Message);
                return null;
            }
        }
    }
}
