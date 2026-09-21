// ============================================================================
// DbgTool — BCD 固件引导项管理器
// 使用 /application bootfw 原生创建 UEFI 固件启动项，保证固件启动顺序生效
// ============================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using DbgTool.Common;

namespace DbgTool.Core
{
    internal static class BcdManager
    {
        public const string BootEntryName = "Custom Bootloader";
        public const string EfiRelativePath = @"\EFI\BOOT\BOOTX64.efi";

        private static bool RunBcd(string args, string actionDesc)
        {
            ProcessResult res = ProcessRunner.Run("bcdedit", args);
            if (res.Success)
            {
                ConsoleUI.Info(actionDesc + " ... 成功");
                return true;
            }

            // 特例容错：删除不存在的项视为成功
            string combined = res.CombinedOutput;
            if (args.Contains("/delete") &&
                (combined.Contains("找不到") || combined.Contains("not found") || combined.Contains("元素") || combined.Contains("element")))
            {
                ConsoleUI.Info(actionDesc + " ... 项不存在，无需清理");
                return true;
            }

            ConsoleUI.Error(actionDesc + " 失败 (ExitCode: " + res.ExitCode + "):");
            if (!string.IsNullOrEmpty(res.StandardError))
            {
                Console.WriteLine("    [stderr] " + res.StandardError.Trim());
            }
            if (!string.IsNullOrEmpty(res.StandardOutput))
            {
                Console.WriteLine("    [stdout] " + res.StandardOutput.Trim());
            }
            return false;
        }

        public static bool SetFirstBoot()
        {
            ConsoleUI.Header("设置 Custom Bootloader 为 UEFI 第一启动项");

            using (EspMount esp = EspManager.Mount())
            {
                if (esp == null) return false;

                string efiFile = Path.Combine(esp.MountPoint, "EFI", "BOOT", "BOOTX64.efi");
                if (!File.Exists(efiFile))
                {
                    ConsoleUI.Error("ESP 中未找到 " + efiFile + "，请先执行部署操作。");
                    return false;
                }

                // 1. 清理现有重复或失效的 Custom Bootloader 项
                List<string> oldGuids = FindEntryGuids(BootEntryName);
                if (oldGuids.Count > 0)
                {
                    ConsoleUI.Info("检测到 " + oldGuids.Count + " 个旧 '" + BootEntryName + "' 项，正在清理...");
                    foreach (string g in oldGuids)
                    {
                        RunBcd("/delete " + g + " /f", "清理旧项 " + g);
                    }
                }

                // 2. 创建 UEFI 固件引导程序类型的全新启动项 (/application bootfw)
                string guid = CreateEntry();
                if (string.IsNullOrEmpty(guid))
                {
                    ConsoleUI.Error("创建 BCD 固件启动项失败。");
                    return false;
                }
                ConsoleUI.Info("创建新固件启动项成功: " + guid);

                // 3. 设置设备与相对路径
                string dev = "partition=" + esp.MountPoint.TrimEnd('\\');
                bool ok = RunBcd("/set " + guid + " device " + dev, "设置引导分区 " + dev);
                ok &= RunBcd("/set " + guid + " path " + EfiRelativePath, "设置 EFI 路径 " + EfiRelativePath);

                // 4. 置顶到固件启动序列
                bool orderOk = RunBcd("/set {fwbootmgr} displayorder " + guid + " /addfirst", "置顶到 {fwbootmgr} displayorder");
                RunBcd("/set {fwbootmgr} timeout 3", "设置固件菜单超时 3s");

                // 5. 校验置顶状态
                string first = GetFwbootmgrFirst();
                if (orderOk && !string.IsNullOrEmpty(first) && first.Equals(guid, StringComparison.OrdinalIgnoreCase))
                {
                    ConsoleUI.Info("校验通过: '" + BootEntryName + "' (" + guid + ") 已成功成为固件第一启动项！");
                }
                else
                {
                    ConsoleUI.Warn("提示: 固件当前首选启动项为 " + (first ?? "未知") + "，可能被主板 BIOS 锁序。");
                    if (RunBcd("/set {fwbootmgr} bootsequence " + guid, "设置一次性 bootsequence 兜底"))
                    {
                        ConsoleUI.Info("已设置一次性引导 bootsequence: 下次重启将由 Custom Bootloader 引导。");
                    }
                }

                Console.WriteLine();
                ConsoleUI.WriteColored("=== 当前固件启动项列表 ===", ConsoleColor.Cyan);
                ProcessResult listRes = ProcessRunner.Run("bcdedit", "/enum firmware");
                Console.WriteLine(listRes.CombinedOutput);
                return ok;
            }
        }

        public static bool Uninstall()
        {
            ConsoleUI.Header("卸载 Custom Bootloader (还原系统引导)");

            Console.WriteLine("  操作内容:");
            Console.WriteLine("    1. 清除 BCD 中所有 'Custom Bootloader' 启动项与 bootsequence");
            Console.WriteLine("    2. 将 Windows Boot Manager 恢复为固件第一启动项");
            Console.WriteLine("    3. 清理 ESP 分区中的引导文件（若有备份则还原原厂 BOOTX64.efi）");
            Console.WriteLine();

            if (!ConsoleUI.Confirm("确认执行卸载？", false))
            {
                ConsoleUI.Warn("操作已取消。");
                return false;
            }

            // 1. 删除 BCD 启动项
            List<string> guids = FindEntryGuids(BootEntryName);
            if (guids.Count == 0)
            {
                ConsoleUI.Info("BCD 中未发现 '" + BootEntryName + "' 启动项。");
            }
            else
            {
                foreach (string g in guids)
                {
                    RunBcd("/delete " + g + " /f", "删除启动项 " + g);
                }
            }

            ProcessRunner.Run("bcdedit", "/deletevalue {fwbootmgr} bootsequence");

            // 2. 恢复 Windows Boot Manager 置顶
            string winGuid = FindWindowsBootMgrGuid();
            if (!string.IsNullOrEmpty(winGuid))
            {
                RunBcd("/set {fwbootmgr} displayorder " + winGuid + " /addfirst", "恢复 Windows Boot Manager 置顶");
            }
            else
            {
                ConsoleUI.Warn("未检索到 Windows Boot Manager GUID，请进入 BIOS 确认启动项顺序。");
            }

            // 3. 清理 ESP 文件
            using (EspMount esp = EspManager.Mount())
            {
                if (esp == null) return false;

                try
                {
                    string bootDir = Path.Combine(esp.MountPoint, "EFI", "BOOT");
                    string mainEfi = Path.Combine(bootDir, "BOOTX64.efi");
                    string bakEfi = Path.Combine(bootDir, "BOOTX64.efi.bak");

                    if (File.Exists(mainEfi))
                    {
                        if (File.Exists(bakEfi))
                        {
                            File.Copy(bakEfi, mainEfi, true);
                            File.Delete(bakEfi);
                            ConsoleUI.Info("已从备份还原原始 BOOTX64.efi");
                        }
                        else
                        {
                            File.Delete(mainEfi);
                            ConsoleUI.Info("已删除 BOOTX64.efi");
                        }
                    }
                    else if (File.Exists(bakEfi))
                    {
                        File.Move(bakEfi, mainEfi);
                        ConsoleUI.Info("已从备份恢复原始 BOOTX64.efi");
                    }

                    string[] auxFiles = { "version.txt", "unlock.log" };
                    foreach (string f in auxFiles)
                    {
                        string p = Path.Combine(bootDir, f);
                        if (File.Exists(p))
                        {
                            File.Delete(p);
                            ConsoleUI.Info("已清理临时文件: " + f);
                        }
                    }

                    ConsoleUI.Info("Custom Bootloader 卸载完成，下次启动将由原生固件与 Windows 正常引导。");
                    return true;
                }
                catch (Exception ex)
                {
                    ConsoleUI.Error("清理 ESP 文件失败: " + ex.Message);
                    return false;
                }
            }
        }

        private static string CreateEntry()
        {
            // /application bootfw 创建 UEFI 固件引导项
            ProcessResult res = ProcessRunner.Run("bcdedit", "/create /d \"" + BootEntryName + "\" /application bootfw");
            if (!res.Success)
            {
                // 回退：直接使用 /create
                res = ProcessRunner.Run("bcdedit", "/create /d \"" + BootEntryName + "\"");
                if (!res.Success) return null;
            }

            Match m = Regex.Match(res.CombinedOutput, @"(\{[0-9a-fA-F-]{36}\})");
            if (!m.Success) return null;

            return m.Groups[1].Value;
        }

        private static List<string> FindEntryGuids(string nameFilter)
        {
            var result = new List<string>();
            ProcessResult res = ProcessRunner.Run("bcdedit", "/enum firmware");
            if (!res.Success) return result;

            string currentId = null;
            string[] lines = res.CombinedOutput.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

            foreach (string raw in lines)
            {
                string line = raw.Trim();
                Match mId = Regex.Match(line, @"^(?:identifier|标识符)\s+(\{[0-9a-fA-F-]{36}\})", RegexOptions.IgnoreCase);
                if (mId.Success)
                {
                    currentId = mId.Groups[1].Value;
                    continue;
                }

                Match mDesc = Regex.Match(line, @"^(?:description|描述)\s+(.+)$", RegexOptions.IgnoreCase);
                if (mDesc.Success && currentId != null)
                {
                    if (mDesc.Groups[1].Value.IndexOf(nameFilter, StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        if (!result.Contains(currentId)) result.Add(currentId);
                    }
                }
            }
            return result;
        }

        private static string FindWindowsBootMgrGuid()
        {
            ProcessResult res = ProcessRunner.Run("bcdedit", "/enum firmware");
            if (!res.Success) return null;

            string currentId = null;
            string[] lines = res.CombinedOutput.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

            foreach (string raw in lines)
            {
                string line = raw.Trim();
                Match mId = Regex.Match(line, @"^(?:identifier|标识符)\s+(\{[0-9a-fA-F-]{36}\})", RegexOptions.IgnoreCase);
                if (mId.Success)
                {
                    currentId = mId.Groups[1].Value;
                    continue;
                }

                Match mDesc = Regex.Match(line, @"^(?:description|描述)\s+(.+)$", RegexOptions.IgnoreCase);
                if (mDesc.Success && currentId != null)
                {
                    string desc = mDesc.Groups[1].Value;
                    if (desc.IndexOf("Windows Boot Manager", StringComparison.OrdinalIgnoreCase) >= 0 ||
                        desc.IndexOf("Windows 启动管理器", StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        return currentId;
                    }
                }
            }
            return null;
        }

        private static string GetFwbootmgrFirst()
        {
            ProcessResult res = ProcessRunner.Run("bcdedit", "/enum firmware");
            if (!res.Success) return null;

            bool inDisplayOrder = false;
            string[] lines = res.CombinedOutput.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

            foreach (string raw in lines)
            {
                string line = raw.Trim();
                Match mOrder = Regex.Match(line, @"^displayorder\s+(.*)$", RegexOptions.IgnoreCase);
                if (mOrder.Success)
                {
                    inDisplayOrder = true;
                    string rest = mOrder.Groups[1].Value.Trim();
                    Match first = Regex.Match(rest, @"\{[0-9a-fA-F-]{36}\}");
                    if (first.Success) return first.Value;
                    continue;
                }
                if (inDisplayOrder)
                {
                    Match cont = Regex.Match(line, @"^\{[0-9a-fA-F-]{36}\}$");
                    if (cont.Success) return cont.Value;
                    if (line.Length > 0) inDisplayOrder = false;
                }
            }
            return null;
        }
    }
}
