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
                // Keep DBFS at 0: with DBFS=0 the firmware MSPL/MFPT clamp block
                // (If (DBFS == One)) never runs, so CPU power limits written via
                // the firmware WMI (MICommonInterface 0x1700) are not re-clamped.
                If (CondRefOf (\DBFS))
                {
                    Store (Zero, \DBFS)
                }
            }

            // -------------------------------------------------------------
            // Master WMI Method Dispatcher (WMTF) - 15 Methods
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
                    // SetDbfs must mirror the firmware's _Q81/_Q82 behavior:
                    //   DBFS = X; FNQS (ITSM); COMM ()
                    // Without FNQS the thermal profile (THMD) never switches,
                    // so toggling DBFS via WMI had no effect.
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
                // Method 8: SetEcRegister (Raw EC Write)
                // =========================================================
                If (LEqual (Arg1, 8))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                    {
                        \_SB.PCI0.LPC0.H_EC.ECWT (INP1, INP0)
                    }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 9: GetEcRegister (Raw EC Read)
                // =========================================================
                If (LEqual (Arg1, 9))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECRD))
                    {
                        Store (\_SB.PCI0.LPC0.H_EC.ECRD (INP0), V1D1)
                    }
                    Else { Store (0xFF, V1D1) }
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

                // Default Fallback for Unknown Method IDs
                Store (0xFFFFFFFF, V1D1)
                Return (R1DW)
            }
        }
    }
}
