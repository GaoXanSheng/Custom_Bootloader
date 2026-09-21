// ============================================================================
// DbgTool — 路径与资源定位解析器
// 彻底解决 build\ / tools\DbgTool\ / 根目录 多种运行路径下的资源定位 Bug
// ============================================================================
using System;
using System.Collections.Generic;
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
            // 候选 1: build\version.txt
            string p1 = Path.Combine(ProjectRoot, "build", "version.txt");
            if (File.Exists(p1)) return p1;

            // 候选 2: 根目录下 version.txt
            string p2 = Path.Combine(ProjectRoot, "version.txt");
            if (File.Exists(p2)) return p2;

            // 候选 3: AppDir 同级
            string p3 = Path.Combine(AppDir, "version.txt");
            if (File.Exists(p3)) return p3;

            return null;
        }

        public static string[] FindEfiCandidates()
        {
            var list = new List<string>();

            AddIfExists(list, Path.Combine(ProjectRoot, "build", "BOOTX64.efi"));
            AddIfExists(list, Path.Combine(AppDir, "BOOTX64.efi"));
            AddIfExists(list, Path.Combine(Environment.CurrentDirectory, "BOOTX64.efi"));
            AddIfExists(list, Path.Combine(ProjectRoot, "BOOTX64.efi"));

            // 递归搜索 ProjectRoot 下的 build 目录
            string buildDir = Path.Combine(ProjectRoot, "build");
            if (Directory.Exists(buildDir))
            {
                CollectRecursive(list, buildDir, 0, 2);
            }

            // 去重
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var result = new List<string>();
            foreach (string p in list)
            {
                if (seen.Add(p)) result.Add(p);
            }
            return result.ToArray();
        }

        private static void AddIfExists(List<string> list, string path)
        {
            try
            {
                if (File.Exists(path))
                {
                    list.Add(Path.GetFullPath(path));
                }
            }
            catch { }
        }

        private static void CollectRecursive(List<string> list, string root, int depth, int maxDepth)
        {
            if (depth > maxDepth) return;
            try
            {
                string[] files = Directory.GetFiles(root, "BOOTX64.efi");
                foreach (string f in files)
                {
                    AddIfExists(list, f);
                }

                string[] dirs = Directory.GetDirectories(root);
                foreach (string d in dirs)
                {
                    if (d.IndexOf(".git", StringComparison.OrdinalIgnoreCase) >= 0) continue;
                    CollectRecursive(list, d, depth + 1, maxDepth);
                }
            }
            catch { }
        }
    }
}
