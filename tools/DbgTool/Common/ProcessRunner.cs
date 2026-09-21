// ============================================================================
// DbgTool — 进程执行辅助类
// 支持精确捕获 ExitCode、标准输出与标准错误分离，兼容不同系统代码页
// ============================================================================
using System;
using System.Diagnostics;
using System.Text;

namespace DbgTool.Common
{
    internal sealed class ProcessResult
    {
        public int ExitCode { get; set; }
        public string StandardOutput { get; set; }
        public string StandardError { get; set; }
        public bool Success { get { return ExitCode == 0; } }

        public string CombinedOutput
        {
            get
            {
                string o = StandardOutput ?? string.Empty;
                string e = StandardError ?? string.Empty;
                if (string.IsNullOrEmpty(o)) return e;
                if (string.IsNullOrEmpty(e)) return o;
                return o + Environment.NewLine + e;
            }
        }
    }

    internal static class ProcessRunner
    {
        public static ProcessResult Run(string fileName, string arguments)
        {
            return Run(fileName, arguments, null);
        }

        public static ProcessResult Run(string fileName, string arguments, string workingDirectory)
        {
            var result = new ProcessResult();
            try
            {
                var psi = new ProcessStartInfo(fileName, arguments)
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                    StandardOutputEncoding = Encoding.Default,
                    StandardErrorEncoding = Encoding.Default
                };

                if (!string.IsNullOrEmpty(workingDirectory))
                {
                    psi.WorkingDirectory = workingDirectory;
                }

                using (Process p = Process.Start(psi))
                {
                    if (p == null)
                    {
                        result.ExitCode = -1;
                        result.StandardError = "无法启动进程: " + fileName;
                        return result;
                    }

                    result.StandardOutput = p.StandardOutput.ReadToEnd();
                    result.StandardError = p.StandardError.ReadToEnd();
                    p.WaitForExit();
                    result.ExitCode = p.ExitCode;
                }
            }
            catch (Exception ex)
            {
                result.ExitCode = -1;
                result.StandardError = "执行 " + fileName + " 异常: " + ex.Message;
            }
            return result;
        }
    }
}
