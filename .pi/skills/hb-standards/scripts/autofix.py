#!/usr/bin/env python3
"""The mechanical half of a standards fix, and nothing that needs judgement.

This script exists so a subagent does not spend tokens on an edit whose correct
result is already determined by the rule text. It is deliberately small: a fix
belongs here only when the transformation is correct without understanding what
the code was for, and when the tokens it removes can be written down so
`prove_format.py` can demand them back.

| Fix | Why no judgement is needed |
|---|---|
| split a joined empty body `void f() {}` | the rule is layout only, and layer 1 re-flattens what it likes |
| split a joined empty record `struct S {};` | same |
| drop `virtual` where `override` is present | `override` already implies `virtual`, so the keyword is dead text |
| `return std::move(x);` to `return x;` for a plain local | the rule's own stated reason is NRVO, and this is its one shape |
| trailing whitespace, missing final newline, missing line-1 copyright | hygiene, no tokens involved |

Everything else layer 2 reports is left alone and counted as work for a reviewer,
because it needs the cross-file view (`m_` and snake_case renames), the control
flow (exceptions), or the AST (member layout). A fixer that reaches past its
proof is worse than a fixer that does less.

Include order is **not** here: layer 1 is clang-format, and `SortIncludes` is on,
so a second sorter would only disagree with the first.

Usage:
    autofix.py [options] file...
      --manifest PATH   write the change manifest (default .Plans/fix-manifest.json)
      --no-manifest     do not write it; the run is still reported
      --dry-run         report what would change, write nothing
      --root DIR        repo root for manifest paths (default cwd)

Exit status: 0 when every reported issue was either fixed or handed off,
2 on usage error. A run that fixed nothing is a success, not a failure.
"""

import argparse
import collections
import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import comments

COPYRIGHT = '// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.'

# A generated header is not the engine's own text: check.sh skips it, the docs
# coverage checker skips it, and a fixer that edited one would have its work
# overwritten by the next codegen run. Same pattern, same three-line window.
GENERATED = re.compile(r'auto-?generated|do not edit', re.I)

# The standard's own exemplars carry deliberate BAD EXAMPLE blocks, so the
# behavioural fixes must not "correct" the very lines that teach the rule. File
# hygiene is not behavioural, so it still applies to them.
BEHAVIOURAL_EXEMPT = {'Engine/CodingStandards.h', 'Engine/CodingStandards.cpp'}
IGNORE_DIRECTIVE = 'hb-standards:ignore'

EMPTY_BODY = re.compile(r'(\)\s*(?:(?:const|noexcept|override|constexpr|noexcept\(true\))\s+)*)\{\s*\}\s*$')
EMPTY_RECORD = re.compile(r'^(\s*)(class|struct|union|enum)(\s+(?:class\s+)?[A-Za-z_]\w*)\s*\{\s*\}\s*;\s*$')
RETURN_MOVE = re.compile(r'^(\s*)return\s+std::move\(\s*([A-Za-z_]\w*)\s*\)\s*;\s*$')
VIRTUAL_OVERRIDE = re.compile(r'\bvirtual\b')

# What layer 2 also reports, which this script must hand off rather than fix.
HANDED_OFF = (
    ('exception use', re.compile(r'\b(?:throw\s+[A-Za-z_(]|try\s*\{|catch\s*\()')),
    ('m_ member prefix', re.compile(r'\bm_[A-Za-z]')),
)


def line_spans(text):
    """Comment spans as (line_index, start_col, end_col), 0-based."""
    starts, spans = [0], []
    for k, ch in enumerate(text):
        if ch == '\n':
            starts.append(k + 1)
    ends = starts[1:] + [len(text)]
    for start, stop, _kind in comments.comment_spans(text):
        first = sum(1 for s in starts if s <= start) - 1
        last = sum(1 for s in starts if s <= stop) - 1
        for line in range(first, last + 1):
            lo = max(start, starts[line]) - starts[line]
            hi = min(stop, ends[line]) - starts[line]
            if hi > lo:
                spans.append((line, lo, hi))
    return spans


class Report:
    def __init__(self, path):
        self.path = path
        self.counts = collections.Counter()
        self.handed_off = collections.Counter()
        self.notes = []

    @property
    def changed(self):
        return sum(self.counts.values())


def fix_empty_bodies(lines, spans, report, exempt):
    if exempt:
        return lines
    out, commented = [], _comment_lines(spans)
    for index, line in enumerate(lines):
        if line.rstrip().endswith(';') or index in commented or IGNORE_DIRECTIVE in line:
            out.append(line)
            continue
        match = EMPTY_BODY.search(line)
        if not match:
            out.append(line)
            continue
        indent = re.match(r'\s*', line).group(0)
        # group(1) captures the qualifiers with their trailing space, so it is
        # rstripped here rather than left for the hygiene pass to find. Without
        # this the manifest reports a trailing-whitespace fix the split itself
        # caused, and a declared count stops describing the original file.
        out.append((line[:match.start()] + match.group(1)).rstrip())
        out.append(indent + '{')
        out.append(indent + '}')
        report.counts['split-joined-empty-body'] += 1
    return out


def fix_empty_records(lines, spans, report, exempt):
    if exempt:
        return lines
    out, commented = [], _comment_lines(spans)
    for index, line in enumerate(lines):
        match = None if index in commented or IGNORE_DIRECTIVE in line else EMPTY_RECORD.match(line)
        if not match:
            out.append(line)
            continue
        indent, kind, name = match.group(1), match.group(2), match.group(3)
        out.append(indent + kind + name)
        out.append(indent + '{')
        out.append(indent + '};')
        report.counts['split-joined-empty-record'] += 1
    return out


def fix_virtual_override(lines, spans, report, exempt):
    if exempt:
        return lines
    out, commented = [], _comment_lines(spans)
    for index, line in enumerate(lines):
        if index in commented or IGNORE_DIRECTIVE in line or 'override' not in line:
            out.append(line)
            continue
        match = VIRTUAL_OVERRIDE.search(line)
        if not match:
            out.append(line)
            continue
        head, tail = line[:match.start()], line[match.end():]
        collapsed = re.sub(r'[ \t]{2,}', ' ', tail) if tail[:1] in (' ', '\t') else tail
        out.append(head + collapsed.lstrip(' '))
        report.counts['drop-virtual-alongside-override'] += 1
    return out


def fix_return_move(lines, spans, report, exempt):
    if exempt:
        return lines
    out, commented = [], _comment_lines(spans)
    for index, line in enumerate(lines):
        match = None if index in commented or IGNORE_DIRECTIVE in line else RETURN_MOVE.match(line)
        if not match:
            out.append(line)
            continue
        out.append(match.group(1) + 'return ' + match.group(2) + ';')
        report.counts['return-std-move-to-local'] += 1
    return out


def fix_hygiene(lines, report):
    stripped = [re.sub(r'[ \t]+$', '', line) for line in lines]
    removed = sum(1 for a, b in zip(lines, stripped) if a != b)
    if removed:
        report.counts['strip-trailing-whitespace'] += removed
    lines = stripped
    if not lines or not re.search(r'Copyright \(c\).*Hansol Park', lines[0]):
        lines.insert(0, COPYRIGHT)
        report.counts['insert-copyright-line-1'] += 1
    text = '\n'.join(lines)
    if not text.endswith('\n'):
        text += '\n'
        report.counts['add-final-newline'] += 1
    return text


def _comment_lines(spans):
    """Every line a comment touches.

    A line carrying a comment is left alone by the behavioural fixes: that
    comment is scheduled for deletion by the comment-ban layer, and rewriting
    code underneath text that is about to disappear is how a wrong edit gets
    made for the right reason.
    """
    return {line for line, _start, _end in spans}


def hand_off(lines, spans, report, exempt):
    if exempt:
        return
    commented = _comment_lines(spans)
    for name, pattern in HANDED_OFF:
        hits = sum(1 for index, line in enumerate(lines)
                   if index not in commented and pattern.search(line))
        if hits:
            report.handed_off[name] += hits


def process(path, dry_run):
    text = open(path, encoding='utf-8', errors='replace').read()
    if GENERATED.search('\n'.join(text.split('\n')[:3])):
        report = Report(path)
        report.notes.append('generated')
        return report, False, {}, {}
    before = collections.Counter(comments.code_tokens(text) or [])
    spans = line_spans(text)
    exempt = os.path.relpath(path, os.getcwd()).replace('\\', '/') in BEHAVIOURAL_EXEMPT
    report = Report(path)
    lines = text.split('\n')
    for step in (fix_empty_bodies, fix_empty_records, fix_virtual_override, fix_return_move):
        lines = step(lines, spans, report, exempt)
    new_text = fix_hygiene(lines, report)
    hand_off(new_text.split('\n'), spans, report, exempt)
    after = collections.Counter(comments.code_tokens(new_text) or [])
    removed = dict(sorted((before - after).items()))
    added = dict(sorted((after - before).items()))
    if new_text != text and not dry_run:
        open(path, 'w', encoding='utf-8').write(new_text)
    return report, new_text != text, removed, added


def main(argv):
    parser = argparse.ArgumentParser(description='Apply the mechanical standards fixes a script may make.')
    parser.add_argument('files', nargs='+')
    parser.add_argument('--manifest', default='.Plans/fix-manifest.json')
    parser.add_argument('--no-manifest', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--root', default=os.getcwd())
    args = parser.parse_args(argv)

    entries, total = [], 0
    for path in args.files:
        if not os.path.isfile(path):
            print('autofix: no such file: %s' % path, file=sys.stderr)
            return 2
        report, changed, removed, added = process(path, args.dry_run)
        total += sum(report.counts.values())
        if report.notes:
            print('%-52s skipped: %s' % (os.path.relpath(path, args.root), report.notes[0]))
            entries.append({'file': os.path.relpath(path, args.root), 'changed': False,
                            'skipped': report.notes[0], 'fixes': {}, 'tokensRemoved': {},
                            'tokensAdded': {}, 'handedOff': {}})
            continue
        if sum(report.counts.values()) or report.handed_off:
            print('%-52s %s%s' % (os.path.relpath(path, args.root),
                                  dict(sorted(report.counts.items())) or '{}',
                                  '  hand-off: %s' % dict(sorted(report.handed_off.items())) if report.handed_off else ''))
        entries.append({
            'file': os.path.relpath(path, args.root),
            'changed': bool(changed),
            'fixes': dict(sorted(report.counts.items())),
            'tokensRemoved': removed,
            'tokensAdded': added,
            'handedOff': dict(sorted(report.handed_off.items())),
        })
    if not args.no_manifest:
        directory = os.path.dirname(os.path.abspath(args.manifest))
        if directory:
            os.makedirs(directory, exist_ok=True)
        json.dump({'files': entries}, open(args.manifest, 'w'), indent=1)
        print('manifest: %s (%d file(s), %d fix(es)%s)'
              % (args.manifest, len(entries), total, ', dry run' if args.dry_run else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
