                    Method (FNQS, 1, Serialized)
                    {
                        // DB 感知换档（R9000P FNQT 协调架构的复刻）。
                        // 极性依据（2026-09-21）：原厂 FNQS 两个分支均以
                        // DBFS == Zero 派发高瓦行（THMD(Zero)/0x13），且历史
                        // 实验"强制 DBFS=Zero → GPU 锁 105W"证明 0=平台视
                        // 为 DB 关。若实机验证极性相反，对调下方两个分支体
                        // 即可（各 2 行）。
                        // 协调逻辑：DB 借电在途（DBFS==One）→ 平台主动降到
                        // 85W 行（THMD(0x13)，实测 CPU 自由的那一行）并同步
                        // EC CSPL=85，期望 EC 因平台已让电而不再施加 40W
                        // 硬压；无借电 → 恢复满血行与 CSPL=110W。
                        If ((DBFS == Zero))
                        {
                            ECWT (0x6E, RefOf (CSPL))
                            If ((ToInteger (Arg0) == Zero))
                            {
                                THMD (0x13)
                            }
                            Else
                            {
                                THMD (Zero)
                            }
                        }
                        Else
                        {
                            ECWT (0x55, RefOf (CSPL))
                            THMD (0x13)
                        }

                        MSPL ()
                        MFPT ()
                    }

                    Method (MSPL, 0, Serialized)
                    {
                        Local0 = ECRD (RefOf (CSPL))
                        If ((Local0 < 0x50))
                        {
                            Local0 = 0x50
                        }

                        Local0 *= 0x03E8
                        MODP (0x05, Local0)
                        MODP (0x07, Local0)
                        MODP (0x13, Local0)
                    }

                    Method (MFPT, 0, Serialized)
                    {
                        Local0 = ECRD (RefOf (FPPT))
                        If ((Local0 < 0x6E))
                        {
                            Local0 = 0x6E
                        }

                        Local0 *= 0x03E8
                        MODP (0x06, Local0)
                    }

                    Method (COMM, 0, Serialized)
                    {
                        MSPL ()
                        MFPT ()
                        Local0 = ECRD (RefOf (CTCL))
                        MODP (0x03, Local0)
                    }


