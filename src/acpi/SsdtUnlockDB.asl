DefinitionBlock ("", "SSDT", 2, "CUSTOM", "DBUNLOCK", 0x00001000)
{

    External (\DBFS, IntObj)
    External (\_SB.PCI0.LPC0, DeviceObj)
    External (\_SB.PCI0.LPC0.H_EC.COMM, MethodObj)

    Scope (\_SB.PCI0.LPC0)
    {
        Device (DBUL)
        {
            Name (_HID, EisaId ("PNP0C14"))
            Name (_UID, 0xAA)

            Name (DBCT, Zero)

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
                0x54, 0x46, 
                0x01,       
                0x02,       

                0x21, 0x12, 0x90, 0x05,
                0x66, 0xD5, 0xD1, 0x11,
                0xB2, 0xF0, 0x00, 0xA0,
                0xC9, 0x06, 0x29, 0x10,
                0x42, 0x41, 
                0x01,       
                0x00        
            })

            Include ("BmfData.asl")

            Method (_INI, 0, NotSerialized)
            {
                If (CondRefOf (\DBFS))
                {
                    Store (Zero, \DBFS)
                }
                If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM))
                {
                    \_SB.PCI0.LPC0.H_EC.COMM ()
                }
            }

            Method (WMTF, 3, Serialized)
            {

                Name (RBUF, Buffer (4) {0, 0, 0, 0})
                CreateDWordField (RBUF, Zero, RVAL)

                If (LEqual (Arg0, Zero)) {}

                If (LEqual (Arg1, One))
                {
                    If (LGreaterEqual (SizeOf (Arg2), One))
                    {
                        Store (DerefOf (Index (Arg2, Zero)), Local0)
                        If (CondRefOf (\DBFS))
                        {
                            Store (Local0, \DBFS)
                        }
                        If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM))
                        {
                            \_SB.PCI0.LPC0.H_EC.COMM ()
                        }
                    }
                    Store (Zero, RVAL) 
                    Return (RBUF)
                }

                If (LEqual (Arg1, 2))
                {
                    If (CondRefOf (\DBFS))
                    {
                        Store (\DBFS, RVAL)
                    }
                    Else
                    {
                        Store (0xFFFFFFFF, RVAL)
                    }
                    Return (RBUF)
                }

                Store (0xFFFFFFFF, RVAL)
                Return (RBUF)
            }
        }
    }
}