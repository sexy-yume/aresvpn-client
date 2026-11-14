#!/usr/bin/env python3
"""AresVPN Client - does every upstream function this fork switched off still open with its switch?

Audit 2026-11-14 H-14, #D177 rule 3. The fork disables a few upstream functions with ONE early
statement before upstream's body (`return;`), so the file merges clean. That leaves two silent
failures: a cherry-pick into the body applies and does nothing, and a conflict resolved by taking
upstream's side drops the statement and re-enables what the fork removed. The list of such
functions is `disabled-functions.txt`, beside this file; this reads it and checks each entry.

Exit 0 when every entry holds, 1 when one does not (each failure is named), 2 when the list itself
cannot be read - an empty or unreadable list is a failure, not a clean bill (#L041).

Usage:
    python disabled-functions.py [--root <fork root>]
    python disabled-functions.py --selftest
"""

import argparse
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LIST = os.path.join(HERE, "disabled-functions.txt")


def read_list(path):
    entries = []
    with open(path, encoding="utf-8") as f:
        for number, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) != 3 or not all(parts):
                raise ValueError("%s:%d: expected '<path> | <definition> | <statement>'" % (path, number))
            entries.append(tuple(parts))
    return entries


def first_statement(lines, start):
    """The first line after the definition's opening brace that is code, not a comment."""
    i = start + 1
    seen_brace = lines[start].rstrip().endswith("{")
    while i < len(lines):
        s = lines[i].strip()
        i += 1
        if not seen_brace:
            if s.startswith("{"):
                seen_brace = True
                s = s[1:].strip()
                if not s:
                    continue
            else:
                return None
        if not s or s.startswith("//") or s.startswith("Q_UNUSED("):
            continue
        return s
    return None


def check(root, entries):
    failures = []
    for path, definition, statement in entries:
        full = os.path.join(root, path)
        if not os.path.isfile(full):
            failures.append("%s: no such file (listed for %s)" % (path, definition))
            continue
        with open(full, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        hits = [n for n, l in enumerate(lines) if l.strip() == definition]
        if len(hits) != 1:
            failures.append("%s: the definition '%s' occurs %d time(s), expected exactly once - the "
                            "function moved or was renamed; update the list" % (path, definition, len(hits)))
            continue
        got = first_statement(lines, hits[0])
        if got is None or not got.startswith(statement):
            failures.append("%s: %s no longer opens with '%s' (first statement: %r) - a merge dropped "
                            "the fork's switch, and upstream's body runs again" % (path, definition, statement, got))
    return failures


def run(root, list_path=LIST):
    try:
        entries = read_list(list_path)
    except (OSError, ValueError) as e:
        print("DISABLED-FUNCTIONS BROKEN: %s" % e)
        return 2
    if not entries:
        print("DISABLED-FUNCTIONS BROKEN: %s lists nothing" % list_path)
        return 2
    failures = check(root, entries)
    for f in failures:
        print("DISABLED-FUNCTIONS FAIL %s" % f)
    print("DISABLED-FUNCTIONS %d entr(y/ies), %d failed" % (len(entries), len(failures)))
    return 1 if failures else 0


def run_selftest(_root):
    """Plants: a dropped switch, a renamed function, an empty list. Each must fail; the clean one pass."""
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "a.cpp")
        lst = os.path.join(tmp, "list.txt")

        def write(code, listing):
            with open(src, "w", encoding="utf-8") as f:
                f.write(code)
            with open(lst, "w", encoding="utf-8") as f:
                f.write(listing)

        listing = "a.cpp | void A::f() | return;\n"
        cases = [
            ("clean", "void A::f()\n{\n    // the fork's reason\n    return;\n    upstream();\n}\n", listing, 0),
            ("switch dropped", "void A::f()\n{\n    upstream();\n}\n", listing, 1),
            ("renamed", "void A::g()\n{\n    return;\n}\n", listing, 1),
            ("empty list", "void A::f()\n{\n    return;\n}\n", "# nothing\n", 2),
        ]
        for name, code, lst_text, want in cases:
            write(code, lst_text)
            got = run(tmp, lst)
            good = got == want
            ok = ok and good
            print("selftest %-15s want %d got %d %s" % (name, want, got, "ok" if good else "WRONG"))
    print("selftest: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.normpath(os.path.join(HERE, "..", "..")))
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    return run_selftest(args.root) if args.selftest else run(args.root)


if __name__ == "__main__":
    sys.exit(main())
