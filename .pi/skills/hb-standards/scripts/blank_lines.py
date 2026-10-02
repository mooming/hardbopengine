#!/usr/bin/env python3
"""Check rule set A of docs/CodingStandards.md — the blank-line shapes, never the paragraphs.

A blank line asserts that the line under it belongs to a different thought than the line above it.
Whether that assertion is TRUE is a judgement about what the code is for, so this script has no opinion
about it and cannot have one. What it checks is the part of the rule that is a shape in the text: how
many blanks sit at each named seam, and the two places a blank is required by the rule rather than
chosen — before `return`, and after the `}` of a nested block.

Rules, named as the standard names them:

  A1   copyright -> `#pragma once`, and `#pragma once` -> the first include: exactly one blank
  A2   between two directive blocks inside the preamble: exactly one
  A3   the preamble -> the first code body: exactly two, the only legal double in a file. A comment
       sitting flush above the first line of code introduces it, so the seam belongs above the comment;
       a comment separated from the code by a blank is preamble, and the seam goes under it. That is the
       shape `Engine/CodingStandards.h` already passes the format gate with
  A4   immediately after an opening `{`, and between a header line and its `{`: none
  A5   immediately before a closing `}` — function body, class body, `} // namespace hbe`: none
  A6   immediately after an access specifier: none
  A10  before `return`: exactly one, unless the return is the only statement in its scope
  A11  after the `}` of a nested block: exactly one
  A12  between two definitions at namespace scope: exactly one
  A13  a `///` line and the declaration under it: none
  A14  before a file-scope `#ifdef __UNIT_TEST__` region: exactly one
  A15  inside a parenthesised list, or between the lines of one: none
  A16  anywhere else: two or more consecutive blanks are forbidden, and a conditional region holding
       nothing but blanks separates nothing

Six precedence decisions, because the rules collide and only one of them can win:

  * A15 beats everything inside a list, where a brace is data rather than a block.
  * A13 beats A3, so a `///` pointer with a blank beneath it is reported as the pointer fault it is. Fix
       that and the two-blank seam moves above the pointer, where the standard wants it.
  * A5 beats A11 and A12: a blank between `} // namespace examples` and `} // namespace hbe` is
       simultaneously after a closing brace and before one, and the rule that keeps it out is the one
       that keeps a block's last line touching its own brace.
  * A14 beats A11, so a test guard opens on exactly one blank even after a closing brace.
  * A4 beats A10, which is how "unless the return is the only statement in its scope" is implemented
       rather than merely asserted: a blank under an opening brace is already illegal, so a lone return
       can never be asked for one.
  * A10 treats a `#if`/`#else` line as an opener, so the first return inside a conditional region is the
       first statement of a scope for reading purposes.

A6 and A12 are also what clang-format already does, so they surface only when something bypassed the
formatter. A8, A9 and the judgement half of A11 — which seam is a real paragraph — are deliberately
absent. A checker that voted on paragraphs would be guessing, and a guess delivered as a finding is how
a sweep starts deleting the blanks a reader put there on purpose.

Exit status: 0 clean, 1 findings, 3 could not run (no files, unreadable file, or a file this lexer
refuses to guess about).
"""

import argparse
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

import comments  # noqa: E402  (the comment lexer and the tokenizer are not to be duplicated)

ACCESS = re.compile(r'^(?:public|protected|private)\s*:')
CASE_LABEL = re.compile(r'^(?:case\b.*|default)\s*:$')
RETURN_LINE = re.compile(r'^(?:co_return|return)\b')
COND_OPEN = re.compile(r'^#\s*(?:if|ifdef|ifndef)\b')
COND_ELSE = re.compile(r'^#\s*else\b')
COND_CLOSE = re.compile(r'^#\s*(?:else|elif|endif)\b')
COND_END = re.compile(r'^#\s*endif\b')
UNIT_TEST_GUARD = '#ifdef __UNIT_TEST__'
INCLUDE_ANGLE = re.compile(r'^#\s*include\s*<')
INCLUDE_QUOTED = re.compile(r'^#\s*include\s*"([^"]+)"')
COND_DIRECTIVE = re.compile(r'^#\s*(?:if|ifdef|ifndef|else|elif|endif)\b')
DEFINE_DIRECTIVE = re.compile(r'^#\s*define\b')
PRAGMA_DIRECTIVE = re.compile(r'^#\s*pragma\b')
INCLUDE_KINDS = ('own', 'std', 'proj')
STATEMENT_END = (';', '{', '}')
A3_EXPECT = 2
RULES = ('A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A10', 'A11', 'A12', 'A13', 'A14', 'A15', 'A16')


class Finding(object):
    def __init__(self, rule, line, message):
        self.rule = rule
        self.line = line
        self.message = message


def directive_kind(stripped, own_stem):
    """Which include block a preamble line belongs to, or which kind of directive it is.

    `own` is the file's own header, matched the way clang matches it: the basename of the quoted path
    with the extension removed is this file's stem. `cond` exists because a conditional that hugs an
    include region is not a seam — it opens no paragraph, and a rule that read `#ifdef` as one asked
    for a blank line before every `#include` inside a platform block.
    """
    if not stripped.startswith('#'):
        return None
    if INCLUDE_ANGLE.match(stripped):
        return 'std'
    quoted = INCLUDE_QUOTED.match(stripped)
    if quoted:
        base = quoted.group(1).rsplit('/', 1)[-1]
        if own_stem and base in ('%s.h' % own_stem, '%s.hpp' % own_stem, '%s.inl' % own_stem):
            return 'own'
        return 'proj'
    if COND_DIRECTIVE.match(stripped):
        return 'cond'
    if PRAGMA_DIRECTIVE.match(stripped):
        return 'pragma'
    if DEFINE_DIRECTIVE.match(stripped):
        return 'define'
    return 'other'


def line_facts(text, own_stem=None):
    """One record per line: the tokens clang would see, plus brace depth and statement-local paren depth.

    The tokenizer runs over the whole file rather than line by line, because a backslash continuation
    carries a string literal across a newline and a per-line pass reads its tail as code. Comments are
    blanked by the comment ban's own lexer, so a brace inside a comment is not a brace.

    `paren_local` is deliberately NOT the running paren depth. `AddTest("x", [this](auto& ls)` leaves one
    paren open for the whole body of the lambda it introduces, so the running depth stays above zero for
    a hundred lines and reported every paragraph inside that lambda as an A15 violation — 1,917 of them
    on the first tree-wide run. Statement-local depth resets at every `;`, `{` or `}`, which is the
    difference between a list still being written and a call whose arguments happen to be a function.
    """
    code = comments.code_only(text)
    code_starts = comments.line_starts(code)
    per_line = {}
    for match in comments.TOKEN.finditer(code):
        per_line.setdefault(comments.line_of(code_starts, match.start()) - 1, []).append(match.group(0))

    facts, brace, paren, seen_directive = [], 0, 0, False
    text_starts = comments.line_starts(text)          # code_only strips trailing whitespace, so offsets differ
    inside_block = set()
    for begin, end, kind in comments.comment_spans(text):
        if kind != 'block':
            continue
        first = comments.line_of(text_starts, begin)              # 1-based, as line_of returns it
        last = comments.line_of(text_starts, end)
        inside_block.update(lineno - 1 for lineno in range(first + 1, last + 1))
    for index, source in enumerate(text.split('\n')):
        tokens = per_line.get(index, [])
        stripped = source.strip()
        directive = stripped.startswith('#')
        seen_directive = seen_directive or directive
        open_b, close_b = tokens.count('{'), tokens.count('}')
        paren = max(0, paren + tokens.count('(') - tokens.count(')'))
        if tokens and tokens[-1] in STATEMENT_END:
            pending = 0
        else:
            pending = paren
        facts.append({
            'index': index,
            'raw': source,
            'stripped': stripped,
            'tokens': tokens,
            'code': bool(tokens),
            'comment': not tokens and bool(stripped),
            'blank': not stripped,
            'continuation': index in inside_block,
            'directive': directive,
            'kind': directive_kind(stripped, own_stem),
            'directive_above': seen_directive,
            'brace_after': brace + open_b - close_b,
            'paren_local': pending,
        })
        brace += open_b - close_b
        paren = pending
    return facts


def first_body_index(facts):
    """The first line that is code and is not a preprocessor directive."""
    for fact in facts:
        if fact['code'] and not fact['directive']:
            return fact['index']
    return -1


def preamble_end(facts):
    """Where the include and define preamble stops, which is not the same question as where A3's seam is.

    A file with no code at all — `Engine/Config/BuildConfig.h` is all platform detection and `#define`s —
    has no A3 seam, because there is no first code body to introduce. It still has a preamble, and rules A1
    and A2 still govern its blanks. Reading "no seam" as "no preamble" switched those two rules off for
    exactly the file that needs them, so the two boundaries are computed separately.
    """
    first = first_body_index(facts)
    return first if first >= 0 else len(facts)


def seam_target(facts):
    """Where the two blanks go: the top of the first body paragraph, introducing comments included.

    Comment lines immediately above the first line of code are adjacent to it, which is the same thing as
    "attached to it" — a blank between them would itself be a line in between. So walking up over
    adjacent comment lines finds the paragraph's top, and one walk covers both the file comment and a
    doc comment on the first declaration.

    A file whose first line of code sits inside a conditional region has no seam at all, which is
    `inside_conditional`.
    """
    first = first_body_index(facts)
    if first < 0 or inside_conditional(facts, first):
        return -1
    target = first
    while target - 1 >= 0 and facts[target - 1]['comment']:
        target -= 1
    return target


def inside_conditional(facts, index):
    """Whether an unclosed `#if` region covers line `index`.

    Rule A3's seam is where the preamble ends and the file's own code begins — at file level. When the first
    line of code is inside a conditional, the preamble never closed: `Engine/Config/BuildConfig.h` is a
    define header whose first code is a platform `static_assert` inside its own `#if`, and the two blanks
    A3 demands would have to be written between that directive and the line it guards, the exact position
    the conditional-hug rule forbids. So the honest answer is that the file owns no A3 seam, and a checker
    that reported one was about to instruct a reader to break a rule to satisfy another.
    """
    depth = 0
    for fact in facts[:index]:
        if COND_OPEN.match(fact['stripped']):
            depth += 1
        elif COND_END.match(fact['stripped']):
            depth -= 1
    return depth > 0


def is_opener(fact):
    """A line after which the next line is the first thing in a fresh scope."""
    if fact['directive'] and (COND_OPEN.match(fact['stripped']) or COND_ELSE.match(fact['stripped'])):
        return True
    if not fact['tokens']:
        return False
    if fact['tokens'][-1] in ('{', ')', ':'):
        return True
    return fact['stripped'].startswith(('else', 'case ', 'default')) or bool(ACCESS.match(fact['stripped']))


def return_is_alone(facts, prev):
    """True when nothing but openers, labels and directives stand above the return."""
    for fact in reversed(facts[:prev['index'] + 1]):
        if fact['blank'] or fact['comment']:
            continue
        if fact['directive'] and not (COND_OPEN.match(fact['stripped']) or COND_ELSE.match(fact['stripped'])):
            continue
        if CASE_LABEL.match(fact['stripped']):
            return True
        return is_opener(fact)
    return True


def classify(facts, target, prev, nxt, preamble=None):
    """The legal blank-line range for one gap, as (low, high, rule)."""
    if prev is None:
        return 0, 0, 'A16'                                       # a file may not open on a blank line
    if nxt is None:
        return 0, 1, 'A16'                                       # the final newline is hygiene, checked elsewhere
    if prev['paren_local'] > 0 or (prev['tokens'] and prev['tokens'][-1] in (',', '(')) \
            or (nxt['tokens'] and nxt['tokens'][0] == ','):
        return 0, 0, 'A15'
    if prev['comment'] and prev['stripped'].startswith('///'):
        return 0, 0, 'A13'
    if COND_OPEN.match(prev['stripped']) and COND_CLOSE.match(nxt['stripped']):
        return 0, 0, 'A16'                                       # a conditional region of blanks alone is no paragraph
    if nxt['stripped'].startswith(UNIT_TEST_GUARD) and not nxt['raw'][:1].isspace():
        return 1, 1, 'A14'
    if nxt['index'] == target and prev['directive_above']:
        return A3_EXPECT, A3_EXPECT, 'A3'
    if 0 <= nxt['index'] < (preamble_end(facts) if preamble is None else preamble):
        return preamble_expectation(prev, nxt)
    if prev['tokens'] and prev['tokens'][-1] == '{':
        return 0, 0, 'A4'
    if nxt['tokens'] and nxt['tokens'][0] == '{':
        return 0, 0, 'A4'                                        # a header line and its brace are one construct
    if nxt['tokens'] and nxt['tokens'][0] == '}':
        return 0, 0, 'A5'
    if prev['tokens'] and prev['tokens'][0] == '}' \
            and nxt['tokens'] and nxt['tokens'][0] in ('else', 'catch', 'while'):
        return 0, 0, 'A5'
    if nxt['directive']:
        return 0, 1, 'A16'            # a preprocessor line is not a definition, so no seam is owed to it
    if prev['directive']:
        # A directive opens a region it wraps, and the seam that organises it sits above the directive, not
        # between the directive and the line it guards — demanding a blank between `#pragma clang diagnostic
        # ignored` and the statement that pragma exists to silence would push a pragma away from the code it
        # governs. This is where A10 stops; `CreateWithMove` in Engine/CodingStandards.cpp is the case.
        return 0, 1, 'A16'
    if prev['tokens'] and prev['tokens'][0] == '}':
        return 1, 1, ('A12' if prev['brace_after'] == 0 else 'A11')
    if RETURN_LINE.match(nxt['stripped']) and not return_is_alone(facts, prev):
        return 1, 1, 'A10'
    if ACCESS.match(prev['stripped']):
        return 0, 0, 'A6'
    return 0, 1, 'A16'


def preamble_expectation(prev, nxt):
    """What a gap inside the preamble may hold.

    A2 is about *blocks*, not lines: includes inside one block sit flush, because the block is one sorted
    list, and exactly one blank separates two blocks. A conditional directive hugs the region around it
    and opens no seam at all, which is what stopped the first version demanding a blank before every
    `#include` in a platform block.
    """
    above, below = prev['kind'], nxt['kind']
    if above == 'pragma':
        return 1, 1, 'A1'
    if below is None:
        if prev['comment']:
            return 0, 1, 'A16'                                   # two comment blocks in a preamble are one paragraph
        return 1, 1, 'A2'                                        # a preamble comment hangs off the block above it
    if above is None or 'cond' in (above, below):
        return 0, 1, 'A16'                                       # a conditional hugs the region it wraps and opens no seam
    if above in INCLUDE_KINDS and below in INCLUDE_KINDS:
        return (0, 0, 'A2') if above == below else (1, 1, 'A2')
    if above in INCLUDE_KINDS or below in INCLUDE_KINDS:
        return 1, 1, 'A2'                                        # the include region and a define block are two blocks
    return 0, 1, 'A16'


def blanks_between(facts, above, below):
    """How many blank lines sit strictly between two line indices."""
    return sum(1 for fact in facts[above + 1:below] if fact['blank'])


def boundaries(facts):
    """Every pair of adjacent comment-block-ending and code lines, as (line above, line below, blanks between).

    Three things this has to get right, each of which produced a false finding in its first draft:

      * a pair with zero blanks between is a boundary like any other, because half the rules require a
        blank rather than forbid one. Reporting only the places a blank already sits would make A10,
        A11, A12 and A14 unfalsifiable, which is the same as saying they were checked;
      * the interior of a block comment is one unit, not a run of solid lines. Treating ` * note` and
        ` */` as neighbours of each other asked for a blank between them and reported a well-formed
        comment as two A2 violations;
      * the count is of *blank* lines between the pair, not of index distance, once continuations are
        skipped — otherwise a five-line comment contributes four phantom blanks to the seam under it.
    """
    solid = [fact for fact in facts if not fact['blank'] and not fact['continuation']]
    found = []
    for position, fact in enumerate(solid):
        if position == 0:
            if fact['index']:
                found.append((None, fact, blanks_between(facts, -1, fact['index'])))
        else:
            found.append((solid[position - 1], fact,
                          blanks_between(facts, solid[position - 1]['index'], fact['index'])))
    if solid and facts[-1]['blank']:
        found.append((solid[-1], None, blanks_between(facts, solid[-1]['index'], len(facts))))
    return found


def check_text(text, path=None):
    facts = line_facts(text, own_stem(text, path))
    target = seam_target(facts)
    findings = []
    for prev, nxt, gap in boundaries(facts):
        low, high, rule = classify(facts, target, prev, nxt, preamble_end(facts))
        line = (nxt['index'] + 1) if nxt is not None else len(facts)
        if gap < low:
            findings.append(Finding(rule, line, 'rule %s wants %d blank line(s) here, found %d' % (rule, low, gap)))
        elif gap > high:
            findings.append(Finding(rule, line,
                                    'rule %s allows at most %d blank line(s) here, found %d' % (rule, high, gap)))
    return findings


def own_stem(text, path):
    """The stem clang would match an own header against, from the file's own path.

    A text checked without a path — every fixture, and stdin — has no own header, which is the honest
    answer rather than a guess: `#include "Foo.h"` in Foo.inl is the file including itself only if the
    file is Foo.h.
    """
    if not path:
        return None
    return os.path.splitext(os.path.basename(path))[0]


def check_file(path):
    try:
        with open(path, encoding='utf-8') as handle:
            text = handle.read()
    except OSError as error:
        return None, str(error)
    try:
        return check_text(text, path), None
    except ValueError as error:                       # unterminated literal: refuse rather than mis-lex
        return None, 'cannot lex: %s' % error


def collapse_seam(text, path=None):
    """Return `text` with rule A3's sanctioned double blank reduced to one.

    The format gate needs this on both sides of its comparison. clang-format deletes the second blank at
    the preamble seam whenever the line under it is a `namespace`, a `class` or a function definition
    (measured: 1, 2, 3 and 4 blanks in, 1 blank out), so a file that obeys A3 can never be byte-identical
    to its own formatted form, and a gate that diffs those two byte-for-byte reports the exemplar as
    unformatted — which is the measurement that retired this convention once already. Collapsing the one
    position where the standard and the formatter disagree makes the gate ask what it means to ask: does
    this file differ from its formatted form anywhere the formatter is the authority.

    Everything else arrives untouched, including a double blank the checker would report under A16: that
    is a rule violation rather than a formatter disagreement, so it stays visible in the diff.
    """
    facts = line_facts(text, own_stem(text, path))
    target = seam_target(facts)
    drop = set()
    for prev, nxt, gap in boundaries(facts):
        if nxt is None:
            continue
        _low, _high, rule = classify(facts, target, prev, nxt, preamble_end(facts))
        if rule == 'A3' and gap > 1:
            drop.update(fact['index'] for fact in facts[prev['index'] + 1:nxt['index']][:gap - 1])
    return '\n'.join(line for index, line in enumerate(text.split('\n')) if index not in drop)


# ----------------------------------------------------------------------- fixtures --
CASES = []


def case(name, source, expect):
    CASES.append((name, source, expect))


HEAD = '#include <atomic>\n\n\n'                                # a preamble sitting at A3, then the body

case('A2: two adjacent includes inside one block sit flush',
     '#include <atomic>\n#include <memory>\n\n#include "Core/Debug.h"\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [])

case('A2: a blank inside one include block is a false seam',
     '#include <atomic>\n\n#include <memory>\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [('A2', 3)])

case('A2: a conditional hugging includes opens no seam',
     '#include <atomic>\n#ifdef HB_PLATFORM_X\n#include <malloc.h>\n#endif\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [])

case('A3 clean: two blanks after the preamble',
     HEAD + 'namespace hbe\n{\n\tvoid F();\n} // namespace hbe\n', [])

case('A3: one blank after the preamble',
     '#include <atomic>\n\nnamespace hbe\n{\n} // namespace hbe\n', [('A3', 3)])

case('A3: no blank after the preamble',
     '#include <atomic>\nnamespace hbe\n{\n} // namespace hbe\n', [('A3', 2)])

case('A3 with a preamble comment separated from the body (the exemplar shape)',
     '#include <atomic>\n\n/*\n * file comment\n */\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [])

case('A3 with a comment flush above the body: the seam moves above the comment',
     '#include <atomic>\n\n/// API reference: docs/Core/Foo/index.html\nclass Foo\n{\n};\n', [('A3', 3)])

case('A1: two blanks after #pragma once',
     '#pragma once\n\n\n#include <atomic>\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [('A1', 4)])

case('A2: no blank between the include blocks',
     '#include <atomic>\n#include "Core/Debug.h"\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [('A2', 2)])

case('A2: two blanks between the include blocks',
     '#include <atomic>\n\n\n#include "Core/Debug.h"\n\n\nnamespace hbe\n{\n} // namespace hbe\n', [('A2', 4)])

case('A4: a blank after an opening brace',
     HEAD + 'namespace hbe\n{\n\n\tvoid F();\n} // namespace hbe\n', [('A4', 7)])

case('A4: a blank between a header line and its brace',
     HEAD + 'class C\n\n{\n};\n', [('A4', 6)])

case('A5: a blank before a closing brace',
     HEAD + 'namespace hbe\n{\n\tvoid F();\n\n} // namespace hbe\n', [('A5', 8)])

case('A5 beats A11: a blank inside the last brace pair of a body',
     HEAD + 'void F()\n{\n\tif (a)\n\t{\n\t\tb = 1;\n\n\t}\n} // namespace hbe\n', [('A5', 10)])

case('A5 beats A12: a blank before the outer namespace close',
     HEAD + 'namespace a\n{\nvoid F();\n} // namespace a\n\n} // namespace hbe\n', [('A5', 9)])

case('A11 clean, and A4 and A5 hold',
     HEAD + 'void F()\n{\n\tif (a)\n\t{\n\t\tb = 1;\n\t}\n\n\tc = 2;\n}\n', [])

case('A11: no blank after the close of a nested block',
     HEAD + 'void F()\n{\n\tif (a)\n\t{\n\t\tb = 1;\n\t}\n\tc = 2;\n}\n', [('A11', 10)])

case('A12: two definitions with no blank between them',
     HEAD + 'void F()\n{\n}\nvoid G()\n{\n}\n', [('A12', 7)])

case('A10: no blank before a return that is not alone',
     HEAD + 'int F()\n{\n\tint a = 1;\n\treturn a;\n}\n', [('A10', 7)])

case('A10 clean: the return is the only statement in its scope',
     HEAD + 'int F()\n{\n\treturn 1;\n}\n', [])

case('A10 clean: a braceless control header, its return, and a blank before the next',
     HEAD + 'bool F(int x)\n{\n\tif (x > 0)\n\t\treturn true;\n\n\treturn false;\n}\n', [])

case('A10: the second return of a function needs its blank',
     HEAD + 'bool F(int x)\n{\n\tif (x > 0)\n\t\treturn true;\n\treturn false;\n}\n', [('A10', 8)])

case('A10 clean: the first return of a conditional region is its first statement',
     HEAD + 'int F()\n{\n\tint a = 1;\n\n#ifdef X\n\treturn a;\n#endif\n}\n', [])

case('A13: a blank under a /// line',
     HEAD + '/// API reference: docs/Core/Foo/index.html\n\nclass Foo\n{\n};\n', [('A2', 4), ('A13', 6)])

case('A14: the unit-test guard opens on no blank',
     HEAD + 'class C\n{\n};\n#ifdef __UNIT_TEST__\n#endif\n', [('A14', 7)])

case('A14 clean, and the guard is not an A12 definition pair',
     HEAD + 'class C\n{\n};\n\n#ifdef __UNIT_TEST__\n#endif\n', [])

case('A14: an in-function guard is not a trailing region',
     HEAD + 'int main()\n{\n\tint a = 1;\n\n#ifdef __UNIT_TEST__\n\treturn a;\n#else\n\treturn 0;\n#endif\n}\n', [])

case('A15: a blank between the lines of an initializer list',
     HEAD + 'C::C()\n\t: a(1)\n\n\t, b(2)\n{\n}\n', [('A15', 7)])

case('A15: a blank inside an argument list',
     HEAD + 'F(a,\n\n\tb);\n', [('A15', 6)])

case('A16: two blanks between statements',
     HEAD + 'void F()\n{\n\ta = 1;\n\n\n\tb = 2;\n}\n', [('A16', 9)])

case('A16: a conditional region holding nothing but blanks',
     HEAD + 'void F()\n{\n\ta = 1;\n\n#ifdef X\n\n#endif\n\n\tb = 2;\n}\n', [('A16', 10)])

case('A12 does not owe a blank to #endif',
     HEAD + 'namespace hbe\n{\nvoid F();\n} // namespace hbe\n#ifdef X\n#endif\n', [])

case('A16: a file may not open on a blank line',
     '\n' + HEAD + 'void F();\n', [('A16', 2)])

case('a multi-line block comment is one unit, not four neighbours',
     HEAD + '/*\n * one\n * two\n */\nclass C\n{\n};\n', [])

case('A3 does not apply to a file with no preamble at all',
     'namespace hbe\n{\n\tvoid F();\n} // namespace hbe\n', [])


# A collapse fixture is (name, text, what the text must become). The interesting rows are the two that
# must NOT collapse: a double blank that rule A16 forbids, and a double blank the formatter itself keeps.
COLLAPSE_CASES = [
    ('A3 at two blanks collapses to the shape clang-format produces',
     '#include <atomic>\n\n\nnamespace hbe\n{\n} // namespace hbe\n',
     '#include <atomic>\n\nnamespace hbe\n{\n} // namespace hbe\n'),
    ('A3 already at one blank is left alone',
     '#include <atomic>\n\nnamespace hbe\n{\n} // namespace hbe\n',
     '#include <atomic>\n\nnamespace hbe\n{\n} // namespace hbe\n'),
    ('a seam under a preamble comment collapses too, above the comment',
     '#include <atomic>\n\n\n/// API reference: docs/Core/Foo/index.html\nclass Foo\n{\n};\n',
     '#include <atomic>\n\n/// API reference: docs/Core/Foo/index.html\nclass Foo\n{\n};\n'),
    ('an A16 double blank is a violation, not a formatter disagreement, so it stays visible',
     '#include <atomic>\n\n\nnamespace hbe\n{\n\n\tint a;\n\n\n\tint b;\n} // namespace hbe\n',
     '#include <atomic>\n\nnamespace hbe\n{\n\n\tint a;\n\n\n\tint b;\n} // namespace hbe\n'),
]


case('A10: a directive and the statement it wraps are one paragraph',
     '#include <atomic>\n\n\nvoid F() noexcept\n{\n\tint a = 1;\n#pragma clang diagnostic push\n'
     '#pragma clang diagnostic ignored "-Wpessimizing-move"\n\treturn a;\n#pragma clang diagnostic pop\n'
     '} // F\n', [])


case('a file whose first code is inside a #if region owns no A3 seam but keeps A1 and A2',
     '// Copyright.\n\n#pragma once\n\n\n#define FOO 1\n\n#if !defined(FOO)\nstatic_assert(false, "no FOO");\n'
     '#endif\n\n#define BAR 2\n', [('A1', 6)])


def run_selftest():
    failures = 0
    for name, source, expect in CASES:
        try:
            got = sorted((f.rule, f.line) for f in check_text(source))
        except ValueError as error:
            print('FAIL %-62s lexer: %s' % (name, error))
            failures += 1
            continue
        if got != sorted(expect):
            print('FAIL %-62s want %s got %s' % (name, sorted(expect), got))
            failures += 1
        else:
            print('ok   %-62s %s' % (name, ','.join(sorted(set(rule for rule, _ in expect))) or 'clean'))
    for name, source, want in COLLAPSE_CASES:
        got = collapse_seam(source)
        if got != want:
            print('FAIL %-62s got %r' % (name, got))
            failures += 1
        else:
            print('ok   %-62s collapse' % name)
    print('%d fixture(s), %d failure(s)' % (len(CASES) + len(COLLAPSE_CASES), failures))
    return 1 if failures else 0


def main(argv):
    parser = argparse.ArgumentParser(description='Check the blank-line rules of docs/CodingStandards.md')
    parser.add_argument('files', nargs='*', help='C++ sources to check')
    parser.add_argument('--selftest', action='store_true', help='run the built-in fixtures')
    parser.add_argument('--collapse-seam', action='store_true',
                        help='read one file (or stdin) and print it with rule A3\'s double blank reduced to one, '
                             'which is the shape clang-format produces; the format gate compares two texts '
                             'through this so the one place the standard and the formatter disagree cannot '
                             'fail a conforming file')
    parser.add_argument('--summary-only', action='store_true', help='print counts, not findings')
    args = parser.parse_args(argv)

    if args.collapse_seam:
        if args.files:
            with open(args.files[0], encoding='utf-8') as handle:
                sys.stdout.write(collapse_seam(handle.read(), args.files[0]))
        else:
            sys.stdout.write(collapse_seam(sys.stdin.read()))
        return 0

    if args.selftest:
        return run_selftest()
    if not args.files:
        print('[NONE] blank lines: no files given, so nothing was measured')
        return 3

    total, dirty, counts = 0, 0, {}
    for path in args.files:
        findings, error = check_file(path)
        if findings is None:
            print('[NONE] blank lines — %s: %s' % (path, error))
            return 3
        for finding in findings:
            counts[finding.rule] = counts.get(finding.rule, 0) + 1
        total += len(findings)
        if findings:
            dirty += 1
            if not args.summary_only:
                print('[FAIL] blank lines — %s' % path)
                for finding in findings:
                    print('    %s:%d: [%s] %s' % (path, finding.line, finding.rule, finding.message))
    detail = ', '.join('%s=%d' % (rule, counts[rule]) for rule in RULES if counts.get(rule))
    print('blank lines: %d finding(s) in %d file(s)%s' % (total, dirty, ' — ' + detail if detail else ''))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
