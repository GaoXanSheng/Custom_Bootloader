            Name (CUSL, Zero)
            Name (T5C, Zero)
            Name (T5V, Zero)
            Name (T6C, Zero)
            Name (T6V, Zero)
            Name (DBOF, Zero)   // 0 = DB on (stock: GPU 140W via borrow), 1 = DB off (GPU base TGP 115W, CPU keeps full power); written by DBUL WMTF Method 1, read by fun#2's DBAC
            Name (MGAF, 0xC8)   // DB borrow cap in 0.5W units (0xC8 = stock 100W max allowance); written by DBUL WMTF Method 20 (watts), read by fun#2's MAGA
            Name (TGPF, 0x0118) // GPU power budget in 0.5W units (0x0118 = stock 140W, performance-mode branches only; office ITSM==0 keeps stock 60W); written by DBUL WMTF Method 22 (watts), read by fun#2's TGPA
