#!/usr/bin/env python3
"""Comment ban checker for HardBop Engine sources.

docs/CodingStandards.md forbids comments in .h and .cpp: implementation and declaration
files carry code only, and prose lives in the HTML reference under docs/. This script is
the mechanical half of that rule. It is a lexer, not a grep: `grep '//'` reports
"http://example.com" inside a string literal as a comment, and a rule that lies about
its own findings is not a rule anyone can act on.

Exemptions, exhaustive and mirrored in docs/CodingStandards.md:
  line 1        the copyright notice, a legal notice rather than documentation
  label         a trailing comment naming only the construct its line closes
  ignore tag    `hb-standards:ignore`, a tool directive in the class of a switch
  exemplar      Engine/CodingStandards.cpp, which teaches the rule by breaking it

Output: one finding per line as `path:line: KIND  text`, exit 1 when anything was found.
"""

import os
import re
import sys

COPYRIGHT = re.compile(r'Copyright \(c\).*Hansol Park')
IGNORE_TAG = 'hb-standards:ignore'

CLOSING_LINE = re.compile(r'^[{}\s]*$|^#[ \t]*(endif|else|elif)\b')
LABEL = re.compile(r'^/{2}\s*!?(?:endif|else|elif|ifdef|ifndef|namespace\s+[\w:]+(\s+[\w:]+)*|[A-Z][A-Z0-9_]*)\s*$')

EXEMPT_FILES = ('Engine/CodingStandards.cpp',)

BLOCK_OPEN = '/*'
LINE_OPEN = '//'


def strip_escapes(text):
    return text


def comment_spans(text):
    """Yield (start_offset, end_offset, kind) for every comment in source text.

    Tracks string literals, character literals and line continuations, because a comment
    marker inside one is not a comment. Raw string literals are absent from this tree
    (measured: 0 files use R"(), which is why the lexer stops at the ordinary forms;
    a file that introduces one fails loudly here rather than silently mis-lexing, since
    an unterminated literal raises below.
    """
    spans = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if c == '/' and nxt == '/':
            end = text.find('\n', i)
            end = n if end < 0 else end
            spans.append((i, end, 'line'))
            i = end
            continue
        if c == '/' and nxt == '*':
            end = text.find('*/', i + 2)
            if end < 0:
                raise ValueError('unterminated block comment')
            spans.append((i, end + 2, 'block'))
            i = end + 2
            continue
        if c in '"\'':
            quote = c
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == quote:
                    break
                if text[i] == '\n' and quote == '"':
                    raise ValueError('unterminated string literal')
                i += 1
            i += 1
            continue
        i += 1
    return spans


def line_starts(text):
    starts = [0]
    for k, ch in enumerate(text):
        if ch == '\n':
            starts.append(k + 1)
    return starts


def line_of(starts, offset):
    lo, hi = 0, len(starts) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if starts[mid] <= offset:
            lo = mid
        else:
            hi = mid - 1
    return lo + 1


def code_prefix(line_text, col):
    return line_text[:col].strip()


def is_label_comment(line_text, comment_col, body):
    """A label names only the construct its own line closes, and nothing else."""
    before = code_prefix(line_text, comment_col)
    if not CLOSING_LINE.match(before):
        return False
    stripped = body.strip()
    if stripped.startswith('//'):
        return bool(LABEL.match(stripped))
    inner = stripped[2:-2].strip() if stripped.endswith('*/') else stripped
    return bool(LABEL.match('//' + inner))


def check_file(path, text):
    findings = []
    if path in EXEMPT_FILES:
        return findings
    starts = line_starts(text)
    try:
        spans = comment_spans(text)
    except ValueError as exc:
        return ['%s:1: LEXER  %s — the file cannot be lexed, so no verdict is possible' % (path, exc)]
    lines = text.split('\n')
    for begin, end, kind in spans:
        lineno = line_of(starts, begin)
        if lineno == 1 and COPYRIGHT.search(text[begin:end]):
            continue
        line_text = lines[lineno - 1]
        comment_col = begin - starts[lineno - 1]
        body = text[begin:end]
        if IGNORE_TAG in body:
            continue
        if is_label_comment(line_text, comment_col, body):
            continue
        first = body.strip().split('\n')[0][:88]
        findings.append('%s:%d: %s  %s' % (path, lineno, 'COMMENT' if kind == 'line' else 'BLOCK-COMMENT', first))
    return findings


def main(argv):
    if not argv:
        print('usage: comments.py <file ...>', file=sys.stderr)
        return 3
    total = 0
    scored = 0
    for path in argv:
        if not path:
            continue
        try:
            text = open(path, encoding='utf-8', errors='ignore').read()
        except OSError as exc:
            print('%s:0: UNREADABLE  %s' % (path, exc))
            total += 1
            continue
        scored += 1
        found = check_file(path, text)
        total += len(found)
        for f in found:
            print(f)
    print('comments examined in %d file(s): %d violation(s)' % (scored, total))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
