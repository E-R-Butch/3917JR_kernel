#!/usr/bin/env python3
"""Strip the "CONFIG_SYSVIPC must be n" requirement from a VINTF framework
compatibility matrix.

Why: Android's Build.isBuildConsistent() -> VintfObject.verifyWithoutAvb()
compares the kernel config in /proc/config.gz against the matrix under
/system/etc/vintf/. Our kernel sets CONFIG_SYSVIPC=y (required by Droidspaces
for SysV IPC inside containers), which violates the matrix and makes
ActivityTaskManagerService show the "your device has an internal problem"
dialog on every boot.

Usage:
    patch_vintf_matrix.py <matrix.xml> <output.xml>
    patch_vintf_matrix.py --all <src-dir> <out-dir>

Exit codes: 0 patched, 3 nothing to remove (matrix changed -> re-check),
2 bad arguments.
"""
import re
import sys
from pathlib import Path

BLOCK = re.compile(
    r'[ \t]*<config>\s*<key>CONFIG_SYSVIPC</key>\s*'
    r'<value type="tristate">n</value>\s*</config>\n'
)


def patch(text: str):
    new, count = BLOCK.subn("", text)
    return new, count


def main(argv):
    if len(argv) == 4 and argv[1] == "--all":
        src, out = Path(argv[2]), Path(argv[3])
        out.mkdir(parents=True, exist_ok=True)
        total = 0
        for f in sorted(src.glob("compatibility_matrix.*.xml")):
            text = f.read_text(encoding="utf-8")
            new, count = patch(text)
            if count:
                (out / f.name).write_text(new, encoding="utf-8")
                print(f"{f.name}: removed {count}")
                total += count
            else:
                print(f"{f.name}: no SYSVIPC requirement, skipped")
        return 0 if total else 3

    if len(argv) == 3:
        src, dst = Path(argv[1]), Path(argv[2])
        new, count = patch(src.read_text(encoding="utf-8"))
        if not count:
            print("no CONFIG_SYSVIPC requirement found", file=sys.stderr)
            return 3
        dst.write_text(new, encoding="utf-8")
        print(f"removed {count} block(s) -> {dst}")
        return 0

    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
