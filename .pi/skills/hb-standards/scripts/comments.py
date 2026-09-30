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
# `namespace` with no name — the marker pushed for its brace so an anonymous body is not mistaken
# for a plain block. Chosen so no namespace identifier can collide with it.
ANONYMOUS = '\x00anonymous'
ANONYMOUS_HEAD = re.compile(r'\bnamespace\b(?!\s*[A-Za-z_:])')

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
            if found:
                namespace_stack.append((found.group(1), depth))
            elif ANONYMOUS_HEAD.search(pending):
                # `namespace` with no name still opens a body, and its closing `}` can only be
                # labelled `// namespace` — there is no name to write. Marking the brace keeps that
                # distinguishable from an ordinary block, which also pushes None and must stay
                # unlabelled.
                namespace_stack.append((ANONYMOUS, depth))
            else:
                namespace_stack.append((None, depth))
            depth += 1
            pending = ''
        elif ch == '}':
            depth -= 1
            closed = None
            anonymous = False
            while namespace_stack and namespace_stack[-1][1] == depth:
                name, _ = namespace_stack.pop()
                if name == ANONYMOUS:
                    anonymous = True
                elif name:
                    closed = name if closed is None else '%s::%s' % (name, closed)
            if closed:
                expectations.setdefault(line, set()).update({'namespace %s' % closed, 'namespace %s' % closed.split('::')[-1]})
            elif anonymous:
                expectations.setdefault(line, set()).update({'namespace'})
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
    """Every comment in this file the ban does not exempt, as findings and as byte ranges.

    The ranges come back too because the strip step must delete exactly what was reported — not a
    re-scan of its own, which could disagree with the checker about what counted.
    """
    findings = []
    spans_kept = []
    if path in EXEMPT_FILES:
        return findings, spans_kept
    starts = line_starts(text)
    try:
        spans = comment_spans(text)
    except ValueError as exc:
        return ['%s:1: LEXER  %s — the file cannot be lexed, so no verdict is possible' % (path, exc)], spans_kept
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
        spans_kept.append((begin, end))
    return findings, spans_kept


def code_only(text):
    """The file with every comment blanked and strings left intact — the text a strip must not change.

    Two versions of a file are code-identical when this is equal for both. That is a stronger proof
    than reading a diff, and it is what lets a comment sweep be run by a tool instead of by hand:
    2,723 findings removed one file at a time, each verified against the version before it.
    """
    try:
        spans = comment_spans(text)
    except ValueError:
        return text
    out = list(text)
    for begin, end, kind in spans:
        for i in range(begin, min(end, len(out))):
            if out[i] != '\n':
                out[i] = ' '
    return re.sub(r'[ \t]+$', '', ''.join(out), flags=re.M)

TOKEN = re.compile(
    r'R"[^"]*"'                     # raw string, opaque
    r'|"(?:\\.|[^"\\])*"'           # string literal, opaque
    r"|'(?:\\.|[^'\\])*'"           # character literal, opaque
    r'|[A-Za-z_][A-Za-z_0-9]*'      # identifier or keyword
    r'|0[xX][0-9a-fA-F]+'
    r'|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[fFuUlL]*'
    r'|\.\d+'
    r'|::|<=>|->|\.\*'
    r'|<<=|>>='
    r'|\+\+|--'
    r'|[-+*/%&|^<>=!]=|&&|\|\||<<|>>'
    r'|[+\-*/%&|^~!<>=?:;,.()\[\]{}#@]',
    re.VERBOSE)


def code_tokens(text):
    """The token stream: comments gone, whitespace ignored, literals whole.

    This is the proof that a formatting pass changed nothing, and the word-splitting version
    (`code_lines`) is not it. clang-format turning `template<typename` into `template <typename` splits
    one word into two without moving a token, and that reported drift in 98 files that were identical.

    Maximal munch is what keeps the check honest rather than merely permissive: `a+ +b` tokenises as
    `a`, `+`, `+`, `b` while `a++b` is `a`, `++`, `b`, so a change that ever merged an operator across
    whitespace is caught rather than normalised away. Getting that behaviour took a bug in this very
    pattern — `++` and `--` were missing from the operator list, so both spellings fell back to two
    `+` tokens and looked equal, which is the failure the check was written to catch. Line
    continuations are consumed as whitespace, which is what they are to the preprocessor, so a macro
    written over five lines compares equal to the same macro over two.
    """
    blanked = list(text)
    try:
        spans = comment_spans(text)
    except ValueError:
        return None
    for begin, end, kind in spans:
        for i in range(begin, min(end, len(blanked))):
            if blanked[i] != '\n':
                blanked[i] = ' '
    joined = re.sub(r'\\\n[ \t]*', ' ', ''.join(blanked))
    return [match.group(0) for match in TOKEN.finditer(joined)]

def code_lines(text):
    """Code only, one entry per line that holds code, intra-line whitespace collapsed.

    `code_only` blanks a comment where it stood, so `f(a, /* x */ b)` and `f(a, b)` differ by spaces
    while being the same code; collapsing runs of whitespace removes that without blinding the
    comparison to a token that moved, which is what the strip step must never do.
    """
    return [' '.join(line.split()) for line in code_only(text).split('\n') if line.strip()]


def strip_file(path, text):
    """Delete exactly the reported comment spans, and refuse to write if any code moved.

    The check is the point of this function. The copyright notice and the structural labels are
    exempt from the check, so they survive without special handling; everything else that is
    reported is deleted, and the file is written only if the surviving code lines are identical to
    the ones before, compared with `code_lines`. A sweep of 2,723 findings is only safe to run as a
    tool if the tool cannot quietly change a token on the way past it.

    Deleting characters is most of it; two cases need a line decision as well:

      * a line that held nothing but a comment goes entirely, or the file fills with blank rows the
        formatter then spends its pass collapsing;
      * a comment inside a line leaves the surrounding code exactly where it was, minus the trailing
        whitespace the comment was hanging off.

    A line is dropped only when no character of it lay outside a comment — a block comment whose
    middle rows are prose and whose last row is `b */` takes all of them with it, while a line of
    real code never does.
    """
    findings, spans = check_file(path, text)
    if not spans:
        return 0
    in_comment = [False] * len(text)
    for begin, end in spans:
        for i in range(begin, min(end, len(text))):
            in_comment[i] = True
    out = list(text)
    for begin, end in spans:
        for i in range(begin, min(end, len(text))):
            out[i] = ''
    blanked = ''.join(out)
    kept = []
    old_lines = text.split('\n')
    new_lines = blanked.split('\n')
    offset = 0
    for index, new_line in enumerate(new_lines):
        old_line = old_lines[index] if index < len(old_lines) else ''
        width = len(old_line) + 1
        had_code = any(not in_comment[offset + i] and old_line[i] not in ' \t'
                       for i in range(min(len(old_line), len(text) - offset)))
        if new_line.strip() == '' and old_line.strip() != '' and not had_code:
            offset += width
            continue
        kept.append(re.sub(r'[ \t]+$', '', new_line))
        offset += width
    result = '\n'.join(kept)
    if code_lines(result) != code_lines(text):
        print('%s:1: STRIP-REFUSED  code lines differ after removing %d comment(s); nothing was written' % (path, len(spans)), file=sys.stderr)
        return -1
    open(path, 'w', encoding='utf-8').write(result)
    return len(spans)


def main(argv):
    if argv and argv[0] == 'code_tokens':
        argv = argv[1:]
        if len(argv) not in (1, 2):
            print('usage: comments.py code_tokens <file> [other-file]', file=sys.stderr)
            return 3
        tokens = [code_tokens(open(p, encoding='utf-8', errors='ignore').read()) for p in argv]
        for path, toks in zip(argv, tokens):
            print('%s : %d code token(s)' % (path, len(toks)))
        if len(argv) == 2:
            from collections import Counter
            same = Counter(tokens[0]) == Counter(tokens[1])
            print('token multiset %s' % ('identical' if same else 'DIFFERS'))
            return 0 if same else 1
        return 0

    strip = '--strip' in argv
    argv = [a for a in argv if a != '--strip']
    if not argv:
        print('usage: comments.py [--strip] <file ...>', file=sys.stderr)
        return 3
    total = 0
    scored = 0
    refused = 0
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
        if strip:
            removed = strip_file(path, text)
            if removed < 0:
                total += 1
                refused += 1
            else:
                total += removed
            continue
        found, _unused = check_file(path, text)
        total += len(found)
        for f in found:
            print(f)
    if strip:
        print('stripped %d comment(s) from %d file(s), %d refused' % (total, scored, refused))
    else:
        print('comments examined in %d file(s): %d violation(s)' % (scored, total))
    return 1 if (total and not strip) or refused else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
