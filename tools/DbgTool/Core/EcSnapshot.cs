// ============================================================================
// DbgTool — EC 快照对比与变量推断工具
// 安全输入校验、快照大小验证、差异高亮与寄存器语义推断
// ============================================================================
using System;
using System.IO;
using System.Text;
using DbgTool.Common;

namespace DbgTool.Core
{
    internal static class EcSnapshot
    {
        private static readonly int[] NamedOffsets = {
            0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
            0x20, 0x23, 0x24, 0x25, 0x26, 0x5F, 0x60, 0x8E, 0x8F,
            0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
            0xD0, 0xD1, 0xD2, 0xE2, 0xE3, 0xE4, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB,
            0xF0, 0xF2, 0xF3, 0xF4, 0xF6, 0xF7, 0xF8
        };

        private static readonly string[] NamedLabels = {
            "TSR1", "TSR2", "TSR3", "TSR4", "TSR5", "TSR6", "TSR7", "TSR8", "TSR9",
            "LSTE/FNHK/CRHK/OCFL", "GSTS", "HKST", "TOCP等位", "AST1", "FASP", "ECWR",
            "AWHG", "AWLW", "BATM", "BBHL", "BBLP", "BBHM", "KBNL", "F1HI", "F1LO",
            "F2HI", "F2LO", "PABD", "TFLG", "GFLG", "GPMD", "SSDK(FWDE/FAAP)", "BPWM",
            "ITSM", "ECTP", "LEDM", "RGBR", "RGBG", "RGBB", "CMEN位", "DGPU", "CPUT",
            "STSM", "CSPL", "FPPT", "CTCL"
        };

        public static string SnapshotDir
        {
            get { return Path.Combine(PathResolver.AppDir, "ecdumps"); }
        }

        public static void Menu()
        {
            while (true)
            {
                ConsoleUI.SafeClear();
                ConsoleUI.Header("EC 快照对比与变量推断");
                Console.WriteLine("  说明: 修改机器状态前保存快照 A，修改后保存快照 B，对比找出变化字节。");
                Console.WriteLine();
                Console.WriteLine("  [1] 采集并保存当前快照 (256 字节)");
                Console.WriteLine("  [2] 对比两个已有快照");
                Console.WriteLine("  [0] 返回上级菜单");
                Console.WriteLine();
                Console.Write("  请选择操作: ");
                string c = Console.ReadLine();
                if (c == null || c == "0") return;

                switch (c)
                {
                    case "1":
                        SaveSnapshot();
                        break;
                    case "2":
                        DiffMenu();
                        break;
                }
                ConsoleUI.Pause();
            }
        }

        public static void SaveSnapshot()
        {
            int fails;
            byte[] buf = WmiClient.ReadEcRam(out fails);
            if (buf == null || fails > 64)
            {
                ConsoleUI.Error(string.Format("读取 EC RAM 失败 (读取失败字节数: {0}/256)", fails));
                return;
            }

            try
            {
                if (!Directory.Exists(SnapshotDir))
                {
                    Directory.CreateDirectory(SnapshotDir);
                }

                string fileName = string.Format("ecdump_{0:yyyyMMdd_HHmmss}.bin", DateTime.Now);
                string fullPath = Path.Combine(SnapshotDir, fileName);
                File.WriteAllBytes(fullPath, buf);

                ConsoleUI.Info("快照已成功保存: " + fullPath);
                if (fails > 0)
                {
                    ConsoleUI.Warn(string.Format("注意: 存在 {0} 个字节未能读出并记为 0。", fails));
                }

                // 打印矩阵预览
                for (int row = 0; row < 16; row++)
                {
                    var sb = new StringBuilder();
                    sb.Append("EC[" + (row * 16).ToString("X2") + "]: ");
                    for (int col = 0; col < 16; col++)
                    {
                        sb.Append(buf[row * 16 + col].ToString("X2")).Append(' ');
                    }
                    Console.WriteLine(sb.ToString());
                }
            }
            catch (Exception ex)
            {
                ConsoleUI.Error("写入快照文件失败: " + ex.Message);
            }
        }

        private static void DiffMenu()
        {
            if (!Directory.Exists(SnapshotDir))
            {
                ConsoleUI.Warn("快照目录不存在，请先保存快照。");
                return;
            }

            string[] files = Directory.GetFiles(SnapshotDir, "ecdump_*.bin");
            Array.Sort(files);

            if (files.Length < 2)
            {
                ConsoleUI.Warn("快照数量不足（至少需要两个快照文件才能进行对比）。");
                return;
            }

            Console.WriteLine("  可用快照列表:");
            for (int i = 0; i < files.Length; i++)
            {
                var fi = new FileInfo(files[i]);
                Console.WriteLine("   [{0}] {1} ({2} 字节, {3:yyyy-MM-dd HH:mm:ss})",
                    i + 1, Path.GetFileName(files[i]), fi.Length, fi.LastWriteTime);
            }
            Console.WriteLine();

            int aIdx = ConsoleUI.ReadIntSafely("选择快照 A 序号", 1, 1, files.Length) - 1;
            int bIdx = ConsoleUI.ReadIntSafely("选择快照 B 序号", files.Length, 1, files.Length) - 1;

            if (aIdx == bIdx)
            {
                ConsoleUI.Warn("选择的两个快照相同，无需对比。");
                return;
            }

            Diff(files[aIdx], files[bIdx]);
        }

        public static void Diff(string pathA, string pathB)
        {
            if (!File.Exists(pathA) || !File.Exists(pathB))
            {
                ConsoleUI.Error("快照文件不存在。");
                return;
            }

            byte[] a = File.ReadAllBytes(pathA);
            byte[] b = File.ReadAllBytes(pathB);

            if (a.Length != 256 || b.Length != 256)
            {
                ConsoleUI.Error("快照文件损坏（长度不为 256 字节）。");
                return;
            }

            ConsoleUI.Header(string.Format("EC 快照差异对比 ({0} vs {1})", Path.GetFileName(pathA), Path.GetFileName(pathB)));
            int changed = 0;

            for (int off = 0; off < 256; off++)
            {
                if (a[off] == b[off]) continue;

                changed++;
                string label = null;
                for (int i = 0; i < NamedOffsets.Length; i++)
                {
                    if (NamedOffsets[i] == off)
                    {
                        label = NamedLabels[i];
                        break;
                    }
                }

                string name = label ?? "(未命名, 见 WMI_VARIABLE_MAP.md)";
                ConsoleUI.WriteColored(
                    string.Format("  EC[0x{0:X2}] {1,-24} 0x{2:X2} -> 0x{3:X2}",
                        off, name, a[off], b[off]),
                    ConsoleColor.Magenta);
            }

            if (changed == 0)
            {
                ConsoleUI.Info("两个快照完全一致 (256 字节均相同)。");
            }
            else
            {
                ConsoleUI.Info(string.Format("共计发现 {0} 处字节变化。", changed));
            }
        }
    }
}
