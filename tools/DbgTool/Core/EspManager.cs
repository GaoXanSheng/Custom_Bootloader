// ============================================================================
// DbgTool — ESP 分区与 EFI 部署管理器
// 基于 Windows 原生 mountvol /S 挂载与 /D 卸载，支持 SHA256 完整性核验
// ============================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using DbgTool.Common;

namespace DbgTool.Core
{
    internal sealed class EspMount : IDisposable
    {
        public string MountPoint { get; private set; }
        public bool SelfMounted { get; private set; }
        private bool _disposed;

        public EspMount(string mountPoint, bool selfMounted)
        {
            MountPoint = mountPoint;
            SelfMounted = selfMounted;
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                if (SelfMounted && !string.IsNullOrEmpty(MountPoint))
                {
                    EspManager.Unmount(MountPoint);
                }
                _disposed = true;
            }
        }
    }

    internal static class EspManager
    {
        public static EspMount Mount()
        {
            // 1. 检查是否已有挂载系统引导文件的盘符
            string existing = FindExistingMountedEsp();
            if (existing != null)
            {
                ConsoleUI.Info("复用已挂载的 ESP 分区: " + existing);
                return new EspMount(existing, false);
            }

            // 2. 获取可用空闲盘符
            char freeLetter = FindFreeDriveLetter();
            if (freeLetter == '\0')
            {
                ConsoleUI.Error("系统无可用空闲盘符供 ESP 挂载。");
                return null;
            }

            string drive = freeLetter + ":";
            ConsoleUI.Info("正在通过 mountvol " + drive + " /S 挂载 EFI 系统分区...");
            ProcessResult res = ProcessRunner.Run("mountvol", drive + " /S");
            if (!res.Success)
            {
                ConsoleUI.Error("mountvol 挂载失败: " + res.CombinedOutput);
                return null;
            }

            string mountPoint = drive + "\\";
            if (!Directory.Exists(Path.Combine(mountPoint, "EFI")))
            {
                ConsoleUI.Error("挂载点 " + mountPoint + " 下未检测到合法的 EFI 目录，正在解除挂载...");
                Unmount(drive);
                return null;
            }

            ConsoleUI.Info("成功挂载系统 ESP 分区到: " + mountPoint);
            return new EspMount(mountPoint, true);
        }

        public static void Unmount(string driveOrMountPoint)
        {
            if (string.IsNullOrEmpty(driveOrMountPoint)) return;
            string drive = driveOrMountPoint.TrimEnd('\\');
            if (!drive.EndsWith(":")) drive += ":";

            ProcessResult res = ProcessRunner.Run("mountvol", drive + " /D");
            if (res.Success)
            {
                ConsoleUI.Info("已安全卸载 ESP 挂载点: " + drive);
            }
            else
            {
                ConsoleUI.Warn("卸载 ESP 挂载点 " + drive + " 提示: " + res.CombinedOutput.Trim());
            }
        }

        public static bool Deploy()
        {
            ConsoleUI.Header("部署 BOOTX64.efi 到 ESP 分区");

            // 1. 查找源文件
            string src = PathResolver.FindEfiFile();
            if (src == null || !File.Exists(src))
            {
                ConsoleUI.Error("未在常规路径下找到构建好的 BOOTX64.efi（请先运行 build.bat）。");
                Console.Write("  请输入 EFI 文件完整路径（直接回车取消）: ");
                string manual = Console.ReadLine();
                if (!string.IsNullOrEmpty(manual) && File.Exists(manual.Trim()))
                {
                    src = Path.GetFullPath(manual.Trim());
                }
                else
                {
                    return false;
                }
            }

            var srcInfo = new FileInfo(src);
            if (srcInfo.Length == 0)
            {
                ConsoleUI.Error("源文件大小为 0 字节，可能损坏: " + src);
                return false;
            }

            ConsoleUI.Info("选定源文件: " + src + " (" + srcInfo.Length + " 字节)");
            string srcSha = ComputeSha256(src);
            ConsoleUI.Info("源文件 SHA256: " + srcSha);

            // 2. 挂载 ESP 并写入
            using (EspMount esp = Mount())
            {
                if (esp == null) return false;

                try
                {
                    string targetDir = Path.Combine(esp.MountPoint, "EFI", "BOOT");
                    if (!Directory.Exists(targetDir))
                    {
                        Directory.CreateDirectory(targetDir);
                        ConsoleUI.Info("创建目标目录: " + targetDir);
                    }

                    string dst = Path.Combine(targetDir, "BOOTX64.efi");
                    if (File.Exists(dst))
                    {
                        string bak = Path.Combine(targetDir, "BOOTX64.efi.bak");
                        File.Copy(dst, bak, true);
                        ConsoleUI.Info("已备份现有引导文件为: " + bak);
                    }

                    File.Copy(src, dst, true);

                    // SHA256 校验
                    string dstSha = ComputeSha256(dst);
                    ConsoleUI.Info("ESP 目标 SHA256: " + dstSha);
                    if (!string.Equals(srcSha, dstSha, StringComparison.OrdinalIgnoreCase))
                    {
                        ConsoleUI.Error("部署后校验失败！ESP 上文件哈希与源文件不匹配！");
                        return false;
                    }
                    ConsoleUI.Info("SHA256 校验通过，BOOTX64.efi 部署成功！");

                    // 同步 version.txt
                    string verSrc = PathResolver.FindVersionFile();
                    if (verSrc != null && File.Exists(verSrc))
                    {
                        string verDst = Path.Combine(targetDir, "version.txt");
                        File.Copy(verSrc, verDst, true);
                        ConsoleUI.Info("已同步 version.txt 到 ESP");
                    }
                    else
                    {
                        ConsoleUI.Warn("未找到 version.txt（跳过版本文件同步）");
                    }

                    return true;
                }
                catch (Exception ex)
                {
                    ConsoleUI.Error("部署失败: " + ex.Message);
                    return false;
                }
            }
        }

        public static void ViewLog()
        {
            ConsoleUI.Header("ESP 日志与版本查看");
            using (EspMount esp = Mount())
            {
                if (esp == null) return;

                string verPath = Path.Combine(esp.MountPoint, "EFI", "BOOT", "version.txt");
                string logPath = Path.Combine(esp.MountPoint, "EFI", "BOOT", "unlock.log");

                if (File.Exists(verPath))
                {
                    Console.WriteLine();
                    ConsoleUI.WriteColored("--- version.txt (" + verPath + ") ---", ConsoleColor.Cyan);
                    Console.WriteLine(File.ReadAllText(verPath).Trim());
                }
                else
                {
                    ConsoleUI.Warn("ESP 中未找到 EFI\\BOOT\\version.txt");
                }

                if (File.Exists(logPath))
                {
                    Console.WriteLine();
                    ConsoleUI.WriteColored("--- unlock.log (" + logPath + ") ---", ConsoleColor.Cyan);
                    Console.WriteLine(File.ReadAllText(logPath).Trim());
                }
                else
                {
                    ConsoleUI.Warn("ESP 中未找到 EFI\\BOOT\\unlock.log (请先通过 Custom Bootloader 引导一次系统)");
                }
            }
        }

        public static string ComputeSha256(string filePath)
        {
            using (var sha = SHA256.Create())
            using (FileStream fs = File.OpenRead(filePath))
            {
                byte[] hash = sha.ComputeHash(fs);
                var sb = new StringBuilder(hash.Length * 2);
                foreach (byte b in hash)
                {
                    sb.Append(b.ToString("x2"));
                }
                return sb.ToString();
            }
        }

        private static string FindExistingMountedEsp()
        {
            try
            {
                string[] drives = Directory.GetLogicalDrives();
                foreach (string d in drives)
                {
                    string bootmgfw = Path.Combine(d, "EFI", "Microsoft", "Boot", "bootmgfw.efi");
                    if (File.Exists(bootmgfw) && !Directory.Exists(Path.Combine(d, "Windows")))
                    {
                        return d.TrimEnd('\\') + "\\";
                    }
                }
            }
            catch { }
            return null;
        }

        private static char FindFreeDriveLetter()
        {
            var taken = new HashSet<char>();
            try
            {
                foreach (string d in Directory.GetLogicalDrives())
                {
                    if (!string.IsNullOrEmpty(d))
                    {
                        taken.Add(char.ToUpperInvariant(d[0]));
                    }
                }
            }
            catch { }

            if (!taken.Contains('S')) return 'S';
            for (char c = 'T'; c <= 'Z'; c++)
            {
                if (!taken.Contains(c)) return c;
            }
            for (char c = 'R'; c >= 'D'; c--)
            {
                if (!taken.Contains(c)) return c;
            }
            return '\0';
        }
    }
}
