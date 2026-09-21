#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 mofcomp 编出的二进制 MOF（BMF）转成 ASL Buffer 声明。

SsdtUnlockDB.asl 通过 Include("BmfData.asl") 引入 WQBA（WMI 二进制 MOF
数据），本脚本生成该文件。取代了旧 build.bat 里的内联 python -c 语句，
便于 CMake 以参数化 custom command 调用。

Usage:
    python tools/bmf_to_asl.py <input.bmf> <output.asl>
"""

import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "rb") as f:
        data = f.read()
    text = [
        "// 自动生成（tools/bmf_to_asl.py）。勿手改。",
        "Name (WQBA, Buffer ()",
        "{",
        "    " + ", ".join("0x{:02X}".format(b) for b in data),
        "})",
    ]
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(text) + "\n")
    print(f"[+] {dst} ({len(data)} bytes of BMF)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
