                        CreateByteField (Arg3, 0x09, NCHP)
                        Switch (ToInteger (INC6))
                        {
                            Case (Zero)
                            {
                                If ((IOBS != Zero))
                                {
                                    F6O0 = HPCT /* \_SB_.NPCF.HPCT */
                                    F6MP = CMPL /* \_SB_.NPCF.CMPL */
                                    F6NP = CNPL /* \_SB_.NPCF.CNPL */
                                    F6O2 = IOBS /* \_SB_.NPCF.IOBS */
                                }
                            }
                            Case (One)
                            {
                                If ((IOBS != Zero))
                                {
                                    OperationRegion (NVIO, SystemIO, IOBS, 0x10)
                                    Field (NVIO, ByteAcc, NoLock, Preserve)
                                    {
                                        CPUC,   8
                                    }

                                    CPUC = NCHP /* \_SB_.NPCF.NPCF.NCHP */
                                    F6MP = Zero
                                    F6NP = Zero
                                    F6O2 = Zero
                                    Notify (\_SB.PLTF.C000, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C001, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C002, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C003, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C004, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C005, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C006, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C007, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C008, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C009, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C00A, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C00B, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C00C, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C00D, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C00E, 0x85) // Device-Specific
                                    Notify (\_SB.PLTF.C00F, 0x85) // Device-Specific
                                }
                            }
                            Default
                            {
                                Return (0x80000002)
                            }

                        }

                        T6C++
                        T6V = NCHP
