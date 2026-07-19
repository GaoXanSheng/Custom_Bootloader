/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of f:/ECtools/isal/ssdt19.dat
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00000868 (2152)
 *     Revision         0x02
 *     Checksum         0xB1
 *     OEM ID           "INSYDE"
 *     OEM Table ID     "EDK2    "
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "ACPI"
 *     Compiler Version 0x00040000 (262144)
 */
DefinitionBlock ("", "SSDT", 2, "INSYDE", "EDK2    ", 0x00000001)
{
    External (_SB_.GPIO, DeviceObj)
    External (_SB_.PCI0.GP17.ACP_, DeviceObj)
    External (_SB_.PCI0.GP17.AZAL, DeviceObj)
    External (_SB_.PCI0.GP17.XHC0, DeviceObj)
    External (_SB_.PCI0.GP17.XHC1, DeviceObj)
    External (_SB_.PCI0.GPP0.PEGP, DeviceObj)
    External (_SB_.PCI0.GPP3, DeviceObj)
    External (_SB_.PCI0.GPP4, DeviceObj)
    External (_SB_.PCI0.GPP5, DeviceObj)
    External (_SB_.PWRB, DeviceObj)
    External (_SB_.UBTC, DeviceObj)
    External (M000, MethodObj)    // 1 Arguments
    External (M037, DeviceObj)
    External (M046, IntObj)
    External (M050, DeviceObj)
    External (M051, DeviceObj)
    External (M052, DeviceObj)
    External (M053, DeviceObj)
    External (M054, DeviceObj)
    External (M055, DeviceObj)
    External (M056, DeviceObj)
    External (M057, DeviceObj)
    External (M058, DeviceObj)
    External (M059, DeviceObj)
    External (M062, DeviceObj)
    External (M068, DeviceObj)
    External (M069, DeviceObj)
    External (M070, DeviceObj)
    External (M071, DeviceObj)
    External (M072, DeviceObj)
    External (M074, DeviceObj)
    External (M075, DeviceObj)
    External (M076, DeviceObj)
    External (M077, DeviceObj)
    External (M078, DeviceObj)
    External (M079, DeviceObj)
    External (M080, DeviceObj)
    External (M081, DeviceObj)
    External (M082, FieldUnitObj)
    External (M083, FieldUnitObj)
    External (M084, FieldUnitObj)
    External (M085, FieldUnitObj)
    External (M086, FieldUnitObj)
    External (M087, FieldUnitObj)
    External (M088, FieldUnitObj)
    External (M089, FieldUnitObj)
    External (M090, FieldUnitObj)
    External (M091, FieldUnitObj)
    External (M092, FieldUnitObj)
    External (M093, FieldUnitObj)
    External (M094, FieldUnitObj)
    External (M095, FieldUnitObj)
    External (M096, FieldUnitObj)
    External (M097, FieldUnitObj)
    External (M098, FieldUnitObj)
    External (M099, FieldUnitObj)
    External (M100, FieldUnitObj)
    External (M101, FieldUnitObj)
    External (M102, FieldUnitObj)
    External (M103, FieldUnitObj)
    External (M104, FieldUnitObj)
    External (M105, FieldUnitObj)
    External (M106, FieldUnitObj)
    External (M107, FieldUnitObj)
    External (M108, FieldUnitObj)
    External (M109, FieldUnitObj)
    External (M110, FieldUnitObj)
    External (M115, BuffObj)
    External (M116, BuffFieldObj)
    External (M117, BuffFieldObj)
    External (M118, BuffFieldObj)
    External (M119, BuffFieldObj)
    External (M120, BuffFieldObj)
    External (M122, FieldUnitObj)
    External (M127, DeviceObj)
    External (M128, FieldUnitObj)
    External (M131, FieldUnitObj)
    External (M132, FieldUnitObj)
    External (M133, FieldUnitObj)
    External (M134, FieldUnitObj)
    External (M135, FieldUnitObj)
    External (M136, FieldUnitObj)
    External (M220, FieldUnitObj)
    External (M221, FieldUnitObj)
    External (M226, FieldUnitObj)
    External (M227, DeviceObj)
    External (M229, FieldUnitObj)
    External (M231, FieldUnitObj)
    External (M233, FieldUnitObj)
    External (M235, FieldUnitObj)
    External (M23A, FieldUnitObj)
    External (M251, FieldUnitObj)
    External (M280, FieldUnitObj)
    External (M290, FieldUnitObj)
    External (M29A, FieldUnitObj)
    External (M310, FieldUnitObj)
    External (M31C, FieldUnitObj)
    External (M31D, MethodObj)    // 0 Arguments
    External (M320, FieldUnitObj)
    External (M321, FieldUnitObj)
    External (M322, FieldUnitObj)
    External (M323, FieldUnitObj)
    External (M324, FieldUnitObj)
    External (M325, FieldUnitObj)
    External (M326, FieldUnitObj)
    External (M327, FieldUnitObj)
    External (M328, FieldUnitObj)
    External (M329, DeviceObj)
    External (M32A, DeviceObj)
    External (M32B, DeviceObj)
    External (M330, DeviceObj)
    External (M331, FieldUnitObj)
    External (M378, FieldUnitObj)
    External (M379, FieldUnitObj)
    External (M380, FieldUnitObj)
    External (M381, FieldUnitObj)
    External (M382, FieldUnitObj)
    External (M383, FieldUnitObj)
    External (M384, FieldUnitObj)
    External (M385, FieldUnitObj)
    External (M386, FieldUnitObj)
    External (M387, FieldUnitObj)
    External (M388, FieldUnitObj)
    External (M389, FieldUnitObj)
    External (M390, FieldUnitObj)
    External (M391, FieldUnitObj)
    External (M392, FieldUnitObj)
    External (M404, BuffObj)
    External (M408, MutexObj)
    External (M414, FieldUnitObj)
    External (M444, FieldUnitObj)
    External (M449, FieldUnitObj)
    External (M453, FieldUnitObj)
    External (M454, FieldUnitObj)
    External (M455, FieldUnitObj)
    External (M456, FieldUnitObj)
    External (M457, FieldUnitObj)
    External (M460, MethodObj)    // 7 Arguments
    External (M4C0, FieldUnitObj)
    External (M4F0, FieldUnitObj)
    External (M610, FieldUnitObj)
    External (M620, FieldUnitObj)
    External (M631, FieldUnitObj)

    Scope (\_SB.GPIO)
    {
        Method (_AEI, 0, NotSerialized)  // _AEI: ACPI Event Interrupts
        {
            Name (BUNP, Buffer (0x0160)
            {
                /* 0000 */  0x8C, 0x20, 0x00, 0x01, 0x00, 0x01, 0x00, 0x11,  // . ......
                /* 0008 */  0x00, 0x00, 0x00, 0x00, 0x88, 0x13, 0x17, 0x00,  // ........
                /* 0010 */  0x00, 0x19, 0x00, 0x23, 0x00, 0x00, 0x00, 0x00,  // ...#....
                /* 0018 */  0x00, 0x5C, 0x5F, 0x53, 0x42, 0x2E, 0x47, 0x50,  // .\_SB.GP
                /* 0020 */  0x49, 0x4F, 0x00, 0x8C, 0x20, 0x00, 0x01, 0x00,  // IO.. ...
                /* 0028 */  0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00,  // ........
                /* 0030 */  0x00, 0x17, 0x00, 0x00, 0x19, 0x00, 0x23, 0x00,  // ......#.
                /* 0038 */  0x00, 0x00, 0x3D, 0x00, 0x5C, 0x5F, 0x53, 0x42,  // ..=.\_SB
                /* 0040 */  0x2E, 0x47, 0x50, 0x49, 0x4F, 0x00, 0x8C, 0x20,  // .GPIO.. 
                /* 0048 */  0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03,  // ........
                /* 0050 */  0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x19,  // ........
                /* 0058 */  0x00, 0x23, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x5C,  // .#...>.\
                /* 0060 */  0x5F, 0x53, 0x42, 0x2E, 0x47, 0x50, 0x49, 0x4F,  // _SB.GPIO
                /* 0068 */  0x00, 0x8C, 0x20, 0x00, 0x01, 0x00, 0x01, 0x00,  // .. .....
                /* 0070 */  0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x17,  // ........
                /* 0078 */  0x00, 0x00, 0x19, 0x00, 0x23, 0x00, 0x00, 0x00,  // ....#...
                /* 0080 */  0x3A, 0x00, 0x5C, 0x5F, 0x53, 0x42, 0x2E, 0x47,  // :.\_SB.G
                /* 0088 */  0x50, 0x49, 0x4F, 0x00, 0x8C, 0x20, 0x00, 0x01,  // PIO.. ..
                /* 0090 */  0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00,  // ........
                /* 0098 */  0x00, 0x00, 0x17, 0x00, 0x00, 0x19, 0x00, 0x23,  // .......#
                /* 00A0 */  0x00, 0x00, 0x00, 0x3B, 0x00, 0x5C, 0x5F, 0x53,  // ...;.\_S
                /* 00A8 */  0x42, 0x2E, 0x47, 0x50, 0x49, 0x4F, 0x00, 0x8C,  // B.GPIO..
                /* 00B0 */  0x20, 0x00, 0x01, 0x00, 0x01, 0x00, 0x13, 0x00,  //  .......
                /* 00B8 */  0x03, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00,  // ........
                /* 00C0 */  0x19, 0x00, 0x23, 0x00, 0x00, 0x00, 0x11, 0x00,  // ..#.....
                /* 00C8 */  0x5C, 0x5F, 0x53, 0x42, 0x2E, 0x47, 0x50, 0x49,  // \_SB.GPI
                /* 00D0 */  0x4F, 0x00, 0x8C, 0x20, 0x00, 0x01, 0x00, 0x01,  // O.. ....
                /* 00D8 */  0x00, 0x13, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,  // ........
                /* 00E0 */  0x17, 0x00, 0x00, 0x19, 0x00, 0x23, 0x00, 0x00,  // .....#..
                /* 00E8 */  0x00, 0x04, 0x00, 0x5C, 0x5F, 0x53, 0x42, 0x2E,  // ...\_SB.
                /* 00F0 */  0x47, 0x50, 0x49, 0x4F, 0x00, 0x8C, 0x20, 0x00,  // GPIO.. .
                /* 00F8 */  0x01, 0x00, 0x01, 0x00, 0x13, 0x00, 0x03, 0x00,  // ........
                /* 0100 */  0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x19, 0x00,  // ........
                /* 0108 */  0x23, 0x00, 0x00, 0x00, 0x18, 0x00, 0x5C, 0x5F,  // #.....\_
                /* 0110 */  0x53, 0x42, 0x2E, 0x47, 0x50, 0x49, 0x4F, 0x00,  // SB.GPIO.
                /* 0118 */  0x8C, 0x20, 0x00, 0x01, 0x00, 0x01, 0x00, 0x13,  // . ......
                /* 0120 */  0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00,  // ........
                /* 0128 */  0x00, 0x19, 0x00, 0x23, 0x00, 0x00, 0x00, 0x09,  // ...#....
                /* 0130 */  0x00, 0x5C, 0x5F, 0x53, 0x42, 0x2E, 0x47, 0x50,  // .\_SB.GP
                /* 0138 */  0x49, 0x4F, 0x00, 0x8C, 0x20, 0x00, 0x01, 0x00,  // IO.. ...
                /* 0140 */  0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xF4,  // ........
                /* 0148 */  0x01, 0x17, 0x00, 0x00, 0x19, 0x00, 0x23, 0x00,  // ......#.
                /* 0150 */  0x00, 0x00, 0x0A, 0x00, 0x5C, 0x5F, 0x53, 0x42,  // ....\_SB
                /* 0158 */  0x2E, 0x47, 0x50, 0x49, 0x4F, 0x00, 0x79, 0x00   // .GPIO.y.
            })
            Return (BUNP) /* \_SB_.GPIO._AEI.BUNP */
        }

        Method (_EVT, 1, Serialized)  // _EVT: Event
        {
            Switch (ToInteger (Arg0))
            {
                Case (Zero)
                {
                    M000 (0x3900)
                    Notify (\_SB.PWRB, 0x80) // Status Change
                }
                Case (0x3A)
                {
                    M000 (0x393A)
                    Notify (\_SB.PCI0.GP17.XHC0, 0x02) // Device Wake
                }
                Case (0x3B)
                {
                    M000 (0x393B)
                    Notify (\_SB.PCI0.GP17.XHC1, 0x02) // Device Wake
                }
                Case (0x3D)
                {
                    M000 (0x393D)
                    Notify (\_SB.PCI0.GP17.AZAL, 0x02) // Device Wake
                }
                Case (0x3E)
                {
                    M000 (0x393D)
                    Notify (\_SB.PCI0.GP17.ACP, 0x02) // Device Wake
                }
                Case (0x11)
                {
                    M000 (0x3911)
                    Notify (\_SB.PCI0.GPP3, 0x02) // Device Wake
                }
                Case (0x04)
                {
                    M000 (0x3904)
                    Notify (\_SB.PCI0.GPP0.PEGP, 0x81) // Information Change
                }
                Case (0x09)
                {
                    M000 (0x3909)
                    Notify (\_SB.PCI0.GPP5, 0x02) // Device Wake
                }
                Case (0x0A)
                {
                    If (CondRefOf (M31D))
                    {
                        M31D ()
                    }

                    If (CondRefOf (UBTC))
                    {
                        Notify (\_SB.UBTC, 0x80) // Status Change
                    }
                }
                Case (0x18)
                {
                    M000 (0x3918)
                    M460 ("    Notify (\\_SB.PCI0.GPP4, 0x00)\n", Zero, Zero, Zero, Zero, Zero, Zero)
                    Notify (\_SB.PCI0.GPP4, Zero) // Bus Check
                }

            }
        }
    }
}

