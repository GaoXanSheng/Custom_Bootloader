#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""面向历史入口路径的轻量兼容垫片。

补丁管线位于 tools/acpi_patch/（布局见其 docstring）。保留本文件是因为
tools/build_patched_tables.py 会导入 `generate_patched_tables`，且 build.bat
直接调用本脚本。

Usage:
    python tools/generate_patched_tables.py [--dry-run|--diff|--list|-v|--audit]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from acpi_patch.cli import main  # noqa: E402


def main_compat(argv=None):
    """历史 API：失败时以非零码退出。argv=None（默认）时解析真实
    命令行（脚本直跑）；build_patched_tables.py 以本模块身份调用时显式传
    argv=[]，避免把调用方自己的参数（如 --out-dir）重复解析。"""
    code = main(argv)
    if code:
        sys.exit(code)


if __name__ == "__main__":
    main_compat()
