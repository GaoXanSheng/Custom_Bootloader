"""补丁引擎：对照固件 dump 定位、应用并校验补丁。

设计规则：
- 匹配基于内容（剥离空白后的行），绝不基于行号。行号只出现在报告里。
- 出现次数是精确断言：期望一次却命中两次的补丁会响亮报错，而不是静默
  补错位置。
- 失败按次收集、一次性汇总报告（编译器式诊断）。
- 报告中的每个事实都来自一次真实检查。
- 本模块是纯函数：读补丁输入（fragment 文件限定在 fragments 目录内）、
  返回补丁后的行缓冲；一切输出写盘都归 CLI 所有。
"""

import difflib
import os
import re

from .model import MATCH_EXACT, MATCH_PREFIX, PatchOutcome, TableReport

# LinePatch 上下文条目距离命中点最多多少行。
CONTEXT_RADIUS = 4
# 匹配失败时显示的最近行建议数。
SUGGESTIONS = 3


class PatchFailure(Exception):
    def __init__(self, patch_id, message):
        super().__init__(message)
        self.patch_id = patch_id
        self.message = message


def _fragment_path(fragments_dir, name):
    """fragment 名来自补丁定义（数据），因此限定为只能解析进
    fragments_dir 内的纯文件名。"""
    if (not name or name != os.path.basename(name)
            or name in (".", "..") or ".." in name
            or "/" in name or "\\" in name):
        raise PatchFailure("fragments", f"invalid fragment name: {name!r}")
    base = os.path.realpath(fragments_dir)
    path = os.path.realpath(os.path.join(base, name))
    if os.path.dirname(path) != base:
        raise PatchFailure("fragments", f"fragment escapes fragments dir: {name!r}")
    return path


def read_lines(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.readlines()


def read_fragment(fragments_dir, name):
    """fragment 内容原样拼接，含尾随空行（它们复刻历史生成器的方法间
    空行排版）。"""
    with open(_fragment_path(fragments_dir, name), "r", encoding="utf-8") as f:
        return f.read()


def _line_match(stripped, needle, mode):
    if mode == MATCH_PREFIX:
        return stripped.startswith(needle)
    return stripped == needle


_LEADING_HEX_COMMENT = re.compile(r"^/\*\s*[0-9A-Fa-f]{4}\s*\*/\s*")


def has_sequence(lines, seq):
    """当存在一段连续行与 seq 逐元素匹配（剥离空白、前缀语义——iASL
    dump 附加尾注，且十六进制 dump 行前有 /* offset */ 注释）时为真。"""
    stripped = [_LEADING_HEX_COMMENT.sub("", l.strip()) for l in lines]
    for i in range(len(stripped) - len(seq) + 1):
        if all(stripped[i + j].startswith(seq[j]) for j in range(len(seq))):
            return True
    return False


def find_hits(lines, needle, mode, context=()):
    stripped = [l.strip() for l in lines]
    hits = [i for i, s in enumerate(stripped) if _line_match(s, needle, mode)]
    if context:
        filtered = []
        for i in hits:
            window = stripped[max(0, i - CONTEXT_RADIUS):i + CONTEXT_RADIUS + 1]
            if all(any(c in w for w in window) for c in context):
                filtered.append(i)
        hits = filtered
    return hits


def _nearest(lines, needle, mode):
    stripped = [l.strip() for l in lines]
    if mode == MATCH_PREFIX:
        scored = [s for s in stripped if needle[: min(len(needle), 8)] in s]
    else:
        scored = stripped
    matches = difflib.get_close_matches(needle, scored, n=SUGGESTIONS, cutoff=0.4)
    out = []
    for m in matches:
        i = stripped.index(m)
        out.append((i + 1, m))
    return out


def apply_line_patch(lines, patch):
    """返回 (新行列表, 命中点)。替换保留被匹配行自身的缩进，使补丁定义
    与缩进无关。每处替换在同一索引空间内即时自检（全局"旧行已消失"检查
    对上下文刻意在别处保留相同行的补丁是错的）。"""
    hits = find_hits(lines, patch.old, patch.mode, patch.context)
    if len(hits) != patch.count:
        where = ", ".join(f"line {h + 1}" for h in hits) or "nowhere"
        msg = (f"expected {patch.count} occurrence(s) of {patch.old!r} "
               f"({patch.mode}), found {len(hits)} at {where}")
        near = _nearest(lines, patch.old, patch.mode)
        if near:
            msg += "\n         nearest lines: " + "; ".join(
                f"{n}: {s}" for n, s in near)
        raise PatchFailure(patch.id, msg)
    new = list(lines)
    for h in hits:
        indent = lines[h][: len(lines[h]) - len(lines[h].lstrip())]
        new[h] = indent + patch.new + "\n"
    for h in hits:
        got = new[h].strip()
        ok = got == patch.new or (patch.mode == MATCH_PREFIX
                                  and got.startswith(patch.new))
        if not ok:
            raise PatchFailure(patch.id,
                               f"post-verify: line {h + 1} is {got!r}, "
                               f"expected {patch.new!r}")
    return new, hits


def apply_block_patch(lines, patch, fragments_dir):
    """把 [起始标记行 .. 结束标记行前一行] 替换为 fragment 文件的原样
    内容。结束标记取起始之后的第一处出现；两个标记都按剥离行前缀匹配。"""
    starts = find_hits(lines, patch.start_marker, MATCH_PREFIX)
    if len(starts) != patch.count:
        where = ", ".join(f"line {h + 1}" for h in starts) or "nowhere"
        raise PatchFailure(
            patch.id,
            f"expected {patch.count} occurrence(s) of start marker "
            f"{patch.start_marker!r}, found {len(starts)} at {where}")
    stripped = [l.strip() for l in lines]
    fragment_lines = read_fragment(fragments_dir, patch.fragment).splitlines(keepends=True)
    new = list(lines)
    spans = []
    for s in sorted(starts, reverse=True):
        end = next((i for i in range(s + 1, len(lines))
                    if stripped[i].startswith(patch.end_marker)), None)
        if end is None:
            raise PatchFailure(
                patch.id,
                f"end marker {patch.end_marker!r} not found after line {s + 1}")
        new[s:end] = fragment_lines
        spans.append((s + 1, end))  # 1-based, end exclusive
    return new, spans


def post_verify_block(new_lines, patch):
    stripped = [l.strip() for l in new_lines]
    n_start = sum(1 for s in stripped if s.startswith(patch.start_marker))
    if n_start != patch.count:
        return f"start marker count changed to {n_start}, expected {patch.count}"
    return None


def parse_definition_block(dsl_path):
    """从 dump 的 DefinitionBlock 头解析运行时匹配键。

    返回 dict(signature=四字符签名, oem_id, table_id, oem_revision)。这些键
    直接进 emit_registry 生成的 C 匹配表，因此解析失败要当场报错，绝不许
    静默回退到某份手写副本。
    """
    pattern = re.compile(
        r'DefinitionBlock\s*\(\s*"[^"]*"\s*,\s*"([A-Za-z]{4})"\s*,\s*\d+\s*,\s*'
        r'"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)')
    for line in read_lines(dsl_path):
        m = pattern.search(line)
        if m:
            sig, oem_id, table_id, oem_rev = m.groups()
            return {
                "signature": sig,
                "oem_id": oem_id,
                "table_id": table_id,
                "oem_revision": int(oem_rev, 0),
            }
    raise PatchFailure(
        "definition-block",
        f"{dsl_path}: DefinitionBlock 头未找到或格式无法解析")


def process_table(src_path, patches, fragments_dir):
    """对一张表的源行应用其全部补丁。返回 TableReport，result_lines 持有
    补丁后的缓冲（此处不写任何文件）；补丁失败收集在 report 上而不抛出。"""
    lines = read_lines(src_path)
    report = TableReport(src_name=os.path.basename(src_path), out_name="")
    report.patches = patches
    for patch in patches:
        if not patch.enabled:
            report.outcomes.append(PatchOutcome(patch.id, ok=True,
                                                detail="disabled, skipped"))
            continue
        verr = None
        try:
            if hasattr(patch, "mode"):  # LinePatch (self-verifying on apply)
                new_lines, hits = apply_line_patch(lines, patch)
            else:  # BlockPatch
                new_lines, hits = apply_block_patch(lines, patch, fragments_dir)
                verr = post_verify_block(new_lines, patch)
        except PatchFailure as exc:
            report.failures.append(exc)
            report.outcomes.append(PatchOutcome(exc.patch_id, ok=False,
                                                detail=exc.message))
            continue
        if verr:
            report.failures.append(PatchFailure(patch.id, f"post-verify: {verr}"))
            report.outcomes.append(PatchOutcome(patch.id, ok=False, detail=verr))
            continue
        lines = new_lines
        report.outcomes.append(PatchOutcome(patch.id, ok=True, hits=hits))
    report.result_lines = lines
    return report


def unified_diff(src_path, patched_lines):
    orig = read_lines(src_path)
    diff = difflib.unified_diff(
        orig, patched_lines,
        fromfile="a/" + os.path.basename(src_path),
        tofile="b/" + os.path.basename(src_path))
    return "".join(diff)


def print_report(report, verbose=False):
    print(f"== {report.src_name} ==")
    for outcome, patch in zip(report.outcomes, report.patches):
        if not outcome.ok:
            print(f"  [FAIL] {outcome.patch_id}")
            for line in outcome.detail.splitlines():
                print(f"         {line}")
            continue
        if outcome.detail == "disabled, skipped":
            print(f"  [skip] {outcome.patch_id} (disabled)")
            continue
        loc = ", ".join(f"line {h + 1}" if isinstance(h, int) else f"lines {h[0]}-{h[1]}"
                        for h in outcome.hits) or "no positional hits"
        print(f"  [ok]   {outcome.patch_id}  ({loc})  {patch.comment}")
        if verbose:
            for h in outcome.hits:
                if isinstance(h, int):
                    star = "*" if patch.mode == MATCH_PREFIX else ""
                    print(f"         -> {patch.old}{star}  =>  {patch.new}")
                else:
                    print(f"         -> replaced {patch.fragment} between "
                          f"'{patch.start_marker}' and '{patch.end_marker}'")
    status = "OK" if not report.failures else f"{len(report.failures)} FAILURE(S)"
    print(f"  {report.applied}/{report.total} patches applied, {status}")
    print()
