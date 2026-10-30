using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using DbgTool.Common;
using Microsoft.Win32;

namespace DbgTool.Core
{
    internal static class AcpiInspector
    {
        private const string AcpiRoot = @"HARDWARE\ACPI";

        private sealed class Table
        {
            public string Name;
            public byte[] Data;
            public string Signature;
            public uint Length;
            public string OemId;
            public string OemTableId;
            public uint OemRevision;
            public string Sha256;
        }

        public static void Run()
        {
            ConsoleUI.Header("Windows ACPI 表与补丁表比对");

            List<Table> live = LoadLiveTables();
            if (live.Count == 0)
            {
                ConsoleUI.Error("未从注册表读取到任何 ACPI 表（HKLM\\HARDWARE\\ACPI 不可用）。");
                return;
            }

            List<Table> originals = LoadFiles(FindOriginalsDir(), "*.dat");
            string customDir = PathResolver.FindCustomDir();
            List<Table> custom = customDir == null
                ? new List<Table>()
                : LoadFiles(customDir, "*.aml");

            ConsoleUI.Info("Windows 本次启动实际使用的表:");
            foreach (Table t in live)
            {
                string origin = MatchName(originals, t.Sha256);
                string suffix = origin == null ? "" : "  [= tools\\acpi\\" + origin + "]";
                Console.WriteLine("    " + Pad(t.Signature, 4) + " len=" + t.Length.ToString().PadRight(7) +
                                  " oem=" + t.OemId + "/" + t.OemTableId +
                                  " rev=0x" + t.OemRevision.ToString("X8") +
                                  " sha=" + Short(t.Sha256) + suffix);
            }

            if (custom.Count == 0)
            {
                ConsoleUI.Warn("未找到 build\\Custom\\*.aml，请先运行 build.bat。");
                return;
            }

            ConsoleUI.Info("补丁表比对:");
            foreach (Table c in custom)
            {
                Table same = FindByHash(live, c.Sha256);
                if (same != null)
                {
                    ConsoleUI.Info(c.Name + " -> 已生效（OS 中可见同 hash 表）");
                    continue;
                }

                Table sibling = FindOriginalInLive(c, originals, live);
                if (sibling != null)
                {
                    string origin = MatchName(originals, sibling.Sha256);
                    string detail = origin == null ? "未知来源" : "仍是 tools\\acpi\\" + origin;
                    ConsoleUI.Warn(c.Name + " -> 未生效（OS 中对应表 len=" + sibling.Length +
                                   " sha=" + Short(sibling.Sha256) + "，" + detail + "）");
                    continue;
                }

                sibling = FindBySignatureAndOem(live, c);
                if (sibling != null)
                {
                    ConsoleUI.Warn(c.Name + " -> 未生效（OS 中同名表 len=" + sibling.Length +
                                   " sha=" + Short(sibling.Sha256) + "）");
                }
                else
                {
                    ConsoleUI.Error(c.Name + " -> 未进入 OS（找不到同签名/OemTableId/OemRevision 的表）");
                }
            }
        }

        private static List<Table> LoadLiveTables()
        {
            var list = new List<Table>();
            using (RegistryKey root = Registry.LocalMachine.OpenSubKey(AcpiRoot))
            {
                if (root == null) return list;
                foreach (string sigKeyName in root.GetSubKeyNames())
                {
                    if (sigKeyName != "DSDT" && !sigKeyName.StartsWith("SSD", StringComparison.Ordinal))
                    {
                        continue;
                    }
                    using (RegistryKey sigKey = root.OpenSubKey(sigKeyName))
                    {
                        if (sigKey == null) continue;
                        foreach (string oem in sigKey.GetSubKeyNames())
                        {
                            using (RegistryKey oemKey = sigKey.OpenSubKey(oem))
                            {
                                if (oemKey == null) continue;
                                foreach (string tid in oemKey.GetSubKeyNames())
                                {
                                    using (RegistryKey tidKey = oemKey.OpenSubKey(tid))
                                    {
                                        if (tidKey == null) continue;
                                        foreach (string rev in tidKey.GetSubKeyNames())
                                        {
                                            using (RegistryKey revKey = tidKey.OpenSubKey(rev))
                                            {
                                                if (revKey == null) continue;
                                                foreach (string valueName in revKey.GetValueNames())
                                                {
                                                    byte[] data = revKey.GetValue(valueName) as byte[];
                                                    Table t = Parse(AcpiRoot + "\\" + sigKeyName + "\\" + oem + "\\" + tid + "\\" + rev, data);
                                                    if (t != null && FindByHash(list, t.Sha256) == null)
                                                    {
                                                        list.Add(t);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return list;
        }

        private static List<Table> LoadFiles(string dir, string pattern)
        {
            var list = new List<Table>();
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return list;
            foreach (string path in Directory.GetFiles(dir, pattern))
            {
                Table t = Parse(Path.GetFileName(path), File.ReadAllBytes(path));
                if (t != null) list.Add(t);
            }
            return list;
        }

        private static Table Parse(string name, byte[] data)
        {
            if (data == null || data.Length < 36) return null;
            var t = new Table();
            t.Name = name;
            t.Data = data;
            t.Signature = Ascii(data, 0, 4);
            t.Length = BitConverter.ToUInt32(data, 4);
            t.OemId = Ascii(data, 10, 6);
            t.OemTableId = Ascii(data, 16, 8);
            t.OemRevision = BitConverter.ToUInt32(data, 24);
            t.Sha256 = Hash(data);
            return t;
        }

        private static Table FindByHash(List<Table> tables, string hash)
        {
            foreach (Table t in tables)
            {
                if (string.Equals(t.Sha256, hash, StringComparison.OrdinalIgnoreCase)) return t;
            }
            return null;
        }

        private static Table FindBySignatureAndOem(List<Table> tables, Table like)
        {
            foreach (Table t in tables)
            {
                if (t.Signature == like.Signature &&
                    t.OemTableId == like.OemTableId &&
                    t.OemRevision == like.OemRevision)
                {
                    return t;
                }
            }
            return null;
        }

        private static Table FindOriginalInLive(Table custom, List<Table> originals, List<Table> live)
        {
            string baseName = Path.GetFileNameWithoutExtension(custom.Name);
            if (baseName.EndsWith("_mod", StringComparison.OrdinalIgnoreCase))
            {
                baseName = baseName.Substring(0, baseName.Length - 4);
            }
            string originalName = baseName + ".dat";
            foreach (Table o in originals)
            {
                if (!string.Equals(o.Name, originalName, StringComparison.OrdinalIgnoreCase)) continue;
                Table inLive = FindByHash(live, o.Sha256);
                if (inLive != null) return inLive;
            }
            return null;
        }

        private static string FindOriginalsDir()
        {
            string[] candidates = {
                Path.Combine(PathResolver.ProjectRoot, "tools", "acpi"),
                Path.GetFullPath(Path.Combine(PathResolver.AppDir, "..", "tools", "acpi")),
                Path.GetFullPath(Path.Combine(PathResolver.AppDir, "..", "..", "tools", "acpi")),
                Path.Combine(Environment.CurrentDirectory, "tools", "acpi")
            };
            foreach (string p in candidates)
            {
                if (Directory.Exists(p)) return p;
            }
            return null;
        }

        private static string MatchName(List<Table> tables, string hash)
        {
            Table t = FindByHash(tables, hash);
            return t == null ? null : t.Name;
        }

        private static string Hash(byte[] data)
        {
            using (SHA256 sha = SHA256.Create())
            {
                return BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "");
            }
        }

        private static string Short(string hash)
        {
            return hash.Length <= 16 ? hash : hash.Substring(0, 16) + "...";
        }

        private static string Ascii(byte[] data, int offset, int length)
        {
            var sb = new StringBuilder(length);
            for (int i = 0; i < length; i++)
            {
                char c = (char)data[offset + i];
                sb.Append(c >= 32 && c < 127 ? c : ' ');
            }
            return sb.ToString().Trim();
        }

        private static string Pad(string s, int width)
        {
            return s.Length >= width ? s : s + new string(' ', width - s.Length);
        }
    }
}
