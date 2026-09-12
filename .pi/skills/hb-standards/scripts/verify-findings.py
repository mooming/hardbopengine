#!/usr/bin/env python3
"""Reject review findings whose citations do not resolve.

A model asked to audit files it never opened will invent defects rather than
report none, and the invention is invisible in prose but cheap to detect: the
line it "quoted" is past the end of the file, or the file does not exist, or the
evidence is not on that line. This script turns that observation into an exit
code, so a reviewer that fabricates fails a gate instead of reaching a human.

Usage: verify-findings.py <findings.json> [--root DIR] [--min-evidence 8]

findings.json is {"findings":[{"file","line","evidence",...}]}. Exit 0 = every
citation resolved; 1 = at least one fabricated/unresolvable finding; 2 = bad usage.
"""

import json
import os
import sys
import re


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 2
    path = sys.argv[1]
    root = os.getcwd()
    if "--root" in sys.argv:
        root = sys.argv[sys.argv.index("--root") + 1]
    if not os.path.isfile(path):
        print("verify-findings: no such file: %s" % path)
        return 2
    try:
        data = json.load(open(path))
    except ValueError as exc:
        print("verify-findings: %s is not valid JSON: %s" % (path, exc))
        return 1

    findings = data.get("findings", [])
    if findings is None:
        findings = []
    if not isinstance(findings, list):
        print("verify-findings: 'findings' must be a list")
        return 1

    bad = []
    for idx, f in enumerate(findings):
        if not isinstance(f, dict):
            bad.append((idx, "<not an object>", "entry is not an object"))
            continue
        rel = str(f.get("file", ""))
        line = f.get("line")
        evidence = str(f.get("evidence", "")).strip()
        full = os.path.join(root, rel)
        if not rel or not os.path.isfile(full):
            bad.append((idx, rel or "<empty>", "file does not exist"))
            continue
        try:
            total = sum(1 for _ in open(full, errors="replace"))
        except OSError as exc:
            bad.append((idx, rel, "unreadable: %s" % exc))
            continue
        if not isinstance(line, int) or line < 1 or line > total:
            bad.append((idx, "%s:%s" % (rel, line),
                        "line outside file (file has %d lines)" % total))
            continue
        if len(evidence) < 8:
            bad.append((idx, "%s:%d" % (rel, line),
                        "evidence too short to check (%r)" % evidence))
            continue

        lines = open(full, errors="replace").read().split("\n")
        window = "\n".join(lines[max(0, line - 3):line + 2])
        # Whitespace-insensitive containment: reformatting must not invalidate a
        # real citation, but invented text still cannot appear by accident.
        needle = re.sub(r"\s+", " ", evidence).strip()
        # Compare against whole lines, not a bare substring of one line: a real
        # citation quotes at least one full statement, whereas a fabricated one
        # is assembled from names, so containment in the surrounding window is
        # the honest test.
        hay = re.sub(r"\s+", " ", window)
        if needle not in hay:
            bad.append((idx, "%s:%d" % (rel, line),
                        "quoted evidence is not in the file at that line"))

    if bad:
        print("verify-findings: %d/%d findings DO NOT resolve in %s"
              % (len(bad), len(findings), os.path.relpath(path, root)))
        for idx, where, why in bad:
            print("   [%d] %-46s %s" % (idx, where[:46], why))
        print("Every finding must cite a real line whose text matches 'evidence'."
              " Open the files, re-read them, and rewrite the list; drop anything"
              " you cannot quote verbatim. Reporting zero findings is a valid"
              " answer; an invented one is not.")
        return 1

    print("verify-findings: all %d citations resolve in %s"
          % (len(findings), os.path.relpath(path, root)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
