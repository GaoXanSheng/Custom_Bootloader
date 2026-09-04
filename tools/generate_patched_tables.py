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
    # Raise AC platform power walls: 180W (0x168) -> 220W (0x1B8)
    (1263, "                                    ATPP = 0x0168", "                                    ATPP = 0x01B8"),
    (1292, "                                    ATPP = 0x0168", "                                    ATPP = 0x01B8"),
    (1293, "                                    ATP2 = 0x0168", "                                    ATP2 = 0x01B8"),
    # Fix Turbo mode (Else) in dGPU mode: disable Dynamic Boost clamp, assign 140W GPU + 100W CPU, use ATPP wall
    (1284, "                                    DBAC = One", "                                    DBAC = Zero\n                                    TGPA = 0x0118\n                                    MIGA = Zero\n                                    MAGA = 0xC8"),
    (1285, "                                    TPPA = 0x0168", "                                    TPPA = ATPP"),
    # Fix Turbo mode (Else) in Hybrid mode: disable Dynamic Boost clamp, assign 140W GPU + 100W CPU, use ATP2 wall
    (1314, "                                    DBAC = One", "                                    DBAC = Zero\n                                    TGPA = 0x0118\n                                    MIGA = Zero\n                                    MAGA = 0xC8"),
    (1315, "                                    TPPA = 0x0168", "                                    TPPA = ATP2"),
    # Neutralize NVPCF sub-func#6 Case(One): disables CPUC NVIO write AND all 16-core 0x85 throttle notifications
    (1493, "                                If ((IOBS != Zero))", "                                If (Zero)"),
]

DSDT_EDITS = []

# FNQS replacement:
# On AC (ECWR & 1):
#   - Arg0 == 0 (Office/Quiet): Dispatch balanced profile THMD(0x14) (55W/65W)
#   - Arg0 == 1 or 2 (Game/Turbo): Dispatch unlocked full-power profile THMD(Zero) (100W/105W/120W)
# FNQS + MSPL + MFPT replacement:
# Immunized against battery-borrowing events:
# - Checks (ECWR & 1) OR adapter wattage ((AWHG << 8) + AWLW > 50W).
#   When plugged into a 240W/280W adapter, battery borrowing transiently clears ECWR bit 0,
#   which previously triggered the 35W battery profile THMD(0x03).
#   With adapter wattage detection, AC full-power is safely maintained during dual-load borrowing!
FNQS_MSPL_MFPT_NEW_BODY = (
    "                    Method (FNQS, 1, Serialized)\n"
    "                    {\n"
    "                        Local1 = (ECRD (RefOf (AWHG)) << 0x08)\n"
    "                        Local1 += ECRD (RefOf (AWLW))\n"
    "                        If (((ECRD (RefOf (ECWR)) & One) || (Local1 > 0x32)))\n"
    "                        {\n"
    "                            If ((ToInteger (Arg0) == Zero))\n"
    "                            {\n"
    "                                THMD (0x14)\n"
    "                            }\n"
    "                            Else\n"
    "                            {\n"
    "                                THMD (Zero)\n"
    "                            }\n"
    "\n"
    "                            MSPL ()\n"
    "                            MFPT ()\n"
    "                        }\n"
    "                        Else\n"
    "                        {\n"
    "                            THMD (0x03)\n"
    "                        }\n"
    "                    }\n"
    "\n"
    "                    Method (MSPL, 0, Serialized)\n"
    "                    {\n"
    "                        Local0 = ECRD (RefOf (CSPL))\n"
    "                        Local1 = (ECRD (RefOf (AWHG)) << 0x08)\n"
    "                        Local1 += ECRD (RefOf (AWLW))\n"
    "                        If (((ECRD (RefOf (ECWR)) & One) || (Local1 > 0x32)))\n"
    "                        {\n"
    "                            If ((Local0 < 0x50))\n"
    "                            {\n"
    "                                Local0 = 0x50\n"
    "                            }\n"
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
    "                        Local1 = (ECRD (RefOf (AWHG)) << 0x08)\n"
    "                        Local1 += ECRD (RefOf (AWLW))\n"
    "                        If (((ECRD (RefOf (ECWR)) & One) || (Local1 > 0x32)))\n"
    "                        {\n"
    "                            If ((Local0 < 0x6E))\n"
    "                            {\n"
    "                                Local0 = 0x6E\n"
    "                            }\n"
    "                        }\n"
    "\n"
    "                        Local0 *= 0x03E8\n"
    "                        MODP (0x06, Local0)\n"
    "                    }\n"
)

# _Q10 replacement: AC/battery transition handler with battery-borrowing immunity
_Q10_NEW_BODY = (
    "                    Method (_Q10, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF\n"
    "                    {\n"
    "                        Sleep (0x012C)\n"
    "                        Notify (BAT0, 0x80) // Status Change\n"
    "                        Notify (ADP1, 0x80) // Status Change\n"
    "                        Local1 = (ECRD (RefOf (AWHG)) << 0x08)\n"
    "                        Local1 += ECRD (RefOf (AWLW))\n"
    "                        If (((ECRD (RefOf (ECWR)) & One) || (Local1 > 0x32)))\n"
    "                        {\n"
    "                            Local0 = ECRD (RefOf (ITSM))\n"
    "                            FNQS (Local0)\n"
    "                        }\n"
    "                        Else\n"
    "                        {\n"
    "                            If ((ECRD (RefOf (CMEN)) == One))\n"
    "                            { \n"
    "                                ECWT (Zero, RefOf (CMEN))\n"
    "                            }\n"
    "\n"
    "                            Local0 = ECRD (RefOf (ITSM))\n"
    "                            FNQS (Local0)\n"
    "                        }\n"
    "\n"
    "                        Local0 = ECRD (RefOf (ITSM))\n"
    "                        ^^^WMID.EVBU [Zero] = One\n"
    "                        ^^^WMID.EVBU [One] = 0x0F\n"
    "                        ^^^WMID.EVBU [0x02] = Local0\n"
    "                        Notify (WMID, 0x20) // Reserved\n"
    "                    }\n"
)

# ADP1._PSR replacement:
# Accurately reports AC connected if ECWR bit 0 is set OR adapter wattage > 50W.
# When a 280W adapter is plugged in, dual-load borrowing will never drop Windows to DC mode!
# Completely omits ^^PCI0.GP17.VGA.AFN4 (0x02) to prevent GPU 40W clock clamping.
ADP1_PSR_NEW_BODY = (
    "                        Method (_PSR, 0, NotSerialized)  // _PSR: Power Source\n"
    "                        {\n"
    "                            Local1 = (^^PCI0.LPC0.H_EC.ECRD (RefOf (^^PCI0.LPC0.H_EC.AWHG)) << 0x08)\n"
    "                            Local1 += ^^PCI0.LPC0.H_EC.ECRD (RefOf (^^PCI0.LPC0.H_EC.AWLW))\n"
    "                            If (((^^PCI0.LPC0.H_EC.ECRD (RefOf (^^PCI0.LPC0.H_EC.ECWR)) & One) || (Local1 > 0x32)))\n"
    "                            {\n"
    "                                Local0 = One\n"
    "                            }\n"
    "                            Else\n"
    "                            {\n"
    "                                Local0 = Zero\n"
    "                            }\n"
    "\n"
    "                            If (((Local0 != ACDC) || (ACDC == 0xFF)))\n"
    "                            {\n"
    "                                CreateWordField (XX00, Zero, SSZE)\n"
    "                                CreateByteField (XX00, 0x02, ACST)\n"
    "                                SSZE = 0x03\n"
    "                                ACDC = Local0\n"
    "                                If (ACDC)\n"
    "                                {\n"
    "                                    P80H = 0xECAC\n"
    "                                    ^^PCI0.GP17.VGA.AFN4 (One)\n"
    "                                    ACST = Zero\n"
    "                                }\n"
    "                                Else\n"
    "                                {\n"
    "                                    P80H = 0xECDC\n"
    "                                    ACST = One\n"
    "                                }\n"
    "\n"
    "                                ALIB (One, XX00)\n"
    "                            }\n"
    "\n"
    "                            Return (Local0)\n"
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
