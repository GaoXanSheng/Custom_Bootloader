/*
 * src/InplacePatches.h
 *
 * In-place byte patch patterns for the ORIGINAL firmware tables.
 *
 * ---------------------------------------------------------------------------
 * Why in-place (instead of / in addition to whole-table replacement):
 *
 * The power/thermal control logic (DSDT MSPL/MFPT/FNQS, SSDT4 NPCF power
 * walls) is executed by TWO classes of consumers:
 *
 *   - Windows ACPICA: reads tables fresh from RSDP/XSDT/FACP at boot, so it
 *     follows pointer redirection and sees the recompiled replacement tables
 *     (see ReplaceAcpiTables).
 *   - Firmware-side consumers (Insyde SMM / EC arbitration): bind themselves
 *     to the ORIGINAL DSDT/SSDT4 physical pages at POST and never consult
 *     RSDP again. Pointer redirection is invisible to them - only modifying
 *     the original pages byte-for-byte (and recomputing their checksums in
 *     place) changes what they execute.
 *
 * This was the empirical difference between the old approach (in-place byte
 * injection + checksum recompute: effective) and pointer redirection alone
 * (tables visible as new, but the control logic still ran from the old
 * pages).
 *
 * ---------------------------------------------------------------------------
 * Derivation (tools/acpi/dsdt.dat @ 0x587E / 0x5901 region, ssdt4.dat):
 *
 *   DSDT MSPL/MFPT - If ((DBFS == One)) { ...CPUT clamp chain (84-byte
 *   body)... } is replaced by If (Zero) keeping the original 2-byte PkgLen;
 *   the leftover dead branch chain stays inside the never-executed body.
 *   Opcodes: A0(If) 44 05(PkgLen=84) 93(LEqual) 44 42 46 53("DBFS") 01(One)
 *   -> A0 44 05 00(Zero) A3(NoOp) x5.
 *
 *   DSDT MSPL/MFPT - Store(0x2D, Local0) ("Local0 = 0x2D") etc.:
 *   70(Store) 0A(Byte) XX 60(Local0). Local0 is opcode 0x60, not 0x00.
 *
 *   SSDT4 NPCF - Name (CMPL, 0x33): 08(Name) "CMPL" 0A XX, and the
 *   0x0168 -> 0x01B8 assignments: 70 0B(Word) 68 01 <name>.
 *
 *   SSDT4 DTPP 0xF0 -> 0x0104 (edit grows 2->3 bytes, cannot be in-place at
 *   the Name site) is instead applied at its ONLY consumer
 *   "TPPD = DTPP" (Store(DTPP, TPPD) -> Store(0x0104, TPPD) + NoOp pad).
 *
 *   SSDT4 "CPUC = NCHP" (removal) is overwritten with 9 x NoOp, skipping
 *   the NVIO I/O write that re-clamps platform power.
 *
 * Edits that cannot be expressed as equal-size in-place patches at all:
 *   - dsdt fnqs_cpu_override (tail insertion)  -> replacement-only (Windows)
 *
 * ---------------------------------------------------------------------------
 * Occurrence counts (ExpectedCount) were verified against tools/acpi/*.dat
 * at derivation time. At RUNTIME the bootloader re-checks the count in the
 * table it is about to patch; on mismatch it logs an ALARM and SKIPS the
 * patch instead of corrupting the wrong location. If a BIOS update changes
 * the bytes, re-dump tools/acpi/*.dat and re-verify each pattern (e.g. with
 * a hex search) before bumping the counts here.
 * ---------------------------------------------------------------------------
 */

#ifndef INPLACE_PATCHES_H
#define INPLACE_PATCHES_H

#include "UefiHelpers.h"

/* Target table ids */
#define INPLACE_TARGET_DSDT  0   /* located via FACP -> X_DSDT/DSDT pointer  */
#define INPLACE_TARGET_SSDT4 1   /* located via XSDT scan (rev 0x1000+NPCF)  */

typedef struct {
    UINTN TargetTable;
    const UINT8 *Search;
    const UINT8 *Replace;
    UINTN SearchLen;
    UINTN ReplaceLen;       /* must equal SearchLen (in-place, size-preserving) */
    UINTN ExpectedCount;    /* verified at derivation; re-checked at runtime   */
    const CHAR16 *Name;
} INPLACE_PATCH;

static const UINT8 gInplaceSearch0[]  = { 0xA0, 0x44, 0x05, 0x93, 0x44, 0x42, 0x46, 0x53, 0x01 };
static const UINT8 gInplaceReplace0[] = { 0xA0, 0x44, 0x05, 0x00, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3 };
static const UINT8 gInplaceSearch1[]  = { 0x70, 0x0A, 0x2D, 0x60 };
static const UINT8 gInplaceReplace1[] = { 0x70, 0x0A, 0x3C, 0x60 };
static const UINT8 gInplaceSearch2[]  = { 0x70, 0x0A, 0x37, 0x60 };
static const UINT8 gInplaceReplace2[] = { 0x70, 0x0A, 0x3C, 0x60 };
static const UINT8 gInplaceSearch3[]  = { 0x70, 0x0A, 0x41, 0x60 };
static const UINT8 gInplaceReplace3[] = { 0x70, 0x0A, 0x50, 0x60 };
static const UINT8 gInplaceSearch4[]  = { 0x70, 0x0A, 0x4B, 0x60 };
static const UINT8 gInplaceReplace4[] = { 0x70, 0x0A, 0x50, 0x60 };

static const UINT8 gInplaceSearch5[]  = { 0x08, 0x43, 0x4D, 0x50, 0x4C, 0x0A, 0x33 };
static const UINT8 gInplaceReplace5[] = { 0x08, 0x43, 0x4D, 0x50, 0x4C, 0x0A, 0x50 };
static const UINT8 gInplaceSearch6[]  = { 0x08, 0x43, 0x4E, 0x50, 0x4C, 0x0A, 0x10 };
static const UINT8 gInplaceReplace6[] = { 0x08, 0x43, 0x4E, 0x50, 0x4C, 0x0A, 0x36 };
static const UINT8 gInplaceSearch7[]  = { 0x08, 0x41, 0x54, 0x50, 0x50, 0x0B, 0xB8, 0x01 };
static const UINT8 gInplaceReplace7[] = { 0x08, 0x41, 0x54, 0x50, 0x50, 0x0B, 0xE0, 0x01 };
static const UINT8 gInplaceSearch8[]  = { 0x70, 0x0B, 0x68, 0x01, 0x41, 0x54, 0x50, 0x50 };
static const UINT8 gInplaceReplace8[] = { 0x70, 0x0B, 0xB8, 0x01, 0x41, 0x54, 0x50, 0x50 };
static const UINT8 gInplaceSearch9[]  = { 0x70, 0x0B, 0x68, 0x01, 0x41, 0x54, 0x50, 0x32 };
static const UINT8 gInplaceReplace9[] = { 0x70, 0x0B, 0xB8, 0x01, 0x41, 0x54, 0x50, 0x32 };
static const UINT8 gInplaceSearch10[]  = { 0x70, 0x0B, 0x68, 0x01, 0x54, 0x50, 0x50, 0x41 };
static const UINT8 gInplaceReplace10[] = { 0x70, 0x0B, 0xB8, 0x01, 0x54, 0x50, 0x50, 0x41 };
static const UINT8 gInplaceSearch11[]  = { 0x70, 0x44, 0x54, 0x50, 0x50, 0x54, 0x50, 0x50, 0x44 };
static const UINT8 gInplaceReplace11[] = { 0x70, 0x0B, 0x04, 0x01, 0x54, 0x50, 0x50, 0x44, 0xA3 };
static const UINT8 gInplaceSearch12[]  = { 0x70, 0x4E, 0x43, 0x48, 0x50, 0x43, 0x50, 0x55, 0x43 };
static const UINT8 gInplaceReplace12[] = { 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3 };

static const INPLACE_PATCH gInplacePatches[] = {
    { INPLACE_TARGET_DSDT,  gInplaceSearch0,  gInplaceReplace0,  sizeof(gInplaceSearch0),  sizeof(gInplaceReplace0),  2, L"dsdt_dbfs_clamp_kill"     },
    { INPLACE_TARGET_DSDT,  gInplaceSearch1,  gInplaceReplace1,  sizeof(gInplaceSearch1),  sizeof(gInplaceReplace1),  2, L"dsdt_cspl_r7_60w"         },
    { INPLACE_TARGET_DSDT,  gInplaceSearch2,  gInplaceReplace2,  sizeof(gInplaceSearch2),  sizeof(gInplaceReplace2),  1, L"dsdt_cspl_r9_60w"         },
    { INPLACE_TARGET_DSDT,  gInplaceSearch3,  gInplaceReplace3,  sizeof(gInplaceSearch3),  sizeof(gInplaceReplace3),  2, L"dsdt_fppt_r7_80w"         },
    { INPLACE_TARGET_DSDT,  gInplaceSearch4,  gInplaceReplace4,  sizeof(gInplaceSearch4),  sizeof(gInplaceReplace4),  1, L"dsdt_fppt_r9_80w"         },
    { INPLACE_TARGET_SSDT4, gInplaceSearch5,  gInplaceReplace5,  sizeof(gInplaceSearch5),  sizeof(gInplaceReplace5),  1, L"ssdt4_cmpl_80w"            },
    { INPLACE_TARGET_SSDT4, gInplaceSearch6,  gInplaceReplace6,  sizeof(gInplaceSearch6),  sizeof(gInplaceReplace6),  1, L"ssdt4_cnpl_54w"            },
    { INPLACE_TARGET_SSDT4, gInplaceSearch7,  gInplaceReplace7,  sizeof(gInplaceSearch7),  sizeof(gInplaceReplace7),  1, L"ssdt4_atpp_name_240w"      },
    { INPLACE_TARGET_SSDT4, gInplaceSearch8,  gInplaceReplace8,  sizeof(gInplaceSearch8),  sizeof(gInplaceReplace8),  2, L"ssdt4_atpp_assign_220w"    },
    { INPLACE_TARGET_SSDT4, gInplaceSearch9,  gInplaceReplace9,  sizeof(gInplaceSearch9),  sizeof(gInplaceReplace9),  1, L"ssdt4_atp2_assign_220w"    },
    { INPLACE_TARGET_SSDT4, gInplaceSearch10, gInplaceReplace10, sizeof(gInplaceSearch10), sizeof(gInplaceReplace10), 2, L"ssdt4_tppa_assign_220w"    },
    { INPLACE_TARGET_SSDT4, gInplaceSearch11, gInplaceReplace11, sizeof(gInplaceSearch11), sizeof(gInplaceReplace11), 1, L"ssdt4_dtpp_via_tppd_260w"  },
    { INPLACE_TARGET_SSDT4, gInplaceSearch12, gInplaceReplace12, sizeof(gInplaceSearch12), sizeof(gInplaceReplace12), 1, L"ssdt4_cpuc_nchp_skip"      },
};

static const UINTN gInplacePatchCount =
    sizeof(gInplacePatches) / sizeof(gInplacePatches[0]);

#endif /* INPLACE_PATCHES_H */
