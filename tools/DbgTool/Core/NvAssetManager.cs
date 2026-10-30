using System;
using System.Collections.Generic;
using System.IO;
using DbgTool.Common;

namespace DbgTool.Core
{
    internal static class NvAssetManager
    {
        public static string DefaultSourceDir
        {
            get
            {
                string local = Path.Combine(PathResolver.ProjectRoot, "nv-assets");
                if (Directory.Exists(local)) return local;
                return Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                    "Downloads", "nv-uefi-unlocker-v2.1.5", "usb_ready");
            }
        }

        private static readonly string[] AssetFiles = {
            "booter_load_ad102.bin",
            "gsp_boot_desc_ad102.bin",
            "gsp_boot_ad102.bin",
            "fwimage_ad102.bin",
            "fwsignature_ad102.bin",
            "scrubber_ad10x.bin",
            "v67-ad102-four-plm.bin",
        };

        public static bool Deploy(string sourceDir)
        {
            ConsoleUI.Header("部署 NVIDIA 解锁资产到 ESP 根目录");
            if (string.IsNullOrWhiteSpace(sourceDir)) sourceDir = DefaultSourceDir;
            if (!Directory.Exists(sourceDir))
            {
                ConsoleUI.Error("资产目录不存在: " + sourceDir);
                return false;
            }

            List<string> missing = new List<string>();
            long total = 0;
            foreach (string name in AssetFiles)
            {
                string path = Path.Combine(sourceDir, name);
                if (!File.Exists(path)) missing.Add(name);
                else total += new FileInfo(path).Length;
            }
            if (missing.Count > 0)
            {
                ConsoleUI.Error("资产不完整，缺少: " + string.Join(", ", missing.ToArray()));
                return false;
            }

            ConsoleUI.Info("源目录: " + sourceDir);
            ConsoleUI.Info("待写入: " + AssetFiles.Length + " 个文件, " + (total / 1048576) + " MB");

            using (EspMount esp = EspManager.Mount())
            {
                if (esp == null) return false;

                DriveInfo di = new DriveInfo(esp.MountPoint);
                long free = di.AvailableFreeSpace;
                ConsoleUI.Info("ESP 可用空间: " + free + " 字节");
                if (free < total)
                {
                    foreach (string rel in new string[] { @"EFI\BOOT\BOOTX64.efi.bak", @"EFI\BOOT\unlock.log" })
                    {
                        string junk = Path.Combine(esp.MountPoint, rel);
                        if (File.Exists(junk))
                        {
                            long junkLen = new FileInfo(junk).Length;
                            File.Delete(junk);
                            free += junkLen;
                            ConsoleUI.Info("已删除 " + rel + " (" + junkLen + " 字节) 腾出空间");
                        }
                    }
                }
                if (free < total)
                {
                    ConsoleUI.Error("ESP 空间不足，至少还需要 " + (total - free) + " 字节");
                    return false;
                }

                foreach (string name in AssetFiles)
                {
                    string src = Path.Combine(sourceDir, name);
                    string dst = Path.Combine(esp.MountPoint, name);
                    File.Copy(src, dst, true);
                    long s = new FileInfo(src).Length;
                    long d = new FileInfo(dst).Length;
                    if (s != d)
                    {
                        ConsoleUI.Error("写入校验失败: \\" + name);
                        return false;
                    }
                    ConsoleUI.Info("已写入 \\" + name + " (" + d + " 字节)");
                }

                ConsoleUI.Info("完成。重启进入 Custom Bootloader 后，TX 链会自动读取这些资产。");
                return true;
            }
        }
    }
}
