"""表清单自动发现：扫描 patches_*.py 中导出 TABLE 的模块。

在此之前，表清单（dsdt + ssdt4）在 cli.py、build_patched_tables.py、
verify_crosscheck.py、src/AcpiPatch.c 的手写数组四处重复，注册一张新表
要同步改四五个地方。现在唯一的注册动作是：dump 放进 tools/acpi/，写一个
patches_<表名>.py 导出 TABLE（TableSpec），其余链路全部从这里取清单。
"""

import importlib
import os
import re

_PACKAGE_DIR = os.path.dirname(os.path.abspath(__file__))
# 仅接受 patches_<小写字母数字下划线>.py 命名的模块，避免误扫 __init__ 等
_MODULE_RE = re.compile(r"^patches_[a-z0-9_]+\.py$")


def discover_tables():
    """返回按模块名排序的 [(模块名, TableSpec)]。

    模块名排序保证生成物（patched dsl、hex、C 注册表）顺序确定，
    可复现构建。导出 TABLE 之外仍导出 PATCHES 的旧模块照常被发现。
    """
    specs = []
    for fname in sorted(os.listdir(_PACKAGE_DIR)):
        if not _MODULE_RE.match(fname):
            continue
        modname = fname[:-3]
        mod = importlib.import_module("." + modname, __package__)
        table = getattr(mod, "TABLE", None)
        if table is None:
            continue
        # 基本完整性：dump 源与指纹必填，缺了当场报错而不是等到编译期
        if not table.source or not table.fingerprint or not table.output:
            raise SystemExit(
                f"[-] {modname}.TABLE 缺少 source/output/fingerprint 之一，"
                f"请补全 TableSpec")
        specs.append((modname, table))
    if not specs:
        raise SystemExit(
            "[-] 未发现任何 patches_*.py 导出 TABLE —— 至少需要注册一张表")
    return specs
