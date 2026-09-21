#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
在构建时生成版本工件。

每次构建（CMake 版本阶段运行本脚本）都会自动刷新：
  1. version.txt            - VERSION_BUILD=0x<YYYYMMDD>, VERSION_STRING=<major>.<YYYYMMDD>_<HHMMSS>
     （形如 "1.0.1" 的主版本前缀从上一次的 version.txt 保留）
  2. build/Version.h        - BUILD_VERSION / BUILD_VERSION_STR 宏（C）

Usage:
    python tools/generate_version.py [--out-dir DIR]
"""

import argparse
import datetime
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION_FILE = os.path.join(REPO, "version.txt")
DEFAULT_OUT_DIR = os.path.join(REPO, "build")


def main():
    parser = argparse.ArgumentParser(description="Generate version artifacts")
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR,
                        help="output dir for Version.h (default: <repo>/build)")
    args = parser.parse_args()

    out_dir = os.path.abspath(args.out_dir)
    output_header = os.path.join(out_dir, "Version.h")

    now = datetime.datetime.now()
    date_str = now.strftime("%Y%m%d")
    time_str = now.strftime("%H%M%S")
    version_build = int(date_str, 16)
    version_epoch = int(now.astimezone(datetime.timezone.utc).timestamp())

    major = "1.0.0"
    if os.path.exists(VERSION_FILE):
        with open(VERSION_FILE, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line.startswith("VERSION_STRING="):
                    val = line.split("=", 1)[1].strip().strip('"')
                    parts = val.split(".")
                    if len(parts) >= 3:
                        major = ".".join(parts[:3])
                        if "_" in major:
                            major = major.split("_", 1)[0]
                    break

    version_str = f"{major}.{date_str}_{time_str}"

    with open(VERSION_FILE, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Auto-generated at build time by tools/generate_version.py\n")
        f.write(f"VERSION_STRING={version_str}\n")
        f.write(f"VERSION_BUILD=0x{version_build:08X}\n")
        f.write(f"VERSION_EPOCH={version_epoch}\n")

    header = [
        "// 自动生成：tools/generate_version.py —— 勿手改。",
        "#ifndef VERSION_H",
        "#define VERSION_H",
        "",
        f"#define BUILD_VERSION 0x{version_build:08X}ULL",
        f'#define BUILD_VERSION_STR L"{version_str}"',
        "",
        "#endif // VERSION_H",
    ]
    os.makedirs(os.path.dirname(output_header), exist_ok=True)
    with open(output_header, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(header) + "\n")

    print(f"[+] version.txt updated: {version_str} (0x{version_build:08X})")
    print(f"[+] Generated: {output_header}")


if __name__ == "__main__":
    main()
