#!/usr/bin/env python3
"""Prove that reorganising a file moved nothing but order and blank lines.

Grouping declarations by concern is a reader's edit: no tool can know that `taskQueueMutex` and
`taskQueue` belong to one concern and `streams` to another. What a tool CAN do is prove the edit stayed
in its lane, which is what this checks. It is run after a reorganisation, never before one.

Four properties, and each fails loudly with the lines involved:

  1. no line changed content — the multiset of non-blank lines must be identical to the revision's, so a
     dropped member, an added one and a silently edited one are all caught;
  2. non-static data members kept their relative order. This is the one that matters, because C++
     initialises them in declaration order and nothing else in the toolchain reports a change. It is read
     from the clang AST through layout.py, on a copy of the old revision written beside the new file so the
     compile database and the includes resolve;
  3. the `__UNIT_TEST__` region is byte-identical. Test-only surface belongs at the end of a file and is
     never part of a regrouping;
  4. informational: which contiguous blocks of the old file now sit in a different position, so the commit
     message can say what was regrouped instead of being asked to guess.

Exit status: 0 proven, 1 a property failed, 3 the run could not complete — and "could not complete" is
never reported as "passed", which is the same rule `check.sh` applies to a missing compile database.
"""

import argparse
import os
import re
import subprocess
import sys
from collections import Counter

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

import layout  # noqa: E402  (same directory; the AST machinery is not to be duplicated)

UNIT_TEST_MARKER = '#ifdef __UNIT_TEST__'
DATA_ORDER_NOTE = 'data members keep their declaration order (C++ initialises in that order)'


def revision_text(path, rev):
    result = subprocess.run(['git', 'show', '%s:%s' % (rev, path)], capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return result.stdout


def multiset_check(before, after):
    old = Counter(line.rstrip() for line in before.split('\n') if line.strip())
    new = Counter(line.rstrip() for line in after.split('\n') if line.strip())
    lost = sorted((old - new).elements())
    gained = sorted((new - old).elements())
    return lost, gained


def unit_test_check(before, after):
    if UNIT_TEST_MARKER not in before or UNIT_TEST_MARKER not in after:
        return None
    return before[before.index(UNIT_TEST_MARKER):] == after[after.index(UNIT_TEST_MARKER):]


def field_sequences(path, text, db_entries, by_name, clang_override=None):
    """The ordered non-static data member names of every record declared in `text`.

    `path` must sit where `text` would normally sit, because the AST is produced with the project's real
    flags from the compile database, and those flags carry the include paths.
    """
    scratch = os.path.join(os.path.dirname(path), '._prove_regroup_%d%s' % (os.getpid(), os.path.splitext(path)[1]))
    open(scratch, 'w', encoding='utf-8').write(text)
    found = {}
    try:
        records, _guarded, _notes = layout.collect_records(scratch, db_entries, by_name, clang_override)
        for record, _line, home in records:
            order, seq = layout.field_order(record)
            if not order:
                continue
            name = record.get('name') or '(anonymous)'
            found.setdefault(name, [filed[1] for filed in sorted((order[f], f) for f in order)])
    finally:
        os.unlink(scratch)
    return found
def moved_blocks(before, after):
    """Contiguous runs of old-file lines that now sit elsewhere, as (first line text, old line, new line)."""
    old_lines = [l.rstrip() for l in before.split('\n') if l.strip()]
    new_lines = [l.rstrip() for l in after.split('\n') if l.strip()]
    new_at = {}
    for index, line in enumerate(new_lines):
        new_at.setdefault(line, []).append(index)
    moved = []
    for index, line in enumerate(old_lines, 1):
        places = new_at.get(line, [])
        if not places:
            continue
        # A line is "in place" while it sits at the same distance from the previous line's new position.
        if moved or (places[0] != index - 1 and index > 1):
            moved.append((line, index, places[0] + 1))
    return moved


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('path', nargs='?')
    parser.add_argument('--against', default='HEAD', help='revision to compare the working file against (default HEAD)')
    parser.add_argument('--between', nargs=2, metavar=('REV_A', 'REV_B'),
                        help='compare two committed revisions instead of the working file, leaving the tree alone')
    parser.add_argument('--db', default='cmake-build-debug/compile_commands.json')
    parser.add_argument('--clang', default=None)
    opts = parser.parse_args(argv)

    if not opts.path:
        print('usage: prove_regroup.py <file> [--against REV | --between REV_A REV_B] [--db compile_commands.json]')
        return 3

    if opts.between:
        before = revision_text(opts.path, opts.between[0])
        after = revision_text(opts.path, opts.between[1])
        if before is None or after is None:
            print('%s: cannot read %s from git' % (opts.path, ' and '.join(opts.between)))
            return 3
        label = '%s..%s' % tuple(opts.between)
    else:
        after = open(opts.path, encoding='utf-8', errors='ignore').read()
        before = revision_text(opts.path, opts.against)
        if before is None:
            print('%s: cannot read %s from git — is the path relative to the repository root?' % (opts.path, opts.against))
            return 3
        label = opts.against

    failures = []

    lost, gained = multiset_check(before, after)
    if lost or gained:
        failures.append('content changed, not just order')
        for line in lost[:12]:
            print('  LOST   %s' % line)
        for line in gained[:12]:
            print('  ADDED  %s' % line)
    else:
        print('[PASS] every non-blank line survives unchanged — %d line(s), same multiset as %s'
              % (sum(Counter(line.rstrip() for line in after.split('\n') if line.strip()).values()), label))

    identical = unit_test_check(before, after)
    if identical is None:
        print('[NONE] no %s region in this file' % UNIT_TEST_MARKER)
    elif identical:
        print('[PASS] the %s region is byte-identical' % UNIT_TEST_MARKER)
    else:
        failures.append('the __UNIT_TEST__ region moved or changed')
        print('[FAIL] the %s region differs — test-only surface is not part of a regrouping' % UNIT_TEST_MARKER)

    try:
        entries, by_name = layout.read_compile_db(opts.db)
    except layout.Skipped as exc:
        print('[NONE] %s' % DATA_ORDER_NOTE)
        print('       %s — an unmeasured rule is not a passing rule, so this run cannot certify %s' % (exc, opts.path))
        return 3 if not failures else 1

    try:
        old_fields = field_sequences(opts.path, before, entries, by_name, opts.clang)
        new_fields = field_sequences(opts.path, after, entries, by_name, opts.clang)
    except Exception as exc:  # clang failing on this file is a run failure, not a rule failure
        print('[NONE] %s — clang could not analyse this file: %s' % (DATA_ORDER_NOTE, exc))
        return 3 if not failures else 1

    if set(old_fields) != set(new_fields):
        failures.append('the set of record types changed')
        print('[FAIL] records before: %s; after: %s' % (sorted(old_fields), sorted(new_fields)))
    else:
        moved = []
        for name in sorted(old_fields):
            if old_fields[name] != new_fields[name]:
                moved.append((name, old_fields[name], new_fields[name]))
        if moved:
            failures.append('a data member changed its position')
            for name, old_list, new_list in moved:
                print('[FAIL] %s: data members were %s, now %s' % (name, old_list, new_list))
        else:
            counts = [len(v) for v in new_fields.values()]
            print('[PASS] %s — %d record(s), %d member(s) total, sequence unchanged'
                  % (DATA_ORDER_NOTE, len(new_fields), sum(counts)))

    moved = moved_blocks(before, after)
    print('[INFO] %d line(s) now sit at a different position; blank lines are not counted' % len(moved))

    if failures:
        print('REORGANISATION NOT PROVEN — %s' % '; '.join(failures))
        return 1
    print('reorganisation proven: order changed, nothing else')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
