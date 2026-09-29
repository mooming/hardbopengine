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
DIRECTIVE = re.compile(r'^(if|ifdef|ifndef|else|elif|endif)\b(.*)$', re.S)

NAMESPACE_HEAD = re.compile(r'\bnamespace\s+([A-Za-z_]\w*)')

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


def condition_variants(condition):
    """Every spelling that legitimately names an `#if` condition on the line closing it.

    The rule book permits the guard named directly (`#endif // PROFILE_ENABLED`) and in negated
    form (`#else // !__DEBUG__`), and conditions written with `defined(...)` too. Enumerating the
    variants is what lets `//__UNIT_TEST__` through while `// TODO` still reads as a comment: the
    label has to *be* the construct's name, not merely look like an identifier.
    """
    condition = ' '.join(condition.split()).strip()
    variants = {condition, '!' + condition}
    for name in re.findall(r'\b[A-Za-z_][A-Za-z_0-9]*\b', condition):
        variants.update({name, '!' + name, '!defined(%s)' % name, '! defined(%s)' % name, '! defined (%s)' % name})
    compact = re.sub(r'\s+', '', condition)
    variants.update({compact, '!' + compact})
    return variants


def label_expectations(text):
    """Map each line that may carry a structural label to the labels that line allows.

    Two constructs qualify, per docs/CodingStandards.md: a preprocessor conditional, closed by
    `#endif`/`#else`/`#elif`, and a namespace, closed by a line whose only code is `}`.

    The first version judged the comment *text* by pattern, and got it wrong in both directions. It
    rejected `#else  // !__DEBUG__`, the rule book's own example, because the guard macros this
    engine uses begin with an underscore — 104 `#endif //__UNIT_TEST__` labels — and it would have
    accepted `// TODO`, since a capitalised word matches `[A-Z][A-Z0-9_]*`. Comparing the label
    against what the line actually closes gets both right, and it is what stops the exemption from
    becoming a hole through which any short comment can pass.
    """
    expectations = {}
    guard_stack = []
    namespace_stack = []
    depth = 0
    pending = ''
    line = 1
    line_start = 0
    blanked = list(text)
    for begin, end, kind in comment_spans(text):
        for k in range(begin, min(end, len(blanked))):
            if blanked[k] != '\n':
                blanked[k] = ' '
    blanked = ''.join(blanked)

    for index, ch in enumerate(blanked):
        if ch == '\n':
            line += 1
            line_start = index + 1
            continue
        if index == line_start:
            line_end = blanked.find('\n', line_start)
            raw = blanked[line_start:line_end if line_end >= 0 else len(blanked)].strip()
            # The '#' must be there before anything is matched. Stripping it first and then
            # matching the keyword makes every C++ `if` and `else` in the file look like a
            # preprocessor guard, which pushes and pops the stack on ordinary statements and
            # mislabels whichever `#endif` happens to be on top when the real guard closes.
            directive = raw[1:].strip() if raw.startswith('#') else None
            match = DIRECTIVE.match(directive) if directive else None
            if match:
                keyword, condition = match.group(1), match.group(2)
                if keyword in ('if', 'ifdef', 'ifndef'):
                    guard_stack.append(condition_variants(condition))
                elif guard_stack:
                    expectations.setdefault(line, set()).update(guard_stack[-1])
        if ch == '{':
            found = NAMESPACE_HEAD.search(pending)
            namespace_stack.append((found.group(1) if found else None, depth))
            depth += 1
            pending = ''
        elif ch == '}':
            depth -= 1
            closed = None
            while namespace_stack and namespace_stack[-1][1] == depth:
                name, _ = namespace_stack.pop()
                if name:
                    closed = name if closed is None else '%s::%s' % (name, closed)
            if closed:
                expectations.setdefault(line, set()).update({'namespace %s' % closed, 'namespace %s' % closed.split('::')[-1]})
            pending = ''
        elif ch == ';':
            pending = ''
        else:
            pending = (pending + ch)[-120:]
    return expectations


def is_label_comment(line_text, comment_col, body, allowed):
    """A label names only the construct its own line closes, and nothing else."""
    if not allowed:
        return False
    if not CLOSING_LINE.match(code_prefix(line_text, comment_col)):
        return False
    stripped = body.strip()
    inner = stripped[2:].strip() if stripped.startswith('//') else stripped[2:-2].strip()
    inner = ' '.join(inner.split()).strip()
    return inner in allowed


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
    expectations = label_expectations(text)
    for begin, end, kind in spans:
        lineno = line_of(starts, begin)
        if lineno == 1 and COPYRIGHT.search(text[begin:end]):
            continue
        line_text = lines[lineno - 1]
        comment_col = begin - starts[lineno - 1]
        body = text[begin:end]
        if IGNORE_TAG in body:
            continue
        if is_label_comment(line_text, comment_col, body, expectations.get(lineno)):
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
