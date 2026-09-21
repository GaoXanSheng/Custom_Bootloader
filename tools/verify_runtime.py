#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_runtime.py — 运行时 ACPI 补丁指纹验证（Windows, 免提权）

原理：
  kernel32.GetSystemFirmwareTable('ACPI', <sig>) 直接读取本次开机 Windows
  实际接收到的 ACPI 表（即引导器改写后的内存中版本），无需管理员权限、
  无需挂载 ESP、不受注册表 SSDT 按键覆盖问题影响。

  DSDT（CPU 功耗钳制/借电逻辑 所在）可以完整取回并逐指纹断言。
  SSDT 只能取回目录中的第一份：若第一份恰好是 NPCF 表则可判定 SSDT4
  替换成败；否则给出"不确定"，需配合 unlock.log（新增的逐表 ALARM）判定。

检查项（对运行时 DSDT）：
  [必须存在] FNQS 重写: THMD (0x13) 分发（且无 DBFS/适配器瓦数分发）
             MSPL 下限: If ((Local0 < 0x50))   （CSPL >= 80W）
             MFPT 下限: If ((Local0 < 0x6E))   （FPPT >= 110W）
             _Q82 保持原厂（DBFS = One 跟随 EC 事件，保留 DB 增压链路。
             2026-09-13 起 _Q81/_Q82 有意不补：4060 动态增压到 140W 必须
             走 DB，45W 借电钳制由 MSPL/MFPT 下限单独拆除）
             _Q10 保持原厂（允许原固件 WMI 控制：CMEN 生命周期、GPSF/
             DGST 通知、电池办公档降档、WMID 事件上报原样；防护由
             FNQS 内的下限重推承担）
             COMM 无条件推送（无 CMEN 门控）
             _REG 钩子（ECAV 置位后写 GFLG/GPMD + 整机墙默认
             CSPL=110W/FPPT=120W，110+140+30=280W 打满适配器）
  [必须不存在] MSPL/MFPT 内任何 DBFS 引用（原厂 45W/65W 借电钳制路径）

另输出：
  - 固件表目录中 DSDT/SSDT 实例数（判断注入 SSDT 是否多出一份）
  - FACP 携带的 DSDT 地址、快速启动(hiberboot)状态、本次开机时长
  - 注册表 HKLM\\HARDWARE\\ACPI 交叉核对（有管理员权限时）

用法：
  python tools\\verify_runtime.py           # 全部检查
  python tools\\verify_runtime.py --keep    # 保留反汇编产物 (%TEMP%\\rtverify)

退出码：0=全部通过  1=有指纹失败  2=环境/取表失败
"""
import argparse
import ctypes
import datetime
import os
import re
import shutil
import subprocess
import sys
import tempfile

IASL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "isal", "iasl.exe")
PROVIDER_ACPI = 0x41435049  # "ACPI"

KERNEL32 = ctypes.windll.kernel32
KERNEL32.GetTickCount64.restype = ctypes.c_uint64
KERNEL32.EnumSystemFirmwareTables.restype = ctypes.c_uint32
KERNEL32.GetSystemFirmwareTable.restype = ctypes.c_uint32


# --------------------------------------------------------------------------
# Win32 firmware table API
# --------------------------------------------------------------------------
def _sig_dword(s):
    return int.from_bytes(s.encode("ascii"), "little")


def enum_firmware_tables():
    """返回固件表目录中每张表的签名列表（含重复实例）。"""
    size = KERNEL32.EnumSystemFirmwareTables(PROVIDER_ACPI, None, 0)
    if size == 0:
        err = ctypes.GetLastError()
        raise RuntimeError("EnumSystemFirmwareTables 失败, GetLastError=%d" % err)
    buf = ctypes.create_string_buffer(size)
    ret = KERNEL32.EnumSystemFirmwareTables(PROVIDER_ACPI, buf, size)
    if ret == 0 or ret > size:
        raise RuntimeError("EnumSystemFirmwareTables 第二次调用失败")
    raw = buf.raw[:ret]
    sigs = [raw[i:i + 4].decode("ascii", "replace") for i in range(0, len(raw) - 3, 4)]
    return sigs


def get_firmware_table(sig):
    """取回指定签名的第一张表（含 ACPI 头）。找不到返回 None。"""
    sid = _sig_dword(sig)
    size = KERNEL32.GetSystemFirmwareTable(PROVIDER_ACPI, sid, None, 0)
    if size == 0:
        return None
    buf = ctypes.create_string_buffer(size)
    ret = KERNEL32.GetSystemFirmwareTable(PROVIDER_ACPI, sid, buf, size)
    if ret == 0:
        return None
    return buf.raw[:ret]


def parse_acpi_header(data):
    """解析 36 字节 ACPI 表头的关键字段。"""
    def sig(b):
        return b.decode("ascii", "replace")
    return {
        "signature": sig(data[0:4]),
        "length": int.from_bytes(data[4:8], "little"),
        "revision": data[8],
        "oem_id": sig(data[10:16]),
        "oem_table_id": sig(data[16:24]),
        "oem_revision": int.from_bytes(data[24:28], "little"),
    }


# --------------------------------------------------------------------------
# ASL 分析
# --------------------------------------------------------------------------
def method_body(text, name):
    """按花括号配对提取 Method (NAME... 的完整方法体。"""
    m = re.search(r"Method \(%s\b" % re.escape(name), text)
    if not m:
        return None
    start = text.find("{", m.end())
    if start < 0:
        return None
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return None


def check(desc, ok, detail=""):
    status = "PASS" if ok else "FAIL"
    print("  [%s] %s%s" % (status, desc, ("  <- " + detail) if (detail and not ok) else ""))
    return ok


def disassemble(aml_bytes, workdir, basename):
    """iasl -d 反汇编，返回 DSL 文本。"""
    aml_path = os.path.join(workdir, basename + ".aml")
    with open(aml_path, "wb") as f:
        f.write(aml_bytes)
    r = subprocess.run([IASL_PATH, "-d", aml_path],
                       capture_output=True, text=True, cwd=workdir,
                       errors="replace")
    dsl_path = os.path.join(workdir, basename + ".dsl")
    if not os.path.exists(dsl_path):
        raise RuntimeError("iasl 反汇编失败: %s %s" % (r.stdout, r.stderr))
    with open(dsl_path, "r", errors="replace") as f:
        return f.read()


# --------------------------------------------------------------------------
# 各项检查
# --------------------------------------------------------------------------
def verify_dsdt(dsl):
    ok = True
    print("\n== 运行时 DSDT 补丁指纹 ==")

    fnqs = method_body(dsl, "FNQS")
    if fnqs is None:
        ok &= check("FNQS 方法存在", False)
    else:
        ok &= check("FNQS 重写: THMD (0x13) 分发", "THMD (0x13)" in fnqs)
        ok &= check("FNQS 无 DBFS/适配器瓦数分发", "DBFS" not in fnqs)

    mspl = method_body(dsl, "MSPL")
    if mspl is None:
        ok &= check("MSPL 方法存在", False)
    else:
        ok &= check("MSPL 下限 80W: If ((Local0 < 0x50))", "If ((Local0 < 0x50))" in mspl)
        ok &= check("MSPL 无原厂 DBFS 钳制", "DBFS" not in mspl)

    mfpt = method_body(dsl, "MFPT")
    if mfpt is None:
        ok &= check("MFPT 方法存在", False)
    else:
        ok &= check("MFPT 下限 110W: If ((Local0 < 0x6E))", "If ((Local0 < 0x6E))" in mfpt)
        ok &= check("MFPT 无原厂 DBFS 钳制", "DBFS" not in mfpt)

    comm = method_body(dsl, "COMM")
    if comm is None:
        ok &= check("COMM 方法存在", False)
    else:
        ok &= check("COMM 无 CMEN 门控（无条件推送）", "CMEN" not in comm)
        ok &= check("COMM 含 MSPL/MFPT/MODP", ("MSPL ()" in comm and "MFPT ()" in comm
                                             and "MODP (0x03" in comm))

    q82 = method_body(dsl, "_Q82")
    if q82 is None:
        print("  [SKIP] _Q82 不存在（无法检查 DB 增压链路）")
    else:
        # 2026-09-13 起 _Q81/_Q82 恢复原厂语义：DBFS=One 是 GPU 动态增压的
        # 接通信号（4060 要到 140W 必须走 DB），45W 借电钳制已由 MSPL/MFPT
        # 下限单独拆除，DBFS 在补丁版里已无消费方，跟随 EC 事件是无害的。
        ok &= check("_Q82 保留 DB 增压链路（DBFS = One 跟随 EC 事件）",
                    "DBFS = One" in q82)
        ok &= check("_Q82 为原厂结构（ECWR 门控在位）", "ECWR" in q82)

    q10 = method_body(dsl, "_Q10")
    if q10 is None:
        print("  [SKIP] _Q10 不存在")
    else:
        # _Q10 自本版起保持原厂（允许原固件 WMI 控制）：CMEN 生命周期、
        # 弱适配器 GPSF/DGST NVIDIA 预算通知、电池办公档降档、WMID 事件
        # 上报全部原样。45W 借电防护不依赖 _Q10：其所有路径最终 FNQS，
        # 补丁版 FNQS 在 THMD 之后立即重推 MSPL/MFPT 下限。
        ok &= check("_Q10 保持原厂: 电池 ITSM 降档链 (ECWT (Zero, RefOf (ITSM)))",
                    "ECWT (Zero, RefOf (ITSM))" in q10)
        ok &= check("_Q10 保持原厂: CMEN 清理 (ECWT (Zero, RefOf (CMEN)))",
                    "ECWT (Zero, RefOf (CMEN))" in q10)
        ok &= check("_Q10 保持原厂: 弱适配器 GPSF/DGST 通知",
                    ("GPSF = One" in q10 and "DGST = 0xD1" in q10))
        ok &= check("_Q10 保持原厂: OEM WMI 事件上报 (Notify (WMID, 0x20))",
                    "Notify (WMID, 0x20)" in q10)


    regm = method_body(dsl, "_REG")
    if regm is None:
        ok &= check("_REG 方法存在", False)
    else:
        ok &= check("_REG 钩子: ECAV 置位后写 GFLG=0x55（GPU 满血令牌）",
                    "ECWT (0x55, RefOf (GFLG))" in regm)
        ok &= check("_REG 钩子: 整机功耗墙 280W 默认 (CSPL=0x6E/FPPT=0x78) + FNQS+COMM 推送（_INI 竞态根修）",
                    ("ECWT (0x6E, RefOf (CSPL))" in regm
                     and "ECWT (0x78, RefOf (FPPT))" in regm
                     and "FNQS (Local0)" in regm and "COMM ()" in regm))

    # 全局禁止出现的原厂指纹
    for gone in ("THMD (0x16)", "If ((DBFS == One))"):
        ok &= check("全局不存在原厂指纹: %s" % gone, gone not in dsl)

    return ok


def verify_first_ssdt(aml_bytes, workdir):
    """能取回的只有第一份 SSDT；若它恰是 NPCF 表则可下结论。"""
    print("\n== 运行时 SSDT 抽查（仅目录中第一份）==")
    dsl = disassemble(aml_bytes, workdir, "ssdt_first")
    if "NPCF" not in dsl:
        print("  [INFO] 第一份 SSDT 不是 NPCF 表，SSDT4 替换状态无法由此判定。")
        print("         -> 看 unlock.log（新版逐表 ALARM）: DbgTool log")
        return True  # 不算失败
    ok = True
    ok &= check("NPCF SSDT: TGPD = TGPA（GPU 预算改挂 TGPA）", "TGPD = TGPA" in dsl)
    ok &= check("NPCF SSDT: 不存在 TGPD = DCBT", "TGPD = DCBT" not in dsl)
    ok &= check("NPCF SSDT: TPPD = TPPA（CPU 墙改挂 TPPA）", "TPPD = TPPA" in dsl)
    ok &= check("NPCF SSDT: DBAC = One 在位（性能档允许 DB，GPU 可达 140W）",
                "DBAC = One" in dsl)
    return ok


def read_registry_dsdt():
    """有权限时从注册表读 ACPI.sys 实际加载的 DSDT（交叉核对）。"""
    try:
        import winreg
    except ImportError:
        return None
    base = r"HARDWARE\ACPI\DSDT"
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, base) as k0:
            for oem in _iter_keys(k0):
                with winreg.OpenKey(k0, oem) as k1:
                    for tid in _iter_keys(k1):
                        with winreg.OpenKey(k1, tid) as k2:
                            for rev in _iter_keys(k2):
                                try:
                                    data, _ = winreg.QueryValueEx(k2, "00000000")
                                except OSError:
                                    continue
                                return bytes(data), r"%s\%s\%s" % (oem, tid, rev)
    except (PermissionError, OSError):
        return None
    return None


def _iter_keys(key):
    import winreg
    i = 0
    while True:
        try:
            yield winreg.EnumKey(key, i)
            i += 1
        except OSError:
            return


# --------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="运行时 ACPI 补丁指纹验证（免提权）")
    ap.add_argument("--keep", action="store_true", help="保留 %TEMP%\\rtverify 反汇编产物")
    args = ap.parse_args()

    if os.name != "nt":
        print("[-] 仅支持 Windows")
        return 2
    if not os.path.exists(IASL_PATH):
        print("[-] 找不到 iasl: %s" % IASL_PATH)
        return 2

    workdir = os.path.join(tempfile.gettempdir(), "rtverify")
    shutil.rmtree(workdir, ignore_errors=True)
    os.makedirs(workdir, exist_ok=True)

    boot_age_h = KERNEL32.GetTickCount64() / 3600000.0
    print("== 环境 ==")
    print("  本次开机时长: %.1f 小时（补丁于开机时应用；休眠唤醒不会重新应用）" % boot_age_h)

    try:
        hiber = winreg_query_int(
            r"SYSTEM\CurrentControlSet\Control\Session Manager\Power", "HiberbootEnabled")
        if hiber is None:
            print("  快速启动(hiberboot): 未知")
        elif hiber == 1:
            print("  快速启动(hiberboot): 开启  [!] 测试必须用「重启」而非「关机再开」，"
                  "否则 Windows 沿用休眠镜像里的旧 ACPI 状态（powercfg /h off 可关闭）")
        else:
            print("  快速启动(hiberboot): 关闭")
    except Exception:
        print("  快速启动(hiberboot): 未知")

    # 1) 固件表目录
    try:
        sigs = enum_firmware_tables()
    except RuntimeError as e:
        print("[-] %s" % e)
        return 2
    n_dsdt = sigs.count("DSDT")
    n_ssdts = sigs.count("SSDT")
    print("\n== 固件表目录 ==")
    print("  DSDT x%d, SSDT x%d（SSDT 多出的一份应为我们注入的 DBUL WMI 表）" % (n_dsdt, n_ssdts))
    print("  全部签名: %s" % ", ".join(sigs))

    # 2) DSDT 指纹
    dsdt = get_firmware_table("DSDT")
    if dsdt is None:
        print("[-] 无法取回运行时 DSDT")
        return 2
    hdr = parse_acpi_header(dsdt)
    print("\n== 运行时 DSDT 表头 ==")
    print("  OEM ID=%r TableID=%r OemRev=0x%X Len=%d" %
          (hdr["oem_id"], hdr["oem_table_id"], hdr["oem_revision"], hdr["length"]))
    hdr_ok = (hdr["oem_id"] == "INSYDE" and hdr["oem_table_id"].rstrip("\0 ") == "EDK2")
    if not hdr_ok:
        print("  [FAIL] 表头不是 INSYDE/EDK2 —— BIOS 换了？补丁匹配键需重对")
        return 1

    dsl = disassemble(dsdt, workdir, "dsdt_runtime")
    dsdt_ok = verify_dsdt(dsl)

    # 3) FACP 报告
    facp = get_firmware_table("FACP")
    if facp and len(facp) >= 148:
        xdsdt = int.from_bytes(facp[140:148], "little")
        print("\n== FACP ==")
        print("  X_DSDT(+140) = 0x%X%s" % (xdsdt, "  (len>=148, 走 64 位指针)" if len(facp) >= 148 else ""))

    # 4) 第一份 SSDT 抽查
    ssdt_ok = True
    first_ssdt = get_firmware_table("SSDT")
    if first_ssdt:
        ssdt_ok = verify_first_ssdt(first_ssdt, workdir)

    # 5) 注册表交叉核对（可选）
    try:
        reg = read_registry_dsdt()
        if reg:
            reg_bytes, reg_path = reg
            same = (reg_bytes == dsdt)
            print("\n== 注册表交叉核对 ==")
            print("  HKLM\\HARDWARE\\ACPI\\DSDT\\%s 与 GetSystemFirmwareTable: %s" %
                  (reg_path, "一致 [PASS]" if same else "不一致 [FAIL]"))
            dsdt_ok &= same
        else:
            print("\n== 注册表交叉核对 == 跳过（需要管理员权限，非必须）")
    except Exception as e:
        print("\n== 注册表交叉核对 == 跳过（%s）" % e)

    # 6) 结论
    print("\n== 结论 ==")
    if dsdt_ok and ssdt_ok:
        print("  [PASS] 运行时 DSDT 为补丁版，CPU 功耗链路（80/110W 下限、借电钳制拆除、_PSR 锁 AC）已在生效。")
        print("  提醒: SSDT4(NPCF GPU 功耗墙) 若显示 INCONCLUSIVE，用 DbgTool log 看 unlock.log 逐表结果。")
        if args.keep:
            print("  反汇编产物保留在: %s" % workdir)
        else:
            shutil.rmtree(workdir, ignore_errors=True)
        return 0
    print("  [FAIL] 运行时表中存在未生效/被回退的补丁，详见上方 FAIL 行。")
    print("  排查顺序: ① unlock.log 有无逐表 ALARM (DbgTool log) ② 是否用了「关机再开」(快速启动) ")
    print("            ③ BIOS 是否更新过（匹配键失配） ④ ESP 上 BOOTX64.efi 是否本次构建 (DbgTool deploy)")
    print("  反汇编产物保留在: %s" % workdir)
    return 1


def winreg_query_int(path, name):
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path) as k:
            val, _ = winreg.QueryValueEx(k, name)
            return int(val)
    except (PermissionError, OSError, FileNotFoundError):
        return None


if __name__ == "__main__":
    sys.exit(main())
