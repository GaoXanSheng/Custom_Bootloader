"""针对 ASL 补丁定义的注释声明审计。

历史上这是针对 src/InplacePatches.h 中 C 原地回退的双通道审计。该回退已于
2026-09-12 移除（整表替换失败时它会悄悄产生漂移值），因此审计如今是单通道
检查：通过 claims.py 校验补丁注释中做出的溯源声明。任何声明失败时退出码
为 1。
"""

from .claims import run_claims


def run_audit():
    print("== comment-claim audit (single ASL channel; C fallback removed 2026-09-12) ==")
    rc = run_claims()
    print()
    return 1 if rc else 0
