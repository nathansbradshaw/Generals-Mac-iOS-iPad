#!/usr/bin/env python3
"""Compare GENERALSX_CRC_TRACE files from two native clients, without a UI."""
# GeneralsX @build Codex 08/10/2026 Compare exact serialization, not just CRCs.
import argparse
from itertools import zip_longest
from pathlib import Path


def compare(left, right):
    a = left.read_text().splitlines()
    b = right.read_text().splitlines()
    label = "before first object"
    differences = 0
    first = None
    for line, (x, y) in enumerate(zip_longest(a, b), 1):
        if x and x.startswith("LABEL "):
            label = x
        if x != y:
            differences += 1
            if first is None:
                first = (line, label, x, y)
    if first:
        line, label, x, y = first
        print(f"DIFFER {left.name}: {differences} lines; first line {line}, {label}")
        print(f"  left:  {x}")
        print(f"  right: {y}")
        return False
    print(f"MATCH {left.name}: {len(a)} lines")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    args = parser.parse_args()
    if args.left.is_file() and args.right.is_file():
        return 0 if compare(args.left, args.right) else 1
    if not args.left.is_dir() or not args.right.is_dir():
        parser.error("Provide two files or two directories of matching trace filenames")
    left = {p.name: p for p in args.left.glob("*-??????.trace")}
    right = {p.name: p for p in args.right.glob("*-??????.trace")}
    if not left or not right:
        parser.error("Both directories must contain CRC trace files")
    ok = True
    for name in sorted(left.keys() | right.keys()):
        if name not in left or name not in right:
            print(f"MISSING {name}: {'left' if name not in left else 'right'}")
            ok = False
        elif not compare(left[name], right[name]):
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
