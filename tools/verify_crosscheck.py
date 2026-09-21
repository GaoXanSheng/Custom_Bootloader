#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""独立交叉校验（校验逻辑刻意不复用引擎的 apply 路径；但表清单来自
discovery，因此本门禁能自动看到每一个已注册的表）。

C 原地回退通道（src/InplacePatches.h）已于 2026-09-12 移除：整表替换失败时
它会悄悄产生漂移值。如今只剩 ASL 通道，因此本脚本针对原始固件 dump 文本
校验其补丁定义：

1. LinePatch 的旧行在原始 dsl 文本（tools/acpi/*.dsl）中的计数，包括上下文
   消歧。
2. BlockPatch 起止标记的唯一性与顺序。
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()

# ---------- ASL 通道 ----------
sys.path.insert(0, os.path.join(REPO, "tools"))
from acpi_patch.discovery import discover_tables
from acpi_patch.model import LinePatch, BlockPatch, MATCH_PREFIX

def base(l):
    return l.split("//")[0].strip()

print("=" * 78)
print("ASL CHANNEL: patch defs vs original DSL text (comment-tolerant)")
print("=" * 78)
bad = 0
for modname, spec in discover_tables():
    table = modname[len("patches_"):]
    lines = read(os.path.join(REPO, "tools", "acpi", spec.source)).splitlines()
    stripped = [base(l) for l in lines]
    for p in spec.patches:
        if isinstance(p, LinePatch):
            use_prefix = (p.mode == MATCH_PREFIX)
            if use_prefix:
                got = sum(1 for l in stripped if l.startswith(p.old))
            else:
                got = sum(1 for l in stripped if l == p.old)
            cnt = getattr(p, "count", 1)
            ctx = getattr(p, "context", None) or ()
            if ctx:
                # 引擎语义：命中会被过滤，只保留上下文每一行都落在
                # CONTEXT_WINDOW（6）行窗口内的命中
                idxs = [i for i, l in enumerate(stripped)
                        if l == p.old or (use_prefix and l.startswith(p.old))]
                got = sum(1 for i in idxs
                          if all(any(stripped[j] == c for j in range(max(0, i - 6), i + 7))
                                 for c in ctx))
            ok = got == cnt
            if not ok:
                bad += 1
            print(f"[{'OK ' if ok else 'MISMATCH'}] {table}:{p.id:22s} old={p.old!r} declared={cnt} found={got}"
                  + (" (context-filtered)" if ctx else ""))
            if ctx:
                for c in ctx:
                    print(f"       context {c!r} applied")
        elif isinstance(p, BlockPatch):
            # 引擎语义：start 必须唯一；end 取 start 之后的第一次出现
            # （不必在全表中唯一）
            starts = [i for i, l in enumerate(stripped) if l == p.start_marker]
            ends_after = [i for i, l in enumerate(stripped) if l == p.end_marker and starts and i > starts[0]]
            ok = len(starts) == 1 and len(ends_after) >= 1
            if not ok:
                bad += 1
            ends_all = [i for i, l in enumerate(stripped) if l == p.end_marker]
            print(f"[{'OK ' if ok else 'MISMATCH'}] {table}:{p.id:22s} start={p.start_marker!r} x{len(starts)}  end={p.end_marker!r} x{len(ends_all)} (first-after-start used)")
            for seq in p.roundtrip_absent:
                found = all(any(s == t for s in stripped) for t in seq)
                print(f"       absent-seq tokens exist in original: {found}  ({' | '.join(seq)[:60]})")

print(f"ASL channel mismatches: {bad}")
sys.exit(1 if bad else 0)
