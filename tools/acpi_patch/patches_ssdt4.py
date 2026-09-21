"""SSDT4（NVIDIA NPCF 功耗管理表）补丁定义。

数值参照 tools/acpi/ssdt4.dsl（2026-07-18 固件 dump）。单位：*PP* 家族
（ATPP/ATP2/DTPP/TPPA/TGPA）按 0.5 W 步长编码（0x0168=360 -> 180 W，
0x0230=560 -> 280 W）；DCBT/CMPL/CNPL 按直接瓦数读（0x28=40、0x50=80）。
单位属推断而非实证的，注释中均有说明——机器检查的声明表见 claims.py。
"""

from .model import LinePatch, BlockPatch, MATCH_PREFIX, TableSpec

PATCHES = [
    LinePatch(
        id="ssdt4-cmpl-80w",
        comment="CMPL 0x33(51) -> 0x50(80); reported as F6MP in NPCF sub-func 6 Case(Zero)",
        old="Name (CMPL, 0x33)",
        new="Name (CMPL, 0x50)",
        evidence="ssdt4.dsl:1160 definition; :1486 'F6MP = CMPL' consumption",
    ),
    LinePatch(
        id="ssdt4-cnpl-54w",
        comment="CNPL 0x10(16) -> 0x36(54); reported as F6NP in NPCF sub-func 6 Case(Zero)",
        old="Name (CNPL, 0x10)",
        new="Name (CNPL, 0x36)",
        evidence="ssdt4.dsl:1161 definition; :1487 'F6NP = CNPL' consumption",
    ),
    LinePatch(
        id="ssdt4-dtpp-280w",
        comment="DTPP 0xF0 -> 0x0230 (0.5 W units: 120 W -> 280 W); DC platform wall. NOTE: after ssdt4-tppd-ac, DTPP has no consumer left in the patched tables (the only reader was the replaced copy) - inert, kept for value consistency",
        old="Name (DTPP, 0xF0)",
        new="Name (DTPP, 0x0230)",
        evidence="ssdt4.dsl:1155 definition; :1322 was the only consumer; cross-table grep finds no other reference",
    ),
    LinePatch(
        id="ssdt4-dcbt-80w",
        comment="DCBT 0x28(40) -> 0x50(80); DC GPU threshold. NOTE: after ssdt4-tgpd-ac, DCBT has no reader left in the patched tables (its only consumer was 'TGPD = DCBT'); the C fallback channel that also kept a copy was removed 2026-09-12, so this is kept for value consistency only",
        old="Name (DCBT, 0x28)",
        new="Name (DCBT, 0x50)",
        evidence="ssdt4.dsl:1148 definition; :1319 was the only consumer; cross-table grep over ssdt1-23+dsdt finds no other reference",
    ),
    LinePatch(
        id="ssdt4-atpp-init-280w",
        comment="ATPP resting 0x01B8 -> 0x0230 (0.5 W: 220 W -> 280 W); keeps non-CPUT-0x07 paths (e.g. ITSM==1 TPPA=ATPP) on the 280 W budget instead of 220 W",
        old="Name (ATPP, 0x01B8)",
        new="Name (ATPP, 0x0230)",
        evidence="ssdt4.dsl:1153 definition; read at :1272/:1280/:1310 via TPPA=ATPP copies",
    ),
    LinePatch(
        id="ssdt4-atpp-280w",
        comment="ATPP assign 0x0168 -> 0x0230 (0.5 W: 180 W -> 280 W); the CPUT==0x07 SKU downgrade paths lower it to 180 W on stock firmware",
        old="ATPP = 0x0168",
        new="ATPP = 0x0230",
        count=2,
        evidence="ssdt4.dsl:1263,:1292 assigns; :1153 'Name (ATPP, 0x01B8)' shows the resting value",
    ),
    LinePatch(
        id="ssdt4-atp2-280w",
        comment="ATP2 assign 0x0168 -> 0x0230 (0.5 W: 180 W -> 280 W); Name(ATP2) resting value is 0x0208 (260 W)",
        old="ATP2 = 0x0168",
        new="ATP2 = 0x0230",
        evidence="ssdt4.dsl:1293 assign; :1154 'Name (ATP2, 0x0208)'",
    ),
    LinePatch(
        id="ssdt4-tppa-280w",
        comment="TPPA (PBD2 buffer field @0x19) assign 0x0168 -> 0x0230 (0.5 W: 180 W -> 280 W); this is the CPU-side platform budget the NVIDIA driver arbitrates against (CPU_allowance = TPPA - GPU)",
        old="TPPA = 0x0168",
        new="TPPA = 0x0230",
        count=2,
        evidence="ssdt4.dsl:1285,:1315 assigns; :1243 'CreateWordField (PBD2, 0x19, TPPA)'",
    ),
    LinePatch(
        id="ssdt4-tgpd-ac",
        comment="TGPD source DCBT -> TGPA (branch-dependent: 0x0118 when ITSM==1, 0x78 when ITSM==0); this - not DCBT - is now the effective GPU budget on the ASL channel",
        old="TGPD = DCBT",
        new="TGPD = TGPA",
        mode=MATCH_PREFIX,
        evidence="ssdt4.dsl:1319; TGPA is CreateWordField(PBD2, 0x05) at :1239, assigned only 0x0118 (:1269/:1299) and 0x78 (:1277/:1307) inside the NIGS==0 branch; the NIGS==1 teardown zeroes TGPD directly (untouched)",
    ),
    LinePatch(
        id="ssdt4-tppd-ac",
        comment="TPPD source DTPP -> TPPA; on the NIGS==0 setup paths every TPPA assign yields 0x0230 after patching (ATPP/ATP2 copies + the two patched 0x0168 assigns), so TPPD tracks 280 W; the NIGS==1 teardown zeroes TPPA/TGPD/TPPD directly (untouched by any patch)",
        old="TPPD = DTPP",
        new="TPPD = TPPA",
        mode=MATCH_PREFIX,
        evidence="ssdt4.dsl:1322; TPPA assigns at :1272/:1280/:1285/:1302/:1310/:1315 all precede the copy inside NIGS==0; 'TPPA = Zero' at :1347 belongs to the NIGS==1 teardown",
    ),
    # ssdt4-dbac-advertise-on (DBAC Zero->One x4): TESTED 2026-09-21 standalone
    # and superseded. Hardcoding DBAC=One made PC02=1, which the platform
    # reports as "DB enabled" - empirically that KEEPS THE GPU AT BASE TGP
    # (~115W, DB never engages; probe 03:52), while stock PC02=0 lets the
    # driver's internal DB boost the GPU to 140W (with the CPU borrow).
    # Replaced by ssdt4-dbac-dbof-switch below, which makes the value a
    # runtime WMI toggle instead of a compile-time constant.
    LinePatch(
        id="ssdt4-dbac-dbof-switch",
        comment="DBAC Zero->DBOF x4: fun#2's PC02 (= DBAC | DBDC << One) is the platform->driver Dynamic-Boost switch. Redirecting DBAC to the NPCF-scope DBOF flag (added by ssdt4-npcf-trace-names) makes it a runtime toggle: DBOF=0 = stock DB on (GPU 140W via borrow, CPU may shed to 40W), DBOF=1 = DB off (GPU held at base TGP ~115W, CPU keeps full power). DBUL WMTF Method 1 (SetDbfs) writes DBOF then Notifies NPCF 0xC0 so the driver re-reads fun#2 immediately",
        old="DBAC = Zero",
        new="DBAC = DBOF",
        count=4,
        evidence="ssdt4.dsl:1268/:1276/:1298/:1306 assigns; consumer :1321 'PC02 = (DBAC | (DBDC << One))'; empirical probe runs 03:33 (PC02=0 -> GPU 139.8W) vs 03:52 (PC02=1 -> GPU 115W), 2026-09-21",
        rt_old_max=0,
    ),
    LinePatch(
        id="ssdt4-maga-boost-cap",
        comment="MAGA 0xC8->MGAF x4: fun#2's MAGA is the max Dynamic-Boost borrow allowance the platform reports to the driver (0.5W units; stock 0xC8 = 100W, MIGA = 0 min). The +25W actually borrowed is the SKU-level driver/vBIOS DB policy (4060 Laptop: 115W base + 25W DB = 140W). Redirecting MAGA to the NPCF-scope MGAF flag makes the cap a runtime WMI knob (DBUL WMTF Method 20, watts): lowering it should clamp how much the GPU may borrow (e.g. 10W -> GPU ~125W) and proportionally shrink the CPU shed - IF the driver honors MAGA",
        old="MAGA = 0xC8",
        new="MAGA = MGAF",
        count=4,
        evidence="ssdt4.dsl:1271/:1279/:1301/:1309 assigns (one per ITSM sub-branch, same shape as DBAC); NVIDIA RTX 4060 Laptop spec TGP 35-115W + 25W DB; MIFS 0xFB00 write subcodes (0x0F00/0x1000/0x1100/0x1200/0x1400/0x1500/0x1700/0x0900) contain no DB knob",
        rt_old_max=0,
    ),
    LinePatch(
        id="ssdt4-tgpa-budget-knob",
        comment="TGPA 0x0118->TGPF x2: fun#2's TGPA is the GPU power budget reported to the NVIDIA driver (0.5W units). Redirecting the performance-mode (ITSM==1) assigns to the NPCF-scope TGPF flag makes the GPU budget a runtime WMI knob (DBUL WMTF Method 22, watts) - the driver is notified via 0xC0 and re-reads fun#2 immediately. The office-mode (ITSM==0) 60W assigns stay stock",
        old="TGPA = 0x0118",
        new="TGPA = TGPF",
        count=2,
        evidence="ssdt4.dsl:1269/:1299 assigns (inside both CPUT outer branches, ITSM==One sub-branches); office branch keeps 'TGPA = 0x78' (:1277/:1307)",
        rt_old_max=0,
    ),
    # ssdt4-npcf-nce2-off: TESTED 2026-09-13 and REVERTED. NCE2=0 made things
    # worse: GPU fell to ~88W (below base TGP - the driver gets its whole TGP
    # picture through NVPCF) while the CPU stayed clamped at ~36-40W. NVPCF is
    # the only budget channel; disabling it is not a CPU-unlock lever.
    # NOTE ordering: ssdt4-npcf-fun6-trace (below) reproduces the ORIGINAL
    # Case(One) text including `If ((IOBS != Zero))`, so ssdt4-iobs-nvio-kill
    # must run AFTER it, or the trace fragment would resurrect the IO-port
    # write with IOBS=0.
    BlockPatch(
        id="ssdt4-npcf-trace-names",
        comment="Add trace variables T5C/T5V (fun#5 invocation count + last F5P1) and T6C/T6V (fun#6 INC=1 count + last NCHP) to the NPCF scope, read back via WMTF Method 21",
        start_marker="Name (CUSL, Zero)",
        end_marker="Name (CUCT, Zero)",
        fragment="npcf_trace_names.asl",
        evidence="ssdt4.dsl:1163; instrumentation for locating the Dynamic Boost CPU-clamp entry point",
        roundtrip_present=(
            ("Name (T5C, Zero)",),
            ("Name (T6V, Zero)",),
        ),
        roundtrip_absent=(),
    ),
    BlockPatch(
        id="ssdt4-npcf-fun5-trace",
        comment="fun#5 INC5=3 (driver sets CPU SL): count invocations in T5C and capture the raw F5P1 the driver sends, alongside the stock dead CUSL store",
        start_marker="CUSL = (F5P1 & 0xFF)",
        end_marker="Case (0x04)",
        fragment="npcf_fun5_trace.asl",
        evidence="ssdt4.dsl:1443; CUSL has no consumer in any dump table (write-only)",
        roundtrip_present=(
            ("T5C++", "T5V = F5P1"),
        ),
        roundtrip_absent=(),
    ),
    BlockPatch(
        id="ssdt4-npcf-fun6-trace",
        comment="fun#6 INC6=1 (driver sets CPU power via the CPUC IO-port path, IOBS-gated dead in this AML): count invocations in T6C and capture the raw NCHP the driver sends. Trace lines run unconditionally at the end of Case(One), before the Return; the If((IOBS != Zero)) body is left stock here and neutralized afterwards by ssdt4-iobs-nvio-kill",
        start_marker="CreateByteField (Arg3, 0x09, NCHP)",
        end_marker="Return (PBD6) /* \\_SB_.NPCF.NPCF.PBD6 */",
        fragment="npcf_fun6_trace.asl",
        evidence="ssdt4.dsl:1478-1530; NCHP has no other consumer",
        roundtrip_present=(
            ("T6C++", "T6V = NCHP"),
        ),
        roundtrip_absent=(),
    ),
    LinePatch(
        id="ssdt4-iobs-nvio-kill",
        comment="Neutralize NPCF sub-func 6 Case(One) body: skips the NVIO CPUC write, the sixteen 0x85 throttle notifies, and (side effect) the F6MP/F6NP/F6O2 zeroing inside the same block. MUST stay after ssdt4-npcf-fun6-trace (see ordering note above)",
        old="If ((IOBS != Zero))",
        new="If (Zero)",
        count=1,
        context=("OperationRegion (NVIO, SystemIO, IOBS, 0x10)",),
        evidence="ssdt4.dsl:1493 target (Case(One)); the identical test at :1483 (Case(Zero)) is intentionally untouched - context line disambiguates",
        rt_old_max=1,
    ),
    BlockPatch(
        id="ssdt4-npcf-fun3-ladder-100w",
        comment="Raise every CPU watt value (0x28..0x50 = 40..80W) in the NVPCF sub-func#3 step ladder to 0x64 (100W), keeping header (0x11,0x04,0x13,0x03), record structure, 0xFF invalid markers, zeros and the 0x02/0x05 bytes. The ladder records mirror the SMU dynamic-boost CPU clamp bands (r0=(0,255,0)=unconstrained at base TGP; r1 starts at 40W - matching the observed 39.5W CPU floor under full GPU boost). If the driver takes its CPU limit from this table the floor moves to 100W; if it is informational the patch is inert",
        start_marker="Return (Buffer (0x3D)",
        end_marker="Case (0x04)",
        fragment="npcf_fun3_ladder_100w.asl",
        evidence="ssdt4.dsl:1360-1370 static buffer; header stride 3 x 19 records = 0x3D bytes; r1 first byte 0x28(40W) vs live CPU floor 39.47W during 140W GPU boost",
        roundtrip_present=(
            ("0x11, 0x04, 0x13, 0x03, 0x00, 0xFF, 0x00, 0x64",),
        ),
        roundtrip_absent=(
            ("0x2D, 0x2D, 0x33, 0x33, 0x39, 0x39, 0x3F, 0x3F",),
        ),
    ),
]

# 2026-09-21 瘦身决定：SSDT4 不再整表替换 —— TABLE 导出已移除（discovery
# 自动跳过本模块），运行时使用固件原版 SSDT4。40W 钳制的根因是 DSDT 里
# THMP 表的组合预算（cmd0C=160W），修复全部落在 DSDT（见 patches_dsdt.py
# 的 dsdt-thmp-total-budget-unlock）。本文件保留：PATCHES 数据与注释仍是
# claims.py 声明审计的对象，也作为 NVPCF 协议分析的历史记录。