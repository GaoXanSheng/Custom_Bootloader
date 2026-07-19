/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of f:/ECtools/isal/ssdt21.dat
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00000761 (1889)
 *     Revision         0x02
 *     Checksum         0x31
 *     OEM ID           "INSYDE"
 *     OEM Table ID     "EDK2    "
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "ACPI"
 *     Compiler Version 0x00040000 (262144)
 */
DefinitionBlock ("", "SSDT", 2, "INSYDE", "EDK2    ", 0x00000001)
{
    External (_SB_.PCI0.GPP3, DeviceObj)
    External (_SB_.PCI0.GPP4, DeviceObj)
    External (_SB_.PCI0.GPP5, DeviceObj)
    External (_SB_.PCI0.GPP5.LAN_, DeviceObj)
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

    Name (DSEN, 0x00)
    Name (DSI1, 0x00001001)
    Scope (\_SB.PCI0.GPP3)
    {
        PowerResource (PWR1, 0x00, 0x0000)
        {
            Name (PWRS, One)
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (PWRS) /* \_SB_.PCI0.GPP3.PWR1.PWRS */
            }

            Method (_ON, 0, NotSerialized)  // _ON_: Power On
            {
                PWRS = One
            }

            Method (_OFF, 0, NotSerialized)  // _OFF: Power Off
            {
                PWRS = Zero
            }
        }

        Name (_S0W, 0x04)  // _S0W: S0 Device Wake State
        Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
        {
            PWR1, 
        })
        Name (_PR2, Package (0x01)  // _PR2: Power Resources for D2
        {
            PWR1, 
        })
        Name (_PR3, Package (0x01)  // _PR3: Power Resources for D3hot
        {
            PWR1, 
        })
    }

    Scope (\_SB.PCI0.GPP4)
    {
        PowerResource (PWR1, 0x00, 0x0000)
        {
            Name (PWRS, One)
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (PWRS) /* \_SB_.PCI0.GPP4.PWR1.PWRS */
            }

            Method (_ON, 0, NotSerialized)  // _ON_: Power On
            {
                PWRS = One
            }

            Method (_OFF, 0, NotSerialized)  // _OFF: Power Off
            {
                PWRS = Zero
            }
        }

        Name (_S0W, 0x04)  // _S0W: S0 Device Wake State
        Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
        {
            PWR1, 
        })
        Name (_PR2, Package (0x01)  // _PR2: Power Resources for D2
        {
            PWR1, 
        })
        Name (_PR3, Package (0x01)  // _PR3: Power Resources for D3hot
        {
            PWR1, 
        })
    }

    Scope (\_SB.PCI0.GPP5)
    {
        PowerResource (PWR1, 0x00, 0x0000)
        {
            Name (PWRS, One)
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (PWRS) /* \_SB_.PCI0.GPP5.PWR1.PWRS */
            }

            Method (_ON, 0, NotSerialized)  // _ON_: Power On
            {
                PWRS = One
            }

            Method (_OFF, 0, NotSerialized)  // _OFF: Power Off
            {
                PWRS = Zero
            }
        }

        Name (_S0W, 0x04)  // _S0W: S0 Device Wake State
        Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
        {
            PWR1, 
        })
        Name (_PR2, Package (0x01)  // _PR2: Power Resources for D2
        {
            PWR1, 
        })
        Name (_PR3, Package (0x01)  // _PR3: Power Resources for D3hot
        {
            PWR1, 
        })
    }

    If (CondRefOf (\_SB.PCI0.GPP5.LAN))
    {
        Scope (\_SB.PCI0.GPP5.LAN)
        {
            Name (SXXX, One)
            OperationRegion (PCIF, PCI_Config, Zero, 0x0100)
            Field (PCIF, ByteAcc, NoLock, Preserve)
            {
                Offset (0x10), 
                BAR0,   32
            }

            Method (_REG, 2, NotSerialized)  // _REG: Region Availability
            {
                Local0 = Arg0
                Local1 = Arg1
                Local3 = SXXX /* \_SB_.PCI0.GPP5.LAN_.SXXX */
                M460 ("  LN00 DSEN =0x%X Arg0 = 0x%X  Arg01 = 0x%X \n", DSEN, Arg0, Arg1, Zero, Zero, Zero)
                If (((DSEN == Zero) && (Local3 == One)))
                {
                    If (((Local0 == 0x02) && (Local1 == One)))
                    {
                        If ((DSI1 != 0x5A5A5A5A))
                        {
                            Local2 = DSI1 /* \DSI1 */
                            Local2 &= 0xFFF0
                            OperationRegion (VAIO, SystemIO, Local2, 0x0100)
                            Field (VAIO, ByteAcc, NoLock, Preserve)
                            {
                                Offset (0x64), 
                                XX64,   32, 
                                XX68,   32
                            }

                            M460 ("  LN00 Addr =0x%X Arg0 = 0x%X  Arg01 = 0x%X \n", Local2, Arg0, Arg1, Zero, Zero, Zero)
                            XX64 = 0xFFFFFFFF
                            XX68 = 0x8001F000
                            Sleep (0x02)
                            XX64 = 0xFFFFFFFF
                            XX68 = 0x8002F000
                            Sleep (0x02)
                            XX64 = 0xFFFFFFFF
                            XX68 = 0x8003F000
                            Sleep (0x02)
                        }
                    }
                }
            }
        }
    }
}

