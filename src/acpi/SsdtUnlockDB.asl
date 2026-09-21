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
    // Method 8/9 whitelist additions (fields the firmware itself writes via
    // MIFS WMAA 0xFB00, ssdt3)
    External (\_SB.PCI0.LPC0.H_EC.FNHK, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.TOCP, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.FASP, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.KBNL, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.FWDE, FieldUnitObj)
    External (\_SB.PCI0.LPC0.H_EC.LEDM, FieldUnitObj)
    External (\_SB.NPCF, DeviceObj)
    External (\_SB.NPCF.ACBT, IntObj)
    External (\_SB.NPCF.DCBT, IntObj)
    External (\_SB.NPCF.AMAT, IntObj)
    External (\_SB.NPCF.AMIT, IntObj)
    // NPCF driver-call trace variables (created by ssdt4-npcf-trace patches)
    External (\_SB.NPCF.CUSL, IntObj)
    External (\_SB.NPCF.CUCT, IntObj)
    External (\_SB.NPCF.T6C, IntObj)
    External (\_SB.NPCF.T6V, IntObj)
    // NPCF Dynamic-Boost 平台开关标志（ssdt4-dbac-dbof-switch 让 fun#2 的
    // DBAC/PC02 读它）。0 = DB 开（原厂：GPU 借电冲 140W，CPU 可能被压 40W）；
    // 1 = DB 关（GPU 锁基础 TGP ~115W，CPU 保持满血）
    External (\_SB.NPCF.DBOF, IntObj)
    // NPCF DB 借电上限（ssdt4-maga-boost-cap 让 fun#2 的 MAGA 读它）。
    // 0.5W 单位，默认 0xC8=100W（原厂上报值）；由 WMTF Method 20 按瓦设置
    External (\_SB.NPCF.MGAF, IntObj)
    // NPCF GPU 功耗预算（ssdt4-tgpa-budget-knob 让 fun#2 性能档的 TGPA 读
    // 它）。0.5W 单位，默认 0x0118=140W（原厂）；由 WMTF Method 22 按瓦设置
    External (\_SB.NPCF.TGPF, IntObj)
    External (\_SB.ALIB, MethodObj)

    Scope (\_SB.PCI0.LPC0)
    {
        Device (DBUL)
        {
            Name (_HID, EisaId ("PNP0C14"))
            Name (_UID, 0xAA)

            Include ("VersionData.asl")

            // WMI Override Persistence Lock Mask & SMI status - REMOVED 2026-09-12:
            // WMOV/WSMI (old WMTF Methods 10-13) had no consumer anywhere, lived
            // only in SSDT namespace variables and reset on every boot. Dead
            // method IDs 10-13/15 now fall through to the default 0xFFFFFFFF.
            // The same applies to ACBT/DCBT/AMAT/AMIT (Method 6/7): kept because
            // they are readable state, but the ASL channel removed every
            // consumer (TGPD=TGPA, TPPD=TPPA), so writes to those names no
            // longer influence power behavior.

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

                // DCBT (电池取电门限) 仍按 80W (0x50) 推送。注：补丁版 SSDT4 的
                // TGPD 已改为读 TGPA (ssdt4-tgpd-ac)，DCBT 在补丁链路中无消费者，
                // 此写仅为保持数值一致（见 patches_ssdt4.py 声明审计）
                If (CondRefOf (\_SB.NPCF.DCBT))
                {
                    Store (0x50, \_SB.NPCF.DCBT)
                }

                // CPU PL1/PL2 初始化配置 (CSPL=110W, FPPT=120W) 并推送 SMU
                // 整机功耗墙 280W：110W CPU + 140W GPU + 30W 外设 = 280W，
                // 打满适配器预算。CSPL 是整机墙本体：此后每次 FNQS/COMM 都
                // 经 MSPL 把 max(EC CSPL, 下限 80W) 重推给 SMU
                If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                {
                    \_SB.PCI0.LPC0.H_EC.ECWT (0x6E, RefOf (\_SB.PCI0.LPC0.H_EC.CSPL))
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.MSPL))
                    {
                        \_SB.PCI0.LPC0.H_EC.MSPL ()
                    }

                    \_SB.PCI0.LPC0.H_EC.ECWT (0x78, RefOf (\_SB.PCI0.LPC0.H_EC.FPPT))
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.MFPT))
                    {
                        \_SB.PCI0.LPC0.H_EC.MFPT ()
                    }
                }
            }

            // -------------------------------------------------------------
            // Master WMI Method Dispatcher (WMTF)
            // Live IDs: 1-9, 14, 16, 17, 19. Dead/removed: 10-13, 15 (fall through
            // to the default 0xFFFFFFFF below).
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
                // Method 1: SetDbfs —— Dynamic Boost 真开关（2026-09-21 重做）
                // =========================================================
                // 旧实现只写 \DBFS：补丁版 FNQS/MSPL/MFPT 已不读它，纯写无效。
                // 新实现拨动 NPCF 作用域的 DBOF 标志（补丁版 SSDT4 的 fun#2
                // 组装 PC02 时读它），再 Notify(NPCF, 0xC0) 逼驱动立即重读
                // fun#2 预算：
                //   INP0 = 1 → DBOF=0 → PC02=0 → DB 开：GPU 借电冲 140W
                //              （原厂路径，CPU 可能被 SMU 压到 40W）
                //   INP0 = 0 → DBOF=1 → PC02=1 → DB 关：GPU 锁基础 TGP
                //              ~115W，CPU 保持满血（实测 2026-09-21：
                //              PC02=1 时 GPU 顶在 115W，PC02=0 时 139.8W）
                // 返回：V1D1 = 0 成功；0xFFFFFFFF = NPCF/DBOF 不在（表未加载）
                If (LEqual (Arg1, 1))
                {
                    Store (0xFFFFFFFF, V1D1)
                    If (CondRefOf (\_SB.NPCF.DBOF))
                    {
                        If (LEqual (INP0, One))
                        {
                            Store (Zero, \_SB.NPCF.DBOF)
                        }
                        Else
                        {
                            Store (One, \_SB.NPCF.DBOF)
                        }

                        // 保持 \DBFS 状态变量与选择一致（EC 侧读它做记录，
                        // 补丁版 AML 已无消费者）
                        If (CondRefOf (\DBFS)) { Store (INP0, \DBFS) }

                        // 逼 NVIDIA 驱动立即重读 fun#2（平台预算快照）
                        Notify (\_SB.NPCF, 0xC0)

                        // 照原厂 DB 事件风格推送一次档位与下限
                        If (CondRefOf (\_SB.PCI0.LPC0.H_EC.FNQS))
                        {
                            \_SB.PCI0.LPC0.H_EC.FNQS (\_SB.PCI0.LPC0.H_EC.ITSM)
                        }
                        If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM)) { \_SB.PCI0.LPC0.H_EC.COMM () }
                        Store (Zero, V1D1)
                    }
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
                // Whitelist = fields the firmware itself writes through its
                // own MIFS WMAA 0xFB00 path (ssdt3) + the power limits.
                // Bit fields (FNHK@0x20.3, TOCP@0x25.0, FWDE@0xE2.2) are
                // written via RefOf so only that bit changes. FAAP shares
                // byte 0xE2 with FWDE and is intentionally NOT exposed
                // (reachable via MIFS 0x1400); writing the byte would be
                // ambiguous.
                // =========================================================
                If (LEqual (Arg1, 8))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                    {
                        If (LEqual (INP0, 0x20))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.FNHK))
                        }
                        ElseIf (LEqual (INP0, 0x25))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.TOCP))
                        }
                        ElseIf (LEqual (INP0, 0x5F))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.FASP))
                        }
                        ElseIf (LEqual (INP0, 0x9A))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.KBNL))
                        }
                        ElseIf (LEqual (INP0, 0xD0))
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
                        ElseIf (LEqual (INP0, 0xE2))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.FWDE))
                        }
                        ElseIf (LEqual (INP0, 0xE3))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.BPWM))
                        }
                        ElseIf (LEqual (INP0, 0xE4))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.ITSM))
                        }
                        ElseIf (LEqual (INP0, 0xE8))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.LEDM))
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
                // Mirrors the Method 8 whitelist.
                // =========================================================
                If (LEqual (Arg1, 9))
                {
                    Store (0xFF, V1D1)
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECRD))
                    {
                        If (LEqual (INP0, 0x20))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.FNHK)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0x25))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.TOCP)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0x5F))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.FASP)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0x9A))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.KBNL)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xD0))
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
                        ElseIf (LEqual (INP0, 0xE2))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.FWDE)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xE3))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.BPWM)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xE4))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.ITSM)), V1D1)
                        }
                        ElseIf (LEqual (INP0, 0xE8))
                        {
                            Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.LEDM)), V1D1)
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
                // Method 10-13 (SetOverrideLock/GetOverrideLock/SetSwSmi/
                // GetSwSmiStatus) and 15 (GetAmdAlibDirect) were REMOVED:
                // dead implementations (no consumer, no persistence, constant
                // return). IDs stay reserved - they fall through to the
                // default 0xFFFFFFFF below.
                // =========================================================

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
                // Method 15: (removed - was a constant-zero placeholder)
                // =========================================================

                // =========================================================
                // Method 16: GetDynamicBoost
                // =========================================================
                // 结合本补丁（_Q81/_Q82 保持原厂、DBFS 跟随 EC 事件 +
                // MSPL/MFPT 改为无条件下限 + FNQS 整方法重写：ITSM 非零→
                // THMD(Zero) 满血档 / 零→THMD(0x13) 办公档）返回数据供工具计算：
                //   V4D1 = DBFS 当前值（0=禁 / 1=启 / 0xFFFFFFFF=不存在）
                //   V4D2 = CSPL（EC 0xF6，CPU PL1，单位 W）
                //   V4D3 = FPPT（EC 0xF7，CPU PL2，单位 W）
                //   V4D4 = 当前 DB 借电上限（瓦，= NPCF.MGAF/2，由 Method 20
                //          设置；表未加载时 0xFF）
                // 最大借用功耗（原厂机制）= V4D3 − V4D4；
                // 补丁版 MSPL/MFPT 下限兜底后，GPU 增压期间 CPU 不会再被钳到
                // 下限以下（V4D4 仅作原厂对照参考）。
                If (LEqual (Arg1, 16))
                {
                    If (CondRefOf (\DBFS)) { Store (\DBFS, V4D1) }
                    Else { Store (0xFFFFFFFF, V4D1) }
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECRD))
                    {
                        Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.CSPL)), V4D2) // CSPL/PL1
                        Store (\_SB.PCI0.LPC0.H_EC.ECRD (RefOf (\_SB.PCI0.LPC0.H_EC.FPPT)), V4D3) // FPPT/PL2
                        If (CondRefOf (\_SB.NPCF.MGAF))
                        {
                            ShiftRight (\_SB.NPCF.MGAF, One, V4D4)
                        }
                        Else
                        {
                            Store (0xFF, V4D4)
                        }
                    }
                    Else
                    {
                        Store (0xFF, V4D2)
                        Store (0xFF, V4D3)
                        Store (0xFF, V4D4)
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

                // =========================================================
                // Method 19: ApplyPowerLimits
                //   INP0 = new CSPL (CPU PL1, W) or Zero = keep current
                //   INP1 = new FPPT (CPU PL2, W) or Zero = keep current
                // Writes the limits then calls COMM() (= MSPL+MFPT+CTCL),
                // which pushes them to the SMU via ALIB(0x0C). This is the
                // sanctioned runtime entry point - the _INI values can be
                // rewritten later by EC events (_Q10) and OEM software
                // (MIFS WMAA 0x1700), so re-apply here instead of hoping
                // the boot-time write sticks.
                // =========================================================
                If (LEqual (Arg1, 0x13))
                {
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.ECWT))
                    {
                        If (LGreater (INP0, Zero))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP0, RefOf (\_SB.PCI0.LPC0.H_EC.CSPL))
                        }
                        If (LGreater (INP1, Zero))
                        {
                            \_SB.PCI0.LPC0.H_EC.ECWT (INP1, RefOf (\_SB.PCI0.LPC0.H_EC.FPPT))
                        }
                    }
                    If (CondRefOf (\_SB.PCI0.LPC0.H_EC.COMM))
                    {
                        \_SB.PCI0.LPC0.H_EC.COMM ()
                    }
                    Store (Zero, V1D1)
                    Return (R1DW)
                }

                // =========================================================
                // Method 20: SetDbBoostCap —— DB 借电上限旋钮（瓦）
                // =========================================================
                // INP0 = 允许 Dynamic Boost 从 CPU 借走的最大功率（0..100W）。
                // 写入 NPCF.MGAF（0.5W 单位）后 Notify(NPCF, 0xC0) 逼驱动
                // 立即重读 fun#2。驱动的 +25W SKU 政策与该上限取小：
                //   100 = 原厂行为（GPU 可借满 25W 到 140W）
                //   10  = 最多借 10W（GPU 顶 ~125W，CPU 少让电）——若驱动
                //         尊重 MAGA 即生效
                //   0   = 等效关闭 DB（GPU 锁 115W）
                // 返回：V1D1 = 0 成功；0xFFFFFFFF = 表未加载；2 = 参数越界
                If (LEqual (Arg1, 20))
                {
                    Store (0xFFFFFFFF, V1D1)
                    If (CondRefOf (\_SB.NPCF.MGAF))
                    {
                        If (LGreater (INP0, 0x64))
                        {
                            Store (0x02, V1D1)
                        }
                        Else
                        {
                            // MGAF 以 0.5W 为单位：瓦 × 2
                            Store (ShiftLeft (INP0, One), Local0)
                            Store (Local0, \_SB.NPCF.MGAF)
                            Notify (\_SB.NPCF, 0xC0)
                            Store (Zero, V1D1)
                        }
                    }
                    Return (R1DW)
                }

                // =========================================================
                // Method 22: SetGpuBudget —— GPU 功耗预算旋钮（瓦）
                // =========================================================
                // INP0 = GPU 功耗预算（35..140W，0.5W 粒度）。
                // 写入 NPCF.TGPF（0.5W 单位）后 Notify(NPCF, 0xC0) 逼驱动
                // 立即重读 fun#2 —— NVIDIA 驱动按 TGPA 作为其功耗目标上限。
                // 仅作用于性能档（ITSM==1 时 fun#2 读 TGPF）；办公档
                // （ITSM==0）保持原厂 60W 预算。
                // 返回：V1D1 = 0 成功；0xFFFFFFFF = 表未加载；2 = 参数越界
                If (LEqual (Arg1, 22))
                {
                    Store (0xFFFFFFFF, V1D1)
                    If (CondRefOf (\_SB.NPCF.TGPF))
                    {
                        If (LGreater (INP0, 0x8C))
                        {
                            Store (0x02, V1D1)
                        }
                        Else
                        {
                            // TGPF 以 0.5W 为单位：瓦 × 2
                            Store (ShiftLeft (INP0, One), Local0)
                            Store (Local0, \_SB.NPCF.TGPF)
                            Notify (\_SB.NPCF, 0xC0)
                            Store (Zero, V1D1)
                        }
                    }
                    Return (R1DW)
                }

                // =========================================================
                // Method 21 (0x15): GetNpcfTrace
                //   Returns R4DW = {CUSL, CUCT, T6C, T6V}
                //   CUSL/CUCT: last values the NVIDIA driver sent via NPCF
                //   fun#5 (INC5=3/4). T6C/T6V: invocation count and last
                //   NCHP (CPU watts) the driver sent via NPCF fun#6 INC6=1
                //   (the CPUC IO-port set path, IOBS-gated dead in this
                //   AML) - instrumented by ssdt4-npcf-trace patches.
                //   Nonzero after a Dynamic Boost clamp = the driver drives
                //   CPU power through that AML entry; all-zero = the clamp
                //   bypasses ACPI entirely (SMU-internal).
                // =========================================================
                If (LEqual (Arg1, 0x15))
                {
                    If (CondRefOf (\_SB.NPCF.CUSL)) { Store (\_SB.NPCF.CUSL, V4D1) }
                    If (CondRefOf (\_SB.NPCF.CUCT)) { Store (\_SB.NPCF.CUCT, V4D2) }
                    If (CondRefOf (\_SB.NPCF.T6C)) { Store (\_SB.NPCF.T6C, V4D3) }
                    If (CondRefOf (\_SB.NPCF.T6V)) { Store (\_SB.NPCF.T6V, V4D4) }
                    Return (R4DW)
                }

                // Default Fallback for Unknown Method IDs
                Store (0xFFFFFFFF, V1D1)
                Return (R1DW)
            }
        }
    }
}
