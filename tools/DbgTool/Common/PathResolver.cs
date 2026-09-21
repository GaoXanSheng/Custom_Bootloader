// ============================================================================
// DbgTool — 路径与资源定位解析器
// 彻底解决 build\ / tools\DbgTool\ / 根目录 多种运行路径下的资源定位
// ============================================================================
using System;
using System.IO;
using System.Reflection;

namespace DbgTool.Common
{
    internal static class PathResolver
    {
        private static readonly string CachedAppDir;
        private static readonly string CachedProjectRoot;

        static PathResolver()
        {
            string loc = Assembly.GetExecutingAssembly().Location;
            CachedAppDir = !string.IsNullOrEmpty(loc) ? Path.GetDirectoryName(Path.GetFullPath(loc)) : Environment.CurrentDirectory;
            CachedProjectRoot = DetectProjectRoot(CachedAppDir);
        }

        public static string AppDir
        {
            get { return CachedAppDir; }
        }

        public static string ProjectRoot
        {
            get { return CachedProjectRoot; }
        }

        private static string DetectProjectRoot(string startDir)
        {
            string current = Path.GetFullPath(startDir);
            for (int i = 0; i < 4; i++)
            {
                if (File.Exists(Path.Combine(current, "build.bat")) ||
                    File.Exists(Path.Combine(current, "version.txt")) ||
                    Directory.Exists(Path.Combine(current, ".git")))
                {
                    return current;
                }
                string parent = Path.GetDirectoryName(current);
                if (string.IsNullOrEmpty(parent) || parent == current) break;
                current = parent;
            }
            return CachedAppDir;
        }

        public static string FindVersionFile()
        {
            string[] candidates = {
                Path.Combine(ProjectRoot, "build", "version.txt"),
                Path.Combine(ProjectRoot, "version.txt"),
                Path.Combine(AppDir, "version.txt")
            };

            foreach (string p in candidates)
            {
                if (File.Exists(p)) return Path.GetFullPath(p);
            }
            return null;
        }

        public static string FindEfiFile()
        {
            string[] candidates = {
                Path.Combine(ProjectRoot, "build", "BOOTX64.efi"),
                Path.Combine(AppDir, "BOOTX64.efi"),
                Path.Combine(Environment.CurrentDirectory, "BOOTX64.efi"),
                Path.Combine(ProjectRoot, "BOOTX64.efi")
            };

            foreach (string p in candidates)
            {
                if (File.Exists(p))
                {
                    try
                    {
                        var fi = new FileInfo(p);
                        if (fi.Length > 0) return Path.GetFullPath(p);
                    }
                    catch { }
                }
            }
            return null;
        }
    }
}
