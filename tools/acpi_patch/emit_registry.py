"""生成 C 侧 ACPI 表替换注册表 build/TableRegistry.h。

数据全部来自单一来源：
- 匹配键（签名 / OEM ID / OEM Table ID / OEM Revision）从 tools/acpi/ 下
  对应 dump 的 DefinitionBlock 头自动解析（engine.parse_definition_block），
  C 侧不再手写、不再有第二份拷贝可以漂移；
- 内容指纹与嵌入表符号名来自各 patches_*.py 的 TableSpec。

OEM Revision 在 C 侧是"软键"（失配仅告警不拒绝）：指纹才是内容身份，
Rev 会随 BIOS 小版本更新漂移，不应因此拒绝替换。
"""

import os

from .discovery import discover_tables
from .engine import parse_definition_block


def _sig_u32(sig):
    return int.from_bytes(sig.encode("ascii"), "little")


def _table_id_u64(table_id):
    raw = table_id.encode("ascii")[:8].ljust(8, b" ")
    return int.from_bytes(raw, "little")


def build_registry_lines(dumps_dir):
    """返回 TableRegistry.h 的文本行（含结尾换行）。"""
    specs = discover_tables()
    lines = [
        "// ---------------------------------------------------------------------------",
        "// 自动生成：tools/acpi_patch/emit_registry.py —— 勿手改。",
        "// 匹配键取自 tools/acpi/ 下各 dump 的 DefinitionBlock 头；指纹声明于",
        "// patches_*.py 的 TableSpec。注册新表请写 patches_<表名>.py，不要改这里。",
        "// ---------------------------------------------------------------------------",
        "",
    ]
    for modname, spec in specs:
        # 每张表对应一个 iasl -tc 生成的 hex 数组头文件（符号 <output>_aml_code）
        lines.append(f'#include "{spec.output}.hex"  // {modname} -> {spec.source}')
    lines += [
        "",
        "static const TABLE_REPLACEMENT gTableReplacements[] = {",
    ]
    for modname, spec in specs:
        hdr = parse_definition_block(os.path.join(dumps_dir, spec.source))
        sig = _sig_u32(hdr["signature"])
        tid = _table_id_u64(hdr["table_id"])
        rev = hdr["oem_revision"]
        fp = spec.fingerprint
        fp_lit = ", ".join(f"0x{b:02X}" for b in fp.encode("ascii"))
        sym = f"{spec.output}_aml_code"
        lines += [
            "    {",
            f"        0x{sig:08X},"
            f"  // 签名 \"{hdr['signature']}\"（解析自 {spec.source}）",
            f"        0x{tid:016X}ULL,"
            f"  // OEM Table ID \"{hdr['table_id']}\"（硬键）",
            f"        0x{rev:08X},"
            f"  // OEM Revision（软键：失配仅告警）",
            f"        (const UINT8 *)\"{fp}\", {len(fp)},"
            f"  // 内容指纹（硬键，唯一身份）",
            f"        {sym},",
            f"        sizeof({sym}),",
            f"        L\"{spec.name}\"",
            "    },",
        ]
    lines += [
        "};",
        "",
        "static const UINTN gTableReplacementCount =",
        "    sizeof(gTableReplacements) / sizeof(gTableReplacements[0]);",
        "",
    ]
    return lines


def emit_registry(out_dir, dumps_dir):
    """写入 <out_dir>/TableRegistry.h，返回其路径。"""
    path = os.path.join(out_dir, "TableRegistry.h")
    os.makedirs(out_dir, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(build_registry_lines(dumps_dir)))
    return path
