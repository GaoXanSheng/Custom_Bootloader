// ============================================================================
// DbgTool — ESP 分区与 EFI 部署管理器
// 支持自动卸载、无泄漏探测、SHA256 完整性核验与双向同步
// ============================================================================
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Management;
using System.Runtime.InteropServices;
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

        public static EspMount Mount()
        {
            // 1) 优先查找已有盘符挂载的 ESP
            string existing = FindMountedEsp();
            if (existing != null)
            {
                ConsoleUI.Info("复用已挂载的 ESP 分区: " + existing);
                return new EspMount(existing, false);
            }

            // 2) 枚举无挂载点的 FAT 卷
            List<string> vols = EnumerateFatVolumesWithoutMount();
            if (vols.Count == 0)
            {
                ConsoleUI.Error("未找到可挂载的 FAT/FAT32 卷。");
                ConsoleUI.Warn("可尝试手动在管理员终端运行: mountvol S: /S，然后重新运行本工具。");
                return null;
            }

            // 3) 逐一测试候选卷，若不是 ESP 立即卸载，严禁盘符残留！
            foreach (string vol in vols)
            {
                string mp = TryMount(vol);
                if (mp == null) continue;

                if (Directory.Exists(Path.Combine(mp, "EFI")))
                {
                    ConsoleUI.Info("已挂载系统 ESP 分区到: " + mp);
                    return new EspMount(mp, true);
                }

                // 不是 ESP，立即解除挂载，防止盘符泄漏！
                DeleteVolumeMountPoint(mp);
            }

            ConsoleUI.Error("未能在候选卷中识别出合法的 EFI 系统分区。");
            return null;
        }

        public static void Unmount(string mountPoint)
        {
            if (string.IsNullOrEmpty(mountPoint)) return;
            if (!DeleteVolumeMountPoint(mountPoint))
            {
                int err = Marshal.GetLastWin32Error();
                ConsoleUI.Warn("卸载 " + mountPoint + " 失败: " + new Win32Exception(err).Message);
            }
            else
            {
                ConsoleUI.Info("已安全卸载 ESP 挂载点: " + mountPoint);
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
                    ConsoleUI.Warn("ESP 中未找到 EFI\\BOOT\\unlock.log");
                }
            }
        }

        public static bool Deploy()
        {
            ConsoleUI.Header("部署 BOOTX64.efi 到 ESP 分区");

            // 1. 查找源文件
            string[] candidates = PathResolver.FindEfiCandidates();
            string src = null;

            if (candidates.Length == 0)
            {
                ConsoleUI.Error("未在常见目录找到构建好的 BOOTX64.efi。");
                Console.Write("  请输入 EFI 文件完整路径（留空取消）: ");
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
            else if (candidates.Length == 1)
            {
                src = candidates[0];
            }
            else
            {
                Console.WriteLine("  找到以下 BOOTX64.efi 构建产物:");
                for (int i = 0; i < candidates.Length; i++)
                {
                    var fi = new FileInfo(candidates[i]);
                    Console.WriteLine("   [{0}] {1}  ({2} 字节, {3:yyyy-MM-dd HH:mm:ss})",
                        i + 1, candidates[i], fi.Length, fi.LastWriteTime);
                }
                int sel = ConsoleUI.ReadIntSafely("选择部署文件序号", 1, 1, candidates.Length);
                src = candidates[sel - 1];
            }

            var srcInfo = new FileInfo(src);
            if (srcInfo.Length == 0)
            {
                ConsoleUI.Error("源文件大小为 0 字节，可能损坏: " + src);
                return false;
            }

            ConsoleUI.Info("源文件: " + src + " (" + srcInfo.Length + " 字节)");
            string srcSha = ComputeSha256(src);
            ConsoleUI.Info("源 SHA256: " + srcSha);

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
                        ConsoleUI.Info("已备份现有文件为: " + bak);
                    }

                    File.Copy(src, dst, true);

                    // SHA256 校验
                    string dstSha = ComputeSha256(dst);
                    ConsoleUI.Info("ESP SHA256: " + dstSha);
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

        private static string FindMountedEsp()
        {
            for (char c = 'C'; c <= 'Z'; c++)
            {
                string mp = c + ":\\";
                try
                {
                    if (Directory.Exists(Path.Combine(mp, "EFI")) &&
                        !Directory.Exists(Path.Combine(mp, "Windows")))
                    {
                        return mp;
                    }
                }
                catch { }
            }
            return null;
        }

        private static List<string> EnumerateFatVolumesWithoutMount()
        {
            var result = new List<string>();

            try
            {
                using (var searcher = new ManagementObjectSearcher(@"\\.\root\cimv2",
                    "SELECT DeviceID, DriveLetter, FileSystem FROM Win32_Volume"))
                using (ManagementObjectCollection coll = searcher.Get())
                {
                    foreach (ManagementObject v in coll)
                    {
                        using (v)
                        {
                            string fs = Convert.ToString(v["FileSystem"] ?? string.Empty);
                            string dl = Convert.ToString(v["DriveLetter"] ?? string.Empty);
                            if (fs.StartsWith("FAT", StringComparison.OrdinalIgnoreCase) && dl.Length == 0)
                            {
                                result.Add(Convert.ToString(v["DeviceID"]));
                            }
                        }
                    }
                }
                if (result.Count > 0) return result;
            }
            catch { }

            // Win32 API 回退
            var buf = new StringBuilder(261);
            IntPtr h = FindFirstVolume(buf, buf.Capacity);
            if (h == INVALID_HANDLE_VALUE) return result;

            do
            {
                string vol = buf.ToString();
                var volName = new StringBuilder(64);
                var fs = new StringBuilder(32);
                uint serial, maxLen, flags;
                if (GetVolumeInformation(vol, volName, volName.Capacity, out serial, out maxLen, out flags, fs, fs.Capacity))
                {
                    if (fs.ToString().StartsWith("FAT", StringComparison.OrdinalIgnoreCase))
                    {
                        var paths = new StringBuilder(1024);
                        uint ret = 0;
                        if (GetVolumePathNamesForVolumeName(vol, paths, (uint)paths.Capacity, out ret) && paths.Length == 0)
                        {
                            result.Add(vol);
                        }
                    }
                }
            } while (FindNextVolume(h, buf, buf.Capacity));
            FindVolumeClose(h);
            return result;
        }

        private static string TryMount(string volumeDeviceId)
        {
            char d = FindFreeDriveLetter();
            if (d == '\0') return null;

            string mp = d + ":\\";
            if (!SetVolumeMountPoint(mp, volumeDeviceId))
            {
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
}
