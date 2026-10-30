#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
批量编译 Custom ACPI 补丁表 (tools/acpi_patched/*.dsl -> build/Custom/*.aml)
供 Custom Bootloader 在引导时通过 ESP:\\EFI\\Custom\\*.aml 动态加载、热替换与注入。

Usage:
    python tools/build_custom_tables.py [--source-dir DIR] [--out-dir DIR]
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_SRC_DIR = os.path.join(REPO_ROOT, "tools", "acpi_patched")
DEFAULT_OUT_DIR = os.path.join(REPO_ROOT, "build", "Custom")
IASL_EXE = os.path.join(REPO_ROOT, "tools", "isal", "iasl.exe")


def find_iasl():
    if os.path.isfile(IASL_EXE):
        return IASL_EXE
    path_iasl = shutil.which("iasl")
    if path_iasl:
        return path_iasl
    return None


def compile_dsl(iasl_path, dsl_path, out_dir):
    base_name = os.path.splitext(os.path.basename(dsl_path))[0]
    out_prefix = os.path.join(out_dir, base_name)
    out_aml = out_prefix + ".aml"

    print(f"[*] 正在编译补丁表: {os.path.basename(dsl_path)}")
    cmd = [iasl_path, "-p", out_prefix, dsl_path]
    proc = subprocess.run(cmd, capture_output=True, text=True)

    if proc.returncode != 0 or not os.path.isfile(out_aml):
        print(f"[-] 编译失败: {dsl_path} (exit code: {proc.returncode})", file=sys.stderr)
        print(proc.stdout, file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        return False

    with open(out_aml, "rb") as f:
        header = f.read(36)
    if len(header) < 36:
        print(f"[-] 错误: 生成的 AML 文件过小: {out_aml}", file=sys.stderr)
        return False

    sig = header[:4].decode("ascii", errors="replace")
    aml_size = os.path.getsize(out_aml)
    print(f"[+] 成功编译: {os.path.basename(out_aml)} (签名: {sig}, 大小: {aml_size} 字节)")
    return True


def main():
    parser = argparse.ArgumentParser(description="Batch compile ACPI DSL override tables.")
    parser.add_argument("--source-dir", default=DEFAULT_SRC_DIR, help="Source directory containing *.dsl files")
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR, help="Output directory for *.aml files")
    args = parser.parse_args()

    src_dir = os.path.abspath(args.source_dir)
    out_dir = os.path.abspath(args.out_dir)

    iasl = find_iasl()
    if not iasl:
        print(f"[-] 错误: 未找到 iasl 编译器，请检查 {IASL_EXE} 是否存在", file=sys.stderr)
        sys.exit(1)

    os.makedirs(out_dir, exist_ok=True)

    if not os.path.isdir(src_dir):
        print(f"[-] 警告: 补丁源目录不存在: {src_dir}")
        sys.exit(0)

    dsl_files = sorted(glob.glob(os.path.join(src_dir, "*.dsl")))
    if not dsl_files:
        print(f"[*] 提示: 目录 {src_dir} 中未找到任何 *.dsl 补丁源文件")
        sys.exit(0)

    print(f"[*] 发现 {len(dsl_files)} 个补丁源文件，开始批量编译...")
    success_count = 0
    for dsl in dsl_files:
        if compile_dsl(iasl, dsl, out_dir):
            success_count += 1
        else:
            sys.exit(1)

    print(f"[+] 批量编译完成: 成功生成 {success_count} 个 ACPI 补丁表至 {out_dir}")


if __name__ == "__main__":
    main()
