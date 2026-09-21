#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 docs/WMI_VARIABLE_MAP.md：本机的全变量地图。

数据来源（全部位于 tools/acpi/，2026-07-18 固件 dump）：
- dsdt.dsl  Device (H_EC) Field (ECF2)  -> EC RAM 偏移、名称、位宽
- ssdt3.dsl Device (WMID, "MIFS") WMAA  -> Bitland MIFS FUN1/FUN2 子码以及
                                          每个子码读写的 EC 字段
- ssdt4.dsl Device (NPCF)               -> GPU 功耗墙 Name() 变量
- ssdt9.dsl Device (AOD)                -> AMD OverDrive AM01-AM09 邮箱
- src/acpi/SsdtUnlockDB(.asl/.mof)      -> 自定义 DBUL WMTF 方法 1-17

语义为手工标注；凡属推断而非从 dump 证实的内容均标注 (推断) —— 与
acpi_patch/claims.py 的约定相同。

Usage: python tools/wmi_varmap.py   （写入 docs/WMI_VARIABLE_MAP.md）
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ACPI = os.path.join(REPO, "tools", "acpi")
OUT_PATH = os.path.join(REPO, "docs", "WMI_VARIABLE_MAP.md")


def read(name):
    with open(os.path.join(ACPI, name), "r", encoding="utf-8", errors="replace") as f:
        return f.read()


# ---------------------------------------------------------------------------
# 1. H_EC ECF2 字段地图（EC RAM 偏移 -> ASL 名称）

def parse_ec_fields():
    """遍历 Device (H_EC) 内的 Field (ECF2 ...)：以与 AML 编译器完全一致的
    方式累计字节偏移与位位置。"""
    text = read("dsdt.dsl")
    lines = text.splitlines()
    start = next(i for i, l in enumerate(lines) if "Device (H_EC)" in l)
    fstart = next(i for i in range(start, len(lines)) if "Field (ECF2" in lines[i])
    fields = []
    offset = 0
    bit = 0
    for line in lines[fstart + 1:]:
        code = line.split("//")[0]
        if "}" in code and fields:
            break
        m = re.search(r"Offset \(0x([0-9A-Fa-f]+)\)", code)
        if m:
            offset = int(m.group(1), 16)
            bit = 0
            continue
        m = re.match(r"\s*,\s*(\d+)\s*,?\s*$", code)
        if m:
            bit += int(m.group(1))
            offset += bit // 8
            bit %= 8
            continue
        m = re.match(r"\s*(\w+)\s*,\s*(\d+)\s*,?\s*$", code)
        if m:
            name, width = m.group(1), int(m.group(2))
            fields.append((name, offset, bit, width))
            bit += width
            offset += bit // 8
            bit %= 8
    return fields


# 手工整理的语义；缺失名称回退为 "(语义未注释)"
# kind: 'b' = 已证实（dump 内有消费者逻辑），'i' = 推断
EC_SEMANTICS = {
    "HKVC":  ("热键事件计数/校验（HKVC）", "i"),
    "EVMR":  ("事件缓冲区（EVMR/EVMN/EVT1/EVT2：事件读出区）", "i"),
    "HTKS":  ("热键状态开始/结束标记（HTKS/HTKE）", "i"),
    "TSR1":  ("温度传感器 1（°C，推断）", "i"),
    "TSR2":  ("温度传感器 2（°C，推断）", "i"),
    "TSR3":  ("温度传感器 3（°C，推断）", "i"),
    "TSR4":  ("温度传感器 4（°C，推断）", "i"),
    "TSR5":  ("温度传感器 5（°C，推断）", "i"),
    "TSR6":  ("温度传感器 6（°C，推断）— MIFS 0xFA00/0x1600 上报", "b"),
    "TSR7":  ("温度传感器 7（°C，推断）", "i"),
    "TSR8":  ("温度传感器 8（°C，推断）", "i"),
    "TSR9":  ("温度传感器 9（°C，推断）", "i"),
    "LSTE":  ("灯效/灯带状态位（LSTE，@0x20 bit0）", "i"),
    "FNHK":  ("Fn 热键位域（@0x20 bit3）— MIFS 0x0B00 读写", "b"),
    "CRHK":  ("组合热键位（@0x20 bit5）", "i"),
    "OCFL":  ("超频标志位（@0x20 bit6）", "i"),
    "PJID":  ("平台/机型 ID 位（@0x21 bit1）", "i"),
    "GSTS":  ("全局状态（GSTS）", "i"),
    "HKST":  ("热键状态（HKST）", "i"),
    "TOCP":  ("OEM 功能开关位（TOCP，@0x25 bit0）— MIFS 0x0C00 读写", "b"),
    "CALK":  ("计算器键位（@0x25 bit1）", "i"),
    "NULK":  ("小键盘位（@0x25 bit2）", "i"),
    "WINK":  ("Win 键锁定位（@0x25 bit5）", "i"),
    "AST1":  ("适配器状态 1（AST1）", "i"),
    "SMPR":  ("SMBus 邮箱：协议（SMPR/SMST/SMAD/SMCD/SDAT*）", "i"),
    "SMCN":  ("SMBus 邮箱：命令（SMCN）", "i"),
    "BS50":  ("电池子系统数据块（BS50/BS54/BS56）", "i"),
    "FASP":  ("风扇策略/加速配置（FASP）— MIFS 0x1500 读写", "b"),
    "ECWR":  ("电源状态位域。bit0=AC 在位(推断)，bit7=MIFS 0x1300 测试位；"
              "bit3 与放电相关（_BST 0x08 测试，推断）", "b"),
    "PAWT":  ("适配器功率暂存（PAWT）", "i"),
    "B1SN":  ("电池 1 序列号（B1SN，16bit）", "b"),
    "B1DC":  ("电池 1 设计容量（B1DC，16bit）", "b"),
    "B1FV":  ("电池 1 设计电压（B1FV，16bit）", "b"),
    "B1FC":  ("电池 1 最后满充容量（B1FC，16bit）", "b"),
    "BTPT":  ("电池 1 温度?（BTPT，16bit，推断）", "i"),
    "B1CR":  ("电池 1 当前电流（B1CR，16bit，推断）", "i"),
    "B1RC":  ("电池 1 剩余容量（B1RC，16bit）", "b"),
    "B1VT":  ("电池 1 电压（B1VT，16bit）", "b"),
    "BALM":  ("电池 1 报警容量（BALM，16bit）", "i"),
    "BCYC":  ("电池 1 循环次数（BCYC，16bit）", "b"),
    "B1DA":  ("电池 1 数据（B1DA，16bit）", "i"),
    "B1TP":  ("电池 1 类型（B1TP，16bit）", "i"),
    "BRSC":  ("电池 1 化学/规格串偏移（BRSC）", "i"),
    "MIDL":  ("制造商 ID 低/高（MIDL/MIDH）", "i"),
    "HIDL":  ("硬件 ID 低/高（HIDL/HIDH）", "i"),
    "FWVL":  ("EC 固件版本 低/高（FWVL/FWVH）", "i"),
    "DAVL":  ("设备可用性 低/高（DAVL/DAVH）", "i"),
    "BFUD":  ("电池固件更新数据（BFUD，16bit）", "i"),
    "B1TE":  ("电池 1 温度（B1TE，16bit，0.1K，推断）", "i"),
    "B1TF":  ("电池 1 状态标志（B1TF，16bit）", "i"),
    "AWHG":  ("适配器功率 高字节（AWHG）— 与 AWLW 组成 W 单位功率值；"
              ">0xC8(200W) 时固件走 200W 分支（_REG/_Q10/FNQS 已被补丁移除）", "b"),
    "AWLW":  ("适配器功率 低字节（AWLW）", "b"),
    "FWEN":  ("固件事件使能位（@0x90 bit1）", "i"),
    "FUEN":  ("固件更新使能位（@0x90 bit2）", "i"),
    "BATM":  ("电池管理命令（BATM）", "i"),
    "BBHL":  ("电池充电上限（%，DBUL WMI Method 4/5 读写）", "b"),
    "BBLP":  ("电池充电下限?（BBLP，推断）", "i"),
    "BBHM":  ("电池健康模式（BBHM，推断）", "i"),
    "KBNL":  ("键盘背光等级（0/1/2/3）— MIFS 0x1200 读写", "b"),
    "F1HI":  ("风扇 1 转速高字节（F1HI，RPM=(hi<<8)|lo，MIFS 0x0D00）", "b"),
    "F1LO":  ("风扇 1 转速低字节（F1LO）", "b"),
    "F2HI":  ("风扇 2 转速高字节（F2HI，MIFS 0x0D00）", "b"),
    "F2LO":  ("风扇 2 转速低字节（F2LO）", "b"),
    "PABD":  ("电源适配器?数据（PABD，推断）", "i"),
    "TFLG":  ("敏感写解锁令牌：写 0x55 后才允许后续 ECWT（固件多处先 ECWT(0x55,TFLG)）"
              "— DBUL WMI 0xD0", "b"),
    "GFLG":  ("GPU 通道解锁标志：0x55=解锁 — MIFS 0x0900 写、DBUL 0xD1/_INI", "b"),
    "GPMD":  ("GPU 功耗模式：1=满血(独显全开)，0=受限 — MIFS 0x0900、DBUL 0xD2", "b"),
    "SSDK":  ("系统 SDK/调试寄存器（SSDK）；bit2=FWDE，bit5=FAAP", "i"),
    "FWDE":  ("固件开关位（@0xE2 bit2）— MIFS 0x0F00 读写", "b"),
    "FAAP":  ("风扇自动加速开关位（@0xE2 bit5）— MIFS 0x1400 读写", "b"),
    "BPWM":  ("风扇 PWM 占空比（0-255，推断）— DBUL WMI 0xE3", "i"),
    "ITSM":  ("电源模式档位 0/1/2（0=满血/1=平衡/2=安静，推断；被 FNQS/_Q10/MIFS 0x0800/0x1700 广泛使用）"
              "— DBUL WMI 0xE4", "b"),
    "ECTP":  ("EC 温度/性能参数（ECTP，推断）", "i"),
    "LEDM":  ("LED 模式（1/2）— MIFS 0x1000 读写", "b"),
    "RGBR":  ("键盘 RGB 红（MIFS 0x1100）", "b"),
    "RGBG":  ("键盘 RGB 绿（MIFS 0x1100）", "b"),
    "RGBB":  ("键盘 RGB 蓝（MIFS 0x1100）", "b"),
    "CMEN":  ("充电管理使能位（@0xF0 bit3）— MIFS 0x1700 充电管理路径", "b"),
    "DGPU":  ("平台配置字：独显模式标志（NIGS 分支 DGPU==One 判断）— DBUL WMI 0xF2", "b"),
    "CPUT":  ("平台配置字：CPU/平台 SKU（NIGS 分支 CPUT==0x07 判断；Method 16 以 0x09 区分 75W 钳制）"
              "— DBUL WMI 0xF3", "b"),
    "STSM":  ("系统模式/状态（充电管理路径写）— DBUL WMI 0xF4", "i"),
    "CSPL":  ("CPU SPL（PL1，W）— DBUL WMI 0xF6；MSPL 下限 0x50(80W) 后经 ALIB 推 SMU", "b"),
    "FPPT":  ("CPU fPPT（PL2，W）— DBUL WMI 0xF7；MFPT 下限 0x6E(110W)", "b"),
    "CTCL":  ("CPU 第三限值（W；经 COMM→ALIB 0x03 推送，语义推断为 cTCL/温度墙）— DBUL WMI 0xF8", "b"),
}


# ---------------------------------------------------------------------------
# 2. ssdt3 WMAA（Bitland MIFS）case 解析

def parse_mifs_cases():
    """返回 ssdt3 中 Method (WMAA) 的 FUN2 子码对应的
    {fun2_code: {'rw': 'R'|'W'|'R/W', 'fields': [names]}}
    （FUN1 0xFA00=读，0xFB00=写）。"""
    lines = read("ssdt3.dsl").splitlines()
    start = next(i for i, l in enumerate(lines) if "Method (WMAA, 3, Serialized)" in l)
    entries = {}   # (fun1, fun2) -> {'R': set, 'W': set}
    depth = 0
    fun1 = None
    fun1_depth = -1
    fun2 = None
    fun2_depth = -1
    for line in lines[start:]:
        code = line.split("//")[0]
        m = re.search(r"Case \(0x([0-9A-Fa-f]+)\)", code)
        if m:
            val = int(m.group(1), 16)
            if val in (0xFA00, 0xFB00):
                fun1 = val
                fun1_depth = depth
                fun2 = None
            elif fun1 is not None and depth == fun1_depth + 2:
                fun2 = val
                fun2_depth = depth
                entries.setdefault((fun1, fun2), {"R": set(), "W": set()})
        if fun2 is not None:
            key = (fun1, fun2)
            for fm in re.finditer(r"ECRD \(RefOf \(([\w.\\]+)\)\)", code):
                entries[key]["R"].add(fm.group(1).split(".")[-1])
            for fm in re.finditer(r"ECWT \(.*, RefOf \(([\w.\\]+)\)\)", code):
                entries[key]["W"].add(fm.group(1).split(".")[-1])
        depth += code.count("{") - code.count("}")
        if fun2_depth >= 0 and depth < fun2_depth:
            fun2 = None
            fun2_depth = -1
        if fun1_depth >= 0 and depth < fun1_depth:
            fun1 = None
            fun1_depth = -1
        if depth < 0:
            break

    cases = {}
    for (f1, f2), rw in entries.items():
        r, wr = rw["R"], rw["W"]
        kind = "R/W" if r and wr else ("R" if r else "W")
        fields = sorted(r | wr)
        old = cases.get(f2)
        if old:  # 同一子码在两个 FUN1 分支下都出现：合并
            kind = "R/W" if old["rw"] != kind else kind
            fields = sorted(set(fields) | set(old["fields"]))
        cases[f2] = {"rw": kind, "fields": fields}
    return cases


MIFS_FUN2_LABELS = {
    0x0800: "电源模式档位 (ITSM)",
    0x0900: "GPU 满血模式开关 (GFLG/GPMD)",
    0x0B00: "Fn 热键位域 (FNHK)",
    0x0C00: "OEM 功能开关 (TOCP)",
    0x0D00: "风扇 1/2 转速读数 (F1LO/F1HI/F2LO/F2HI)",
    0x0F00: "固件开关位 (FWDE)",
    0x1000: "LED 模式 (LEDM)",
    0x1100: "键盘 RGB (RGBR/RGBG/RGBB)",
    0x1200: "键盘背光 (KBNL)",
    0x1300: "电源状态位 ECWR&0x80 分类",
    0x1400: "风扇自动加速 (FAAP)",
    0x1500: "风扇策略 (FASP)",
    0x1600: "温度传感器 6 (TSR6)",
    0x1700: "充电管理 + CPU 功耗直写 (CMEN/STSM/ITSM/CSPL/FPPT/CTCL)",
}


# ---------------------------------------------------------------------------
# 3. NPCF 变量（ssdt4）

def parse_npcf():
    text = read("ssdt4.dsl")
    start = text.index("Device (NPCF)")
    end = text.index("Method (_HID", start)
    block = text[start:end]
    vars_ = re.findall(r"Name \((\w+), (0x[0-9A-Fa-f]+|Zero|One)\)", block)
    return vars_


NPCF_SEMANTICS = {
    "ACBT": ("AC 侧 GPU 门限（W）。原始固件无任何消费者（死变量）", "dead"),
    "DCBT": ("DC 侧 GPU 门限（W）。唯一消费者 TGPD=DCBT 已被 ssdt4-tgpd-ac 移除 → "
             "对 ASL 通道无效；DBUL WMI Method 6/7 和 _INI 仍读写它但不再影响 GPU 预算", "dead"),
    "DBAC": ("Dynamic Boost AC 开关。保持原厂未补（2026-09-13 反转：DB 增压是 4060 "
             "动态到 140W 的必要通道），值由 NIGS 分支按需置 Zero/One", "stock"),
    "DBDC": ("Dynamic Boost DC 钳制开关（无消费者）", "dead"),
    "AMAT": ("（无消费者，原始即死）", "dead"),
    "AMIT": ("（无消费者，原始即死）", "dead"),
    "ATPP": ("CPU/平台 AC 功耗墙，0.5W 单位。原始 0x01B8(220W)，NIGS 分支会降为 0x0168(180W)；"
             "ssdt4-atpp-280w 把全路径恢复为 0x0230(280W)", "patched"),
    "ATP2": ("平台 AC 功耗墙 2，0.5W 单位。原始 0x0208(260W)，NIGS 降档 0x0168→补丁 0x0230", "patched"),
    "DTPP": ("DC 平台墙，0.5W 单位。唯一消费者 TPPD=DTPP 已被 ssdt4-tppd-ac 改为 TPPD=TPPA → 死", "dead"),
    "TPPL": ("平台功耗墙锁存（无读者，推断）", "dead"),
    "DROS": ("（无消费者）", "dead"),
    "HPCT": ("HP 计数/高温保护计数（推断）", "live"),
    "IOBS": ("NVIO IO region 基址；Case(One) 主体已被 ssdt4-iobs-nvio-kill 用 If(Zero) 杀掉", "patched"),
    "CMPL": ("CPU ML 上报值（F6MP）。0x33(51)→0x50(80)，ssdt4-cmpl-80w", "patched"),
    "CNPL": ("CPU NL 上报值（F6NP）。0x10(16)→0x36(54)，ssdt4-cnpl-54w", "patched"),
    "CDIS": ("NPCF 设备禁用标志（_STA/_HID 用）", "live"),
    "CUSL": ("（无消费者）", "dead"),
    "CUCT": ("（无消费者）", "dead"),
}

NPCF_FIELDS = {
    "TGPA": ("GPU AC 功耗预算（PBD2 @0x05，0.5W）：ITSM==1→0x0118(140W)，ITSM==0→0x78(60W)。"
             "补丁后 TGPD=TGPA，是 GPU 的实际预算来源", "live"),
    "TPPA": ("CPU AC 功耗预算（PBD2 @0x19，0.5W）=ATPP 副本；补丁后 TPPD=TPPA", "live"),
    "TGPD": ("上报给 NVIDIA 驱动的 GPU 当前平台墙。补丁前 =DCBT，补丁后 =TGPA", "live"),
    "TPPD": ("上报的 CPU 当前平台墙。补丁前 =DTPP，补丁后 =TPPA", "live"),
    "NIGS": ("NVIDIA 通知的子功能选择（CreateField @0x28，2bit）：0=setup，1=teardown", "live"),
}


# ---------------------------------------------------------------------------
# 4. DBUL WMTF 方法状态表（来自 src/acpi/SsdtUnlockDB.asl/.mof）

WMTF = [
    (1, "SetDbfs", "残", "写 \\DBFS 后调 FNQS+COMM。补丁后 FNQS 不再读 DBFS（档位由 ITSM 分发），"
                       "且 _Q81/_Q82 保持原厂：EC 事件会按真实增压状态改写 DBFS —— 设置可能被打回"),
    (2, "GetDbfs", "活", "读 \\DBFS（0=禁/1=启/0xFFFFFFFF=不存在）"),
    (3, "GetVersion", "活", "返回 VERS=构建时间 UTC epoch。**表生效探针**：能读到新值=注入 SSDT 已被 Windows 加载"),
    (4, "SetBatteryChargeThreshold", "活", "写 BBHL（充电上限 %）"),
    (5, "GetBatteryChargeThreshold", "活", "读 BBHL"),
    (6, "SetThresholds", "死效", "写 ACBT/DCBT/AMAT/AMIT —— 四者在 ASL 通道均无消费者（见死变量登记表），"
                                 "写入无效果，仅 EC/NPCF 镜像值变化"),
    (7, "GetThresholds", "活(读)", "读 ACBT/DCBT/AMAT/AMIT（值本身可读，但与实际功耗行为无关）"),
    (8, "SetEcRegister", "活", "EC 写白名单（走 ECRD/ECWT 互斥）。0xD0 TFLG、0xD1 GFLG、0xD2 GPMD、"
                               "0xE3 BPWM、0xE4 ITSM、0xF2 DGPU、0xF3 CPUT、0xF4 STSM、0xF6 CSPL、"
                               "0xF7 FPPT、0xF8 CTCL（+ 本次扩充的 7 个固件亲自写过的字段）"),
    (9, "GetEcRegister", "活", "EC 读白名单，与 Method 8 同一组 offset"),
    (10, "SetOverrideLock", "死", "写 SSDT 局部 Name(WMOV)，无任何消费者、重启即失"),
    (11, "GetOverrideLock", "死", "读 WMOV（永远读到上一次 Method 10 写的值）"),
    (12, "SetSwSmi", "死", "写 Name(WSMI)，不触发任何 SMI，无消费者"),
    (13, "GetSwSmiStatus", "死", "读 WSMI"),
    (14, "SetAmdAlibDirect", "活", "调 \\_SB.ALIB(0x0C, {sub, data}) —— MODP 通道（SMU 功耗邮箱）"),
    (15, "(已删)", "-", "GetAmdAlibDirect 已移除（占位实现无意义）；ID 保留，落到默认 Return (0xFFFFFFFF)"),
    (16, "GetDynamicBoost", "活", "V4D1=DBFS、V4D2=CSPL、V4D3=FPPT、V4D4=原厂钳制上限(仅参考)"),
    (17, "GetEcByte", "活", "全量 EC RAM 读 0x00-0xFF（自建 ECMR region，只读安全）"),
    (18, "(空号)", "-", "保留；本次新增能力并入 Method 8 白名单扩充"),
    (19, "ApplyPowerLimits", "活(新)", "ECWT(CSPL/FPPT/CTCL) 后调 COMM()（MSPL+MFPT+CTCL→ALIB 推 SMU）"
                                       "—— 运行时改功耗的正规入口，解决'_INI 写完被回写'"),
]


# ---------------------------------------------------------------------------
# 5. AOD AM01-AM09（ssdt9）

AOD = [
    ("AM01", "读", "返回 AOD 接口版本（AODV=0x06）"),
    ("AM02", "写", "初始化标志 GF01=One"),
    ("AM03", "读", "返回 \\OBID（接口信息块）"),
    ("AM04", "读", "按索引查 OBIT 只读条目（WCNS 快照）"),
    ("AM05", "写", "SMU 邮箱命令：SCMI/SCMD 写入后经 IO 端口 0xB0 触发 SMI，返回 OUTB"),
    ("AM06", "读", "按索引查 OBIT 可写条目"),
    ("AM07", "读", "返回 BSPD（板级速度/档位表）"),
    ("AM08", "读", "返回 \\OBIE"),
    ("AM09", "读", "返回 RMPD（1120bit 结果缓冲）"),
]


# ---------------------------------------------------------------------------
# 辅助函数

def _esc(s):
    return s.replace("|", "\\|")


def fmt_offset(off, bit):
    if bit:
        return f"0x{off:02X}.{bit}"
    return f"0x{off:02X}"


def field_sem(name):
    return EC_SEMANTICS.get(name, ("（语义未注释）", "i"))


def dbul_access_for(offset):
    """若裸 EC 偏移在白名单内，返回其 DBUL WMI 访问路径。"""
    m = {0x20: "Method 8/9", 0x25: "Method 8/9", 0x5F: "Method 8/9",
         0x97: "Method 4/5", 0x9A: "Method 8/9",
         0xD0: "Method 8/9", 0xD1: "Method 8/9", 0xD2: "Method 8/9",
         0xE2: "Method 8/9", 0xE3: "Method 8/9", 0xE4: "Method 8/9",
         0xE8: "Method 8/9", 0xF2: "Method 8/9", 0xF3: "Method 8/9",
         0xF4: "Method 8/9", 0xF6: "Method 8/9, 19", 0xF7: "Method 8/9, 19",
         0xF8: "Method 8/9"}
    return m.get(offset, "Method 17(仅读)" if offset <= 0xFF else "")


# ---------------------------------------------------------------------------
# 主流程

def build_doc():
    ec_fields = parse_ec_fields()
    mifs = parse_mifs_cases()
    npcf = parse_npcf()
    by_offset = {}
    for name, off, bit, width in ec_fields:
        by_offset.setdefault((off, bit, width), []).append(name)

    L = []
    w = L.append

    w("# WMI 变量全景地图（蛟龙 16 PRO / INSYDE EDK2 固件）")
    w("")
    w("> 由 `tools/wmi_varmap.py` 自动生成于固件 dump（tools/acpi/，2026-07-18）。"
      "手工注释中标 (推断) 的语义未经 dump 证实；标 (b) 的有 dump 内消费者逻辑佐证，"
      "标 (i) 的为推断。修改变量前先读 §7 死变量登记表。")
    w("")

    # --- 0 验收
    w("## 0. 如何确认重定向 ACPI 表已被 Windows 使用（验收清单）")
    w("")
    w("1. 刷入新 EFI 后启动进 Windows，`\\EFI\\BOOT\\unlock.log` 应有 "
      "`[+] All ACPI table replacements applied successfully.` 且无任何 `ALARM` 行。")
    w("2. DbgTool → `GetVersion`（WMTF Method 3）：返回的构建时间应为最新构建 —— "
      "只有注入的 DBUL SSDT 被加载才可能读到。")
    w("3. DbgTool → `CheckRegistration`：`root\\wmi` 中能枚举到 `CustomDbUnlock` 实例。")
    w("4. `acpidump`（或 `asl /tab=DSDT`）导出运行时 DSDT：OEM ID 应为 `INSYDE`、"
      "Table ID `EDK2`，且包含补丁特征（FNQS 中 `THMD (0x13)`/`THMD (Zero)` 调度、"
      "MSPL `If ((Local0 < 0x50))` 下限）。原理：Windows 在 ExitBootServices 后一次性消费 "
      "bootloader 递的表，之后不会回 SPI flash 重读 —— 指针重定向即生效。")
    w("5. 交叉验证：`GetDynamicBoost`（Method 16）的 CSPL/FPPT 应等于 _INI 写入的 "
      "0x55(85W)/0x6E(110W)，而不是原厂默认。")
    w("")

    # --- 1 三个 WMI 接口
    w("## 1. 三个 WMI 接口总览")
    w("")
    w("| 设备 | _UID | GUID | 方法 | 用途 |")
    w("|---|---|---|---|---|")
    w("| `\\_SB.PCI0.LPC0.DBUL`（注入） | 0xAA | `{850C1B62-A3D9-4E65-B8D5-DE3C74C9B38D}` | WMTF 1-19 | "
      "本 bootloader 自定义：EC 白名单读写、阈值、版本探针 |")
    w("| `\\_SB.PCI0.WMID`（固件，Bitland MIFS） | \"MIFS\" | `{B60BFB48-3E5B-49E4-A0E9-8CFFE1B3434B}` | WMAA | "
      "OEM 控制中心接口：电源模式、GPU 模式、风扇、RGB、**CPU 功耗直写**（Linux 内核已收录驱动文档） |")
    w("| `\\AOD`（固件，AMD OverDrive） | \"AOD\" | `{ABCB0F6A-8EA1-11D1-00A0-C90629100000}` | WMAA→AM01-09 | "
      "AMD SMU 邮箱：性能/功耗参数 |")
    w("")
    w("MIFS 调用约定（WMAA，32 字节 InData/OutData 缓冲）：`FUN1`(word@0)=0xFA00 读 / 0xFB00 写；"
      "`FUN2`(word@2)=子功能码；`FUN3`(byte@4)/`FUN4`(byte@5)/`FUN5`(byte@6)=参数；"
      "返回 `SGER`(word@0)=0x8000 表示完成，`FRD0..FRD4`=数据。")
    w("")

    # --- 2 MIFS 子码
    w("## 2. Bitland MIFS FUN2 子码 → EC 字段（自动解析自 ssdt3.dsl）")
    w("")
    w("| FUN2 | 功能 | 方向 | 触达的 EC 字段 |")
    w("|---|---|---|---|")
    for code in sorted(mifs):
        label = MIFS_FUN2_LABELS.get(code, "(未注释)")
        e = mifs[code]
        w(f"| 0x{code:04X} | {_esc(label)} | {e['rw']} | {', '.join(e['fields']) or '(无)'} |")
    w("")
    w("> **0x1700 是固件亲自写 CPU 功耗的正规通道**：写入路径为 CMEN/STSM（充电管理）与 "
      "CSPL/FPPT/CTCL（FUN3/FUN4 直写 W 值）。OEM 控制软件改性能档走的就是这里。")
    w("")

    # --- 3 EC RAM 地图
    w("## 3. EC RAM 变量地图（H_EC Field ECF2，自动解析 offset）")
    w("")
    w("| Offset | 名称 | 位宽 | 语义 | DBUL 访问 | MIFS 访问 |")
    w("|---|---|---|---|---|---|")
    mifs_by_field = {}
    for code, e in mifs.items():
        for f in e["fields"]:
            mifs_by_field.setdefault(f, []).append(
                f"0x{code:04X}({e['rw']})")
    for name, off, bit, width in ec_fields:
        sem, _ = field_sem(name)
        dbul = dbul_access_for(off)
        mifs = ", ".join(mifs_by_field.get(name, []))
        w(f"| {fmt_offset(off, bit)} | {name} | {width} | {_esc(sem)} | {dbul or '-'} | {mifs or '-'} |")
    w("")

    # --- 4 NPCF
    w("## 4. `_SB.NPCF` GPU 功耗变量（ssdt4，补丁状态已标注）")
    w("")
    w("| 名称 | 原始值 | 语义/状态 |")
    w("|---|---|---|")
    patch_values = {"CMPL": ("0x33", "0x50"), "CNPL": ("0x10", "0x36"),
                    "DCBT": ("0x28", "0x50")}
    for name, val in npcf:
        sem, status = NPCF_SEMANTICS.get(name, ("", "live"))
        orig = f"`{val}`"
        if name in patch_values:
            old, new = patch_values[name]
            orig = f"`{val}` → 补丁后 `{new}`"
        w(f"| {name} | {orig} | {_esc(sem)} |")
    for name, (sem, _st) in NPCF_FIELDS.items():
        if name in ("TGPA", "TPPA", "TGPD", "TPPD", "NIGS"):
            w(f"| {name} | （字段/缓冲） | {_esc(sem)} |")
    w("")

    # --- 5 WMTF 状态
    w("## 5. DBUL WMTF 方法状态表（自定义接口 1-19）")
    w("")
    w("| ID | 方法 | 状态 | 说明 |")
    w("|---|---|---|---|")
    for mid, name, status, note in WMTF:
        w(f"| {mid} | {name} | {status} | {_esc(note)} |")
    w("")

    # --- 6 AOD
    w("## 6. AMD AOD AM01-AM09（ssdt9）")
    w("")
    w("| 方法 | 方向 | 说明 |")
    w("|---|---|---|")
    for name, direction, note in AOD:
        w(f"| {name} | {direction} | {_esc(note)} |")
    w("")
    w("> 自定义接口的 Method 14（SetAmdAlibDirect）只覆盖 ALIB(0x0C) 一条路径"
      "（= MSPL/MFPT/COMM 用的 MODP 通道）；更底层的 SMU 读写走本表 AM05。")
    w("")

    # --- 7 死变量登记
    w("## 7. 死变量 / 断链登记表（\"设了没生效\"的原因）")
    w("")
    w("| 变量/方法 | 状态 | 原因 |")
    w("|---|---|---|")
    w("| DCBT（NPCF） | 断链 | 唯一消费者 `TGPD = DCBT` 被 ssdt4-tgpd-ac 改为 `TGPD = TGPA`；"
      "_INI 与 WMI Method 6 的 DCBT 写入不再影响 GPU |")
    w("| DTPP（NPCF） | 断链 | 唯一消费者被 ssdt4-tppd-ac 改为 `TPPD = TPPA` |")
    w("| ACBT / AMAT / AMIT（NPCF） | 原始死 | 全部 dump 中零消费者（补丁前后都无效） |")
    w("| DBFS | 不持久 | ssdt4 无关；_Q81/_Q82 保持原厂（DBFS 跟随 EC 事件），"
      "WMI Method 1 的设置会被后续 EC 事件按真实增压状态改写 |")
    w("| WMOV / WSMI（DBUL Method 10-13） | 死 | 只写 SSDT 局部 Name，无消费者，不持久 |")
    w("| Method 15 | 已删 | 占位实现已移除；未匹配方法统一落到默认 Return (0xFFFFFFFF) |")
    w("| CSPL/FPPT/GPMD（_INI 写入） | 会被回写 | OEM 软件与 _Q10 等 EC 事件会经 "
      "MIFS 0x1700/0x0900 与 FNQS 重写；运行时调整请用 Method 19 (ApplyPowerLimits) 推送 |")
    w("")

    # --- 8 工作流
    w("## 8. 推断工作流（摸清未注释变量）")
    w("")
    w("1. DbgTool → `DumpEcRam`（Method 17）存快照 A。")
    w("2. 改变一个状态（按 Fn 组合键 / 切电源模式 / 插拔 AC / 改一项 OEM 软件设置）。")
    w("3. 再存快照 B，diff 高亮变化的 offset —— 变化者即为该功能涉及的 EC 寄存器。")
    w("4. 对变化的 offset 反查 §3 表得 ASL 名称；再全表 grep 该名称得消费者方法/事件。")
    w("5. 写入类推断：优先走 MIFS 0xFB00（固件自己的写路径），确认无害后再考虑收编进 "
      "DBUL Method 8 白名单。")
    w("")

    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")
    print(f"[+] wrote {OUT_PATH}")
    print(f"    EC fields: {len(ec_fields)}, MIFS sub-codes: {len(mifs)}, "
          f"NPCF vars: {len(npcf)}")


if __name__ == "__main__":
    sys.exit(build_doc())
