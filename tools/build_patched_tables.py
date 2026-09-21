#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把补丁后的 ACPI 表源（src/acpi/patched/*.dsl）编译为 AML + C 数组 hex
（<out-dir>/*.hex，AcpiPatch.c 经生成的 TableRegistry.h 引用），随后做
两道校验：

1. 往返门：编译出的 AML 反汇编回 ASL，每条补丁必须在反汇编结果中可观
   察（新值出现、被替换内容消失）。用于拦截"编译能过但进不了 AML
   编码"的改动。
2. 指纹门：每张表的 TableSpec 内容指纹必须
   a) 存在于原始 dump .dat；
   b) 在补丁后 AML 中保留（即选自补丁未触及的区域）；
   c) 不出现在任何其他 tools/acpi/*.dat —— 保证运行时扫描能仅凭指纹
   唯一定位该表。

最后生成 <out-dir>/TableRegistry.h（匹配键从各 dump 的 DefinitionBlock
头自动解析）供 C 侧消费。

Usage:
    python tools/build_patched_tables.py [--skip-verify] [--out-dir DIR]
"""

import argparse
import os
import subprocess
import sys
import yaml

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IASL = os.path.join(REPO, "tools", "isal", "iasl.exe")
DUMPS = os.path.join(REPO, "tools", "acpi")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_patched_tables  # noqa: E402  (regenerates the patched DSLs)
from acpi_patch.discovery import discover_tables  # noqa: E402
from acpi_patch.emit_registry import emit_registry  # noqa: E402
from acpi_patch.engine import has_sequence, read_lines  # noqa: E402
from acpi_patch.model import MATCH_PREFIX  # noqa: E402


def run_iasl(args):
    """以固定参数列表（不经 shell）运行仓库自带的 iASL。"""
    return subprocess.run([IASL] + args, capture_output=True, text=True,
                          shell=False)


def roundtrip_disassemble(out_dir, base):
    """把 <out-dir>/<base>.aml 反汇编为 <out-dir>/roundtrip/<base>.dsl 并
    返回其行。先复制 AML，让 iASL 的输出落在 roundtrip 目录。"""
    roundtrip = os.path.join(out_dir, "roundtrip")
    os.makedirs(roundtrip, exist_ok=True)
    aml_copy = os.path.join(roundtrip, base + ".aml")
    with open(os.path.join(out_dir, base + ".aml"), "rb") as src, \
         open(aml_copy, "wb") as dst:
        dst.write(src.read())
    r = run_iasl(["-d", aml_copy])
    dsl = os.path.join(roundtrip, base + ".dsl")
    if r.returncode != 0 or not os.path.exists(dsl):
        print(f"[-] round-trip disassembly failed for {base}.aml")
        print(r.stdout + r.stderr)
        sys.exit(1)
    return read_lines(dsl)


def verify_table(out_dir, base, patches):
    """每条补丁都必须在反汇编后的 AML 中可观察。"""
    lines = roundtrip_disassemble(out_dir, base)
    stripped = [l.strip() for l in lines]
    failures = []
    for p in patches:
        if not p.enabled:
            continue
        if hasattr(p, "mode"):  # LinePatch
            if p.mode == MATCH_PREFIX:
                left = [s for s in stripped if s.startswith(p.old)]
            else:
                left = [s for s in stripped if s == p.old]
            if len(left) > p.rt_old_max:
                failures.append(f"{p.id}: replaced content still present "
                                f"({len(left)}x, allowed {p.rt_old_max}): {p.old!r}")
            # 反汇编会附加路径注释，因此按前缀匹配
            got = sum(1 for s in stripped if s.startswith(p.new))
            if got < p.count:
                failures.append(f"{p.id}: new content {p.new!r} found {got}x, "
                                f"expected >= {p.count}")
        else:  # BlockPatch
            for seq in p.roundtrip_present:
                if not has_sequence(lines, seq):
                    failures.append(f"{p.id}: expected sequence missing: {' | '.join(seq)!r}")
            for seq in p.roundtrip_absent:
                if has_sequence(lines, seq):
                    failures.append(f"{p.id}: removed sequence still present: {' | '.join(seq)!r}")
    return failures


def _dump_dat_names():
    return sorted(f for f in os.listdir(DUMPS) if f.endswith(".dat"))


def verify_fingerprint(out_dir, spec):
    """三重指纹门：在原 dump、在补丁后 AML、不在任何其他 dump。"""
    fp = spec.fingerprint.encode("ascii")
    failures = []
    own_dat = spec.source[:-len(".dsl")] + ".dat"
    with open(os.path.join(DUMPS, own_dat), "rb") as f:
        own = f.read()
    if fp not in own:
        failures.append(f"{spec.output}: 指纹 {spec.fingerprint!r} 不在原 dump "
                        f"{own_dat} 中 —— TableSpec.fingerprint 声明有误")
    with open(os.path.join(out_dir, spec.output + ".aml"), "rb") as f:
        aml = f.read()
    if fp not in aml:
        failures.append(f"{spec.output}: 指纹 {spec.fingerprint!r} 未在补丁后 "
                        f"AML 中保留 —— 请改选不被补丁触及的序列")
    for name in _dump_dat_names():
        if name == own_dat:
            continue
        with open(os.path.join(DUMPS, name), "rb") as f:
            if fp in f.read():
                failures.append(f"{spec.output}: 指纹 {spec.fingerprint!r} 与 "
                                f"dump {name} 冲突 —— 运行时无法唯一定位")
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--skip-verify", action="store_true",
                        help="skip the AML round-trip and fingerprint gates")
    parser.add_argument("--out-dir", default=os.path.join(REPO, "build"),
                        help="output directory for .aml/.hex/TableRegistry.h "
                             "(default: <repo>/build)")
    args = parser.parse_args()
    out_dir = args.out_dir

    print("[*] Generating patched ACPI table DSLs...")
    generate_patched_tables.main_compat([])

    os.makedirs(out_dir, exist_ok=True)
    all_failures = []
    for modname, spec in discover_tables():
        src = os.path.join(REPO, "src", "acpi", "patched", spec.output + ".dsl")
        out = os.path.join(out_dir, spec.output)

        r = run_iasl(["-p", out, "-tc", src])
        combined = r.stdout + r.stderr
        for line in combined.splitlines():
            low = line.lower()
            if ("error" in low) or ("compilation successful" in low):
                print(line)

        if r.returncode != 0:
            print(f"[-] iASL failed for {src}")
            sys.exit(1)

        hex_path = out + ".hex"
        if not os.path.exists(hex_path):
            print(f"[-] Missing expected output: {hex_path}")
            sys.exit(1)

        print(f"[+] {hex_path}")

        if args.skip_verify:
            continue
        failures = verify_table(out_dir, spec.output, spec.patches)
        if failures:
            all_failures.extend(failures)
        else:
            print(f"[+] round-trip verified: {os.path.join(out_dir, 'roundtrip', spec.output + '.dsl')}")
        all_failures.extend(verify_fingerprint(out_dir, spec))

    if all_failures:
        print("[-] AML round-trip / fingerprint verification FAILED:")
        for f in all_failures:
            print(f"    {f}")
        sys.exit(1)
    if not args.skip_verify:
        print("[+] all patches observable in compiled AML; fingerprints verified")

    registry = emit_registry(out_dir, DUMPS)
    print(f"[+] C-side registry generated: {registry}")

    thmp_cfg = os.path.join(REPO, "tools", "thmp_config.yaml")
    if os.path.exists(thmp_cfg):
        import generate_thmp_config
        with open(thmp_cfg, "r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f)
        thmp_out = os.path.join(out_dir, "ThmpConfig.h")
        with open(thmp_out, "w", encoding="utf-8", newline="\n") as f:
            f.write(generate_thmp_config.generate_header_content(cfg))
        print(f"[+] THMP config generated: {thmp_out}")

    print("[+] Patched ACPI tables built successfully.")


if __name__ == "__main__":
    main()
