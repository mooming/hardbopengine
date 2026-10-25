#!/usr/bin/env python3
"""prove_feature_macros.py — compile the code that no build configuration compiles.

  prove_feature_macros.py [--db PATH] [--all] [file.cpp ...]

Engine/Config/BuildConfig.h hardwires the feature macros to 0, so `check.sh`'s three
configurations never compile a single line inside `#if PROFILE_ENABLED` or
`#if MEMORY_VERIFICATION_ENABLED`. That is where defects hid: a profiling block can be
added, renamed against a member it no longer matches, and stay green forever, because the
one configuration that would reject it is nobody's build.

This layer compiles each file with both macros forced to 1, with `-fsyntax-only`, using the
compile database's own flags so the probe measures what the build measures.

The macros are forced by a `-include` prelude that includes BuildConfig.h first, then
`#undef`s and re-`#define`s them. A `-D` on the command line cannot do this: the header's
own `#define` arrives later and wins. A shadow copy of the header on an earlier `-I` cannot
do it either: `Engine/Config/ConfigParam.h` and `Engine/Config/EngineConfig.h` include
`"BuildConfig.h"` by bare name, which resolves in the includer's own directory, so the
shadow and the real header would both be included and every macro would be redefined.

Exit status: 0 = every file compiles with the macros on, 1 = at least one does not,
3 = the layer could not run (no database, no entry for the file, or the header no longer
looks like the file this probe exists to override). A rule that could not run never
reports a pass.
"""

import argparse
import json
import os
import shlex
import subprocess
import sys
import tempfile

FEATURE_MACROS = ("PROFILE_ENABLED", "MEMORY_VERIFICATION_ENABLED",
                  "MEMORY_INVESTIGATION_ENABLED", "MEMORY_DANGLING_POINTER_CHECK_ENABLED",
                  "MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED")
HEADER = "Engine/Config/BuildConfig.h"
DATABASES = ("cmake-build-debug/compile_commands.json", "cmake-build-release/compile_commands.json",
             "build/compile_commands.json")
MAX_ERRORS_PER_FILE = 3


def find_database(explicit):
    candidates = [explicit] if explicit else [os.path.join(os.getcwd(), d) for d in DATABASES]
    for path in candidates:
        if path and os.path.isfile(path):
            return path
    return None


def load_database(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def entry_for(entries, source):
    wanted = os.path.normpath(os.path.abspath(source))
    for entry in entries:
        if os.path.normpath(entry["file"]) == wanted:
            return entry
    return None


def prelude_text(header_path):
    lines = ['#include "%s"' % header_path]
    for macro in FEATURE_MACROS:
        lines += ["#ifdef %s" % macro, "#undef %s" % macro, "#endif", "#define %s 1" % macro]
    return "\n".join(lines) + "\n"


def header_is_the_expected_shape(root):
    """The probe overrides hardwired zeros; if they stopped being zeros, its premise is gone."""
    path = os.path.join(root, HEADER)
    if not os.path.isfile(path):
        return False, "%s not found" % HEADER
    with open(path, encoding="utf-8") as handle:
        body = handle.read()
    for macro in FEATURE_MACROS:
        if "#define %s 0" % macro not in body:
            return False, "%s does not define %s as 0, so forcing it is not this script's job" % (HEADER, macro)
    return True, ""


def compile_command(entry, prelude):
    args = shlex.split(entry["command"])
    out, skip = [], False
    for arg in args:
        if skip:
            skip = False
            continue
        if arg == "-o":
            skip = True
            continue
        if arg in ("-c", "-fcolor-diagnostics"):
            continue
        out.append(arg)
    return out + ["-fsyntax-only", "-include", prelude]


def errors_of(stderr):
    return [line for line in stderr.splitlines() if " error: " in line][:MAX_ERRORS_PER_FILE]


def main():
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("--db", help="compile database to read flags from")
    parser.add_argument("--all", action="store_true", help="every translation unit in the database")
    parser.add_argument("files", nargs="*", help="cpp files to probe (the run's scope)")
    ns = parser.parse_args()

    root = os.getcwd()
    shape_ok, shape_why = header_is_the_expected_shape(root)
    if not shape_ok:
        print("feature macros: [NONE] %s" % shape_why)
        return 3

    db_path = find_database(ns.db)
    if not db_path:
        print("feature macros: [NONE] no compile_commands.json — run generate_cmake_files.sh, "
              "or the flags this probe needs are unknown")
        return 3
    entries = load_database(db_path)

    targets = [os.path.normpath(e["file"]) for e in entries] if ns.all else [f for f in ns.files if f]
    if not targets:
        print("feature macros: [NONE] no file in scope, so nothing was compiled and this says nothing")
        return 3

    checked = failed = missing = 0
    with tempfile.TemporaryDirectory(prefix="hbe-feature-macros-") as tmp:
        prelude = os.path.join(tmp, "hbe_feature_macros.h")
        with open(prelude, "w", encoding="utf-8") as handle:
            handle.write(prelude_text(HEADER))

        for source in targets:
            entry = entry_for(entries, source)
            if entry is None:
                missing += 1
                print("  [SKIP] %s — no entry in %s (a new file, or configured out of the build)"
                      % (source, os.path.relpath(db_path, root)))
                continue
            checked += 1
            run = subprocess.run(compile_command(entry, prelude), cwd=entry.get("directory", root),
                                 capture_output=True, text=True)
            if run.returncode == 0:
                continue
            failed += 1
            print("  [FAIL] %s" % source)
            for line in errors_of(run.stderr) or ["(no error line; exit %d)" % run.returncode]:
                print("         %s" % line.strip())

    summary = ("feature macros: %d file(s) compiled with %s, %d clean, %d failing, %d with no database entry"
               % (checked, " and ".join("%s=1" % m for m in FEATURE_MACROS), checked - failed, failed, missing))
    print(summary)
    if not checked:
        print("         [NONE] every file was skipped, so this layer measured nothing")
        return 3
    if failed:
        print("         [FAIL] a path no build configuration compiles is broken — the three-configuration "
              "build gate cannot see this code")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
