// ============================================================================
// DbgTool — 控制台交互与输出辅助
// 统一终端 UTF-8、彩色日志、安全输入与提示交互
// ============================================================================
using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace DbgTool.Common
{
    internal static class ConsoleUI
    {
        static ConsoleUI()
        {
            try
            {
                Console.OutputEncoding = Encoding.UTF8;
                Console.InputEncoding = Encoding.UTF8;
            }
            catch
            {
                // 忽略在某些重定向或特殊控制台下的编码设置异常
            }
        }

        public static void Init()
        {
            // 触发静态构造
        }

        public static void SafeClear()
        {
            try { Console.Clear(); }
            catch (IOException) { }
            catch (PlatformNotSupportedException) { }
        }

        public static void Info(string msg) { WriteColored("[+] " + msg, ConsoleColor.Green); }
        public static void Warn(string msg) { WriteColored("[!] " + msg, ConsoleColor.Yellow); }
        public static void Error(string msg) { WriteColored("[-] " + msg, ConsoleColor.Red); }
        public static void Header(string title)
        {
            Console.WriteLine();
            WriteColored("========== " + title + " ==========", ConsoleColor.Cyan);
        }

        public static void WriteColored(string msg, ConsoleColor color)
        {
            ConsoleColor old = Console.ForegroundColor;
            Console.ForegroundColor = color;
            Console.WriteLine(msg);
            Console.ForegroundColor = old;
        }

        public static void Pause()
        {
            Console.WriteLine();
            try
            {
                Console.Write("  按任意键返回菜单...");
                Console.ReadKey(true);
            }
            catch (InvalidOperationException)
            {
                // stdin 被重定向时跳过等待
            }
        }

        public static bool Confirm(string prompt, bool defaultYes)
        {
            string hint = defaultYes ? "[Y/n]" : "[y/N]";
            Console.Write("  " + prompt + " " + hint + ": ");
            string s = Console.ReadLine();
            if (s == null) return defaultYes;
            s = s.Trim();
            if (s.Length == 0) return defaultYes;
            if (string.Equals(s, "y", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(s, "yes", StringComparison.OrdinalIgnoreCase) ||
                s == "1" || s == "是")
            {
                return true;
            }
            if (string.Equals(s, "n", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(s, "no", StringComparison.OrdinalIgnoreCase) ||
                s == "0" || s == "否")
            {
                return false;
            }
            return defaultYes;
        }

        public static int ReadIntSafely(string prompt, int def, int min, int max)
        {
            while (true)
            {
                Console.Write("  " + prompt + " [" + min + ".." + max + "] (默认 " + def + "): ");
                string s = Console.ReadLine();
                if (s == null) return def;
                s = s.Trim();
                if (s.Length == 0) return def;

                int val;
                if (int.TryParse(s, out val) && val >= min && val <= max)
                {
                    return val;
                }
                Error("输入无效，请输入范围在 [" + min + ".." + max + "] 内的整数。");
            }
        }

        public static uint ReadHexSafely(string prompt, uint def, uint max)
        {
            while (true)
            {
                string maxHex = max == uint.MaxValue ? "FFFFFFFF" : max.ToString("X");
                Console.Write("  " + prompt + " [hex 0x00..0x" + maxHex + "] (默认 0x" + def.ToString("X2") + "): ");
                string s = Console.ReadLine();
                if (s == null) return def;
                s = s.Trim();
                if (s.Length == 0) return def;

                if (s.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                {
                    s = s.Substring(2);
                }

                uint val;
                if (uint.TryParse(s, NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture, out val) && val <= max)
                {
                    return val;
                }
                Error("输入无效，请输入十六进制数（例如 0x1F 或 1F）。");
            }
        }
    }
}
