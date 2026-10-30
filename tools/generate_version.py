#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import datetime
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION_FILE = os.path.join(REPO, "version.txt")


def main():
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
        f.write(f"VERSION_STRING={version_str}\n")
        f.write(f"VERSION_BUILD=0x{version_build:08X}\n")
        f.write(f"VERSION_EPOCH={version_epoch}\n")

    print(f"[+] version.txt updated: {version_str} (0x{version_build:08X})")


if __name__ == "__main__":
    main()
