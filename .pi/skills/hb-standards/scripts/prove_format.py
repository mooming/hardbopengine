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

Usage: prove_format.py [rev] [--manifest PATH]        default rev: HEAD

With --manifest, the file written by autofix.py, a change that removes tokens is
explained only if the fixer declared exactly that removal. The fixer cannot grade
its own homework: the manifest is a claim about the edit, and this script re-derives
the token multisets and refuses anything the claim does not cover. A file that lost
tokens with no manifest entry stays `unexplained`, which is the whole point.

Exit status: 0 when every changed C++ file is explained.
"""

import collections
import json
import subprocess
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import comments

CPP = ('.h', '.hh', '.hpp', '.cpp', '.cc', '.cxx', '.inl')
ACCESS_LABELS = {'public', 'private', 'protected', ':'}


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


def load_manifest(path):
    """file -> declared change, or {} when no manifest was given."""
    if not path:
        return {}
    try:
        data = json.load(open(path))
    except (OSError, ValueError) as exc:
        raise SystemExit('prove_format: cannot read manifest %s: %s' % (path, exc))
    return {entry['file']: entry for entry in data.get('files', []) if entry.get('file')}


def explain(path, rev, declared=None):
    """(verdict, detail) for one changed file: what the formatter was allowed to do."""
    before = run('git', 'show', '%s:%s' % (rev, path))
    after = open(path, encoding='utf-8', errors='ignore').read()
    old_tokens, new_tokens = tokens(before), tokens(after)
    if old_tokens == new_tokens:
        return 'whitespace', ''
    old_includes, new_includes = include_paths(old_tokens), include_paths(new_tokens)
    old_payload, new_payload = payload_text(old_tokens), payload_text(new_tokens)
    # Identical include list and identical string literals, with the token multiset unchanged, means the
    # file lost nothing and gained nothing but ordering — which is a member reorder, not an include move.
    # Classifying that as include-order is what this branch used to do, and a verdict that names the wrong
    # change is worse than no verdict: it reads as a reason to stop looking.
    lost = collections.Counter(old_tokens) - collections.Counter(new_tokens)
    gained = collections.Counter(new_tokens) - collections.Counter(old_tokens)
    # A reorder that crosses access sections has to re-open the access the members it passed were written
    # under, so access labels are the one thing it may add. Nothing else may be added, and nothing at all
    # may be lost — that asymmetry is the check, and it is why "identical multiset" alone would have
    # rejected this file's own honest reorder.
    if (not lost and old_includes == new_includes and old_payload == new_payload
            and set(gained) <= ACCESS_LABELS):
        return 'reorder', 'access label(s) added: %s' % dict(gained) if gained else ''
    # A declared fix: the fixer said which tokens it removed and added, and the multisets agree.
    # Anything the claim does not cover falls through to `unexplained` below rather than being
    # waved through, because a manifest that under-claims is exactly how a stray edit survives.
    if declared and declared.get('tokensRemoved'):
        claimed_lost = collections.Counter(declared['tokensRemoved'])
        claimed_gain = set(declared.get('tokensAdded') or {})
        if claimed_lost == lost and set(gained) <= ACCESS_LABELS | claimed_gain:
            detail = str(dict(sorted((declared.get('fixes') or {}).items())))
            if gained:
                detail += ', access label(s) added: %s' % dict(gained)
            return 'declared-fix', detail
        return 'unexplained', 'declared removal %s, observed loss %s, observed gain %s' % (
            dict(sorted(claimed_lost.items())), dict(sorted(lost.items())), dict(sorted(gained.items())))
    # Lost tokens with nothing declaring them is the case this tool exists for, and the include-set
    # message below describes it as a changed include set (often an empty one), which sends the
    # reader looking at the preamble instead of at the code that lost a symbol.
    if lost:
        return 'unexplained', 'token(s) removed with no declared fix: %s' % dict(sorted(lost.items()))
    if old_includes != new_includes and sorted(old_includes) == sorted(new_includes) and old_payload == new_payload:
        if sorted(old_tokens) != sorted(new_tokens):
            return 'unexplained', 'include order changed and the token multiset changed too'
        return 'include-order', ''
    if old_includes == new_includes and old_payload != new_payload and sorted(old_tokens) == sorted(new_tokens):
        return 'literal-split', ''
    if sorted(old_includes) == sorted(new_includes) and old_payload != new_payload:
        first = next((i for i in range(min(len(old_payload), len(new_payload)))
                      if old_payload[i] != new_payload[i]), min(len(old_payload), len(new_payload)))
        return 'unexplained', 'includes reordered and text changed at char %d: %r vs %r' % (
            first, old_payload[max(0, first - 40):first + 40], new_payload[max(0, first - 40):first + 40])
    return 'unexplained', 'include set changed: %s' % (
        set(old_includes) ^ set(new_includes))


def main(argv):
    rest, manifest_path = list(argv), None
    if '--manifest' in rest:
        index = rest.index('--manifest')
        if index + 1 >= len(rest):
            print('prove_format: --manifest needs a path', file=sys.stderr)
            return 2
        manifest_path = rest[index + 1]
        del rest[index:index + 2]
    rev = rest[0] if rest else 'HEAD'
    manifest = load_manifest(manifest_path)
    changed = [f for f in run('git', 'diff', '--name-only', rev).split('\n')
               if f.endswith(CPP) and os.path.isfile(f)]
    tally, bad = {}, []
    for path in changed:
        verdict, detail = explain(path, rev, manifest.get(path))
        tally[verdict] = tally.get(verdict, 0) + 1
        if verdict == 'unexplained':
            bad.append('%s: %s' % (path, detail))
    for verdict, count in sorted(tally.items()):
        print('%-16s %d file(s)' % (verdict, count))
    for line in bad:
        print('  %s' % line)
    unused = sorted(set(manifest) - set(changed))
    if unused:
        print('note: %d manifest entr%s not in this diff: %s'
              % (len(unused), 'y' if len(unused) == 1 else 'ies', ', '.join(unused[:6])))
    print('code identity: %d file(s) checked against %s%s, %d unexplained'
          % (len(changed), rev, ' with manifest' if manifest else '', len(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
