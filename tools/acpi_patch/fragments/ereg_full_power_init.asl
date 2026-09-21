                    Method (_REG, 2, NotSerialized)  // _REG: Region Availability
                    {
                        If ((Arg0 == 0x03))
                        {
                            ECAV = Arg1
                        }

                        If (ECAV)
                        {
                            Local0 = (ECRD (RefOf (AWHG)) << 0x08)
                            Local0 += ECRD (RefOf (AWLW))
                            Local1 = ECRD (RefOf (ECWR))
                            If (((Local0 > 0xC8) && (Local1 & One))){}
                            ElseIf ((ECRD (RefOf (CMEN)) == One))
                            {
                                ECWT (Zero, RefOf (CMEN))
                            }

                            // 开机满血初始化。这是权威钩子：此处 ECAV 必已
                            // 置位，每条 ECWT 都会落地。DBUL SSDT 的 _INI 只是
                            // 备份，部分开机中会与本 _REG 竞态（EC region 接通
                            // 前其 ECWT 静默空跑）。与 DBUL _INI 同值：
                            // GFLG=0x55/GPMD=1 GPU 满血令牌+模式；
                            // CSPL=0x6E(110W)/FPPT=0x78(120W) 整机功耗墙默认
                            // —— 110W CPU + 140W GPU + 30W 外设 = 280W 打满
                            // 适配器预算。CSPL 是整机墙的本体：此后每次
                            // FNQS/COMM 都经 MSPL 把 max(EC CSPL, 下限) 重推
                            // 给 SMU，覆盖 THMD profile 的 SPL 值。
                            ECWT (0x55, RefOf (GFLG))
                            ECWT (One, RefOf (GPMD))
                            ECWT (0x6E, RefOf (CSPL))
                            ECWT (0x78, RefOf (FPPT))
                            Local0 = ECRD (RefOf (ITSM))
                            FNQS (Local0)
                            COMM ()
                        }
                    }

