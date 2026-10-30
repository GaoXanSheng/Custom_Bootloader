// ============================================================================
// DbgTool — Custom Bootloader 部署与管理工具（主入口与菜单路由）
// ============================================================================
using System;
using System.Diagnostics;
using System.IO;
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
            Console.Title = "Custom Bootloader 部署与管理工具";

            bool noElevate = false;
            string command = null;
            string commandArg = null;

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
                else if (commandArg == null && !a.StartsWith("-"))
                {
                    commandArg = a;
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
                        psi.Arguments = command;
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

            // CLI 命令行模式
            if (!string.IsNullOrEmpty(command))
            {
                return RunCliCommand(command, commandArg);
            }

            // 交互式菜单模式
            MenuLoop();
            return 0;
        }

        private static void PrintUsage()
        {
            Console.WriteLine("Custom Bootloader 部署与管理工具 (构建: " + BuildInfo.BuildStamp + ")");
            Console.WriteLine("用法: DbgTool.exe [选项] [命令]");
            Console.WriteLine();
            Console.WriteLine("选项:");
            Console.WriteLine("  --no-elevate    跳过管理员提权请求");
            Console.WriteLine("  --help, -h      显示本帮助信息");
            Console.WriteLine();
            Console.WriteLine("支持的命令:");
            Console.WriteLine("  deploy          部署 build\\BOOTX64.efi 到 ESP 分区");
            Console.WriteLine("  boot            设置 Custom Bootloader 为固件第一启动项");
            Console.WriteLine("  log             查看 ESP 日志与版本 (unlock.log / version.txt)");
            Console.WriteLine("  acpi            比对 Windows 当前 ACPI 表与 build\\Custom 补丁表");
            Console.WriteLine("  nvassets [dir]  部署 NVIDIA 解锁资产到 ESP 根目录（默认 usb_ready 目录）");
            Console.WriteLine("  uninstall       卸载 Custom Bootloader (清理 BCD 项并还原系统引导)");
        }

        private static int RunCliCommand(string cmd, string arg)
        {
            switch (cmd.ToLowerInvariant())
            {
                case "deploy":
                    return EspManager.Deploy() ? 0 : 1;
                case "boot":
                    return BcdManager.SetFirstBoot() ? 0 : 1;
                case "log":
                    EspManager.ViewLog();
                    return 0;
                case "acpi":
                    AcpiInspector.Run();
                    return 0;
                case "nvassets":
                    return NvAssetManager.Deploy(arg) ? 0 : 1;
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
                Console.WriteLine("  [1] 部署 BOOTX64.efi 到 ESP 分区 (deploy)");
                Console.WriteLine("  [2] 设置 Custom Bootloader 为第一启动项 (boot)");
                Console.WriteLine("  [3] 查看 ESP 日志与版本 (log: unlock.log / version.txt)");
                Console.WriteLine("  [4] 比对 Windows ACPI 表与补丁表 (acpi)");
                Console.WriteLine("  [5] 部署 NVIDIA 解锁资产到 ESP 根目录 (nvassets)");
                Console.WriteLine("  [6] 卸载 Custom Bootloader (uninstall: 还原系统引导)");
                Console.WriteLine("  [0] 退出");
                Console.WriteLine();
                Console.Write("  请选择操作: ");

                string choice = Console.ReadLine();
                if (choice == null || choice == "0" ||
                    choice.Equals("q", StringComparison.OrdinalIgnoreCase) ||
                    choice.Equals("exit", StringComparison.OrdinalIgnoreCase))
                {
                    return;
                }

                switch (choice.ToUpperInvariant())
                {
                    case "1":
                    case "DEPLOY":
                        EspManager.Deploy();
                        break;
                    case "2":
                    case "BOOT":
                        BcdManager.SetFirstBoot();
                        break;
                    case "3":
                    case "LOG":
                        EspManager.ViewLog();
                        break;
                    case "4":
                    case "ACPI":
                        AcpiInspector.Run();
                        break;
                    case "5":
                    case "NVASSETS":
                        Console.Write("  资产源目录（回车使用默认 " + NvAssetManager.DefaultSourceDir + "）: ");
                        string assetDir = Console.ReadLine();
                        if (string.IsNullOrWhiteSpace(assetDir)) assetDir = NvAssetManager.DefaultSourceDir;
                        NvAssetManager.Deploy(assetDir);
                        break;
                    case "6":
                    case "UNINSTALL":
                    case "U":
                        BcdManager.Uninstall();
                        break;
                    default:
                        ConsoleUI.Warn("无效输入，请重新选择。");
                        break;
                }

                ConsoleUI.Pause();
            }
        }

        private static void PrintBanner()
        {
            ConsoleUI.WriteColored("==========================================================", ConsoleColor.Cyan);
            ConsoleUI.WriteColored("           Custom Bootloader 部署与管理工具", ConsoleColor.Cyan);
            ConsoleUI.WriteColored("           构建版本时间戳: " + BuildInfo.BuildStamp, ConsoleColor.Cyan);
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
