// ============================================================================
// DbgTool — BCD 固件引导项管理器
// 基于 ExitCode 与输出特征双重判定，彻底兼容多语言环境与不同固件 NVRAM 策略
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

            // 特例容错：某些删除指令在目标本就不存在时也视为 OK
            string combined = res.CombinedOutput;
            if (args.Contains("/delete") &&
                (combined.Contains("找不到") || combined.Contains("not found") || combined.Contains("元素")))
            {
                ConsoleUI.Info(actionDesc + " ... 本就不存在，无需清理");
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
            ConsoleUI.Header("设置 Custom Bootloader 为第一启动项");

            using (EspMount esp = EspManager.Mount())
            {
                if (esp == null) return false;

                string efiFile = Path.Combine(esp.MountPoint, "EFI", "BOOT", "BOOTX64.efi");
                if (!File.Exists(efiFile))
                {
                    ConsoleUI.Error("ESP 中未找到 " + efiFile + "，请先执行部署。");
                    return false;
                }

                // 1) 清理现有重复项
                List<string> oldGuids = FindEntryGuids(BootEntryName);
                if (oldGuids.Count > 0)
                {
                    ConsoleUI.Info("发现 " + oldGuids.Count + " 个旧 '" + BootEntryName + "' 项，正在清理...");
                    foreach (string g in oldGuids)
                    {
                        RunBcd("/delete " + g + " /f", "清理旧项 " + g);
                    }
                }

                // 2) 创建全新启动项
                string guid = CreateEntry();
                if (string.IsNullOrEmpty(guid))
                {
                    ConsoleUI.Error("创建 BCD 启动项失败。");
                    return false;
                }
                ConsoleUI.Info("创建新启动项成功: " + guid);

                // 3) 设置设备与路径
                string dev = "partition=" + esp.MountPoint.TrimEnd('\\');
                bool ok = RunBcd("/set " + guid + " device " + dev, "设置引导设备 " + dev);
                ok &= RunBcd("/set " + guid + " path " + EfiRelativePath, "设置引导程序路径 " + EfiRelativePath);

                // 4) 置顶
                bool orderOk = RunBcd("/set {fwbootmgr} displayorder " + guid + " /addfirst", "置顶到 {fwbootmgr} displayorder");
                RunBcd("/set {fwbootmgr} timeout 3", "设置固件菜单等待超时 3s");

                // 5) 复查
                string first = GetFwbootmgrFirst();
                if (orderOk && !string.IsNullOrEmpty(first) && first.Equals(guid, StringComparison.OrdinalIgnoreCase))
                {
                    ConsoleUI.Info("验证通过: '" + BootEntryName + "' (" + guid + ") 已成功成为固件第一启动项！");
                }
                else
                {
                    ConsoleUI.Warn("验证告警: {fwbootmgr} 首位为 " + (first ?? "未知") + "，可能被主板固件重排。");
                    if (RunBcd("/set {fwbootmgr} bootsequence " + guid, "启用一次性 bootsequence 引导兜底"))
                    {
                        ConsoleUI.Info("已设置一次性引导 bootsequence: 下次重启将自动引导 Custom Bootloader。");
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
            Console.WriteLine("    1. 清理 BCD 中所有 'Custom Bootloader' 启动项");
            Console.WriteLine("    2. 清除一次性 bootsequence 并确保 Windows Boot Manager 恢复第一位");
            Console.WriteLine("    3. 清理 ESP 分区 EFI\\BOOT 下的 bootloader 注入文件（若有备份则恢复）");
            Console.WriteLine();

            if (!ConsoleUI.Confirm("确认执行卸载？", false))
            {
                ConsoleUI.Warn("操作已取消。");
                return false;
            }

            // 1. 删除启动项
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

            // 清理 bootsequence
            ProcessRunner.Run("bcdedit", "/deletevalue {fwbootmgr} bootsequence");

            // 2. 恢复 Windows Boot Manager 首位
            string winGuid = FindWindowsBootMgrGuid();
            if (!string.IsNullOrEmpty(winGuid))
            {
                RunBcd("/set {fwbootmgr} displayorder " + winGuid + " /addfirst", "恢复 Windows Boot Manager 置顶");
            }
            else
            {
                ConsoleUI.Warn("未检测到 Windows Boot Manager GUID，请进入 BIOS 确认启动项顺序。");
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

                    if (Directory.Exists(bootDir) && Directory.GetFiles(bootDir).Length == 0)
                    {
                        Directory.Delete(bootDir);
                        ConsoleUI.Info("EFI\\BOOT 为空，已移除目录。");
                    }
                    ConsoleUI.Info("Custom Bootloader 卸载完成，下次启动将由原生固件与 Windows 引导。");
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
            ProcessResult res = ProcessRunner.Run("bcdedit", "/copy {bootmgr} /d \"" + BootEntryName + "\"");
            if (!res.Success) return null;

            Match m = Regex.Match(res.CombinedOutput, @"(\{[0-9a-fA-F-]{36}\})");
            if (!m.Success) return null;

            string guid = m.Groups[1].Value;
            // 清理多余属性
            string[] cleanup = { "default", "resumeobject", "displayorder", "toolsdisplayorder", "timeout" };
            foreach (string prop in cleanup)
            {
                ProcessRunner.Run("bcdedit", "/deletevalue " + guid + " " + prop);
            }
            return guid;
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
