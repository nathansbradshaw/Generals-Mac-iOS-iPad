#!/usr/bin/env python3
"""3-way merge a GeneralsOnline hook file and guard their changes.

Produces a version of <relpath> where every line that differs from our
current tree is wrapped in #if defined(SAGE_GENERALS_ONLINE) guards, so the
OFF build preprocesses to exactly today's code and the ON build matches the
GeneralsOnline fork's behavior (plus our port changes).

Pipeline per file:
  1. base   = git show <base>:<relpath>            (common ancestor)
  2. ours   = working-tree <relpath>
  3. theirs = <theirs-repo>/<relpath>, with whitespace-only churn undone
              (where a region is identical to base ignoring whitespace, keep
              base's formatting so the guard pass doesn't see reformat noise)
  4. merged = git merge-file(ours, base, theirs)
       - conflicts: writes <relpath>.merged with markers; resolve by hand,
         then re-run with --guard-only <resolved-file>
       - clean: continues to step 5
  5. guarded = interleave(ours, merged) with SAGE_GENERALS_ONLINE guards,
       written back to <relpath>

Usage:
  merge_guard.py <relpath> [--base SHA] [--theirs-repo DIR]
  merge_guard.py <relpath> --guard-only <resolved-merged-file>
"""

import argparse
import difflib
import subprocess
import sys
import tempfile
from pathlib import Path

DEFAULT_BASE = "bf8d5be0294fc71b601444ff9446c12f4720022f"
DEFAULT_THEIRS = "references/generalsonline-gameclient"
GUARD = "SAGE_GENERALS_ONLINE"
# adjacent guard hunks separated by <= this many equal lines get coalesced
COALESCE_GAP = 2


def read_lines(text: str) -> list[str]:
    # their fork sprinkles UTF-8 BOMs; a BOM anywhere but byte 0 is a compile error
    return text.replace("\r\n", "\n").replace("﻿", "").split("\n")


def git_show(base: str, relpath: str) -> str:
    return subprocess.run(
        ["git", "show", f"{base}:{relpath}"],
        check=True, capture_output=True, text=True,
    ).stdout


import re

_NULL_RE = re.compile(r"\bNULL\b")


def _canon(line: str) -> str:
    # NULL/nullptr churn: their fork writes NULL where ours has nullptr
    return "".join(_NULL_RE.sub("nullptr", line).split())


def normalize_ws(base_lines: list[str], theirs_lines: list[str]) -> list[str]:
    """Return theirs, but where content equals base ignoring whitespace
    (and NULL-vs-nullptr churn), use base's original lines."""
    a = [_canon(l) for l in base_lines]
    b = [_canon(l) for l in theirs_lines]
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    out: list[str] = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            out.extend(base_lines[i1:i2])
        else:
            out.extend(theirs_lines[j1:j2])
    return out


def resolve_go_macro(text: str) -> str:
    """Their fork guards some changes with #if defined(GENERALS_ONLINE), which
    is always defined when SAGE_GENERALS_ONLINE is ON. Resolve those guards
    now so the interleave step doesn't split their #if/#else across hunks."""
    r = subprocess.run(["unifdef", "-DGENERALS_ONLINE"],
                       input=text, capture_output=True, text=True)
    if r.returncode > 1:
        sys.exit(f"unifdef failed: {r.stderr}")
    return r.stdout


def merge_file(ours: str, base: str, theirs: str) -> tuple[str, int]:
    with tempfile.TemporaryDirectory() as td:
        p_ours = Path(td, "ours"); p_ours.write_text(ours)
        p_base = Path(td, "base"); p_base.write_text(base)
        p_theirs = Path(td, "theirs"); p_theirs.write_text(theirs)
        r = subprocess.run(
            ["git", "merge-file", "-p",
             "-L", "OURS", "-L", "BASE", "-L", "THEIRS",
             str(p_ours), str(p_base), str(p_theirs)],
            capture_output=True, text=True,
        )
        if r.returncode < 0:
            sys.exit(f"git merge-file died: {r.stderr}")
        return r.stdout, r.returncode  # returncode = number of conflicts


def coalesce(opcodes):
    """Fold [non-equal, tiny-equal, non-equal] runs into one replace hunk
    (the equal gap lands in both guard branches) so we emit fewer blocks."""
    folded = []
    for op in opcodes:
        tag, i1, i2, j1, j2 = op
        if (
            tag != "equal" and len(folded) >= 2
            and folded[-1][0] == "equal"
            and (folded[-1][2] - folded[-1][1]) <= COALESCE_GAP
            and folded[-2][0] != "equal"
        ):
            folded.pop()
            _, pi1, _, pj1, _ = folded.pop()
            folded.append(("replace", pi1, i2, pj1, j2))
        else:
            folded.append(op)
    return folded


def comment_states(lines: list[str]) -> list[bool]:
    """states[k] = True if line k starts inside a /* block comment.
    Has len(lines)+1 entries (last = state after final line). Tracks string
    literals so '/*' inside quotes doesn't count; ignores raw strings."""
    states = [False]
    in_comment = False
    for line in lines:
        i, n = 0, len(line)
        in_string = False
        quote = ""
        while i < n:
            c = line[i]
            if in_comment:
                if c == "*" and i + 1 < n and line[i + 1] == "/":
                    in_comment = False
                    i += 2
                    continue
            elif in_string:
                if c == "\\":
                    i += 2
                    continue
                if c == quote:
                    in_string = False
            else:
                if c in ('"', "'"):
                    in_string = True
                    quote = c
                elif c == "/" and i + 1 < n:
                    if line[i + 1] == "/":
                        break  # rest of line is a // comment
                    if line[i + 1] == "*":
                        in_comment = True
                        i += 2
                        continue
            i += 1
        states.append(in_comment)
    return states


def pp_self_contained(lines: list[str]) -> bool:
    """True if the hunk's conditional directives are self-contained: nesting
    never dips below the hunk's own level (no #else/#elif/#endif belonging to
    an enclosing #if) and ends balanced."""
    depth = 0
    for l in lines:
        s = l.lstrip()
        if s.startswith(("#if", "# if")):
            depth += 1
        elif s.startswith(("#endif", "# endif")):
            depth -= 1
            if depth < 0:
                return False
        elif s.startswith(("#else", "# else", "#elif", "# elif")):
            if depth == 0:
                return False
    return depth == 0


def fix_hunk_boundaries(ops, ours_lines, merged_lines):
    """Guard directives can't sit inside a /* */ span (comment stripping
    precedes preprocessing), and a guarded hunk must not contain conditional
    directives that belong to an enclosing #if. Fuse offending hunks with
    their neighbours until every hunk is safe to wrap."""
    ost = comment_states(ours_lines)
    mst = comment_states(merged_lines)

    def clean(op):
        tag, i1, i2, j1, j2 = op
        if tag == "equal":
            return True
        if ost[i1] or ost[i2] or mst[j1] or mst[j2]:
            return False
        return (pp_self_contained(ours_lines[i1:i2])
                and pp_self_contained(merged_lines[j1:j2]))

    ops = list(ops)
    changed = True
    while changed:
        changed = False
        for k, op in enumerate(ops):
            if clean(op):
                continue
            if k + 1 < len(ops):
                nxt = ops.pop(k + 1)
                ops[k] = ("replace", op[1], nxt[2], op[3], nxt[4])
            elif k > 0:
                prev = ops.pop(k - 1)
                ops[k - 1] = ("replace", prev[1], op[2], prev[3], op[4])
            else:
                raise SystemExit("comment span covers whole file; port by hand")
            changed = True
            break
    return ops


def pp_balance(lines: list[str]) -> int:
    """Net #if-nesting delta of a hunk; nonzero means wrapping it in a guard
    would mis-nest the guard's own #else/#endif."""
    n = 0
    for l in lines:
        s = l.lstrip()
        if s.startswith(("#if", "# if")):
            n += 1
        elif s.startswith(("#endif", "# endif")):
            n -= 1
    return n


def guard_interleave(ours_lines: list[str], merged_lines: list[str]) -> tuple[list[str], int]:
    sm = difflib.SequenceMatcher(a=ours_lines, b=merged_lines, autojunk=False)
    ops = fix_hunk_boundaries(coalesce(sm.get_opcodes()), ours_lines, merged_lines)
    out: list[str] = []
    nguards = 0
    for tag, i1, i2, j1, j2 in ops:
        if tag == "equal":
            out.extend(ours_lines[i1:i2])
            continue
        ours_hunk = ours_lines[i1:i2]
        theirs_hunk = merged_lines[j1:j2]
        if ours_hunk == theirs_hunk:  # after coalescing this can't differ, but be safe
            out.extend(ours_hunk)
            continue
        # blank-only churn: keep ours
        if all(not l.strip() for l in ours_hunk) and all(not l.strip() for l in theirs_hunk):
            out.extend(ours_hunk)
            continue
        nguards += 1
        for name, hunk in (("ours", ours_hunk), ("theirs", theirs_hunk)):
            if pp_balance(hunk):
                print(f"WARNING: unbalanced #if/#endif in {name} hunk near "
                      f"output line {len(out) + 1} — fix guard nesting by hand",
                      file=sys.stderr)
        if theirs_hunk and ours_hunk:
            out.append(f"#if defined({GUARD})")
            out.extend(theirs_hunk)
            out.append("#else")
            out.extend(ours_hunk)
            out.append("#endif")
        elif theirs_hunk:
            out.append(f"#if defined({GUARD})")
            out.extend(theirs_hunk)
            out.append(f"#endif // {GUARD}")
        else:
            out.append(f"#if !defined({GUARD})")
            out.extend(ours_hunk)
            out.append(f"#endif // !{GUARD}")
    return out, nguards


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("relpath")
    ap.add_argument("--base", default=DEFAULT_BASE)
    ap.add_argument("--theirs-repo", default=DEFAULT_THEIRS)
    ap.add_argument("--guard-only", metavar="RESOLVED",
                    help="skip merge; guard-interleave ours with this resolved merged file")
    ap.add_argument("--dry-run", action="store_true",
                    help="write to <relpath>.guarded instead of in place")
    args = ap.parse_args()

    ours_path = Path(args.relpath)
    ours = ours_path.read_text()
    ours_lines = read_lines(ours)

    if args.guard_only:
        merged_lines = read_lines(resolve_go_macro(Path(args.guard_only).read_text()))
    else:
        base = git_show(args.base, args.relpath)
        theirs_path = Path(args.theirs_repo, args.relpath)
        theirs_lines = normalize_ws(read_lines(base), read_lines(theirs_path.read_text()))
        merged, conflicts = merge_file(ours, base, "\n".join(theirs_lines))
        if conflicts:
            out = ours_path.with_suffix(ours_path.suffix + ".merged")
            out.write_text(merged)
            print(f"{conflicts} conflict(s) — resolve {out} then re-run with "
                  f"--guard-only {out}")
            sys.exit(2)
        merged_lines = read_lines(resolve_go_macro(merged))

    guarded, nguards = guard_interleave(ours_lines, merged_lines)
    dest = ours_path.with_suffix(ours_path.suffix + ".guarded") if args.dry_run else ours_path
    dest.write_text("\n".join(guarded))
    print(f"{dest}: {nguards} guard block(s) written")


if __name__ == "__main__":
    main()
