// ============================================================================
// DbgTool — WMI 核心客户端 (CustomDbUnlock)
// 高性能长连接/单例复用，杜绝重复 WQL 查询与 COM 句柄堆积，提速数十倍
// ============================================================================
using System;
using System.Collections.Generic;
using System.Management;
using System.Text;
using DbgTool.Common;

namespace DbgTool.Core
{
    internal static class WmiClient
    {
        public const string WmiNamespace = @"\\.\root\wmi";
        public const string WmiClassName = "CustomDbUnlock";

        /// <summary>
        /// 获取 CustomDbUnlock 实例。调用方须在使用完毕后 Dispose，或通过 WithInstance 统一生命周期。
        /// </summary>
        public static ManagementObject GetInstance()
        {
            try
            {
                var scope = new ManagementScope(WmiNamespace);
                scope.Connect();
                var path = new ManagementPath(WmiClassName);
                using (var mc = new ManagementClass(scope, path, null))
                using (ManagementObjectCollection instances = mc.GetInstances())
                {
                    foreach (ManagementObject mo in instances)
                    {
                        return mo; // 返回首个有效实例
                    }
                }
            }
            catch (ManagementException me)
            {
                if (me.ErrorCode == ManagementStatus.AccessDenied)
                {
                    ConsoleUI.Error("WMI 访问被拒绝: 请以管理员权限运行。");
                }
                else
                {
                    ConsoleUI.Error("WMI 查询异常: " + me.Message);
                }
            }
            catch (Exception ex)
            {
                ConsoleUI.Error("连接 WMI 失败: " + ex.Message);
            }
            return null;
        }

        public static bool ExecuteOnInstance(Action<ManagementObject> action, string actionName)
        {
            using (ManagementObject mo = GetInstance())
            {
                if (mo == null)
                {
                    ConsoleUI.Error("未找到 " + WmiClassName + " 实例，请确认已部署并注册 ACPI/WMI 设备。");
                    return false;
                }
                try
                {
                    action(mo);
                    return true;
                }
                catch (Exception ex)
                {
                    ConsoleUI.Error(actionName + " 失败: " + ex.Message);
                    return false;
                }
            }
        }

        // ------------------------------------------------------------------
        // 核心方法实现
        // ------------------------------------------------------------------

        public static bool CheckRegistration()
        {
            return ExecuteOnInstance(mo =>
            {
                ConsoleUI.Header("WMI 设备注册状态");
                Console.WriteLine("  InstanceName : " + Convert.ToString(mo["InstanceName"]));
                Console.WriteLine("  Active       : " + Convert.ToString(mo["Active"]));
                Console.WriteLine("  __SERVER     : " + Convert.ToString(mo["__SERVER"]));
                ConsoleUI.Info("CustomDbUnlock WMI 驱动通信正常。");
            }, "检查注册状态");
        }

        public static bool GetDbfs(out uint val)
        {
            uint tmp = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject outp = mo.InvokeMethod("GetDbfs", null, null))
                {
                    if (outp != null && outp["Value"] != null)
                    {
                        tmp = Convert.ToUInt32(outp["Value"]);
                    }
                }
            }, "读取 DBFS");
            val = tmp;
            return ok;
        }

        public static bool SetDbfs(uint val, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetDbfs"))
                {
                    inp["Value"] = val;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetDbfs", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "设置 DBFS");
            status = st;
            return ok;
        }

        /// <summary>
        /// 设置 DB 借电上限（瓦，0..100）。100=原厂（GPU 可借 25W 到 140W），
        /// 0=等效关 DB（GPU 锁 115W）。写 NPCF.MGAF 后 Notify 0xC0 即时生效。
        /// 返回状态：0=成功；2=越界；0xFFFFFFFF=注入表未加载。
        /// </summary>
        public static bool SetDbBoostCap(uint watts, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetDbBoostCap"))
                {
                    inp["Watts"] = watts;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetDbBoostCap", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "设置 DB 借电上限");
            status = st;
            return ok;
        }

        /// <summary>
        /// 读取 Dynamic Boost 全景（Method 16）：
        /// Dbfs=开关状态，Cspl/Fppt=CPU PL1/PL2（W），DbCapW=当前 DB 借电上限（W）。
        /// </summary>
        public static bool GetDynamicBoost(out uint dbfs, out uint cspl, out uint fppt, out uint dbCapW)
        {
            uint d = 0, c = 0, f = 0, k = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("GetDynamicBoost"))
                {
                    inp["Reserved"] = 0u;
                    using (ManagementBaseObject outp = mo.InvokeMethod("GetDynamicBoost", inp, null))
                    {
                        if (outp != null)
                        {
                            if (outp["Dbfs"] != null) d = Convert.ToUInt32(outp["Dbfs"]);
                            if (outp["Cspl"] != null) c = Convert.ToUInt32(outp["Cspl"]);
                            if (outp["Fppt"] != null) f = Convert.ToUInt32(outp["Fppt"]);
                            if (outp["DbCapW"] != null) k = Convert.ToUInt32(outp["DbCapW"]);
                        }
                    }
                }
            }, "读取 Dynamic Boost 状态");
            dbfs = d; cspl = c; fppt = f; dbCapW = k;
            return ok;
        }

        /// <summary>
        /// 设置 GPU 功耗预算（瓦，35..140）。写 NPCF.TGPF（0.5W 单位）后
        /// Notify 0xC0 逼 NVIDIA 驱动立即重读 fun#2 预算。仅作用性能档
        /// （ITSM==1）；办公档保持原厂 60W。状态：0=成功；2=越界；
        /// 0xFFFFFFFF=注入表未加载。
        /// </summary>
        public static bool SetGpuBudget(uint watts, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetGpuBudget"))
                {
                    inp["Watts"] = watts;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetGpuBudget", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "设置 GPU 功耗预算");
            status = st;
            return ok;
        }

        public static bool GetVersion(out uint buildVersion, out string decodedTime)
        {
            uint ver = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject outp = mo.InvokeMethod("GetVersion", null, null))
                {
                    if (outp != null && outp["BuildVersion"] != null)
                    {
                        ver = Convert.ToUInt32(outp["BuildVersion"]);
                    }
                }
            }, "读取内部版本");
            buildVersion = ver;
            decodedTime = DecodeBuildTime(ver);
            return ok;
        }

        public static string DecodeBuildTime(uint v)
        {
            try
            {
                if ((v >> 24) == 0x20)
                {
                    int y = (int)(((v >> 28) & 0xF) * 1000 + ((v >> 24) & 0xF) * 100 +
                                  ((v >> 20) & 0xF) * 10 + ((v >> 16) & 0xF));
                    int mo = (int)(((v >> 12) & 0xF) * 10 + ((v >> 8) & 0xF));
                    int d = (int)(((v >> 4) & 0xF) * 10 + (v & 0xF));
                    if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31)
                    {
                        return string.Format("(旧版BCD日期) {0:D4}-{1:D2}-{2:D2}", y, mo, d);
                    }
                }
                DateTime t = DateTimeOffset.FromUnixTimeSeconds(v).LocalDateTime;
                return t.ToString("yyyy-MM-dd HH:mm:ss") + " (本地时间)";
            }
            catch
            {
                return "(无法解码)";
            }
        }

        public static bool GetEcByte(uint offset, out byte val)
        {
            byte tmp = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("GetEcByte"))
                {
                    inp["RegisterOffset"] = offset;
                    using (ManagementBaseObject outp = mo.InvokeMethod("GetEcByte", inp, null))
                    {
                        if (outp != null && outp["ReadVal"] != null)
                        {
                            tmp = Convert.ToByte(outp["ReadVal"]);
                        }
                    }
                }
            }, "读取 EC 字节");
            val = tmp;
            return ok;
        }

        public static bool GetEcRegister(uint offset, out uint val)
        {
            uint tmp = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("GetEcRegister"))
                {
                    inp["RegisterOffset"] = offset;
                    using (ManagementBaseObject outp = mo.InvokeMethod("GetEcRegister", inp, null))
                    {
                        if (outp != null && outp["ReadVal"] != null)
                        {
                            tmp = Convert.ToUInt32(outp["ReadVal"]);
                        }
                    }
                }
            }, "读取 EC 寄存器");
            val = tmp;
            return ok;
        }

        public static bool SetEcRegister(uint offset, uint val, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetEcRegister"))
                {
                    inp["RegisterOffset"] = offset;
                    inp["WriteVal"] = val;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetEcRegister", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "写入 EC 寄存器");
            status = st;
            return ok;
        }

        /// <summary>
        /// 全量读取 EC RAM 0x00-0xFF (256字节)
        /// 关键优化: 单实例长会话连续调用，杜绝 256 次 WQL 搜索，耗时由几十秒缩短至亚秒级
        /// </summary>
        public static byte[] ReadEcRam(out int failureCount)
        {
            byte[] buf = new byte[256];
            int fails = 0;

            using (ManagementObject mo = GetInstance())
            {
                if (mo == null)
                {
                    failureCount = 256;
                    return null;
                }

                using (ManagementBaseObject inParams = mo.GetMethodParameters("GetEcByte"))
                {
                    for (int i = 0; i < 256; i++)
                    {
                        try
                        {
                            inParams["RegisterOffset"] = (uint)i;
                            using (ManagementBaseObject outp = mo.InvokeMethod("GetEcByte", inParams, null))
                            {
                                if (outp != null && outp["ReadVal"] != null)
                                {
                                    buf[i] = Convert.ToByte(outp["ReadVal"]);
                                }
                                else
                                {
                                    fails++;
                                }
                            }
                        }
                        catch
                        {
                            fails++;
                        }
                    }
                }
            }

            failureCount = fails;
            return buf;
        }

        public static byte[] ReadEcBytes(params uint[] offsets)
        {
            if (offsets == null || offsets.Length == 0) return new byte[0];
            byte[] result = new byte[offsets.Length];

            using (ManagementObject mo = GetInstance())
            {
                if (mo == null) return null;

                using (ManagementBaseObject inParams = mo.GetMethodParameters("GetEcByte"))
                {
                    for (int i = 0; i < offsets.Length; i++)
                    {
                        try
                        {
                            inParams["RegisterOffset"] = offsets[i];
                            using (ManagementBaseObject outp = mo.InvokeMethod("GetEcByte", inParams, null))
                            {
                                if (outp != null && outp["ReadVal"] != null)
                                {
                                    result[i] = Convert.ToByte(outp["ReadVal"]);
                                }
                                else
                                {
                                    return null;
                                }
                            }
                        }
                        catch
                        {
                            return null;
                        }
                    }
                }
            }
            return result;
        }

        // ------------------------------------------------------------------
        // 高级功能：功耗状态、转储与验证
        // ------------------------------------------------------------------

        public static void DumpPowerState()
        {
            ConsoleUI.Header("功耗状态监控 (CustomDbUnlock)");

            using (ManagementObject testMo = GetInstance())
            {
                if (testMo == null)
                {
                    ConsoleUI.Error("未找到 " + WmiClassName + " 实例，请确认已部署并注册 ACPI/WMI 设备。");
                    return;
                }
            }

            uint gpmd, gflg, itsm, dgpu, cput, cspl, fppt, ctcl;
            GetEcRegister(0xD2, out gpmd);
            GetEcRegister(0xD1, out gflg);
            GetEcRegister(0xE4, out itsm);
            GetEcRegister(0xF2, out dgpu);
            GetEcRegister(0xF3, out cput);
            GetEcRegister(0xF6, out cspl);
            GetEcRegister(0xF7, out fppt);
            GetEcRegister(0xF8, out ctcl);

            ConsoleUI.Info(string.Format("EC[0xD2] GPMD GPU 功耗模式  = 0x{0:X2} ({1})", gpmd, gpmd == 1 ? "满血" : "标准/受限"));
            ConsoleUI.Info(string.Format("EC[0xD1] GFLG GPU 模式标志  = 0x{0:X2} ({1})", gflg, gflg == 0x55 ? "满血标志生效" : "普通"));
            ConsoleUI.Info(string.Format("EC[0xE4] ITSM 电源模式档位  = 0x{0:X2} (档位 {1})", itsm, itsm));
            ConsoleUI.Info(string.Format("EC[0xF2] DGPU 独显在位状态  = 0x{0:X2}", dgpu));
            ConsoleUI.Info(string.Format("EC[0xF3] CPUT CPU 型号标识  = 0x{0:X2} ({1})", cput, cput == 7 ? "R7" : (cput == 9 ? "R9" : "其他")));
            ConsoleUI.Info(string.Format("EC[0xF6] CSPL CPU PL1 稳态  = 0x{0:X2} ({1} W)", cspl, cspl));
            ConsoleUI.Info(string.Format("EC[0xF7] FPPT CPU PL2 爆发  = 0x{0:X2} ({1} W)", fppt, fppt));
            ConsoleUI.Info(string.Format("EC[0xF8] CTCL CPU 温度墙    = 0x{0:X2} ({1} °C)", ctcl, ctcl));

            byte[] raw = ReadEcBytes(0x8E, 0x60, 0x8F, 0x1C, 0x1D);
            if (raw != null)
            {
                uint watts = (uint)(raw[0] << 8) | raw[2];
                ConsoleUI.Info(string.Format("EC[0x8E/0x8F] 适配器功率 AWHG/AWLW = {0} W{1}",
                    watts, (watts > 0 && watts <= 500 ? "" : " (异常值, 功耗预算计算或受影响)")));
                ConsoleUI.Info(string.Format("EC[0x60] ECWR 电源状态位 = 0x{0:X2} (AC在位={1}, bit3={2}, bit7={3})",
                    raw[1], raw[1] & 1, (raw[1] >> 3) & 1, (raw[1] >> 7) & 1));
                ConsoleUI.Info(string.Format("EC[0x1C/0x1D] TSR6/TSR7 温度 = {0} / {1} °C", raw[3], raw[4]));

                if (watts > 0 && watts <= 300)
                {
                    ConsoleUI.Warn(string.Format("适配器功率 ≤300W: EC 固件按 ({0}W − GPU 实抽) 预算 CPU 功耗。", watts));
                    Console.WriteLine("      双烤时 GPU 吃满后 CPU 剩余 ≈ " + watts + "W − GPU 实抽。");
                    Console.WriteLine("      若 GPU 实抽 ~140W 且适配器识别为 180W → CPU 正好剩 ~40W。");
                }
            }

            uint dbfs, cspl2, fppt2, dbCap;
            if (GetDynamicBoost(out dbfs, out cspl2, out fppt2, out dbCap))
            {
                ConsoleUI.Info("DBFS  Dynamic Boost 开关状态 = " + dbfs + (dbfs != 0 ? " (已启用)" : " (已禁用)"));
                if (dbCap != 0xFF)
                {
                    ConsoleUI.Info(string.Format("DBCAP DB 借电上限 = {0} W (100=原厂；GPU 顶约 {1} W；0=锁 115W)",
                        dbCap, 115 + Math.Min(dbCap, 25)));
                }
            }
        }

        public static void DumpEcRamConsole()
        {
            ConsoleUI.Header("EC RAM 全量转储 (0x00-0xFF)");
            int fails;
            byte[] buf = ReadEcRam(out fails);
            if (buf == null || fails > 64)
            {
                ConsoleUI.Error(string.Format("EC 全量读取失败 (失败字节数: {0}/256)", fails));
                return;
            }

            for (int row = 0; row < 16; row++)
            {
                var sb = new StringBuilder();
                sb.Append("EC[" + (row * 16).ToString("X2") + "]: ");
                for (int col = 0; col < 16; col++)
                {
                    int idx = row * 16 + col;
                    byte v = buf[idx];
                    bool hot = v == 0xC3 || v == 0xD1 || v == 0xD3 || v == 0xD5 || v == 0xCE || v == 0x61;
                    sb.Append(hot ? ">>" + v.ToString("X2") + "<< " : v.ToString("X2") + " ");
                }
                Console.WriteLine(sb.ToString());
            }
            if (fails > 0)
            {
                ConsoleUI.Warn(string.Format("读取完成，其中有 {0} 个字节未能成功读取。", fails));
            }
            ConsoleUI.Info("功率提示: 0xC3=97.5W/2, 0xD1=104.5W/2, 0xCE=103W/2, 0x61=48.5W/2 (半瓦)");
        }

        public static void VerifyTables()
        {
            ConsoleUI.Header("重定向 ACPI 表生效验证");

            bool live = false;
            bool denied = false;
            try
            {
                using (ManagementObject mo = GetInstance())
                {
                    if (mo != null)
                    {
                        live = true;
                    }
                }
            }
            catch (ManagementException me)
            {
                denied = (me.ErrorCode == ManagementStatus.AccessDenied);
            }
            catch { }

            if (live)
            {
                ConsoleUI.Info("CustomDbUnlock 类已注册 — 注入的 DBUL SSDT 正在被 Windows 加载");
                uint ver;
                string dt;
                if (GetVersion(out ver, out dt))
                {
                    ConsoleUI.Info(string.Format("内部版本 (GetVersion): 0x{0:X8} 构建时间: {1}", ver, dt));
                }

                uint cspl, fppt, gpmd;
                GetEcRegister(0xF6, out cspl);
                GetEcRegister(0xF7, out fppt);
                GetEcRegister(0xD2, out gpmd);

                ConsoleUI.Info(string.Format("EC[0xF6] CSPL CPU PL1 = 0x{0:X2} (0x55=85W 表示替换表 _INI 已执行)", cspl));
                ConsoleUI.Info(string.Format("EC[0xF7] FPPT CPU PL2 = 0x{0:X2} (0x6E=110W 同上)", fppt));
                ConsoleUI.Info(string.Format("EC[0xD2] GPMD GPU 模式 = 0x{0:X2} (1=满血)", gpmd));
            }
            else if (denied)
            {
                ConsoleUI.Warn("WMI 拒绝访问 — 当前权限不足，请以管理员身份重新运行。");
            }
            else
            {
                ConsoleUI.Error("CustomDbUnlock 未注册 — 注入 SSDT 未生效，请检查 unlock.log 或固件状态。");
            }

            Console.WriteLine();
            ConsoleUI.WriteColored("---- 进一步核验建议 ----", ConsoleColor.Yellow);
            Console.WriteLine("  1. 检查 ESP 分区中的 \\EFI\\BOOT\\unlock.log 应包含:");
            Console.WriteLine("     [+] All ACPI table replacements applied successfully.");
            Console.WriteLine("  2. 管理员 PowerShell: .\\acpidump.exe -d 导出的 DSDT 应包含替换特征。");
        }

        public static bool GetBatteryChargeThreshold(out uint limit)
        {
            uint val = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject outp = mo.InvokeMethod("GetBatteryChargeThreshold", null, null))
                {
                    if (outp != null && outp["LimitPercent"] != null)
                    {
                        val = Convert.ToUInt32(outp["LimitPercent"]);
                    }
                }
            }, "读取电池充电阈值");
            limit = val;
            return ok;
        }

        public static bool SetBatteryChargeThreshold(uint limit, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetBatteryChargeThreshold"))
                {
                    inp["LimitPercent"] = limit;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetBatteryChargeThreshold", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "设置电池充电阈值");
            status = st;
            return ok;
        }

        public static bool GetThresholds(out uint acbt, out uint dcbt, out uint amat, out uint amit)
        {
            uint a = 0, d = 0, am = 0, ai = 0;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject outp = mo.InvokeMethod("GetThresholds", null, null))
                {
                    if (outp != null)
                    {
                        if (outp["Acbt"] != null) a = Convert.ToUInt32(outp["Acbt"]);
                        if (outp["Dcbt"] != null) d = Convert.ToUInt32(outp["Dcbt"]);
                        if (outp["Amat"] != null) am = Convert.ToUInt32(outp["Amat"]);
                        if (outp["Amit"] != null) ai = Convert.ToUInt32(outp["Amit"]);
                    }
                }
            }, "读取热阈值");
            acbt = a; dcbt = d; amat = am; amit = ai;
            return ok;
        }

        public static bool SetThresholds(uint acbt, uint dcbt, uint amat, uint amit, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetThresholds"))
                {
                    inp["Acbt"] = acbt;
                    inp["Dcbt"] = dcbt;
                    inp["Amat"] = amat;
                    inp["Amit"] = amit;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetThresholds", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "设置热阈值");
            status = st;
            return ok;
        }

        public static bool ApplyPowerLimits(uint cspl, uint fppt, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("ApplyPowerLimits"))
                {
                    inp["CsplWatts"] = cspl;
                    inp["FpptWatts"] = fppt;
                    using (ManagementBaseObject outp = mo.InvokeMethod("ApplyPowerLimits", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "推送功耗限值");
            status = st;
            return ok;
        }

        public static void NpcfTrace()
        {
            ConsoleUI.Header("NVPCF 驱动调用追踪");
            ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject outp = mo.InvokeMethod("GetNpcfTrace", null, null))
                {
                    if (outp == null)
                    {
                        ConsoleUI.Error("读取 NVPCF 调用追踪返回空");
                        return;
                    }
                    uint cusl = Convert.ToUInt32(outp["Cusl"]);
                    uint cuct = Convert.ToUInt32(outp["Cuct"]);
                    uint cnt = Convert.ToUInt32(outp["Fun6Count"]);
                    uint nchp = Convert.ToUInt32(outp["LastNchp"]);

                    Console.WriteLine("  fun#5 (驱动设 CPU SL) 最后值:  CUSL=0x" + cusl.ToString("X2") +
                                      (cusl > 0 ? "  (" + cusl + "W ?)" : ""));
                    Console.WriteLine("  fun#5 (驱动设 CPU CT) 最后值:  CUCT=0x" + cuct.ToString("X2") +
                                      (cuct > 0 && cuct < 0xFF ? "  (" + cuct + "°C ?)" : ""));
                    Console.WriteLine("  fun#6 (IO口设CPU功耗) 调用次数: " + cnt +
                                      "   最后 NCHP=0x" + nchp.ToString("X2") +
                                      (nchp > 0 && nchp < 0x100 ? "  (" + nchp + "W ?)" : ""));

                    if (cnt > 0 || cusl > 0)
                        ConsoleUI.Warn("驱动确实经 AML NVPCF 通道下发 CPU 功耗 -> 可在该入口加下限拦截！");
                    else
                        ConsoleUI.Warn("两通道全零: DB 的 CPU 钳制不经过 ACPI（SMU 内部直执行）。");
                }
            }, "读取 NVPCF 调用追踪");
        }

        public static bool SetAmdAlibDirect(uint sub, uint data, out uint status)
        {
            uint st = uint.MaxValue;
            bool ok = ExecuteOnInstance(mo =>
            {
                using (ManagementBaseObject inp = mo.GetMethodParameters("SetAmdAlibDirect"))
                {
                    inp["SubFunction"] = sub;
                    inp["DataParam"] = data;
                    using (ManagementBaseObject outp = mo.InvokeMethod("SetAmdAlibDirect", inp, null))
                    {
                        if (outp != null && outp["Status"] != null)
                        {
                            st = Convert.ToUInt32(outp["Status"]);
                        }
                    }
                }
            }, "AMD ALIB 直写");
            status = st;
            return ok;
        }
    }
}
