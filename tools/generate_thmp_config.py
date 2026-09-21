#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 C 侧 THMP 补丁配置头文件 build/ThmpConfig.h。

数据源：tools/thmp_config.yaml
包含：
  - THMP 锚点字符串 (anchor，为空则全表搜索)
  - 搜索窗口大小 (search_window)
  - 启用的特征搜索替换规则列表 (gThmpRules)

支持两种模式定义：
  1. 数值模式：old_value / new_value (32位毫瓦值，自动转 AML DWordConst: 0x0C + 4字节LE)
  2. 字节模式：old_hex / new_hex (十六进制字节串，例如 "0C 00 71 02 00")
两者必须保持长度一致，以保证就地替换不改变 AML 长度。
"""

import argparse
import os
import struct
import sys
import yaml

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def dword_to_aml_bytes(val: int) -> list[int]:
    """把 32 位无符号整数转换为 AML DWordConst 字节列表 (0x0C + 4 字节 LE)。"""
    le_bytes = list(struct.pack("<I", val & 0xFFFFFFFF))
    return [0x0C] + le_bytes


def parse_pattern_bytes(rule: dict, key_hex: str, key_val: str) -> list[int]:
    """解析规则中的模式字节，支持十六进制字符串或数值。"""
    if key_hex in rule and rule[key_hex] is not None:
        raw = str(rule[key_hex]).replace(",", " ")
        return [int(x, 16) for x in raw.split()]
    elif key_val in rule and rule[key_val] is not None:
        val = int(rule[key_val])
        return dword_to_aml_bytes(val)
    else:
        raise ValueError(f"Rule '{rule.get('name')}' must specify either '{key_hex}' or '{key_val}'")


def generate_header_content(cfg: dict) -> str:
    anchor = cfg.get("anchor", "THMP") or ""
    search_window = cfg.get("search_window", 3072)
    raw_rules = cfg.get("rules", [])

    enabled_rules = []
    for r in raw_rules:
        if r.get("enabled", True):
            enabled_rules.append(r)

    lines = [
        "// ---------------------------------------------------------------------------",
        "// 自动生成：tools/generate_thmp_config.py（基于 tools/thmp_config.yaml）",
        "// 勿手改本文件。如需调整规则，请编辑 tools/thmp_config.yaml 后重新构建。",
        "// ---------------------------------------------------------------------------",
        "",
        "#ifndef THMP_CONFIG_H",
        "#define THMP_CONFIG_H",
        "",
        f'#define THMP_ANCHOR           "{anchor}"',
        f"#define THMP_SEARCH_WINDOW    {search_window}",
        "#define THMP_MAX_PATTERN_LEN  32",
        "",
        "typedef struct {",
        "    const CHAR16 *Name;",
        "    UINT8  PatternLength;",
        "    UINT8  OldPattern[THMP_MAX_PATTERN_LEN];",
        "    UINT8  NewPattern[THMP_MAX_PATTERN_LEN];",
        "    UINTN  ExpectedCount;",
        "} THMP_PATCH_RULE;",
        "",
    ]

    if not enabled_rules:
        lines += [
            "static const THMP_PATCH_RULE *gThmpRules = NULL;",
            "static const UINTN gThmpRuleCount = 0;",
        ]
    else:
        lines.append("static const THMP_PATCH_RULE gThmpRules[] = {")
        for r in enabled_rules:
            name = r.get("name", "unnamed_rule")
            expected = int(r.get("expected_count", 0))

            old_bytes = parse_pattern_bytes(r, "old_hex", "old_value")
            new_bytes = parse_pattern_bytes(r, "new_hex", "new_value")

            if len(old_bytes) != len(new_bytes):
                raise ValueError(
                    f"Rule '{name}': old pattern length ({len(old_bytes)}) "
                    f"!= new pattern length ({len(new_bytes)})"
                )
            if len(old_bytes) > 32:
                raise ValueError(
                    f"Rule '{name}': pattern length ({len(old_bytes)}) exceeds maximum 32 bytes"
                )

            pat_len = len(old_bytes)
            # 补齐到 32 字节
            old_padded = old_bytes + [0] * (32 - pat_len)
            new_padded = new_bytes + [0] * (32 - pat_len)

            old_str = ", ".join(f"0x{b:02X}" for b in old_padded)
            new_str = ", ".join(f"0x{b:02X}" for b in new_padded)

            desc = r.get("description", "")
            if desc:
                lines.append(f"    // {desc}")
            lines += [
                "    {",
                f'        L"{name}",',
                f"        {pat_len},",
                f"        {{ {old_str} }},",
                f"        {{ {new_str} }},",
                f"        {expected}",
                "    },",
            ]
        lines += [
            "};",
            "",
            "static const UINTN gThmpRuleCount = sizeof(gThmpRules) / sizeof(gThmpRules[0]);",
        ]

    lines += [
        "",
        "#endif // THMP_CONFIG_H",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate ThmpConfig.h from YAML config")
    parser.add_argument("--config", default=os.path.join(REPO, "tools", "thmp_config.yaml"),
                        help="Path to thmp_config.yaml")
    parser.add_argument("--out-dir", default=os.path.join(REPO, "build"),
                        help="Output directory for ThmpConfig.h")
    args = parser.parse_args()

    if not os.path.exists(args.config):
        print(f"[-] Config file not found: {args.config}")
        sys.exit(1)

    with open(args.config, "r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f)

    os.makedirs(args.out_dir, exist_ok=True)
    out_path = os.path.join(args.out_dir, "ThmpConfig.h")
    content = generate_header_content(cfg)

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)

    print(f"[+] Generated: {out_path}")


if __name__ == "__main__":
    main()
