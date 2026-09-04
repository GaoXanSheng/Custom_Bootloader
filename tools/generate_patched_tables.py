#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate patched ACPI table ASL sources (src/acpi/patched/*.dsl) from the
original firmware tables (tools/acpi/*.dsl).

Replaces the old runtime byte-SearchAndReplace patch engine (patches/*.txt):
patches are now applied as ASL source edits, compiled offline by iASL into
legal AML, and the bootloader swaps the whole table via XSDT redirection.

Usage:
    python tools/generate_patched_tables.py
"""

import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_ACPI = os.path.join(REPO, "tools", "acpi")
OUT_DIR = os.path.join(REPO, "src", "acpi", "patched")

# ---------------------------------------------------------------------------
# Modification specs: (1-based line, expected original text, new text or None to delete)
#
# IMPORTANT (2026-08-14 final findings — verified by bisection):
#   Dynamic Boost (DBAC/DBFS) must stay STOCK (disabled): DBAC=1 pins the
#   GPU at 97.5W; DB off lets the driver use the platform max (139.5W).
#   TGPA 140W -> 102.5W; MAGA=0 -> 120W; platform walls 260W -> 131.5W.
#   ALL must stay stock. Only CMPL/CNPL raise + CPUC removal are kept
#   (they push the limit to 139.54W, the VBIOS ceiling).
# ---------------------------------------------------------------------------

SSDT4_EDITS = [
    # nvpcf_cmpl_unlock: CMPL 0x33(51W) -> 0x50(80W)
    (1160, "            Name (CMPL, 0x33)", "            Name (CMPL, 0x50)"),
    # nvpcf_cnpl_unlock: CNPL 0x10(16W) -> 0x36(54W)
    (1161, "            Name (CNPL, 0x10)", "            Name (CNPL, 0x36)"),
    # platform_walls_220w: raise AC platform power wall 180W -> 220W (0x1B8).
    # Stock 180W caps CPU+GPU at ~180W (dual-burn: GPU 139.9 + CPU 39.8).
    # 260W previously dropped the GPU driver limit to 131.5W; 220W is the
    # middle ground — GPU ~135W + CPU ~80W in dual-burn.
    (1153, "            Name (ATPP, 0x01B8)", "            Name (ATPP, 0x01E0)"),
    (1155, "            Name (DTPP, 0xF0)", "            Name (DTPP, 0x0104)"),
    (1263, "                                    ATPP = 0x0168", "                                    ATPP = 0x01B8"),
    (1285, "                                    TPPA = 0x0168", "                                    TPPA = 0x01B8"),
    (1292, "                                    ATPP = 0x0168", "                                    ATPP = 0x01B8"),
    (1293, "                                    ATP2 = 0x0168", "                                    ATP2 = 0x01B8"),
    (1315, "                                    TPPA = 0x0168", "                                    TPPA = 0x01B8"),
    # tgpa: KEEP STOCK. TGPA 140W made the driver drop the GPU limit to
    # 102.5W (measured); stock 120W leaves it at 138-139.5W.
    # maga: KEEP STOCK. MAGA=0 dropped the limit 131.5->120W (measured).
    # nvpcf_bypass: remove `CPUC = NCHP` (skip I/O write that re-clamps platform power)
    (1501, "                                    CPUC = NCHP /* \\_SB_.NPCF.NPCF.NCHP */", None),
    # NOTE: DBAC = Zero assignments (1268/1276/1298/1306) intentionally kept
    # at stock — Dynamic Boost must stay DISABLED (see header comment).
]

DSDT_EDITS = [
    # cpu_clamp_DBFS_unlock: MSPL/MFPT DBFS==1 clamp block never runs (If Zero)
    (5106, "                        If ((DBFS == One))", "                        If (Zero)"),
    (5140, "                        If ((DBFS == One))", "                        If (Zero)"),
    # r7_cspl_60w: 0x2D(45W) -> 0x3C(60W)
    (5112, "                                    Local0 = 0x2D", "                                    Local0 = 0x3C"),
    (5126, "                                    Local0 = 0x2D", "                                    Local0 = 0x3C"),
    # r9_cspl_60w: 0x37(55W) -> 0x3C(60W)
    (5119, "                                    Local0 = 0x37", "                                    Local0 = 0x3C"),
    # r7_fppt_80w: 0x41(65W) -> 0x50(80W)
    (5146, "                                    Local0 = 0x41", "                                    Local0 = 0x50"),
    (5160, "                                    Local0 = 0x41", "                                    Local0 = 0x50"),
    # r9_fppt_80w: 0x4B(75W) -> 0x50(80W)
    (5153, "                                    Local0 = 0x4B", "                                    Local0 = 0x50"),
    # fnqs_cpu_override: at the END of FNQS, on AC + adapter >= 200W, re-push
    # CSPL/FPPT = 80W/80W to the SMU. (CPU-side unlock; FNQS branch logic
    # itself stays stock so DBFS/DB state is untouched.)
    (5100, "                        }",
     "                        }\n"
     "\n"
     "                        If (((Local0 >= 0xC8) && (Local1 & One)))\n"
     "                        {\n"
     "                            ECWT (0x50, RefOf (CSPL))\n"
     "                            MSPL ()\n"
     "                            ECWT (0x50, RefOf (FPPT))\n"
     "                            MFPT ()\n"
     "                        }"),
]


def apply_edits(src_name, edits, out_name):
    src_path = os.path.join(SRC_ACPI, src_name)
    out_path = os.path.join(OUT_DIR, out_name)

    with open(src_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    for lineno, old, new in edits:
        idx = lineno - 1
        if idx >= len(lines):
            print(f"[-] {out_name}: line {lineno} out of range")
            sys.exit(1)
        actual = lines[idx].rstrip("\r\n")
        if actual != old:
            print(f"[-] {out_name}: line {lineno} mismatch!\n"
                  f"    expected: {old!r}\n    actual:   {actual!r}")
            sys.exit(1)
        if new is None:
            lines[idx] = None  # mark for deletion
        else:
            lines[idx] = new + "\n"

    out_lines = [l for l in lines if l is not None]

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(out_lines)

    print(f"[+] {out_name}: {len(edits)} edits applied -> {out_path}")
    return out_path


def main():
    p1 = apply_edits("ssdt4.dsl", SSDT4_EDITS, "ssdt4_patched.dsl")
    p2 = apply_edits("dsdt.dsl", DSDT_EDITS, "dsdt_patched.dsl")
    print("[+] Done. Compile with iASL:")
    print(f"    iasl -p build/ssdt4_patched -tc {p1}")
    print(f"    iasl -p build/dsdt_patched  -tc {p2}")


if __name__ == "__main__":
    main()
