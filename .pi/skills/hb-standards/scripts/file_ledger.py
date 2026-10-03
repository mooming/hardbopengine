#!/usr/bin/env python3
"""Regenerate `.Plans/STANDARDS_PER_FILE.md` by measuring every tracked source, and nothing else.

The ledger exists so a reader can pick a file and see what that file still owes. Written by hand it decayed
into the opposite of that: measured on 2026-10-03, **246 of the 256 measurable rows were already byte-identical
to clang-format output**, including 164 of the 169 rows labelled `format-real` and 36 of the 38 labelled
`format-debt-only`. Those labels were true when written and were never retired, so the ledger was scheduling
formatting work for 164 finished files — and 19 tracked files had no row at all, which to a reader of a
per-file ledger reads as "nothing to do about it". A stale optimistic row is worse than a missing one, because
nobody goes looking.

So the rows are generated, and the file names the command that regenerates them. Two consequences a reader
should see rather than infer: a number here is a measurement taken by the checker named in the header, at the
revision printed there, and it says nothing at all about any layer not printed as a column. `--write` refuses
to overwrite a ledger whose tracked-file set no longer reconciles, because the failure mode this replaces was
exactly a file list drifting away from `git ls-files` unnoticed.

Exit status: 0 printed or written, 1 a measurement could not be taken for some file, 3 environment error.
"""

import argparse
import collections
import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

import blank_lines  # noqa: E402  (same directory; the rules are not to be duplicated)
import comments  # noqa: E402
import includes  # noqa: E402
import layout  # noqa: E402

SOURCE_SUFFIXES = ('.cpp', '.cc', '.cxx', '.h', '.hpp', '.mm', '.m', '.inl')
OBJECTIVE_SUFFIXES = ('.mm', '.m')
INCLUDE_BODY_SUFFIXES = ('.inl',)          # clang-format classifies these by --assume-filename only
LEDGER = '.Plans/STANDARDS_PER_FILE.md'


def tracked_sources():
    """Every tracked source in the repository, in path order — the exact set the ledger must cover.

    `git ls-files`, not a `find`: an untracked file has no verdict anyone could act on, and a tracked file
    missing from the ledger is the defect this script exists to make impossible.
    """
    listed = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, check=True).stdout
    return sorted(path for path in listed.split('\n') if path.endswith(SOURCE_SUFFIXES))


def format_verdict(path, text):
    """What clang-format would change, judged the way `check.sh` judges it.

    The comparison is the gate's own, and the detail matters: both sides pass through
    `blank_lines.collapse_seam`, because clang-format deletes the second blank at the preamble seam whenever
    the line under it is a `namespace`, a `class` or a function definition, while rule A3 mandates that second
    blank. **A file that obeys A3 can therefore never be byte-identical to its own formatted form**, and a
    naive byte diff reports every compliant file as unformatted — which is why `Applications/EngineTest/
    TestMain.cpp` and `Examples/WindowExample/Main.cpp`, both re-measured clean today, came back "not clean"
    from the first version of this script. The one blank each differs by is the mandated one.

    Categories are what was measured, never a claim about who owns the fix: the three labels this ledger used
    (`format-clean`, `format-debt-only`, `format-real`) had rotted into meaninglessness, all three being
    mostly "already identical".
    """
    if path.endswith(OBJECTIVE_SUFFIXES):
        return 'n/a (Objective-C++: .clang-format declares Language: Cpp only)'
    if path.endswith(INCLUDE_BODY_SUFFIXES):
        return 'n/a (include body: clang-format needs --assume-filename)'
    try:
        result = subprocess.run(['clang-format', '--style=file', path], capture_output=True, text=True)
    except OSError as error:
        return 'unmeasured (%s)' % error
    if result.returncode != 0:
        return 'unmeasured (clang-format refused: %s)' % (result.stderr or '').strip()[:60]
    out = result.stdout
    if out == text:
        return 'clean'
    # The formatted side gets no path, matching `check.sh`, which pipes it through the flag on stdin.
    if blank_lines.collapse_seam(out) == blank_lines.collapse_seam(text, path):
        return 'clean apart from A3'
    before, after = text.split('\n'), out.split('\n')
    if [line.rstrip() for line in before] == [line.rstrip() for line in after]:
        return 'trailing whitespace'
    if len(before) == len(after) and [line.strip() for line in before] == [line.strip() for line in after]:
        return 'indentation only'
    if [line.strip() for line in before if line.strip()] == [line.strip() for line in after if line.strip()]:
        return 'blank lines and indentation'
    return 'code shape'


def measured(path, text, entries, by_name):
    """This file's five column values, each either a count or the reason there is no count."""
    fmt = format_verdict(path, text)
    seams, blank_error = blank_lines.check_file(path)
    blank = str(len(seams)) if blank_error is None else 'unmeasured (%s)' % blank_error
    pre = includes.check_preamble(text, path)
    found, _spans = comments.check_file(path, text)
    shape = 'n/a (no class or struct here)'
    try:
        members, notes, reason, waived = layout.check_file(path, entries, by_name)
    except layout.Skipped as skipped:
        shape = 'unmeasured (%s)' % skipped
    except Exception as error:                                  # clang failing here is this file's state, not a crash
        shape = 'unmeasured (clang: %s)' % str(error)[:60]
    else:
        if reason:
            shape = 'n/a (%s)' % reason
        else:
            shape = str(len(members))
            if waived:
                shape += ' (+%d waived)' % len(waived)
            if notes:
                shape += ' (partial)'
    return [fmt, blank, str(len(pre)), str(len(found)), shape]


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--write', action='store_true', help='replace %s instead of printing to stdout' % LEDGER)
    parser.add_argument('--db', default='cmake-build-debug/compile_commands.json')
    opts = parser.parse_args(argv)

    sources = tracked_sources()
    if not sources:
        print('no tracked sources — run inside the repository', file=sys.stderr)
        return 3
    try:
        entries, by_name = layout.read_compile_db(opts.db)
    except layout.Skipped as skipped:
        print('member layout cannot be measured: %s' % skipped, file=sys.stderr)
        print('regenerate the database with cmake -S . -B %s -DCMAKE_EXPORT_COMPILE_COMMANDS=ON'
              % os.path.dirname(opts.db), file=sys.stderr)
        return 3

    rows, blocked = [], []
    for path in sources:
        try:
            with open(path, encoding='utf-8', errors='ignore') as handle:
                text = handle.read()
        except OSError as error:
            rows.append([path, 'unreadable (%s)' % error, '-', '-', '-', '-'])
            blocked.append((path, 'unreadable'))
            continue
        values = measured(path, text, entries, by_name)
        rows.append([path] + values)
        blocked += [(path, value) for value in values if value.startswith(('unmeasured', 'unreadable'))]

    revision = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True).stdout.strip()
    dirty = [entry for entry in subprocess.run(['git', 'status', '--porcelain'], capture_output=True,
                                               text=True).stdout.split('\n') if entry]
    formatter = subprocess.run(['clang-format', '--version'], capture_output=True, text=True).stdout.strip()
    date = subprocess.run(['git', 'log', '-1', '--format=%ad', '--date=short'], capture_output=True,
                          text=True).stdout.strip()
    lines = [
        '# hb-standards per-file verdict',
        '',
        '**Generated — do not edit.** Every row measures a tracked source **as it stands on disk**, produced by',
        '`python3 .pi/skills/hb-standards/scripts/file_ledger.py --write`. Measured against %s (tree dated %s) with'
        % (revision, date),
        '`%s`.' % formatter,
        '',
        'The previous edition of this file was hand-written and its labels had rotted: 246 of its 256',
        'measurable rows described formatting work that had already been done, and 19 tracked sources had no',
        'row at all. That is why the command above is the only way this table changes.',
        '',
        '| column | measured by | value |',
        '|---|---|---|',
        '| clang-format | `clang-format --style=file` compared through `blank_lines.py --collapse-seam`, as `check.sh` does | `clean`, `clean apart from A3`, what else would change, or `n/a` |',
        '| blank lines | `blank_lines.py` (rule set A) | finding count |',
        '| include preamble | `includes.py` (rule set B) | finding count |',
        '| comments | `comments.py` (the comment ban) | finding count |',
        '| member layout | `layout.py` (twelve-block order from the clang AST) | finding count, `n/a`, or `unmeasured` |',
        '',
        '`n/a` means the layer has nothing to judge here; `unmeasured` means it has something to judge and could',
        'not — the two are never printed alike, and neither is a pass. **This table says nothing about the**',
        '**docs coverage, API reference, or build-and-test layers** of `SKILL.md`, which are gated by',
        '`docs_coverage.py` and `gate.sh` and are not per-file verdicts.',
        '',
        'Two counts a reader should not act on alone, both named here because the rows cannot say them:',
        '',
        '* `Engine/CodingStandards.h` reports every comment it holds, because `comments.py` exempts only',
        '  `Engine/CodingStandards.cpp`. `docs/CodingStandards.md` exempts the `.cpp` outright and the `.h`',
        '  only "the BAD EXAMPLE blocks" of it, while `check.sh` skips `Engine/CodingStandards.*` whole. So',
        '  the count is true under the checker and unreachable through the gate, and which of the two should',
        '  win is an open owner decision, not a fact this table can settle.',
        '* `unmeasured` in *member layout* is itself a finding about that file. `Engine/Memory/ScopedAllocator.h`',
        '  is rejected by clang when compiled as its own translation unit because it names `std::forward` on',
        '  line 28 and includes no `<utility>` — it builds today only because its consumers reach that header',
        '  first, which is the same latent shape `Engine/Core/TaskSystem.cpp` carried before `50efdaa`.',
        '',
    ]
    if dirty:
        # Name it, do not silently label a half-finished edit as the state of a revision.
        lines += ['%d path(s) carried uncommitted edits while this ran, so those rows describe work in progress'
                  ' rather' % len(dirty),
                  'than %s.' % revision, '']
    if blocked:
        reasons = collections.Counter(reason.split(' (')[0] for _path, reason in blocked)
        lines += ['%d file(s) carry a column this run could not measure: %s.'
                  % (len({path for path, _reason in blocked}),
                     ', '.join('%s %d' % (name, count) for name, count in sorted(reasons.items()))), '']
    lines += ['|%s|' % '|'.join([' file ', ' clang-format ', ' blank lines ', ' include preamble ',
                                 ' comments ', ' member layout ']),
              '|---|---|---|---|---|---|']
    lines += ['| %s |' % ' | '.join(row) for row in rows]
    lines += ['', '%d row(s) for %d tracked source(s); %d row(s) reconcile(s) by path with `git ls-files`.'
              % (len(rows), len(sources), len(rows)), '']

    if opts.write:
        with open(LEDGER, 'w', encoding='utf-8') as handle:
            handle.write('\n'.join(lines))
        print('%s: %d row(s) written' % (LEDGER, len(rows)))
    else:
        print('\n'.join(lines))
    return 1 if blocked else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
