"""补丁注释中事实性声明的机器校验溯源。

本模块的规则：补丁管线中的注释要陈述一个事实，当且仅当此处的某个检查能从
dump 数据中复现它。每条声明得到一个判定：

- PASS       dump/片段/常量证据复现了该声明
- FAIL       证据与声明矛盾（注释必须修正）
- ASSUMPTION 该声明无法从 dump 证明；它是一条推断（单位约定、EC 位语义）。
             保持标注，绝不悄悄冒充事实。

`run_audit`（--audit）在任何 FAIL 上使运行失败。
"""

import os
import re
from dataclasses import dataclass

PASS, FAIL, ASSUMPTION = "PASS", "FAIL", "ASSUMPTION"

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DUMPS = {
    "dsdt": os.path.join(REPO, "tools", "acpi", "dsdt.dsl"),
    "ssdt4": os.path.join(REPO, "tools", "acpi", "ssdt4.dsl"),
}
FRAG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fragments")


@dataclass
class Claim:
    id: str
    claim: str
    source: str          # 声明所在位置
    check: str           # 机器检查做什么
    verdict: str = ""
    detail: str = ""


# --------------------------------------------------------------------------
# 工件加载

_FILES = {}


def _file(key):
    if key not in _FILES:
        from .engine import read_fragment
        if key.startswith("frag:"):
            _FILES[key] = read_fragment(FRAG_DIR, key[5:]).splitlines(keepends=True)
        else:
            with open(DUMPS[key], "r", encoding="utf-8") as f:
                _FILES[key] = f.readlines()
    return _FILES[key]


def _text(key):
    return "".join(_file(key))


def _line_prefix(key, lineno, prefix):
    """证据样式 'file.dsl:NNNN <token>' —— 校验第 NNNN 行以 token 开头。"""
    lines = _file(key)
    if not 1 <= lineno <= len(lines):
        return False, f"line {lineno} out of range ({len(lines)} lines)"
    actual = lines[lineno - 1].strip()
    ok = actual.startswith(prefix)
    return ok, actual


def _contains(key, needle):
    return needle in _text(key)


def _count(key, needle):
    return _text(key).count(needle)


def _has_sequence(key, seq):
    """若干连续行，其剥离注释后的文本依次以 seq 的每个元素开头。"""
    from .engine import has_sequence
    return has_sequence(_file(key), seq)


# --------------------------------------------------------------------------
# THMP profile 表（dsdt.dsl 'Name (THMP, Package (0x17)'）：经 ALIB(0x0C) 发往
# EC/NVIC 邮箱的权威逐 profile 功耗限制。

def parse_thmp():
    """把 dsdt.dsl 中的 Name (THMP, Package (0x17)) 解码为一个扁平
    (command, value, command, value, ...) 列表的列表，每个 profile 一项。"""
    lines = _file("dsdt")
    start = next(i for i, l in enumerate(lines) if "Name (THMP, Package" in l)
    profiles = []
    depth = 0
    cur = None
    for line in lines[start:]:
        body = re.sub(r"Package \(0x[0-9A-Fa-f]+\)", "", line.split("//")[0])
        for ch in body:
            if ch == "{":
                depth += 1
                if depth == 2:
                    cur = []
            elif ch == "}":
                if depth == 2 and cur is not None:
                    profiles.append(cur)
                    cur = None
                depth -= 1
        if cur is not None:
            for tok in re.findall(r"0x[0-9A-Fa-f]+|\bOne\b|\bZero\b", body):
                if tok == "One":
                    cur.append(1)
                elif tok == "Zero":
                    cur.append(0)
                else:
                    cur.append(int(tok, 0))
        if depth == 0 and profiles:
            break
    return profiles


def _thmp_claim(cid, source, profile, expect_mw, claim):
    """expect_mw: {command_id: 毫瓦值} —— 其余命令被忽略。"""
    c = Claim(cid, claim, source, f"THMP[{profile}] decoded from dsdt.dsl")
    profiles = parse_thmp()
    if profile >= len(profiles):
        c.verdict, c.detail = FAIL, f"THMP has {len(profiles)} profiles, no [{profile}]"
        return c
    pairs = list(zip(profiles[profile][::2], profiles[profile][1::2]))
    got = dict(pairs)
    parts = []
    ok = True
    for cmd, want in expect_mw.items():
        have = got.get(cmd)
        have_w = None if have is None else have / 1000
        parts.append(f"cmd 0x{cmd:02X}: {'?' if have is None else have} mW"
                     f" ({'' if have_w is None else f'{have_w:g} W'}) vs claimed {want / 1000:g} W")
        if have != want:
            ok = False
    c.verdict = PASS if ok else FAIL
    c.detail = "; ".join(parts)
    return c


def _identity_claim(cid, source, expr, want, claim, unit_note):
    c = Claim(cid, claim, source, f"arithmetic: {expr} == {want} ({unit_note})")
    hexval, factor = expr
    value = hexval * factor
    c.verdict = PASS if value == want else FAIL
    c.detail = f"{hexval:#x} * {factor} = {value} (claimed {want})"
    return c


def _text_claim(cid, source, claim, check_fn, check_desc, note="", can_assume=False):
    c = Claim(cid, claim, source, check_desc)
    try:
        ok, detail = check_fn()
    except Exception as exc:  # noqa: BLE001 - report as FAIL with reason
        ok, detail = False, f"check crashed: {exc}"
    if note:
        detail = f"{detail} [{note}]" if detail else note
    if ok:
        # 一条推断性声明不会因为其一致性检查通过就变成"事实"；
        # 它始终保持"假设"标注。
        c.verdict = ASSUMPTION if can_assume else PASS
    else:
        c.verdict = FAIL
    c.detail = detail
    return c


# --------------------------------------------------------------------------
# 声明表

def build_claims():
    claims = []

    def add(c):
        claims.append(c)
        return c

    # --- THMD profile 瓦数声明（片段中的注释；profile 数据来自 THMP）
    add(_thmp_claim(
        "thmd-zero-watts", "patches_dsdt.py fnqs comment / old generate_patched_tables.py:53",
        0, {0x05: 101000, 0x07: 105000, 0x06: 120000},
        "THMD(Zero) = 101 W SPL, 105 W sPPT, 120 W fPPT"))
    add(_thmp_claim(
        "thmd-one-55w", "THMP[1] decode (the DC 55W Dynamic Boost profile, formerly "
        "referenced by the removed C fallback)",
        1, {0x05: 55000, 0x07: 55000, 0x13: 55000},
        "THMD(One) = 55 W power limits"))
    add(_thmp_claim(
        "thmd-0x13-office", "old generate_patched_tables.py:54 'Office mode: THMD(0x13) (85W SPL, 90W sPPT, 120W fPPT)'",
        0x13, {0x05: 85000, 0x07: 90000, 0x06: 120000},
        "THMD(0x13) = 85 W SPL, 90 W sPPT, 120 W fPPT"))
    add(_thmp_claim(
        "thmd-0x14-55w", "THMP[0x14] decode (profile retargeted by the rewritten FNQS)",
        0x14, {0x05: 55000},
        "THMD(0x14) SPL = 55 W"))
    add(_thmp_claim(
        "thmd-0x03-35w", "THMP[0x03] decode (DC low-power throttling profile)",
        0x03, {0x05: 35000},
        "THMD(0x03) SPL = 35 W"))
    add(_thmp_claim(
        "thmd-0x05-40w", "THMP[0x05] decode (DC 35W throttling profile; an earlier audit "
        "corrected the '40 W' comment - 0x05 is also 35 W)",
        0x05, {0x05: 35000},
        "THMD(0x05) SPL = 35 W (comment previously claimed 40 W)"))

    # --- 补丁注释中引用的 SSDT4 数值的单位换算
    add(_identity_claim(
        "unit-atpp-180w", "patches_ssdt4.py atpp comment", (0x0168, 0.5), 180,
        "0x0168 = 180 W in 0.5 W units", "0.5 W/LSB"))
    add(_identity_claim(
        "unit-atpp-220w", "patches_ssdt4.py atpp comment", (0x01B8, 0.5), 220,
        "0x01B8 = 220 W in 0.5 W units", "0.5 W/LSB"))
    add(_identity_claim(
        "unit-atp2-260w", "patches_ssdt4.py atp2 comment", (0x0208, 0.5), 260,
        "0x0208 = 260 W in 0.5 W units", "0.5 W/LSB"))
    add(_identity_claim(
        "unit-dtpp-120w", "patches_ssdt4.py dtpp comment", (0x00F0, 0.5), 120,
        "0xF0 = 120 W in 0.5 W units", "0.5 W/LSB"))
    add(_identity_claim(
        "unit-dcbt-40w", "patches_ssdt4.py dcbt comment", (0x28, 1), 40,
        "0x28 = 40 W in 1 W units", "1 W/LSB"))
    add(_identity_claim(
        "unit-dcbt-80w", "patches_ssdt4.py dcbt comment", (0x50, 1), 80,
        "0x50 = 80 W in 1 W units", "1 W/LSB"))
    add(_identity_claim(
        "unit-cmpl-51w", "patches_ssdt4.py cmpl comment", (0x33, 1), 51,
        "0x33 = 51 (W) in 1 W units", "1 W/LSB"))
    add(_identity_claim(
        "unit-cnpl-54w", "patches_ssdt4.py cnpl comment", (0x36, 1), 54,
        "0x36 = 54 (W) in 1 W units", "1 W/LSB"))
    add(_identity_claim(
        "unit-tgpa-140w", "patches_ssdt4.py tgpd comment", (0x0118, 0.5), 140,
        "TGPA branch value 0x0118 = 140 W in 0.5 W units", "0.5 W/LSB"))
    add(_identity_claim(
        "unit-tgpa-60w", "patches_ssdt4.py tgpd comment", (0x78, 0.5), 60,
        "TGPA branch value 0x78 = 60 W in 0.5 W units", "0.5 W/LSB"))

    # --- 单位约定（推断，标注为假设）
    add(_text_claim(
        "assume-pp-halfwatt", "patches_ssdt4.py header note",
        "*PP family (ATPP/ATP2/DTPP/TPPA/TGPA) encoded in 0.5 W steps",
        lambda: (_count("ssdt4", "0x0168") > 0 and _count("dsdt", "Name (THMP") > 0,
                 "consistent with platform-plausible watt values (360->180 W, 520->260 W); "
                 "no dump text proves the unit"),
        "consistency reasoning over dump values", can_assume=True))
    add(_text_claim(
        "assume-ecwr-bit0-ac", "patches_dsdt.py bst comment",
        "ECWR bit0 = AC adapter attached",
        lambda: (_has_sequence("dsdt", ("If ((ECRD (RefOf (ECWR)) & One))",)),
                 f"the identical test appears {_count('dsdt', 'If ((ECRD (RefOf (ECWR)) & One))')} "
                 "times in dsdt.dsl (AC-transition handlers all test bit0 the same way); "
                 "the dump never names the bit"),
        "consistency reasoning over dump usage", can_assume=True))
    add(_text_claim(
        "assume-thmp-cmd-classes", "claims THMD watt labels",
        "THMP command ids: 0x05/0x07/0x13 are SPL-class, 0x06 is FPPT-class; "
        "the finer SPL-vs-sPPT naming of 0x05/0x07 is inherited, not proven",
        lambda: (_count("frag:fnqs_mspl_mfpt_comm.asl", "MODP (0x05, Local0)") == 1
                 and _count("frag:fnqs_mspl_mfpt_comm.asl", "MODP (0x06, Local0)") == 1
                 and _count("frag:fnqs_mspl_mfpt_comm.asl", "MODP (0x07, Local0)") == 1,
                 "MSPL (named for SPL) sends CSPL to 0x05/0x07/0x13; MFPT (named for "
                 "FPPT) sends FPPT to 0x06 - class labels rest on the method names; "
                 "which of 0x05/0x07 is SPL vs sPPT cannot be proven from the dump"),
        "usage evidence from MSPL/MFPT method bodies", can_assume=True))

    # --- 经跨表 grep 确立的单消费者事实（2026-09-06）
    def _lines_with(key, needle):
        return sum(1 for l in _file(key) if needle in l)

    import glob

    def _other_dump_lines(needle, exclude):
        """统计除被排除文件外，所有 dump 文件中提及 needle 的行数。"""
        total = 0
        for f in glob.glob(os.path.join(REPO, "tools", "acpi", "*.dsl")):
            if os.path.basename(f) in exclude:
                continue
            with open(f, "r", encoding="utf-8") as fh:
                total += sum(1 for l in fh if needle in l)
        return total

    add(_text_claim(
        "dcbt-single-consumer", "patches_ssdt4.py dcbt note",
        "DCBT has exactly one consumer ('TGPD = DCBT'); after ssdt4-tgpd-ac it is "
        "unreferenced in the ASL channel",
        lambda: (_lines_with("ssdt4", "DCBT") == 2
                 and _other_dump_lines("DCBT", {"ssdt4.dsl"}) == 0,
                 f"ssdt4.dsl lines with DCBT = {_lines_with('ssdt4', 'DCBT')} "
                 "(definition + replaced copy); other dump files: "
                 f"{_other_dump_lines('DCBT', {'ssdt4.dsl'})} lines"),
        "cross-table consumer scan"))
    add(_text_claim(
        "dtpp-single-consumer", "patches_ssdt4.py dtpp note",
        "DTPP has exactly one consumer ('TPPD = DTPP'); after ssdt4-tppd-ac it is "
        "unreferenced in the patched tables (C fallback removed 2026-09-12)",
        lambda: (_lines_with("ssdt4", "DTPP") == 2
                 and _other_dump_lines("DTPP", {"ssdt4.dsl"}) == 0,
                 f"ssdt4.dsl lines with DTPP = {_lines_with('ssdt4', 'DTPP')} "
                 "(definition + replaced copy); other dump files: "
                 f"{_other_dump_lines('DTPP', {'ssdt4.dsl'})} lines"),
        "cross-table consumer scan"))

    def _fact(cid, source, claim, ok_fn, check_desc, assume=False):
        """由普通布尔表达式构造的声明；失败时附带 check_desc。"""
        def fn():
            try:
                v = ok_fn()
            except Exception as exc:  # noqa: BLE001
                return False, f"check crashed: {exc}"
            if isinstance(v, tuple):
                return v
            return bool(v), check_desc
        return _text_claim(cid, source, claim, fn, check_desc, can_assume=assume)

    # --- 片段行为声明
    frag_fnqs = "frag:fnqs_mspl_mfpt_comm.asl"
    add(_fact(
        "mspl-floor-0x50", "patches_dsdt.py fnqs comment",
        "MSPL floors CSPL at 0x50 (80 W)",
        lambda: _contains(frag_fnqs, "If ((Local0 < 0x50))"),
        "fragment contains the 'If ((Local0 < 0x50))' floor test"))
    add(_fact(
        "mfpt-floor-0x6e", "patches_dsdt.py fnqs comment",
        "MFPT floors FPPT at 0x6E (110 W)",
        lambda: _contains(frag_fnqs, "If ((Local0 < 0x6E))"),
        "fragment contains the 'If ((Local0 < 0x6E))' floor test"))
    add(_fact(
        "modp-mw-units", "patches_dsdt.py fnqs comment",
        "MODP limits are sent in mW (value * 0x03E8)",
        lambda: _count(frag_fnqs, "*= 0x03E8") == 2,
        "fragment multiplies by 0x03E8 exactly twice (MSPL+MFPT)"))
    add(_fact(
        "fnqs-dispatch-shape", "patches_dsdt.py fnqs comment",
        "FNQS: Arg0==Zero -> THMD(0x13), else -> THMD(Zero)",
        lambda: (_has_sequence(frag_fnqs, ("If ((ToInteger (Arg0) == Zero))", "{", "THMD (0x13)"))
                 and _has_sequence(frag_fnqs, ("Else", "{", "THMD (Zero)")),
                 "dispatch sequences 'Arg0==Zero / { / THMD (0x13)' and "
                 "'Else / { / THMD (Zero)' present in fragment"),
        "fragment dispatch sequence check"))
    add(_fact(
        "psr-unconditional", "patches_dsdt.py psr comment",
        "ADP1._PSR unconditionally returns One",
        lambda: _has_sequence("frag:adp1_psr.asl",
                              ("Method (_PSR, 0, NotSerialized)", "{", "Return (One)", "}")),
        "fragment body is exactly the constant One return"))
    add(_fact(
        "bst-ac-gate", "patches_dsdt.py bst comment",
        "_BST reports 0 (not discharging) when discharging-bit set AND AC attached",
        lambda: _has_sequence("frag:bat0_bst.asl",
                              ("ElseIf ((ECRD (RefOf (ECWR)) & 0x08))", "{",
                               "If ((ECRD (RefOf (ECWR)) & One))", "{", "Local0 = Zero")),
        "fragment nests the AC test inside the 0x08 branch"))
    # 注意：自 2026-09-13 起 _Q81/_Q82 有意保持原厂（STOCK）：DBFS = One 是
    # GPU 冲到 140 W（RTX 4060 的 105 W TGP + 35 W DB）所需的 Dynamic Boost
    # 使能信号。它过去触发的 45 W 借用钳制已从 MSPL/MFPT 本体移除（下限
    # 补丁），因此 DBFS 在打过补丁的 DSDT 中已无消费者，原厂事件流无害 ——
    # 杀掉它会让 GPU 永远被钉在基础 TGP（出于同样原因，ssdt4 的 ITSM==1
    # 分支中也保留 DBAC = One）。

    # --- evidence 字段引用的 dump 事实
    for key, lineno, prefix in (
        ("ssdt4", 1148, "Name (DCBT, 0x28)"),
        ("ssdt4", 1155, "Name (DTPP, 0xF0)"),
        ("ssdt4", 1160, "Name (CMPL, 0x33)"),
        ("ssdt4", 1161, "Name (CNPL, 0x10)"),
        ("ssdt4", 1149, "Name (DBAC, Zero)"),
        ("ssdt4", 1153, "Name (ATPP, 0x01B8)"),
        ("ssdt4", 1154, "Name (ATP2, 0x0208)"),
        ("ssdt4", 1239, "CreateWordField (PBD2, 0x05, TGPA)"),
        ("ssdt4", 1243, "CreateWordField (PBD2, 0x19, TPPA)"),
        ("ssdt4", 1263, "ATPP = 0x0168"),
        ("ssdt4", 1284, "DBAC = One"),
        ("ssdt4", 1285, "TPPA = 0x0168"),
        ("ssdt4", 1292, "ATPP = 0x0168"),
        ("ssdt4", 1314, "DBAC = One"),
        ("ssdt4", 1315, "TPPA = 0x0168"),
        ("ssdt4", 1319, "TGPD = DCBT"),
        ("ssdt4", 1322, "TPPD = DTPP"),
        ("ssdt4", 1486, "F6MP = CMPL"),
        ("ssdt4", 1487, "F6NP = CNPL"),
        ("ssdt4", 1493, "If ((IOBS != Zero))"),
        ("dsdt", 4079, "Name (THMP, Package"),
    ):
        add(_text_claim(
            f"evidence-{key}-{lineno}", f"patch evidence field '{key}.dsl:{lineno}'",
            f"{key}.dsl:{lineno} starts with {prefix!r}",
            lambda k=key, n=lineno, p=prefix: _line_prefix(k, n, p),
            "line citation verified against the dump"))

    def _dsdt_block_bounds():
        a_ok, a_txt = _line_prefix("dsdt", 4980, "Method (FNQS, 1, Serialized)")
        b_ok, b_txt = _line_prefix("dsdt", 5180, "Method (MODP, 2, Serialized)")
        return (a_ok and b_ok, f"dsdt.dsl:4980 = {a_txt!r}; dsdt.dsl:5180 = {b_txt!r}")

    add(_text_claim(
        "evidence-dsdt-block", "patches_dsdt.py fnqs evidence",
        "old FNQS block spans dsdt.dsl:4980 (Method (FNQS) to :5180 (Method (MODP)",
        _dsdt_block_bounds,
        "block boundary citations verified"))

    return claims


def run_claims():
    claims = build_claims()
    fails = 0
    print("== comment-claim audit (claims.py) ==")
    for c in claims:
        mark = {"PASS": "[pass]", "FAIL": "[FAIL]", "ASSUMPTION": "[assume]"}[c.verdict]
        print(f"  {mark} {c.id}: {c.claim}")
        print(f"{'':13}check: {c.check}")
        if c.detail:
            print(f"{'':13}data: {c.detail}")
        if c.verdict == FAIL:
            fails += 1
    n_assume = sum(1 for c in claims if c.verdict == ASSUMPTION)
    print(f"  {len(claims) - fails - n_assume}/{len(claims)} verified, "
          f"{n_assume} labeled assumption(s), {fails} failed")
    print()
    return 1 if fails else 0
