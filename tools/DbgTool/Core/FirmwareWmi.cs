// ============================================================================
// DbgTool — 固件原生 WMI 访问模块 (Bitland MIFS + AMD AOD)
// 包含类名缓存优化，避免每次调用扫描数百个 WMI 类造成卡顿
// ============================================================================
using System;
using System.Management;
using System.Text;
using DbgTool.Common;

namespace DbgTool.Core
{
    internal static class FirmwareWmi
    {
        public const string MifsGuid = "B60BFB48-3E5B-49E4-A0E9-8CFFE1B3434B";
        public const string AodGuid = "ABCB0F6A-8EA1-11D1-00A0-C90629100000";

        private static string _cachedMifsClass;
        private static string _cachedAodClass;

        private static readonly string[] MifsReadCodes = {
            "0800:电源模式档位 (ITSM)", "0900:GPU 满血模式 (GPMD)", "0B00:Fn 热键位域 (FNHK)",
            "0C00:OEM 功能开关 (TOCP)", "0D00:风扇转速 (FRD0=F2, FRD1=F1)", "0F00:固件开关 (FWDE)",
            "1000:LED 模式 (LEDM)", "1100:键盘 RGB", "1200:键盘背光 (KBNL)",
            "1300:电源状态位 (ECWR&0x80)", "1400:风扇自动加速 (FAAP)", "1500:风扇策略 (FASP)",
            "1600:温度传感器 6 (TSR6)", "1700:充电管理状态"
        };

        private static readonly string[] MifsWriteHints = {
            "0800:FUN3=电源模式 0/1/2 (自动写 0x55→TFLG 解锁)", "0900:FUN3=1 满血 / 0 受限 (写 GFLG+GPMD)",
            "0B00:FUN3/FUN4=热键位域", "0C00:FUN3=TOCP 位", "0F00:FUN3=0/1 FWDE",
            "1000:FUN3=1/2 LEDM", "1100:FUN3/4/5=R,G,B", "1200:FUN3=背光 0-3",
            "1400:FUN3=FAAP 位", "1500:FUN3=FASP", "1700:充电管理/CPU 功耗直写 (协议复杂, 谨慎)"
        };

        public static void Menu()
        {
            while (true)
            {
                ConsoleUI.SafeClear();
                ConsoleUI.Header("固件原生 WMI 通道 (OEM 控制)");
                Console.WriteLine("  MIFS  GUID {" + MifsGuid + "}");
                Console.WriteLine("  AOD   GUID {" + AodGuid + "}");
                Console.WriteLine();
                Console.WriteLine("  [1] MIFS 读 (FUN1=0xFA00)");
                Console.WriteLine("  [2] MIFS 写 (FUN1=0xFB00)");
                Console.WriteLine("  [3] AOD AM01-AM09 (AMD SMU 通道)");
                Console.WriteLine("  [0] 返回上级菜单");
                Console.WriteLine();
                Console.Write("  请选择: ");
                string c = Console.ReadLine();
                if (c == null || c == "0") return;

                switch (c)
                {
                    case "1": MifsCall(false); break;
                    case "2": MifsCall(true); break;
                    case "3": AodMenu(); break;
                }
                ConsoleUI.Pause();
            }
        }

        private static ManagementObject FindInstance(string guid, ref string cachedClass, out string resolvedClass)
        {
            resolvedClass = null;

            // 1. 如果有缓存类名，优先尝试快速获取
            if (!string.IsNullOrEmpty(cachedClass))
            {
                try
                {
                    using (var searcher = new ManagementObjectSearcher(WmiClient.WmiNamespace, "SELECT * FROM " + cachedClass))
                    using (ManagementObjectCollection coll = searcher.Get())
                    {
                        foreach (ManagementObject mo in coll)
                        {
                            resolvedClass = cachedClass;
                            return mo;
                        }
                    }
                }
                catch { }
            }

            // 2. 全量扫描并缓存
            ConsoleUI.Info("正在检索匹配 GUID 的 WMI 固件类...");
            try
            {
                using (var s = new ManagementObjectSearcher(WmiClient.WmiNamespace, "SELECT * FROM Meta_Class"))
                using (ManagementObjectCollection coll = s.Get())
                {
                    foreach (ManagementBaseObject ob in coll)
                    {
                        string cn = ob.ClassPath.ClassName;
                        ManagementClass mc;
                        try { mc = new ManagementClass(WmiClient.WmiNamespace, cn, null); }
                        catch { continue; }

                        using (mc)
                        {
                            if (mc.Methods["WMAA"] == null) continue;

                            bool hit = false;
                            try
                            {
                                foreach (QualifierData q in mc.Qualifiers)
                                {
                                    if (q.Name == "guid" && string.Equals(Convert.ToString(q.Value), guid, StringComparison.OrdinalIgnoreCase))
                                    {
                                        hit = true;
                                        break;
                                    }
                                }
                            }
                            catch { }

                            if (!hit) continue;

                            cachedClass = cn;
                            resolvedClass = cn;
                            using (var inst = new ManagementObjectSearcher(WmiClient.WmiNamespace, "SELECT * FROM " + cn))
                            using (ManagementObjectCollection instColl = inst.Get())
                            {
                                foreach (ManagementObject mo in instColl)
                                {
                                    return mo;
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                ConsoleUI.Error("扫描 WMI 类失败: " + ex.Message);
            }
            return null;
        }

        private static void MifsCall(bool write)
        {
            string className;
            using (ManagementObject mo = FindInstance(MifsGuid, ref _cachedMifsClass, out className))
            {
                if (mo == null)
                {
                    ConsoleUI.Error("未找到 MIFS 实例 (GUID: " + MifsGuid + ")，当前固件可能未暴露此接口。");
                    return;
                }
                ConsoleUI.Info("命中 MIFS 类: " + className);

                if (!write)
                {
                    Console.WriteLine();
                    for (int i = 0; i < MifsReadCodes.Length; i++)
                    {
                        Console.WriteLine("  [{0}] {1}", i + 1, MifsReadCodes[i]);
                    }
                    int sel = ConsoleUI.ReadIntSafely("选择子功能", 1, 1, MifsReadCodes.Length);
                    ushort fun2 = (ushort)Convert.ToInt32(MifsReadCodes[sel - 1].Substring(0, 4), 16);
                    InvokeWmaa(mo, BuildMifsBytes(0xFA00, fun2, 0, 0, 0));
                }
                else
                {
                    Console.WriteLine();
                    for (int i = 0; i < MifsWriteHints.Length; i++)
                    {
                        Console.WriteLine("  [{0}] {1}", i + 1, MifsWriteHints[i]);
                    }
                    int sel = ConsoleUI.ReadIntSafely("选择子功能", 1, 1, MifsWriteHints.Length);
                    ushort fun2 = (ushort)Convert.ToInt32(MifsWriteHints[sel - 1].Substring(0, 4), 16);
                    uint f3 = ConsoleUI.ReadHexSafely("FUN3", 0, 255);
                    uint f4 = ConsoleUI.ReadHexSafely("FUN4", 0, 255);
                    uint f5 = ConsoleUI.ReadHexSafely("FUN5", 0, 255);
                    InvokeWmaa(mo, BuildMifsBytes(0xFB00, fun2, (byte)f3, (byte)f4, (byte)f5));
                }
            }
        }

        private static byte[] BuildMifsBytes(ushort fun1, ushort fun2, byte f3, byte f4, byte f5)
        {
            byte[] b = new byte[32];
            b[0] = (byte)fun1; b[1] = (byte)(fun1 >> 8);
            b[2] = (byte)fun2; b[3] = (byte)(fun2 >> 8);
            b[4] = f3; b[5] = f4; b[6] = f5;
            return b;
        }

        private static void InvokeWmaa(ManagementObject mo, byte[] inBuf)
        {
            using (ManagementBaseObject inp = mo.GetMethodParameters("WMAA"))
            {
                bool filled = false;
                foreach (PropertyData p in inp.Properties)
                {
                    if (p.IsArray && p.Type == CimType.UInt8 && !filled)
                    {
                        inp[p.Name] = inBuf;
                        filled = true;
                    }
                    else if (!p.IsArray && p.Type == CimType.UInt32 && !filled)
                    {
                        inp[p.Name] = 0u;
                    }
                }

                if (!filled)
                {
                    ConsoleUI.Error("WMAA 入参结构不符合预期 (缺少 byte[] 参数)。");
                    return;
                }

                using (ManagementBaseObject outp = mo.InvokeMethod("WMAA", inp, null))
                {
                    if (outp == null)
                    {
                        ConsoleUI.Error("WMAA 返回空");
                        return;
                    }

                    foreach (PropertyData p in outp.Properties)
                    {
                        if (p.Name == "InstanceName" || p.Name == "Active") continue;
                        if (p.Value is byte[])
                        {
                            byte[] o = (byte[])p.Value;
                            Console.WriteLine("  out." + p.Name + " (" + o.Length + "B):");
                            for (int row = 0; row < Math.Min(2, (o.Length + 15) / 16); row++)
                            {
                                var sb = new StringBuilder("    ");
                                for (int i = row * 16; i < Math.Min(o.Length, row * 16 + 16); i++)
                                    sb.Append(o[i].ToString("X2")).Append(' ');
                                Console.WriteLine(sb.ToString());
                            }
                            if (o.Length >= 32 && p.Name.ToLowerInvariant().Contains("out"))
                            {
                                ushort sger = (ushort)(o[0] | (o[1] << 8));
                                ushort frd0 = (ushort)(o[4] | (o[5] << 8));
                                uint frd1 = BitConverter.ToUInt32(o, 6);
                                Console.WriteLine("    解码: SGER=0x{0:X4}{1}  FRD0=0x{2:X4}  FRD1=0x{3:X8}",
                                    sger, (sger == 0x8000 ? " (成功)" : " (异常)"), frd0, frd1);
                            }
                        }
                        else
                        {
                            Console.WriteLine("  out." + p.Name + " = " + p.Value);
                        }
                    }
                }
            }
        }

        private static void AodMenu()
        {
            string className;
            using (ManagementObject mo = FindInstance(AodGuid, ref _cachedAodClass, out className))
            {
                if (mo == null)
                {
                    ConsoleUI.Error("未找到 AMD AOD WMI 实例 (GUID: " + AodGuid + ")。");
                    return;
                }
                ConsoleUI.Info("命中 AOD 类: " + className);
                Console.WriteLine("  AM01 版本 | AM03 OBID | AM07 BSPD | AM08 OBIE | AM09 RMPD");
                Console.WriteLine("  AM04/AM06 索引读 | AM05 SMU 邮箱 (SCMI/SCMD)");
                int sub = ConsoleUI.ReadIntSafely("AM 子功能 (1-9)", 1, 1, 9);

                byte[] inBuf = null;
                if (sub == 4 || sub == 5 || sub == 6 || sub == 7)
                {
                    uint v0 = ConsoleUI.ReadHexSafely("buffer[0..3]", 0, uint.MaxValue);
                    uint v4 = ConsoleUI.ReadHexSafely("buffer[4..7]", 0, uint.MaxValue);
                    inBuf = new byte[64];
                    inBuf[0] = (byte)v0; inBuf[1] = (byte)(v0 >> 8); inBuf[2] = (byte)(v0 >> 16); inBuf[3] = (byte)(v0 >> 24);
                    inBuf[4] = (byte)v4; inBuf[5] = (byte)(v4 >> 8); inBuf[6] = (byte)(v4 >> 16); inBuf[7] = (byte)(v4 >> 24);
                }

                using (ManagementBaseObject inp = mo.GetMethodParameters("WMAA"))
                {
                    bool numSet = false, bufSet = false;
                    foreach (PropertyData p in inp.Properties)
                    {
                        if (!numSet && !p.IsArray && p.Type == CimType.UInt32)
                        {
                            inp[p.Name] = (uint)sub;
                            numSet = true;
                        }
                        else if (!bufSet && p.IsArray && p.Type == CimType.UInt8)
                        {
                            inp[p.Name] = inBuf ?? new byte[64];
                            bufSet = true;
                        }
                    }

                    using (ManagementBaseObject outp = mo.InvokeMethod("WMAA", inp, null))
                    {
                        if (outp == null)
                        {
                            ConsoleUI.Error("AOD 调用返回空");
                            return;
                        }

                        foreach (PropertyData p in outp.Properties)
                        {
                            if (p.Name == "InstanceName" || p.Name == "Active") continue;
                            if (p.Value is byte[])
                            {
                                byte[] o = (byte[])p.Value;
                                Console.WriteLine("  out." + p.Name + " (" + o.Length + "B):");
                                for (int row = 0; row < Math.Min(4, (o.Length + 15) / 16); row++)
                                {
                                    var sb = new StringBuilder("    ");
                                    for (int i = row * 16; i < Math.Min(o.Length, row * 16 + 16); i++)
                                        sb.Append(o[i].ToString("X2")).Append(' ');
                                    Console.WriteLine(sb.ToString());
                                }
                            }
                            else
                            {
                                Console.WriteLine("  out." + p.Name + " = " + p.Value);
                            }
                        }
                    }
                }
            }
        }
    }
}
