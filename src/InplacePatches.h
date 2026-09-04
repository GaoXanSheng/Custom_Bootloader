/*
 * src/InplacePatches.h
 *
 * Size-preserving byte patch patterns for fallback in-place patching
 * on original DSDT and SSDT4 tables.
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

// NOTE: gInplaceSearch1..4 (Store 0x2D/0x37/0x41/0x4B -> 0x3C/0x50, the old
// "cspl/fppt 60w/80w" raises) were REMOVED on 2026-09-05: their targets live
// inside the MSPL/MFPT DBFS==1 clamp bodies, which patch 0 turns into
// If(Zero) dead code, so they never executed. (Same cleanup as the ASL-side
// dead edits in tools/generate_patched_tables.py.)

static const UINT8 gInplaceSearch0[]  = { 0xA0, 0x44, 0x05, 0x93, 0x44, 0x42, 0x46, 0x53, 0x01 };
static const UINT8 gInplaceReplace0[] = { 0xA0, 0x44, 0x05, 0x00, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3 };
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
