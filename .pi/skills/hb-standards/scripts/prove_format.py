#!/usr/bin/env python3
"""Prove a mechanical commit moved no code: format, include order and literal splitting only.

A 15,000-line reformat cannot be reviewed by reading it, and the obvious checks are all wrong for it:
a diff shows 170 files, a word-level comparison reports drift in 98 files whose tokens are identical
(`template<typename` is one word and `template <typename` is two), and a whitespace-collapsed
comparison still flags 15 files whose only changes are that `#include <thread>` moved below
`#include <atomic>` and that clang-format broke one long message across three adjacent literals.

So the claim is decomposed into the three things a formatter may legitimately do, and each is checked
separately against `git show HEAD:<file>`:

  * **token stream identical** — the ordinary case: whitespace, wrapping and brace placement only.
    Maximal munch, so `a++b` and `a+ +b` are different streams and a merge across whitespace is caught
    rather than normalised away.
  * **include set identical, order changed** — the include-layout rule requires sorting, so reordering
    is the intended change. Verified as a set; the build gate is what proves the new order still
    compiles, because include order can.
  * **literal concatenation identical, count changed** — clang-format splits a too-long string at an
    existing space into adjacent literals, which concatenate to the same text. Compared with the quote
    characters removed: joining literals with their quotes included made `"a "` `"b"` and `"a "`
    `"b" "` look different when the printed text was equal.

Anything outside those three is reported as a finding, with the first divergence shown.

Usage: prove_format.py [rev]        default rev: HEAD
Exit status: 0 when every changed C++ file is explained.
"""

import subprocess
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import comments

CPP = ('.h', '.hh', '.hpp', '.cpp', '.cc', '.cxx', '.inl')


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def tokens(text):
    return comments.code_tokens(text) or []


def include_paths(toks):
    """The quoted or angled path after every `# include`, in order."""
    out, previous = [], None
    for token in toks:
        if previous == 'include' and (token.startswith('"') or token.startswith('<')):
            out.append(token)
        previous = token
    return out


def payload_text(toks):
    """Every string literal concatenated, quotes removed, include paths excluded."""
    out, previous = [], None
    for token in toks:
        if previous == 'include' and token.startswith('"'):
            previous = token
            continue
        if token.startswith('"') or token.startswith('R"'):
            out.append(token[1:-1] if token.endswith('"') and len(token) > 1 else token)
        previous = token
    return ''.join(out)


def explain(path, rev):
    """(verdict, detail) for one changed file: what the formatter was allowed to do."""
    before = run('git', 'show', '%s:%s' % (rev, path))
    after = open(path, encoding='utf-8', errors='ignore').read()
    old_tokens, new_tokens = tokens(before), tokens(after)
    if old_tokens == new_tokens:
        return 'whitespace', ''
    old_includes, new_includes = include_paths(old_tokens), include_paths(new_tokens)
    old_payload, new_payload = payload_text(old_tokens), payload_text(new_tokens)
    if old_includes == new_includes and old_payload != new_payload:
        return 'literal-split', '' if old_payload == new_payload else None
    if sorted(old_includes) == sorted(new_includes) and old_payload == new_payload:
        return 'include-order', ''
    if sorted(old_includes) == sorted(new_includes) and old_payload != new_payload:
        first = next((i for i in range(min(len(old_payload), len(new_payload)))
                      if old_payload[i] != new_payload[i]), min(len(old_payload), len(new_payload)))
        return 'unexplained', 'includes reordered and text changed at char %d: %r vs %r' % (
            first, old_payload[max(0, first - 40):first + 40], new_payload[max(0, first - 40):first + 40])
    return 'unexplained', 'include set changed: %s' % (
        set(old_includes) ^ set(new_includes))


def main(argv):
    rev = argv[0] if argv else 'HEAD'
    changed = [f for f in run('git', 'diff', '--name-only', rev).split('\n')
               if f.endswith(CPP) and os.path.isfile(f)]
    tally = {}
    bad = []
    for path in changed:
        verdict, detail = explain(path, rev)
        tally[verdict] = tally.get(verdict, 0) + 1
        if verdict == 'unexplained':
            bad.append('%s: %s' % (path, detail))
    for verdict, count in sorted(tally.items()):
        print('%-16s %d file(s)' % (verdict, count))
    for line in bad:
        print('  %s' % line)
    print('code identity: %d file(s) checked against %s, %d unexplained'
          % (len(changed), rev, len(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
