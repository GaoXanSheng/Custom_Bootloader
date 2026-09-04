#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate patched ACPI table ASL sources (src/acpi/patched/*.dsl) from original
firmware tables (tools/acpi/*.dsl).

Patches:
  - SSDT4: CMPL/CNPL power raises, AC platform walls (180W -> 220W), remove CPUC NVIO throttle.
  - DSDT: Disable DBFS CPU clamps in MSPL/MFPT, replace FNQS with safe AC/DC profile dispatch,
          and streamline _Q10 power events.

Usage:
    python tools/generate_patched_tables.py
"""

import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_ACPI = os.path.join(REPO, "tools", "acpi")
OUT_DIR = os.path.join(REPO, "src", "acpi", "patched")

SSDT4_EDITS = [
    # CMPL 0x33 -> 0x50
    (1160, "            Name (CMPL, 0x33)", "            Name (CMPL, 0x50)"),
    # CNPL 0x10 -> 0x36
    (1161, "            Name (CNPL, 0x10)", "            Name (CNPL, 0x36)"),
    # DTPP 120W (0xF0) -> 130W (0x0104)
    (1155, "            Name (DTPP, 0xF0)", "            Name (DTPP, 0x0104)"),
    # DCBT 40W (0x28) -> 80W (0x50): lifts the DC/battery-borrowing power threshold
    (1148, "            Name (DCBT, 0x28)", "            Name (DCBT, 0x50)"),
    # Raise AC platform power walls: 180W (0x168) -> 220W (0x1B8)
    (1263, "                                    ATPP = 0x0168", "                                    ATPP = 0x01B8"),
    (1292, "                                    ATPP = 0x0168", "                                    ATPP = 0x01B8"),
    (1293, "                                    ATP2 = 0x0168", "                                    ATP2 = 0x01B8"),
    # Fix Turbo mode (Else) in dGPU mode: DBAC=0 (unlock GPU to platform ceiling) + TPPA=220W (0x1B8)
    (1284, "                                    DBAC = One", "                                    DBAC = Zero"),
    (1285, "                                    TPPA = 0x0168", "                                    TPPA = 0x01B8"),
    # Fix Turbo mode (Else) in Hybrid mode: DBAC=0 + TPPA=220W
    (1314, "                                    DBAC = One", "                                    DBAC = Zero"),
    (1315, "                                    TPPA = 0x0168", "                                    TPPA = 0x01B8"),
    # Neutralize NVPCF sub-func#6 Case(One): disables CPUC NVIO write AND all 16-core 0x85 throttle notifications
    (1493, "                                If ((IOBS != Zero))", "                                If (Zero)"),
]

DSDT_EDITS = []

# FNQS replacement:
# On AC (ECWR & 1):
#   - Arg0 == 0 (Office/Quiet): Dispatch balanced profile THMD(0x14) (55W/65W)
#   - Arg0 == 1 or 2 (Game/Turbo): Dispatch unlocked full-power profile THMD(Zero) (100W/105W/120W)
# FNQS + MSPL + MFPT replacement:
# Unconditional performance mode:
# - FNQS never calls THMD(0x03) (which caused the 35W/40W throttling during battery borrowing).
#   It always pushes THMD(Zero) (or 0x14 in Quiet mode), followed by MSPL() and MFPT().
# - MSPL / MFPT unconditionally clamp floor values: CSPL >= 80W (0x50), FPPT >= 110W (0x6E).
#   No software or EC event can ever set CPU power below 80W.
FNQS_MSPL_MFPT_NEW_BODY = (
    "                    Method (FNQS, 1, Serialized)\n"
    "                    {\n"
    "                        If ((ToInteger (Arg0) == Zero))\n"
    "                        {\n"
    "                            THMD (0x14)\n"
    "                        }\n"
    "                        Else\n"
    "                        {\n"
    "                            THMD (Zero)\n"
    "                        }\n"
    "\n"
    "                        MSPL ()\n"
    "                        MFPT ()\n"
    "                    }\n"
    "\n"
    "                    Method (MSPL, 0, Serialized)\n"
    "                    {\n"
    "                        Local0 = ECRD (RefOf (CSPL))\n"
    "                        If ((Local0 < 0x50))\n"
    "                        {\n"
    "                            Local0 = 0x50\n"
    "                        }\n"
    "\n"
    "                        Local0 *= 0x03E8\n"
    "                        MODP (0x05, Local0)\n"
    "                        MODP (0x07, Local0)\n"
    "                        MODP (0x13, Local0)\n"
    "                    }\n"
    "\n"
    "                    Method (MFPT, 0, Serialized)\n"
    "                    {\n"
    "                        Local0 = ECRD (RefOf (FPPT))\n"
    "                        If ((Local0 < 0x6E))\n"
    "                        {\n"
    "                            Local0 = 0x6E\n"
    "                        }\n"
    "\n"
    "                        Local0 *= 0x03E8\n"
    "                        MODP (0x06, Local0)\n"
    "                    }\n"
)

# _Q10 replacement: AC/battery transition handler
# Streamlined to never tamper with ITSM or trigger throttling profiles
_Q10_NEW_BODY = (
    "                    Method (_Q10, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF\n"
    "                    {\n"
    "                        Sleep (0x012C)\n"
    "                        Notify (BAT0, 0x80) // Status Change\n"
    "                        Notify (ADP1, 0x80) // Status Change\n"
    "                        Local0 = ECRD (RefOf (ITSM))\n"
    "                        FNQS (Local0)\n"
    "                        ^^^WMID.EVBU [Zero] = One\n"
    "                        ^^^WMID.EVBU [One] = 0x0F\n"
    "                        ^^^WMID.EVBU [0x02] = Local0\n"
    "                        Notify (WMID, 0x20) // Reserved\n"
    "                    }\n"
)

# ADP1._PSR replacement:
# Unconditionally returns 1 (AC mode) to completely prevent Windows and AMD SMU ALIB(1)
# from entering DC throttling states during dual-burn battery-borrowing spikes.
ADP1_PSR_NEW_BODY = (
    "                        Method (_PSR, 0, NotSerialized)  // _PSR: Power Source\n"
    "                        {\n"
    "                            Return (One)\n"
    "                        }\n"
)


def replace_dsdt_power_methods(text):
    """Replace FNQS, _Q10, and ADP1._PSR in DSDT."""
    # 1. FNQS + MSPL + MFPT replacement
    fnqs_start = "                    Method (FNQS, 1, Serialized)\n"
    fnqs_end = "\n\n                    Method (COMM, 0, Serialized)"
    i = text.find(fnqs_start)
    j = text.find(fnqs_end, i) if i >= 0 else -1
    if i < 0 or j < 0:
        sys.exit("[-] dsdt: FNQS/COMM method pair not found")
    text = text[:i] + FNQS_MSPL_MFPT_NEW_BODY + text[j:]

    # 2. _Q10 replacement
    q10_start = "                    Method (_Q10, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF\n"
    q10_end = "\n\n                    Method (_Q11, 0, NotSerialized)"
    i = text.find(q10_start)
    j = text.find(q10_end, i) if i >= 0 else -1
    if i < 0 or j < 0:
        sys.exit("[-] dsdt: _Q10/_Q11 method pair not found")
    text = text[:i] + _Q10_NEW_BODY + text[j:]

    # 3. ADP1._PSR replacement
    psr_start = "                        Method (_PSR, 0, NotSerialized)  // _PSR: Power Source\n"
    psr_end = "\n\n                        Method (_PCL, 0, NotSerialized)"
    i = text.find(psr_start)
    j = text.find(psr_end, i) if i >= 0 else -1
    if i < 0 or j < 0:
        sys.exit("[-] dsdt: ADP1._PSR/_PCL method pair not found")
    text = text[:i] + ADP1_PSR_NEW_BODY + text[j:]

    return text


def apply_edits(src_name, edits, out_name, transform=None):
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
    if transform is not None:
        joined = transform("".join(out_lines))
        out_lines = joined.splitlines(keepends=True)

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(out_lines)

    print(f"[+] {out_name}: {len(edits)} edits applied -> {out_path}")
    return out_path


def main():
    p1 = apply_edits("ssdt4.dsl", SSDT4_EDITS, "ssdt4_patched.dsl")
    p2 = apply_edits("dsdt.dsl", DSDT_EDITS, "dsdt_patched.dsl",
                     transform=replace_dsdt_power_methods)
    print("[+] dsdt_patched.dsl: FNQS/_Q10/ADP1._PSR replaced (battery drop lock eliminated)")
    print("[+] Done. Compile with iASL:")
    print(f"    iasl -p build/ssdt4_patched -tc {p1}")
    print(f"    iasl -p build/dsdt_patched  -tc {p2}")


if __name__ == "__main__":
    main()
