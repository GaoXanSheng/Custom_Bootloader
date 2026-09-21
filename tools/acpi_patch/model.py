"""ACPI 表补丁流水线的数据模型。

两类补丁覆盖整条流水线：

- LinePatch：按行*内容*（绝不按行号）定位并替换一行，带精确出现次数
  断言。用于 SSDT4。
- BlockPatch：把起始标记到结束标记（不含）之间的所有行，原样替换为
  fragment 文件的内容。用于 DSDT 方法整体重写。

每个补丁都携带：
- id / comment：供报告与跨通道审计映射使用的稳定标识。
- count：在源 dump 中的精确期望出现次数（不是"至少一次"）。
- evidence：注释中事实性声明的机器可查证据出处（见 claims.py）。
  注释只允许写 evidence 能支撑的内容。
"""

from dataclasses import dataclass, field

# 标记匹配基于"剥离行首尾空白后的前缀"：iASL dump 会在方法头后附加
# 尾注（如 "// _Qxx: EC Query"），整行精确匹配会不必要地脆弱。
MATCH_EXACT = "exact"
MATCH_PREFIX = "prefix"


@dataclass(frozen=True)
class LinePatch:
    id: str
    comment: str
    old: str
    new: str
    count: int = 1
    mode: str = MATCH_EXACT
    # 必须出现在每个命中点 CONTEXT_RADIUS 行范围内的剥离行。
    # 仅当 `old` 自身有歧义时才需要（见 engine）。
    context: tuple = ()
    evidence: str = ""
    # 往返门：反汇编 AML 中允许存留的 `old` 最大出现次数。默认 0（全部
    # 替换）。上下文刻意在别处保留相同行的补丁需要更大的值。
    rt_old_max: int = 0
    enabled: bool = True


@dataclass(frozen=True)
class BlockPatch:
    id: str
    comment: str
    start_marker: str
    end_marker: str
    fragment: str
    count: int = 1
    evidence: str = ""
    enabled: bool = True
    # 往返门标记（对 build_patched_tables.py 中 iASL 反汇编出的 AML 检查）。
    # present/absent 必须是可区分的字符串，claims.py 会对照源 dump 校验其
    # 唯一性。
    roundtrip_present: tuple = ()
    roundtrip_absent: tuple = ()


@dataclass
class TableSpec:
    """一张待修补 ACPI 表的注册信息（补丁系统的单一数据源）。

    注册一张新表 = 把固件 dump 放进 tools/acpi/ + 写一个 patches_<表名>.py
    导出 TABLE；生成、编译、roundtrip 校验、交叉检查、C 侧匹配表全部由
    discovery.py 自动发现驱动，不再需要在多处硬编码表清单。

    运行时匹配键（签名 / OEM ID / OEM Table ID / OEM Revision）不在 Python
    或 C 里手写：emit_registry.py 从 dump 的 DefinitionBlock 头自动解析，
    消灭 C 侧与 dump 双份键漂移。
    """

    # dump 文件名（tools/acpi/ 下，含 .dsl 后缀）
    source: str
    # 输出基名：src/acpi/patched/<output>.dsl、<out-dir>/<output>.hex、
    # C 符号 <output>_aml_code 三者共用
    output: str
    # 运行时内容指纹（ASCII 字节串）。构建期三重校验：必须存在于原 dump、
    # 在补丁后 AML 中保留、且不出现在其他任何 dump 中（唯一性）。
    fingerprint: str
    # C 侧日志显示名（仅诊断用）
    name: str
    # 补丁定义列表（LinePatch / BlockPatch）
    patches: list


@dataclass
class PatchOutcome:
    patch_id: str
    ok: bool
    # 命中位置的行号（1 起，*源* dump 中）。
    hits: list = field(default_factory=list)
    detail: str = ""


@dataclass
class TableReport:
    src_name: str
    out_name: str
    outcomes: list = field(default_factory=list)
    failures: list = field(default_factory=list)

    @property
    def applied(self):
        return sum(1 for o in self.outcomes if o.ok)

    @property
    def total(self):
        return len(self.outcomes)
