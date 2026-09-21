"""ACPI 补丁流水线命令行入口。

所有文件写盘都集中在这里。输出写到固定仓库目录 src/acpi/patched/，
路径由 TableSpec.output 拼出、write_bytes() 逐字节写出（无平台换行
转换）。表清单来自 discovery.discover_tables() 自动发现 —— 注册新表
不再需要改这里。

用法：
    python tools/generate_patched_tables.py [--dry-run|--diff|--list|-v|--audit|--table <名>]
"""

import argparse
import os
import pathlib
import sys

from .discovery import discover_tables
from .engine import print_report, process_table, unified_diff
from .model import MATCH_PREFIX

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_ACPI = os.path.join(REPO, "tools", "acpi")
OUT_DIR = pathlib.Path(REPO) / "src" / "acpi" / "patched"
FRAGMENTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fragments")


def table_keys():
    """--table 可选名 = 模块名去掉 patches_ 前缀（dsdt、ssdt4、……）。"""
    return [modname[len("patches_"):] for modname, _ in discover_tables()]


def build_parser():
    p = argparse.ArgumentParser(
        prog="generate_patched_tables",
        description="Apply ASL patches to the firmware table dumps and emit "
                    "src/acpi/patched/*.dsl. Run with no arguments to generate.")
    p.add_argument("--dry-run", action="store_true",
                   help="locate and verify every patch against the current dumps, write nothing")
    p.add_argument("--diff", action="store_true",
                   help="print a unified diff (dump -> patched) for each table")
    p.add_argument("--list", action="store_true",
                   help="print the patch inventory and exit")
    p.add_argument("--verbose", "-v", action="store_true",
                   help="show per-patch details")
    p.add_argument("--table", choices=table_keys(),
                   help="process only one table (name = patches_<name>.py)")
    p.add_argument("--audit", action="store_true",
                   help="comment-claim checks over the ASL patch definitions; "
                        "exits 1 on failed claims")
    return p


def cmd_list(args):
    for modname, spec in discover_tables():
        key = modname[len("patches_"):]
        if args.table and key != args.table:
            continue
        patches = spec.patches
        sites = sum(p.count for p in patches if p.enabled)
        kind = "line patches" if any(hasattr(p, "mode") for p in patches) else "method rewrites"
        print(f"== {spec.source} -> {spec.output}.dsl ({kind}, fingerprint {spec.fingerprint!r}) ==")
        for p in patches:
            if hasattr(p, "mode"):
                star = "*" if p.mode == MATCH_PREFIX else ""
                print(f"  {p.id:<26} x{p.count}  {p.old}{star}  =>  {p.new}")
            else:
                print(f"  {p.id:<26} x{p.count}  [{p.fragment}]  "
                      f"{p.start_marker} .. {p.end_marker}")
            print(f"{'':30}{p.comment}")
            if p.evidence:
                print(f"{'':30}evidence: {p.evidence}")
        print(f"  {sites} patch sites in {spec.source}")
        print()


def write_output(spec, lines):
    """Emit one patched table. The target file is derived from TableSpec.output;
    there is no output whitelist to keep in sync anymore."""
    text = "".join(lines).encode("utf-8")
    target = OUT_DIR / f"{spec.output}.dsl"
    os.makedirs(OUT_DIR, exist_ok=True)
    target.write_bytes(text)
    return target


def run_tables(args):
    rc = 0
    wrote_any = False
    for modname, spec in discover_tables():
        key = modname[len("patches_"):]
        if args.table and key != args.table:
            continue
        src_path = os.path.join(SRC_ACPI, spec.source)
        report = process_table(src_path, spec.patches, FRAGMENTS)
        print_report(report, verbose=args.verbose)
        if report.failures:
            rc = 1
            print(f"  ({spec.output}.dsl not written: {len(report.failures)} failure(s))")
            print()
            continue
        if not args.dry_run:
            target = write_output(spec, report.result_lines)
            print(f"  [write] {os.path.relpath(target, REPO)}")
            wrote_any = True
        if args.diff:
            sys.stdout.write(unified_diff(src_path, report.result_lines))
    if args.dry_run:
        print("dry run: no files written" if not rc
              else "dry run FAILED: dumps no longer match the patch definitions")
    elif wrote_any and not rc:
        print(f"[+] patched DSLs regenerated under {OUT_DIR}")
    return rc


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.list:
        cmd_list(args)
        return 0
    if args.audit:
        from .audit import run_audit
        return run_audit()
    return run_tables(args)
