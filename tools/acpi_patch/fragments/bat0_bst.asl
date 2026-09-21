                        Method (_BST, 0, NotSerialized)  // _BST: Battery Status
                        {
                            Name (PKG1, Package (0x04)
                            {
                                Ones, 
                                Ones, 
                                Ones, 
                                Ones
                            })
                            Local0 = Zero
                            If ((ECRD (RefOf (ECWR)) & 0x04))
                            {
                                Local0 = 0x02
                            }
                            ElseIf ((ECRD (RefOf (ECWR)) & 0x08))
                            {
                                If ((ECRD (RefOf (ECWR)) & One))
                                {
                                    Local0 = Zero
                                }
                                Else
                                {
                                    Local0 = One
                                }
                            }

                            If ((ECRD (RefOf (ECWR)) & 0x10))
                            {
                                Local0 |= 0x04
                            }

                            PKG1 [Zero] = Local0
                            Local1 = (B1CR * 0x0A)
                            PKG1 [One] = Local1
                            PKG1 [0x02] = (B1RC * 0x0A)
                            PKG1 [0x03] = B1FV /* \_SB_.PCI0.LPC0.H_EC.B1FV */
                            Return (PKG1) /* \_SB_.PCI0.LPC0.H_EC.BAT0._BST.PKG1 */
                        }


