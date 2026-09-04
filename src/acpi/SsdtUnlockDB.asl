DefinitionBlock ("", "SSDT", 2, "CUSTOM", "DBUNLOCK", 0x00002000)
{
    // -------------------------------------------------------------
    // External Symbols Declaration (only what the remaining 15 methods need)
    // -------------------------------------------------------------
    External (\DBFS, IntObj)
    External (\_SB.PCI0.LPC0, DeviceObj)
    External (\_SB.PCI0.LPC0.H_EC, DeviceObj)
    External (\_SB.PCI0.LPC0.H_EC.COMM, MethodObj)
    External (\_SB.PCI0.LPC0.H_EC.ITSM, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.DGPU, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.CPUT, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.STSM, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.CSPL, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.FPPT, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.CTCL, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.TFLG, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.GFLG, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.GPMD, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.BPWM, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.MSPL, MethodObj)
    External (\_SB.PCI0.LPC0.H_EC.MFPT, MethodObj)
    External (\_SB.PCI0.LPC0.H_EC.FNQS, MethodObj)
    External (\_SB.PCI0.LPC0.H_EC.BBHL, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.ECRD, MethodObj)
    External (\_SB.PCI0.LPC0.H_EC.ECWT, MethodObj)
    External (\_SB.NPCF, DeviceObj)
    External (\_SB.NPCF.ACBT, IntObj)
    External (\_SB.NPCF.DCBT, IntObj)
    External (\_SB.NPCF.AMAT, IntObj)
    External (\_SB.NPCF.AMIT, IntObj)
    External (\_SB.ALIB, MethodObj)

    Scope (\_SB.PCI0.LPC0)
    {
        Device (DBUL)
        {
            Name (_HID, EisaId ("PNP0C14"))
            Name (_UID, 0xAA)

            Include ("VersionData.asl")

            // WMI Override Persistence Lock Mask & SMI status
            Name (WMOV, Zero)   // Bitmask: Bit0=MAGA/TGPA Lock, Bit1=ATPP/ATP2 Lock, Bit2=CSPL/FPPT Lock
            Name (WSMI, Zero)   // Last SMI status

        OperationRegion (ECMR, EmbeddedControl, 0, 0x100)
        Field (ECMR, ByteAcc, NoLock, Preserve)
        {
            Offset (0x00), EC00, 8,
            Offset (0x01), EC01, 8,
            Offset (0x02), EC02, 8,
            Offset (0x03), EC03, 8,
            Offset (0x04), EC04, 8,
            Offset (0x05), EC05, 8,
            Offset (0x06), EC06, 8,
            Offset (0x07), EC07, 8,
            Offset (0x08), EC08, 8,
            Offset (0x09), EC09, 8,
            Offset (0x0A), EC0A, 8,
            Offset (0x0B), EC0B, 8,
            Offset (0x0C), EC0C, 8,
            Offset (0x0D), EC0D, 8,
            Offset (0x0E), EC0E, 8,
            Offset (0x0F), EC0F, 8,
            Offset (0x10), EC10, 8,
            Offset (0x11), EC11, 8,
            Offset (0x12), EC12, 8,
            Offset (0x13), EC13, 8,
            Offset (0x14), EC14, 8,
            Offset (0x15), EC15, 8,
            Offset (0x16), EC16, 8,
            Offset (0x17), EC17, 8,
            Offset (0x18), EC18, 8,
            Offset (0x19), EC19, 8,
            Offset (0x1A), EC1A, 8,
            Offset (0x1B), EC1B, 8,
            Offset (0x1C), EC1C, 8,
            Offset (0x1D), EC1D, 8,
            Offset (0x1E), EC1E, 8,
            Offset (0x1F), EC1F, 8,
            Offset (0x20), EC20, 8,
            Offset (0x21), EC21, 8,
            Offset (0x22), EC22, 8,
            Offset (0x23), EC23, 8,
            Offset (0x24), EC24, 8,
            Offset (0x25), EC25, 8,
            Offset (0x26), EC26, 8,
            Offset (0x27), EC27, 8,
            Offset (0x28), EC28, 8,
            Offset (0x29), EC29, 8,
            Offset (0x2A), EC2A, 8,
            Offset (0x2B), EC2B, 8,
            Offset (0x2C), EC2C, 8,
            Offset (0x2D), EC2D, 8,
            Offset (0x2E), EC2E, 8,
            Offset (0x2F), EC2F, 8,
            Offset (0x30), EC30, 8,
            Offset (0x31), EC31, 8,
            Offset (0x32), EC32, 8,
            Offset (0x33), EC33, 8,
            Offset (0x34), EC34, 8,
            Offset (0x35), EC35, 8,
            Offset (0x36), EC36, 8,
            Offset (0x37), EC37, 8,
            Offset (0x38), EC38, 8,
            Offset (0x39), EC39, 8,
            Offset (0x3A), EC3A, 8,
            Offset (0x3B), EC3B, 8,
            Offset (0x3C), EC3C, 8,
            Offset (0x3D), EC3D, 8,
            Offset (0x3E), EC3E, 8,
            Offset (0x3F), EC3F, 8,
            Offset (0x40), EC40, 8,
            Offset (0x41), EC41, 8,
            Offset (0x42), EC42, 8,
            Offset (0x43), EC43, 8,
            Offset (0x44), EC44, 8,
            Offset (0x45), EC45, 8,
            Offset (0x46), EC46, 8,
            Offset (0x47), EC47, 8,
            Offset (0x48), EC48, 8,
            Offset (0x49), EC49, 8,
            Offset (0x4A), EC4A, 8,
            Offset (0x4B), EC4B, 8,
            Offset (0x4C), EC4C, 8,
            Offset (0x4D), EC4D, 8,
            Offset (0x4E), EC4E, 8,
            Offset (0x4F), EC4F, 8,
            Offset (0x50), EC50, 8,
            Offset (0x51), EC51, 8,
            Offset (0x52), EC52, 8,
            Offset (0x53), EC53, 8,
            Offset (0x54), EC54, 8,
            Offset (0x55), EC55, 8,
            Offset (0x56), EC56, 8,
            Offset (0x57), EC57, 8,
            Offset (0x58), EC58, 8,
            Offset (0x59), EC59, 8,
            Offset (0x5A), EC5A, 8,
            Offset (0x5B), EC5B, 8,
            Offset (0x5C), EC5C, 8,
            Offset (0x5D), EC5D, 8,
            Offset (0x5E), EC5E, 8,
            Offset (0x5F), EC5F, 8,
            Offset (0x60), EC60, 8,
            Offset (0x61), EC61, 8,
            Offset (0x62), EC62, 8,
            Offset (0x63), EC63, 8,
            Offset (0x64), EC64, 8,
            Offset (0x65), EC65, 8,
            Offset (0x66), EC66, 8,
            Offset (0x67), EC67, 8,
            Offset (0x68), EC68, 8,
            Offset (0x69), EC69, 8,
            Offset (0x6A), EC6A, 8,
            Offset (0x6B), EC6B, 8,
            Offset (0x6C), EC6C, 8,
            Offset (0x6D), EC6D, 8,
            Offset (0x6E), EC6E, 8,
            Offset (0x6F), EC6F, 8,
            Offset (0x70), EC70, 8,
            Offset (0x71), EC71, 8,
            Offset (0x72), EC72, 8,
            Offset (0x73), EC73, 8,
            Offset (0x74), EC74, 8,
            Offset (0x75), EC75, 8,
            Offset (0x76), EC76, 8,
            Offset (0x77), EC77, 8,
            Offset (0x78), EC78, 8,
            Offset (0x79), EC79, 8,
            Offset (0x7A), EC7A, 8,
            Offset (0x7B), EC7B, 8,
            Offset (0x7C), EC7C, 8,
            Offset (0x7D), EC7D, 8,
            Offset (0x7E), EC7E, 8,
            Offset (0x7F), EC7F, 8,
            Offset (0x80), EC80, 8,
            Offset (0x81), EC81, 8,
            Offset (0x82), EC82, 8,
            Offset (0x83), EC83, 8,
            Offset (0x84), EC84, 8,
            Offset (0x85), EC85, 8,
            Offset (0x86), EC86, 8,
            Offset (0x87), EC87, 8,
            Offset (0x88), EC88, 8,
            Offset (0x89), EC89, 8,
            Offset (0x8A), EC8A, 8,
            Offset (0x8B), EC8B, 8,
            Offset (0x8C), EC8C, 8,
            Offset (0x8D), EC8D, 8,
            Offset (0x8E), EC8E, 8,
            Offset (0x8F), EC8F, 8,
            Offset (0x90), EC90, 8,
            Offset (0x91), EC91, 8,
            Offset (0x92), EC92, 8,
            Offset (0x93), EC93, 8,
            Offset (0x94), EC94, 8,
            Offset (0x95), EC95, 8,
            Offset (0x96), EC96, 8,
            Offset (0x97), EC97, 8,
            Offset (0x98), EC98, 8,
            Offset (0x99), EC99, 8,
            Offset (0x9A), EC9A, 8,
            Offset (0x9B), EC9B, 8,
            Offset (0x9C), EC9C, 8,
            Offset (0x9D), EC9D, 8,
            Offset (0x9E), EC9E, 8,
            Offset (0x9F), EC9F, 8,
            Offset (0xA0), ECA0, 8,
            Offset (0xA1), ECA1, 8,
            Offset (0xA2), ECA2, 8,
            Offset (0xA3), ECA3, 8,
            Offset (0xA4), ECA4, 8,
            Offset (0xA5), ECA5, 8,
            Offset (0xA6), ECA6, 8,
            Offset (0xA7), ECA7, 8,
            Offset (0xA8), ECA8, 8,
            Offset (0xA9), ECA9, 8,
            Offset (0xAA), ECAA, 8,
            Offset (0xAB), ECAB, 8,
            Offset (0xAC), ECAC, 8,
            Offset (0xAD), ECAD, 8,
            Offset (0xAE), ECAE, 8,
            Offset (0xAF), ECAF, 8,
            Offset (0xB0), ECB0, 8,
            Offset (0xB1), ECB1, 8,
            Offset (0xB2), ECB2, 8,
            Offset (0xB3), ECB3, 8,
            Offset (0xB4), ECB4, 8,
            Offset (0xB5), ECB5, 8,
            Offset (0xB6), ECB6, 8,
            Offset (0xB7), ECB7, 8,
            Offset (0xB8), ECB8, 8,
            Offset (0xB9), ECB9, 8,
            Offset (0xBA), ECBA, 8,
            Offset (0xBB), ECBB, 8,
            Offset (0xBC), ECBC, 8,
            Offset (0xBD), ECBD, 8,
            Offset (0xBE), ECBE, 8,
            Offset (0xBF), ECBF, 8,
            Offset (0xC0), ECC0, 8,
            Offset (0xC1), ECC1, 8,
            Offset (0xC2), ECC2, 8,
            Offset (0xC3), ECC3, 8,
            Offset (0xC4), ECC4, 8,
            Offset (0xC5), ECC5, 8,
            Offset (0xC6), ECC6, 8,
            Offset (0xC7), ECC7, 8,
            Offset (0xC8), ECC8, 8,
            Offset (0xC9), ECC9, 8,
            Offset (0xCA), ECCA, 8,
            Offset (0xCB), ECCB, 8,
            Offset (0xCC), ECCC, 8,
            Offset (0xCD), ECCD, 8,
            Offset (0xCE), ECCE, 8,
            Offset (0xCF), ECCF, 8,
            Offset (0xD0), ECD0, 8,
            Offset (0xD1), ECD1, 8,
            Offset (0xD2), ECD2, 8,
            Offset (0xD3), ECD3, 8,
            Offset (0xD4), ECD4, 8,
            Offset (0xD5), ECD5, 8,
            Offset (0xD6), ECD6, 8,
            Offset (0xD7), ECD7, 8,
            Offset (0xD8), ECD8, 8,
            Offset (0xD9), ECD9, 8,
            Offset (0xDA), ECDA, 8,
            Offset (0xDB), ECDB, 8,
            Offset (0xDC), ECDC, 8,
            Offset (0xDD), ECDD, 8,
            Offset (0xDE), ECDE, 8,
            Offset (0xDF), ECDF, 8,
            Offset (0xE0), ECE0, 8,
            Offset (0xE1), ECE1, 8,
            Offset (0xE2), ECE2, 8,
            Offset (0xE3), ECE3, 8,
            Offset (0xE4), ECE4, 8,
            Offset (0xE5), ECE5, 8,
            Offset (0xE6), ECE6, 8,
            Offset (0xE7), ECE7, 8,
            Offset (0xE8), ECE8, 8,
            Offset (0xE9), ECE9, 8,
            Offset (0xEA), ECEA, 8,
            Offset (0xEB), ECEB, 8,
            Offset (0xEC), ECEC, 8,
            Offset (0xED), ECED, 8,
            Offset (0xEE), ECEE, 8,
            Offset (0xEF), ECEF, 8,
            Offset (0xF0), ECF0, 8,
            Offset (0xF1), ECF1, 8,
            Offset (0xF2), ECF2, 8,
            Offset (0xF3), ECF3, 8,
            Offset (0xF4), ECF4, 8,
            Offset (0xF5), ECF5, 8,
            Offset (0xF6), ECF6, 8,
            Offset (0xF7), ECF7, 8,
            Offset (0xF8), ECF8, 8,
            Offset (0xF9), ECF9, 8,
            Offset (0xFA), ECFA, 8,
            Offset (0xFB), ECFB, 8,
            Offset (0xFC), ECFC, 8,
            Offset (0xFD), ECFD, 8,
            Offset (0xFE), ECFE, 8,
            Offset (0xFF), ECFF, 8,
        }

            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }

            Name (_WDG, Buffer (0x28)
            {
                0x62, 0x1B, 0x0C, 0x85,
                0xD9, 0xA3, 0x65, 0x4E,
                0xB8, 0xD5, 0xDE, 0x3C,
                0x74, 0xC9, 0xB3, 0x8D,
                0x54, 0x46, // "TF" (WMTF Method)
                0x01,
                0x02,

                0x21, 0x12, 0x90, 0x05,
                0x66, 0xD5, 0xD1, 0x11,
                0xB2, 0xF0, 0x00, 0xA0,
                0xC9, 0x06, 0x29, 0x10,
                0x42, 0x41, // "BA" (Binary MOF Data WQBA)
                0x01,
                0x00
            })

            Include ("BmfData.asl")

            Method (_INI, 0, NotSerialized)
            {
                // GPU 功耗模式开启: GFLG=0x55, GPMD=1 解除独显功耗限制
                If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                {
                    \_SB.PCI0.LPC0.H_EC.ECWT (0x55, RefOf (\_SB.PCI0.LPC0.H_EC.GFLG))
                    \_SB.PCI0.LPC0.H_EC.ECWT (One, RefOf (\_SB.PCI0.LPC0.H_EC.GPMD))
                }

                // DCBT (电池取电门限) 提升至 80W (0x50)，彻底解除 40W 锁
                If (CondRefOf (\_SB.NPCF.DCBT))
                {
                    Store (0x50, \_SB.NPCF.DCBT)
                }

                // CPU PL1/PL2 初始化配置 (CSPL=85W, FPPT=110W) 并推送 SMU
                // 85W CPU + 140W GPU + 30W 外围 = 255W，完美匹配 280W 适配器，防止整机峰值拉爆导致过流借电
                If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                {
                    \_SB.PCI0.LPC0.H_EC.ECWT (0x55, RefOf (\_SB.PCI0.LPC0.H_EC.CSPL))
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.MSPL))
                    {
                        \_SB.PCI0.LPC0.H_EC.MSPL ()
                    }

                    \_SB.PCI0.LPC0.H_EC.ECWT (0x6E, RefOf (\_SB.PCI0.LPC0.H_EC.FPPT))
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.MFPT))
                    {
                        \_SB.PCI0.LPC0.H_EC.MFPT ()
                    }
                }
            }

            // -------------------------------------------------------------
            // Master WMI Method Dispatcher (WMTF) - 16 Methods
            // -------------------------------------------------------------
            Method (WMTF, 3, Serialized)
            {
                // Arg0: Instance Index
                // Arg1: Method ID (1 ~ 15)
                // Arg2: Input Buffer

                If (LEqual (Arg0, Zero)) {}

                // Dispatch Helper Buffers
                Name (R1DW, Buffer (4) {0,0,0,0})
                CreateDWordField (R1DW, 0, V1D1)

                Name (R4DW, Buffer (16) {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0})
                CreateDWordField (R4DW, 0, V4D1)
                CreateDWordField (R4DW, 4, V4D2)
                CreateDWordField (R4DW, 8, V4D3)
                CreateDWordField (R4DW, 12, V4D4)

                // Input Field Extractors
                If (LGreaterEqual (SizeOf (Arg2), 4))
                {
                    CreateDWordField (Arg2, 0, INP0)
                }
                If (LGreaterEqual (SizeOf (Arg2), 8))
                {
                    CreateDWordField (Arg2, 4, INP1)
                }
                If (LGreaterEqual (SizeOf (Arg2), 12))
                {
                    CreateDWordField (Arg2, 8, INP2)
                }
                If (LGreaterEqual (SizeOf (Arg2), 16))
                {
                    CreateDWordField (Arg2, 12, INP3)
                }

                // =========================================================
                // Method 1: SetDbfs
                // =========================================================
                If (LEqual (Arg1, 1))
                {
                    // Kept for parity with the firmware's own DBFS flow
                    // (_Q81/_Q82 style: DBFS = X; FNQS (ITSM); COMM ()).
                    // NOTE: since the power-wall unlock, the patched FNQS no
                    // longer reads DBFS (AC always dispatches the full-power
                    // profile) and the MSPL/MFPT clamps are disabled, so this
                    // toggle only affects firmware-side consumers of the DBFS
                    // EC field - it can no longer lower the CPU profile.
                    If (CondRefOf (\DBFS)) { Store (INP0, \DBFS) }
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.FNQS))
                    {
                        \_SB.PCI0.LPC0.H_EC.FNQS (\_SB.PCI0.LPC0.H_EC.ITSM)
                    }
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM)) { \_SB.PCI0.LPC0.H_EC.COMM () }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 2: GetDbfs
                // =========================================================
                If (LEqual (Arg1, 2))
                {
                    If (CondRefOf (\DBFS)) { Store (\DBFS, V1D1) }
                    Else { Store (0xFFFFFFFF, V1D1) }
                    Return (R1DW)
                }

                // =========================================================
                // Method 3: GetVersion
                //   VERS (VersionData.asl) = build time as UTC Unix epoch
                //   seconds (32-bit DWORD cannot hold BCD YYYYMMDDHHMMSS).
                //   DbgTool decodes it to local "yyyy-MM-dd HH:mm:ss".
                // =========================================================
                If (LEqual (Arg1, 3))
                {
                    Store (VERS, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 4: SetBatteryChargeThreshold
                // =========================================================
                If (LEqual (Arg1, 4))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.BBHL)) { Store (INP0, \_SB.PCI0.LPC0.H_EC.BBHL) }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 5: GetBatteryChargeThreshold
                // =========================================================
                If (LEqual (Arg1, 5))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.BBHL)) { Store (\_SB.PCI0.LPC0.H_EC.BBHL, V1D1) }
                    Else { Store (100, V1D1) }
                    Return (R1DW)
                }

                // =========================================================
                // Method 6: SetThresholds (ACBT/DCBT/AMAT/AMIT)
                // =========================================================
                If (LEqual (Arg1, 6))
                {
                    If (CondRefOf (\_SB.NPCF.ACBT)) { Store (INP0, \_SB.NPCF.ACBT) }
                    If (CondRefOf (\_SB.NPCF.DCBT)) { Store (INP1, \_SB.NPCF.DCBT) }
                    If (CondRefOf (\_SB.NPCF.AMAT)) { Store (INP2, \_SB.NPCF.AMAT) }
                    If (CondRefOf (\_SB.NPCF.AMIT)) { Store (INP3, \_SB.NPCF.AMIT) }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 7: GetThresholds
                // =========================================================
                If (LEqual (Arg1, 7))
                {
                    If (CondRefOf (\_SB.NPCF.ACBT)) { Store (\_SB.NPCF.ACBT, V4D1) }
                    If (CondRefOf (\_SB.NPCF.DCBT)) { Store (\_SB.NPCF.DCBT, V4D2) }
                    If (CondRefOf (\_SB.NPCF.AMAT)) { Store (\_SB.NPCF.AMAT, V4D3) }
                    If (CondRefOf (\_SB.NPCF.AMIT)) { Store (\_SB.NPCF.AMIT, V4D4) }
                    Return (R4DW)
                }

                // =========================================================
                // Method 8: SetEcRegister (EC Write, known fields only)
                // ECRD/ECWT take RefOf(field), not numeric offsets —
                // dispatch INP0 (offset) to the matching EC field.
                // =========================================================
                If (LEqual (Arg1, 8))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                    {
                        If (LEqual (INP0, 0xD0))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.TFLG))
                        }
                        ElseIf (LEqual (INP0, 0xD1))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.GFLG))
                        }
                        ElseIf (LEqual (INP0, 0xD2))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.GPMD))
                        }
                        ElseIf (LEqual (INP0, 0xE3))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.BPWM))
                        }
                        ElseIf (LEqual (INP0, 0xE4))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.ITSM))
                        }
                        ElseIf (LEqual (INP0, 0xF2))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.DGPU))
                        }
                        ElseIf (LEqual (INP0, 0xF3))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.CPUT))
                        }
                        ElseIf (LEqual (INP0, 0xF4))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.STSM))
                        }
                        ElseIf (LEqual (INP0, 0xF6))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.CSPL))
                        }
                        ElseIf (LEqual (INP0, 0xF7))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.FPPT))
                        }
                        ElseIf (LEqual (INP0, 0xF8))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.CTCL))
                        }
                    }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 9: GetEcRegister (EC Read, known fields only)
                // =========================================================
                If (LEqual (Arg1, 9))
                {
                    Store (0xFF, V1D1)
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECRD))
                    {
                        If (LEqual (INP0, 0xD0))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.TFLG)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xD1))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.GFLG)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xD2))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.GPMD)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xE3))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.BPWM)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xE4))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.ITSM)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xF2))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.DGPU)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xF3))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CPUT)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xF4))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.STSM)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xF6))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CSPL)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xF7))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.FPPT)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xF8))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CTCL)), V1D1)
                        }
                    }
                    Return (R1DW)
                }

                // =========================================================
                // Method 10: SetOverrideLock
                // =========================================================
                If (LEqual (Arg1, 10))
                {
                    Store (INP0, WMOV)
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 11: GetOverrideLock
                // =========================================================
                If (LEqual (Arg1, 11))
                {
                    Store (WMOV, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 12: SetSwSmi
                // =========================================================
                If (LEqual (Arg1, 12))
                {
                    Store (INP0, WSMI)
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 13: GetSwSmiStatus
                // =========================================================
                If (LEqual (Arg1, 13))
                {
                    Store (WSMI, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 14: SetAmdAlibDirect
                // =========================================================
                If (LEqual (Arg1, 14))
                {
                    If (CondRefOf (\_SB.ALIB))
                    {
                        Name (ALBF, Buffer (8) {0,0,0,0, 0,0,0,0})
                        CreateByteField (ALBF, 0, ASUB)
                        CreateDWordField (ALBF, 1, ADAT)
                        Store (INP0, ASUB)
                        Store (INP1, ADAT)
                        \_SB.ALIB (0x0C, ALBF)
                    }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 15: GetAmdAlibDirect
                // =========================================================
                If (LEqual (Arg1, 15))
                {
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 16: GetDynamicBoost
                // =========================================================
                // 结合本补丁（DBFS 保持原厂值不做强制 + MSPL/MFPT 钳制块 If(Zero)
                // 禁用 + FNQS 整方法替换为 AC 满血档）返回数据供工具计算：
                //   V4D1 = DBFS 当前值（0=禁 / 1=启 / 0xFFFFFFFF=不存在）
                //   V4D2 = CSPL（EC 0xF6，CPU PL1，单位 W）
                //   V4D3 = FPPT（EC 0xF7，CPU PL2，单位 W）
                //   V4D4 = 原厂固件 DBFS=1 时的 PL2 钳制上限（CPUT 0x09→75W，其余→65W）
                // 最大借用功耗（原厂机制）= V4D3 − V4D4；
                // 本补丁禁用钳制后 CPU 不再让电，借用恒为 0（V4D4 仅作参考）。
                If (LEqual (Arg1, 16))
                {
                    If (CondRefOf (\DBFS)) { Store (\DBFS, V4D1) }
                    Else { Store (0xFFFFFFFF, V4D1) }
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECRD))
                    {
                        Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CSPL)), V4D2) // CSPL/PL1
                        Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.FPPT)), V4D3) // FPPT/PL2
                        If (LEqual (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CPUT)), 0x09))
                        {
                            Store (0x4B, V4D4) // CPUT 0x09 → 75W
                        }
                        Else
                        {
                            Store (0x41, V4D4) // CPUT 0x07/0x05 → 65W
                        }
                    }
                    Else
                    {
                        Store (0xFF, V4D2)
                        Store (0xFF, V4D3)
                        Store (0x41, V4D4)
                    }
                    Return (R4DW)
                }

                // =========================================================
                // Method 17: GetEcByte (full EC RAM read, 0-255)
                // =========================================================
                If (LEqual (Arg1, 17))
                {
                    Store (0xFF, V1D1)
                        If (LEqual (INP0, 0x00)) { Store (EC00, V1D1) }
                        ElseIf (LEqual (INP0, 0x01)) { Store (EC01, V1D1) }
                        ElseIf (LEqual (INP0, 0x02)) { Store (EC02, V1D1) }
                        ElseIf (LEqual (INP0, 0x03)) { Store (EC03, V1D1) }
                        ElseIf (LEqual (INP0, 0x04)) { Store (EC04, V1D1) }
                        ElseIf (LEqual (INP0, 0x05)) { Store (EC05, V1D1) }
                        ElseIf (LEqual (INP0, 0x06)) { Store (EC06, V1D1) }
                        ElseIf (LEqual (INP0, 0x07)) { Store (EC07, V1D1) }
                        ElseIf (LEqual (INP0, 0x08)) { Store (EC08, V1D1) }
                        ElseIf (LEqual (INP0, 0x09)) { Store (EC09, V1D1) }
                        ElseIf (LEqual (INP0, 0x0A)) { Store (EC0A, V1D1) }
                        ElseIf (LEqual (INP0, 0x0B)) { Store (EC0B, V1D1) }
                        ElseIf (LEqual (INP0, 0x0C)) { Store (EC0C, V1D1) }
                        ElseIf (LEqual (INP0, 0x0D)) { Store (EC0D, V1D1) }
                        ElseIf (LEqual (INP0, 0x0E)) { Store (EC0E, V1D1) }
                        ElseIf (LEqual (INP0, 0x0F)) { Store (EC0F, V1D1) }
                        ElseIf (LEqual (INP0, 0x10)) { Store (EC10, V1D1) }
                        ElseIf (LEqual (INP0, 0x11)) { Store (EC11, V1D1) }
                        ElseIf (LEqual (INP0, 0x12)) { Store (EC12, V1D1) }
                        ElseIf (LEqual (INP0, 0x13)) { Store (EC13, V1D1) }
                        ElseIf (LEqual (INP0, 0x14)) { Store (EC14, V1D1) }
                        ElseIf (LEqual (INP0, 0x15)) { Store (EC15, V1D1) }
                        ElseIf (LEqual (INP0, 0x16)) { Store (EC16, V1D1) }
                        ElseIf (LEqual (INP0, 0x17)) { Store (EC17, V1D1) }
                        ElseIf (LEqual (INP0, 0x18)) { Store (EC18, V1D1) }
                        ElseIf (LEqual (INP0, 0x19)) { Store (EC19, V1D1) }
                        ElseIf (LEqual (INP0, 0x1A)) { Store (EC1A, V1D1) }
                        ElseIf (LEqual (INP0, 0x1B)) { Store (EC1B, V1D1) }
                        ElseIf (LEqual (INP0, 0x1C)) { Store (EC1C, V1D1) }
                        ElseIf (LEqual (INP0, 0x1D)) { Store (EC1D, V1D1) }
                        ElseIf (LEqual (INP0, 0x1E)) { Store (EC1E, V1D1) }
                        ElseIf (LEqual (INP0, 0x1F)) { Store (EC1F, V1D1) }
                        ElseIf (LEqual (INP0, 0x20)) { Store (EC20, V1D1) }
                        ElseIf (LEqual (INP0, 0x21)) { Store (EC21, V1D1) }
                        ElseIf (LEqual (INP0, 0x22)) { Store (EC22, V1D1) }
                        ElseIf (LEqual (INP0, 0x23)) { Store (EC23, V1D1) }
                        ElseIf (LEqual (INP0, 0x24)) { Store (EC24, V1D1) }
                        ElseIf (LEqual (INP0, 0x25)) { Store (EC25, V1D1) }
                        ElseIf (LEqual (INP0, 0x26)) { Store (EC26, V1D1) }
                        ElseIf (LEqual (INP0, 0x27)) { Store (EC27, V1D1) }
                        ElseIf (LEqual (INP0, 0x28)) { Store (EC28, V1D1) }
                        ElseIf (LEqual (INP0, 0x29)) { Store (EC29, V1D1) }
                        ElseIf (LEqual (INP0, 0x2A)) { Store (EC2A, V1D1) }
                        ElseIf (LEqual (INP0, 0x2B)) { Store (EC2B, V1D1) }
                        ElseIf (LEqual (INP0, 0x2C)) { Store (EC2C, V1D1) }
                        ElseIf (LEqual (INP0, 0x2D)) { Store (EC2D, V1D1) }
                        ElseIf (LEqual (INP0, 0x2E)) { Store (EC2E, V1D1) }
                        ElseIf (LEqual (INP0, 0x2F)) { Store (EC2F, V1D1) }
                        ElseIf (LEqual (INP0, 0x30)) { Store (EC30, V1D1) }
                        ElseIf (LEqual (INP0, 0x31)) { Store (EC31, V1D1) }
                        ElseIf (LEqual (INP0, 0x32)) { Store (EC32, V1D1) }
                        ElseIf (LEqual (INP0, 0x33)) { Store (EC33, V1D1) }
                        ElseIf (LEqual (INP0, 0x34)) { Store (EC34, V1D1) }
                        ElseIf (LEqual (INP0, 0x35)) { Store (EC35, V1D1) }
                        ElseIf (LEqual (INP0, 0x36)) { Store (EC36, V1D1) }
                        ElseIf (LEqual (INP0, 0x37)) { Store (EC37, V1D1) }
                        ElseIf (LEqual (INP0, 0x38)) { Store (EC38, V1D1) }
                        ElseIf (LEqual (INP0, 0x39)) { Store (EC39, V1D1) }
                        ElseIf (LEqual (INP0, 0x3A)) { Store (EC3A, V1D1) }
                        ElseIf (LEqual (INP0, 0x3B)) { Store (EC3B, V1D1) }
                        ElseIf (LEqual (INP0, 0x3C)) { Store (EC3C, V1D1) }
                        ElseIf (LEqual (INP0, 0x3D)) { Store (EC3D, V1D1) }
                        ElseIf (LEqual (INP0, 0x3E)) { Store (EC3E, V1D1) }
                        ElseIf (LEqual (INP0, 0x3F)) { Store (EC3F, V1D1) }
                        ElseIf (LEqual (INP0, 0x40)) { Store (EC40, V1D1) }
                        ElseIf (LEqual (INP0, 0x41)) { Store (EC41, V1D1) }
                        ElseIf (LEqual (INP0, 0x42)) { Store (EC42, V1D1) }
                        ElseIf (LEqual (INP0, 0x43)) { Store (EC43, V1D1) }
                        ElseIf (LEqual (INP0, 0x44)) { Store (EC44, V1D1) }
                        ElseIf (LEqual (INP0, 0x45)) { Store (EC45, V1D1) }
                        ElseIf (LEqual (INP0, 0x46)) { Store (EC46, V1D1) }
                        ElseIf (LEqual (INP0, 0x47)) { Store (EC47, V1D1) }
                        ElseIf (LEqual (INP0, 0x48)) { Store (EC48, V1D1) }
                        ElseIf (LEqual (INP0, 0x49)) { Store (EC49, V1D1) }
                        ElseIf (LEqual (INP0, 0x4A)) { Store (EC4A, V1D1) }
                        ElseIf (LEqual (INP0, 0x4B)) { Store (EC4B, V1D1) }
                        ElseIf (LEqual (INP0, 0x4C)) { Store (EC4C, V1D1) }
                        ElseIf (LEqual (INP0, 0x4D)) { Store (EC4D, V1D1) }
                        ElseIf (LEqual (INP0, 0x4E)) { Store (EC4E, V1D1) }
                        ElseIf (LEqual (INP0, 0x4F)) { Store (EC4F, V1D1) }
                        ElseIf (LEqual (INP0, 0x50)) { Store (EC50, V1D1) }
                        ElseIf (LEqual (INP0, 0x51)) { Store (EC51, V1D1) }
                        ElseIf (LEqual (INP0, 0x52)) { Store (EC52, V1D1) }
                        ElseIf (LEqual (INP0, 0x53)) { Store (EC53, V1D1) }
                        ElseIf (LEqual (INP0, 0x54)) { Store (EC54, V1D1) }
                        ElseIf (LEqual (INP0, 0x55)) { Store (EC55, V1D1) }
                        ElseIf (LEqual (INP0, 0x56)) { Store (EC56, V1D1) }
                        ElseIf (LEqual (INP0, 0x57)) { Store (EC57, V1D1) }
                        ElseIf (LEqual (INP0, 0x58)) { Store (EC58, V1D1) }
                        ElseIf (LEqual (INP0, 0x59)) { Store (EC59, V1D1) }
                        ElseIf (LEqual (INP0, 0x5A)) { Store (EC5A, V1D1) }
                        ElseIf (LEqual (INP0, 0x5B)) { Store (EC5B, V1D1) }
                        ElseIf (LEqual (INP0, 0x5C)) { Store (EC5C, V1D1) }
                        ElseIf (LEqual (INP0, 0x5D)) { Store (EC5D, V1D1) }
                        ElseIf (LEqual (INP0, 0x5E)) { Store (EC5E, V1D1) }
                        ElseIf (LEqual (INP0, 0x5F)) { Store (EC5F, V1D1) }
                        ElseIf (LEqual (INP0, 0x60)) { Store (EC60, V1D1) }
                        ElseIf (LEqual (INP0, 0x61)) { Store (EC61, V1D1) }
                        ElseIf (LEqual (INP0, 0x62)) { Store (EC62, V1D1) }
                        ElseIf (LEqual (INP0, 0x63)) { Store (EC63, V1D1) }
                        ElseIf (LEqual (INP0, 0x64)) { Store (EC64, V1D1) }
                        ElseIf (LEqual (INP0, 0x65)) { Store (EC65, V1D1) }
                        ElseIf (LEqual (INP0, 0x66)) { Store (EC66, V1D1) }
                        ElseIf (LEqual (INP0, 0x67)) { Store (EC67, V1D1) }
                        ElseIf (LEqual (INP0, 0x68)) { Store (EC68, V1D1) }
                        ElseIf (LEqual (INP0, 0x69)) { Store (EC69, V1D1) }
                        ElseIf (LEqual (INP0, 0x6A)) { Store (EC6A, V1D1) }
                        ElseIf (LEqual (INP0, 0x6B)) { Store (EC6B, V1D1) }
                        ElseIf (LEqual (INP0, 0x6C)) { Store (EC6C, V1D1) }
                        ElseIf (LEqual (INP0, 0x6D)) { Store (EC6D, V1D1) }
                        ElseIf (LEqual (INP0, 0x6E)) { Store (EC6E, V1D1) }
                        ElseIf (LEqual (INP0, 0x6F)) { Store (EC6F, V1D1) }
                        ElseIf (LEqual (INP0, 0x70)) { Store (EC70, V1D1) }
                        ElseIf (LEqual (INP0, 0x71)) { Store (EC71, V1D1) }
                        ElseIf (LEqual (INP0, 0x72)) { Store (EC72, V1D1) }
                        ElseIf (LEqual (INP0, 0x73)) { Store (EC73, V1D1) }
                        ElseIf (LEqual (INP0, 0x74)) { Store (EC74, V1D1) }
                        ElseIf (LEqual (INP0, 0x75)) { Store (EC75, V1D1) }
                        ElseIf (LEqual (INP0, 0x76)) { Store (EC76, V1D1) }
                        ElseIf (LEqual (INP0, 0x77)) { Store (EC77, V1D1) }
                        ElseIf (LEqual (INP0, 0x78)) { Store (EC78, V1D1) }
                        ElseIf (LEqual (INP0, 0x79)) { Store (EC79, V1D1) }
                        ElseIf (LEqual (INP0, 0x7A)) { Store (EC7A, V1D1) }
                        ElseIf (LEqual (INP0, 0x7B)) { Store (EC7B, V1D1) }
                        ElseIf (LEqual (INP0, 0x7C)) { Store (EC7C, V1D1) }
                        ElseIf (LEqual (INP0, 0x7D)) { Store (EC7D, V1D1) }
                        ElseIf (LEqual (INP0, 0x7E)) { Store (EC7E, V1D1) }
                        ElseIf (LEqual (INP0, 0x7F)) { Store (EC7F, V1D1) }
                        ElseIf (LEqual (INP0, 0x80)) { Store (EC80, V1D1) }
                        ElseIf (LEqual (INP0, 0x81)) { Store (EC81, V1D1) }
                        ElseIf (LEqual (INP0, 0x82)) { Store (EC82, V1D1) }
                        ElseIf (LEqual (INP0, 0x83)) { Store (EC83, V1D1) }
                        ElseIf (LEqual (INP0, 0x84)) { Store (EC84, V1D1) }
                        ElseIf (LEqual (INP0, 0x85)) { Store (EC85, V1D1) }
                        ElseIf (LEqual (INP0, 0x86)) { Store (EC86, V1D1) }
                        ElseIf (LEqual (INP0, 0x87)) { Store (EC87, V1D1) }
                        ElseIf (LEqual (INP0, 0x88)) { Store (EC88, V1D1) }
                        ElseIf (LEqual (INP0, 0x89)) { Store (EC89, V1D1) }
                        ElseIf (LEqual (INP0, 0x8A)) { Store (EC8A, V1D1) }
                        ElseIf (LEqual (INP0, 0x8B)) { Store (EC8B, V1D1) }
                        ElseIf (LEqual (INP0, 0x8C)) { Store (EC8C, V1D1) }
                        ElseIf (LEqual (INP0, 0x8D)) { Store (EC8D, V1D1) }
                        ElseIf (LEqual (INP0, 0x8E)) { Store (EC8E, V1D1) }
                        ElseIf (LEqual (INP0, 0x8F)) { Store (EC8F, V1D1) }
                        ElseIf (LEqual (INP0, 0x90)) { Store (EC90, V1D1) }
                        ElseIf (LEqual (INP0, 0x91)) { Store (EC91, V1D1) }
                        ElseIf (LEqual (INP0, 0x92)) { Store (EC92, V1D1) }
                        ElseIf (LEqual (INP0, 0x93)) { Store (EC93, V1D1) }
                        ElseIf (LEqual (INP0, 0x94)) { Store (EC94, V1D1) }
                        ElseIf (LEqual (INP0, 0x95)) { Store (EC95, V1D1) }
                        ElseIf (LEqual (INP0, 0x96)) { Store (EC96, V1D1) }
                        ElseIf (LEqual (INP0, 0x97)) { Store (EC97, V1D1) }
                        ElseIf (LEqual (INP0, 0x98)) { Store (EC98, V1D1) }
                        ElseIf (LEqual (INP0, 0x99)) { Store (EC99, V1D1) }
                        ElseIf (LEqual (INP0, 0x9A)) { Store (EC9A, V1D1) }
                        ElseIf (LEqual (INP0, 0x9B)) { Store (EC9B, V1D1) }
                        ElseIf (LEqual (INP0, 0x9C)) { Store (EC9C, V1D1) }
                        ElseIf (LEqual (INP0, 0x9D)) { Store (EC9D, V1D1) }
                        ElseIf (LEqual (INP0, 0x9E)) { Store (EC9E, V1D1) }
                        ElseIf (LEqual (INP0, 0x9F)) { Store (EC9F, V1D1) }
                        ElseIf (LEqual (INP0, 0xA0)) { Store (ECA0, V1D1) }
                        ElseIf (LEqual (INP0, 0xA1)) { Store (ECA1, V1D1) }
                        ElseIf (LEqual (INP0, 0xA2)) { Store (ECA2, V1D1) }
                        ElseIf (LEqual (INP0, 0xA3)) { Store (ECA3, V1D1) }
                        ElseIf (LEqual (INP0, 0xA4)) { Store (ECA4, V1D1) }
                        ElseIf (LEqual (INP0, 0xA5)) { Store (ECA5, V1D1) }
                        ElseIf (LEqual (INP0, 0xA6)) { Store (ECA6, V1D1) }
                        ElseIf (LEqual (INP0, 0xA7)) { Store (ECA7, V1D1) }
                        ElseIf (LEqual (INP0, 0xA8)) { Store (ECA8, V1D1) }
                        ElseIf (LEqual (INP0, 0xA9)) { Store (ECA9, V1D1) }
                        ElseIf (LEqual (INP0, 0xAA)) { Store (ECAA, V1D1) }
                        ElseIf (LEqual (INP0, 0xAB)) { Store (ECAB, V1D1) }
                        ElseIf (LEqual (INP0, 0xAC)) { Store (ECAC, V1D1) }
                        ElseIf (LEqual (INP0, 0xAD)) { Store (ECAD, V1D1) }
                        ElseIf (LEqual (INP0, 0xAE)) { Store (ECAE, V1D1) }
                        ElseIf (LEqual (INP0, 0xAF)) { Store (ECAF, V1D1) }
                        ElseIf (LEqual (INP0, 0xB0)) { Store (ECB0, V1D1) }
                        ElseIf (LEqual (INP0, 0xB1)) { Store (ECB1, V1D1) }
                        ElseIf (LEqual (INP0, 0xB2)) { Store (ECB2, V1D1) }
                        ElseIf (LEqual (INP0, 0xB3)) { Store (ECB3, V1D1) }
                        ElseIf (LEqual (INP0, 0xB4)) { Store (ECB4, V1D1) }
                        ElseIf (LEqual (INP0, 0xB5)) { Store (ECB5, V1D1) }
                        ElseIf (LEqual (INP0, 0xB6)) { Store (ECB6, V1D1) }
                        ElseIf (LEqual (INP0, 0xB7)) { Store (ECB7, V1D1) }
                        ElseIf (LEqual (INP0, 0xB8)) { Store (ECB8, V1D1) }
                        ElseIf (LEqual (INP0, 0xB9)) { Store (ECB9, V1D1) }
                        ElseIf (LEqual (INP0, 0xBA)) { Store (ECBA, V1D1) }
                        ElseIf (LEqual (INP0, 0xBB)) { Store (ECBB, V1D1) }
                        ElseIf (LEqual (INP0, 0xBC)) { Store (ECBC, V1D1) }
                        ElseIf (LEqual (INP0, 0xBD)) { Store (ECBD, V1D1) }
                        ElseIf (LEqual (INP0, 0xBE)) { Store (ECBE, V1D1) }
                        ElseIf (LEqual (INP0, 0xBF)) { Store (ECBF, V1D1) }
                        ElseIf (LEqual (INP0, 0xC0)) { Store (ECC0, V1D1) }
                        ElseIf (LEqual (INP0, 0xC1)) { Store (ECC1, V1D1) }
                        ElseIf (LEqual (INP0, 0xC2)) { Store (ECC2, V1D1) }
                        ElseIf (LEqual (INP0, 0xC3)) { Store (ECC3, V1D1) }
                        ElseIf (LEqual (INP0, 0xC4)) { Store (ECC4, V1D1) }
                        ElseIf (LEqual (INP0, 0xC5)) { Store (ECC5, V1D1) }
                        ElseIf (LEqual (INP0, 0xC6)) { Store (ECC6, V1D1) }
                        ElseIf (LEqual (INP0, 0xC7)) { Store (ECC7, V1D1) }
                        ElseIf (LEqual (INP0, 0xC8)) { Store (ECC8, V1D1) }
                        ElseIf (LEqual (INP0, 0xC9)) { Store (ECC9, V1D1) }
                        ElseIf (LEqual (INP0, 0xCA)) { Store (ECCA, V1D1) }
                        ElseIf (LEqual (INP0, 0xCB)) { Store (ECCB, V1D1) }
                        ElseIf (LEqual (INP0, 0xCC)) { Store (ECCC, V1D1) }
                        ElseIf (LEqual (INP0, 0xCD)) { Store (ECCD, V1D1) }
                        ElseIf (LEqual (INP0, 0xCE)) { Store (ECCE, V1D1) }
                        ElseIf (LEqual (INP0, 0xCF)) { Store (ECCF, V1D1) }
                        ElseIf (LEqual (INP0, 0xD0)) { Store (ECD0, V1D1) }
                        ElseIf (LEqual (INP0, 0xD1)) { Store (ECD1, V1D1) }
                        ElseIf (LEqual (INP0, 0xD2)) { Store (ECD2, V1D1) }
                        ElseIf (LEqual (INP0, 0xD3)) { Store (ECD3, V1D1) }
                        ElseIf (LEqual (INP0, 0xD4)) { Store (ECD4, V1D1) }
                        ElseIf (LEqual (INP0, 0xD5)) { Store (ECD5, V1D1) }
                        ElseIf (LEqual (INP0, 0xD6)) { Store (ECD6, V1D1) }
                        ElseIf (LEqual (INP0, 0xD7)) { Store (ECD7, V1D1) }
                        ElseIf (LEqual (INP0, 0xD8)) { Store (ECD8, V1D1) }
                        ElseIf (LEqual (INP0, 0xD9)) { Store (ECD9, V1D1) }
                        ElseIf (LEqual (INP0, 0xDA)) { Store (ECDA, V1D1) }
                        ElseIf (LEqual (INP0, 0xDB)) { Store (ECDB, V1D1) }
                        ElseIf (LEqual (INP0, 0xDC)) { Store (ECDC, V1D1) }
                        ElseIf (LEqual (INP0, 0xDD)) { Store (ECDD, V1D1) }
                        ElseIf (LEqual (INP0, 0xDE)) { Store (ECDE, V1D1) }
                        ElseIf (LEqual (INP0, 0xDF)) { Store (ECDF, V1D1) }
                        ElseIf (LEqual (INP0, 0xE0)) { Store (ECE0, V1D1) }
                        ElseIf (LEqual (INP0, 0xE1)) { Store (ECE1, V1D1) }
                        ElseIf (LEqual (INP0, 0xE2)) { Store (ECE2, V1D1) }
                        ElseIf (LEqual (INP0, 0xE3)) { Store (ECE3, V1D1) }
                        ElseIf (LEqual (INP0, 0xE4)) { Store (ECE4, V1D1) }
                        ElseIf (LEqual (INP0, 0xE5)) { Store (ECE5, V1D1) }
                        ElseIf (LEqual (INP0, 0xE6)) { Store (ECE6, V1D1) }
                        ElseIf (LEqual (INP0, 0xE7)) { Store (ECE7, V1D1) }
                        ElseIf (LEqual (INP0, 0xE8)) { Store (ECE8, V1D1) }
                        ElseIf (LEqual (INP0, 0xE9)) { Store (ECE9, V1D1) }
                        ElseIf (LEqual (INP0, 0xEA)) { Store (ECEA, V1D1) }
                        ElseIf (LEqual (INP0, 0xEB)) { Store (ECEB, V1D1) }
                        ElseIf (LEqual (INP0, 0xEC)) { Store (ECEC, V1D1) }
                        ElseIf (LEqual (INP0, 0xED)) { Store (ECED, V1D1) }
                        ElseIf (LEqual (INP0, 0xEE)) { Store (ECEE, V1D1) }
                        ElseIf (LEqual (INP0, 0xEF)) { Store (ECEF, V1D1) }
                        ElseIf (LEqual (INP0, 0xF0)) { Store (ECF0, V1D1) }
                        ElseIf (LEqual (INP0, 0xF1)) { Store (ECF1, V1D1) }
                        ElseIf (LEqual (INP0, 0xF2)) { Store (ECF2, V1D1) }
                        ElseIf (LEqual (INP0, 0xF3)) { Store (ECF3, V1D1) }
                        ElseIf (LEqual (INP0, 0xF4)) { Store (ECF4, V1D1) }
                        ElseIf (LEqual (INP0, 0xF5)) { Store (ECF5, V1D1) }
                        ElseIf (LEqual (INP0, 0xF6)) { Store (ECF6, V1D1) }
                        ElseIf (LEqual (INP0, 0xF7)) { Store (ECF7, V1D1) }
                        ElseIf (LEqual (INP0, 0xF8)) { Store (ECF8, V1D1) }
                        ElseIf (LEqual (INP0, 0xF9)) { Store (ECF9, V1D1) }
                        ElseIf (LEqual (INP0, 0xFA)) { Store (ECFA, V1D1) }
                        ElseIf (LEqual (INP0, 0xFB)) { Store (ECFB, V1D1) }
                        ElseIf (LEqual (INP0, 0xFC)) { Store (ECFC, V1D1) }
                        ElseIf (LEqual (INP0, 0xFD)) { Store (ECFD, V1D1) }
                        ElseIf (LEqual (INP0, 0xFE)) { Store (ECFE, V1D1) }
                        ElseIf (LEqual (INP0, 0xFF)) { Store (ECFF, V1D1) }
                    Return (R1DW)
                }

                // Default Fallback for Unknown Method IDs
                Store (0xFFFFFFFF, V1D1)
                Return (R1DW)
            }
        }
    }
}
