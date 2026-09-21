"""DSDT 补丁定义（2026-09-21 瘦身后仅保留功耗三件套）：
1. dsdt-fnqs-mspl-mfpt-comm —— MSPL/MFPT 无条件下限（80/110W，防 SKU 分支
   借电钳制复发）+ FNQS 简化分发；
2. dsdt-thmp-total-budget-unlock —— THMP cmd0C 组合预算 160W→260W（40W
   钳制的本体修复）；
3. dsdt-ereg-full-power-init —— 开机 110/120W 满血初始化（280W 整机墙）。
历史：_PSR/BAT0 电池伪装与 _Q10 改写已删（与功耗链路无关/保持原厂）。

原说明——从 fragment 文件原样拼接的方法整体重写。

每条 BlockPatch 把起始标记行到结束标记行（起始后的第一处出现，不含）
之间的内容替换为 fragments/<fragment> 的原样文本。全部起始标记均已
验证在 tools/acpi/dsdt.dsl（2026-07-18 dump）中恰好出现一次。

往返标记由 build_patched_tables.py 对照编译后 AML 的 iASL 反汇编检查：
"present" 序列必须出现在新表中（且不得存在于原 dump），"absent" 序列必须
消失（且在原 dump 中只存在于被替换区域内）。claims.py 会对照原 dump
校验这两条出处规则。
"""

from .model import BlockPatch, LinePatch, TableSpec

PATCHES = [
    BlockPatch(
        id="dsdt-fnqs-mspl-mfpt-comm",
        comment="Rewrite FNQS/MSPL/MFPT/COMM with DB-aware row switching (R9000P FNQT architecture): when DB engages (_Q82 set DBFS=One) the platform PROACTIVELY drops to the office row THMD(0x13) (85W SPL, the empirically CPU-free row) and syncs EC CSPL to 85W; when DB releases, restore the full-power row and CSPL=110W. MSPL/MFPT stay unconditional floors (CSPL>=0x50, FPPT>=0x6E). Rationale: field-proven that the EC/SMU hard-sheds CPU to 40W on DB engagement regardless of THMP cmd0C (160W/260W/1600W all identical); office mode stays free only because its 60W GPU budget never engages DB. Hypothesis under test: the EC skips its own hard shed when the platform coordinates (as on Lenovo)",
        start_marker="Method (FNQS, 1, Serialized)",
        end_marker="Method (MODP, 2, Serialized)",
        fragment="fnqs_mspl_mfpt_comm.asl",
        evidence="old block dsdt.dsl:4980-5179; THMD profile watts come from the THMP table at :4079",
        roundtrip_present=(
            ("If ((Local0 < 0x50))",),
            ("If ((Local0 < 0x6E))",),
            ("If ((DBFS == Zero))", "{", "ECWT (0x6E, RefOf (CSPL))"),
            ("ECWT (0x55, RefOf (CSPL))", "THMD (0x13)"),
        ),
        roundtrip_absent=(
            ("THMD (0x12)",),
            ("THMD (0x15)",),
        ),
    ),
    # NOTE dsdt-q10: _Q10 is intentionally NOT patched. The OEM WMI control
    # path (CMEN lifecycle, GPSF/DGST NVIDIA budget notify on weak adapters,
    # battery office downgrade, WMID event report) must stay original.
    # Protection is unaffected: every _Q10 path ends in FNQS, and the patched
    # FNQS re-pushes the MSPL/MFPT floors right after THMD, so the 45W borrow
    # cannot return. Runtime assertion: verify_runtime.py checks the stock
    # _Q10 body is present in the live DSDT.
    # NOTE: THMP cmd0C 补丁已迁移到 C 侧运行时 AML 内存搜索替换
    # （AcpiPatch.c PatchThmpInPlace）。构建期不再修改 THMP 值，避免
    # 与运行时双重替换。保留定义供 claims.py 审计和文档引用。
    LinePatch(
        id="dsdt-thmp-total-budget-unlock",
        comment="THMP cmd0C (combined CPU+GPU budget) 160W -> 1600W on all AC profiles (x9). Field-proven 2026-09-21: with the 260W interim value, office mode (profile[19], OEM cmd0C=1600W stock) left the CPU UNRESTRICTED while both performance modes (profile[0], 260W) still clamped the CPU to 40W - the SMU does not honor intermediate values. 0x186A00 is the OEM disable-sentinel, live in office mode on this exact machine (CPU bounded only by SPL; no thermal event - SPL/SPPT/FPPT, GPU TGP 140W, thermal trips and the 280W adapter all remain in force; physical ceiling 110+140=250W). Battery profiles untouched",
        old="0x00027100,",
        new="0x00186A00,",
        count=9,
        evidence="dsdt.dsl:4096..4404 (cmd0C pairs inside THMP profiles 0/1/2/6/7/8/12/13/14, all AC); no-op placeholder precedent 0x186A00 at profiles 18-22; R9000P FNQ0 uses the same cmd0C with 160-176W on AC rows and explicit DB row-switching instead; observed GPU~120W + CPU 40W = 160W under full load",
        rt_old_max=0,
        enabled=False,
    ),
    BlockPatch(
        id="dsdt-ereg-full-power-init",
        comment="Rewrite _REG: once ECAV is set, write the full-power init (GFLG/GPMD token+mode, CSPL=0x6E/FPPT=0x78 whole-machine 280W defaults: 110W CPU + 140W GPU + 30W peripherals) before the FNQS/COMM push. CSPL is the real machine wall: every FNQS/COMM re-pushes max(EC CSPL, floor) to the SMU via MSPL, overriding THMD profile SPLs. This replaces the DBUL _INI as the authoritative boot hook: ECWT silently no-ops until the EC region connects (ECAV), so the _INI races and its writes are lost on some boots",
        start_marker="Method (_REG, 2, NotSerialized)",
        end_marker="OperationRegion (ECF2, SystemMemory, 0xFE800400, 0xFF)",
        fragment="ereg_full_power_init.asl",
        evidence="old body dsdt.dsl:4624-4646; ECWT gates every write on ECAV (dsdt.dsl:4881-4883); TGPA=0x118(140W) at ssdt4.dsl:1269 is the GPU side of the 280W budget",
        roundtrip_present=(
            ("ECWT (0x55, RefOf (GFLG))", "ECWT (One, RefOf (GPMD))",
             "ECWT (0x6E, RefOf (CSPL))", "ECWT (0x78, RefOf (FPPT))"),
        ),
        roundtrip_absent=(),
    ),
]

# 表注册信息（discovery 自动发现；匹配键由 emit_registry 从 dsdt.dsl 头解析）。
# DSDT 历史上没有内容指纹（仅按 TabId+Rev 匹配），BIOS 更新后若表头键仍
# 碰巧一致，会用 2026-07 的旧 dump 覆盖新固件表。THMP（THMD profile 表，
# 仅存在于 DSDT）补上内容身份。
TABLE = TableSpec(
    source="dsdt.dsl",
    output="dsdt_patched",
    fingerprint="THMP",
    name="DSDT_CpuPowerClamp",
    patches=PATCHES,
)
