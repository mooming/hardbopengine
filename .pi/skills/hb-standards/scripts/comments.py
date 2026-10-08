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

Arguments are source files, never directories, and the exit codes are the sibling gate's: a directory,
an unreadable file or an empty argument is a usage error and exits 3, the way `blank_lines.py` and
`includes.py` treat one. A finding is only ever a comment in a file this run actually read, because a
count that mixes "the rule was broken" with "the checker could not look" cannot be reconciled against
the sum of the per-file runs, and a run that read nothing must never exit 0.
"""

import os
import re
import sys

COPYRIGHT = re.compile(r'Copyright \(c\).*Hansol Park')
IGNORE_TAG = 'hb-standards:ignore'

CLOSING_LINE = re.compile(r'^[{}\s]*$|^#[ \t]*(endif|else|elif)\b')
DIRECTIVE = re.compile(r'^(if|ifdef|ifndef|else|elif|endif)\b(.*)$', re.S)

NAMESPACE_HEAD = re.compile(r'\bnamespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)')
# `namespace` with no name — the marker pushed for its brace so an anonymous body is not mistaken
# for a plain block. Chosen so no namespace identifier can collide with it.
ANONYMOUS = '\x00anonymous'
ANONYMOUS_HEAD = re.compile(r'\bnamespace\b(?!\s*[A-Za-z_:])')

POINTER = re.compile(r'^///\s*API reference:\s*(\S+)\s*$')
POINTER_SHAPE = re.compile(r'^docs/([^/]+)/([^/]+)/index\.html$')
# A small surface — a header of aliases, free functions and macros — is documented in a section of its module's
# page rather than in a page of its own, because a page for `TByte` would be three sentences of nothing and the
# module page already carries the tables for Types, Runnable and the Debug and Time functions. The address is
# then the module page plus the anchor of the section that owns it, and `anchor_resolves` below is what stops
# that from becoming a pointer to a heading somebody deleted.
POINTER_ANCHOR = re.compile(r'^docs/([^/]+)/index\.html#([A-Za-z][\w:-]*)$')
# An attribute-specifier-seq sits between a type keyword and the name it declares, so a pattern that reads
# the first identifier after the keyword captures `alignas` instead of the entry. The same grammar is in
# `docs_coverage.py`'s ENTRY, which found this on `Engine/Core/ResultPacket.h` first; the two are edited
# together so the two readers of one fact cannot disagree about what a declaration is called.
TYPE_HEAD = re.compile(r'^\s*(?:class|struct|union|enum(?:\s+(?:class|struct))?)\b')
ATTRIBUTE_PREFIX = re.compile(r'\s*(?:alignas\s*\((?:[^()]|\([^()]*\))*\)|\[\[[^\]]*\]\]'
                             r'|__attribute__\s*\(\(.*?\)\))')
# A name that comes back one of these is not a name, so the reader says it cannot read the line rather than
# hand back a token that would let a pointer be accepted, or refused, for the wrong reason. An export macro
# (`struct HBE_API ScopedLock`) lands here on purpose: guessing which identifier is the macro and which is
# the type is a coin toss, and a coin toss reported as a fact is worse than no answer.
SPECIFIER_WORDS = frozenset(('alignas', 'alignof', 'declspec', '__declspec', 'attribute', 'final',
                            'constexpr', 'consteval', 'constinit', 'inline', 'static', 'extern',
                            'thread_local', 'mutable', 'friend', 'explicit', 'operator', 'struct',
                            'class', 'union', 'enum', 'template', 'typename', 'void'))
# What may legally follow the name a type keyword declares. A second bare identifier in that position means
# the name was not the first identifier after all — an export macro (`struct HBE_API ScopedLock`), or an
# attribute spelling this reader does not consume — and the difference between "that token is the name" and
# "that token is a macro" cannot be settled from one line. So the reader declines, and says so, instead of
# answering with a coin toss. `final` is the one keyword the standard allows there.
AFTER_NAME = re.compile(r'^\s*(?:final\b|[{:;,=]|$)')
ENTRY_ALIAS = re.compile(r'^\s*using\s+([A-Za-z_]\w*)\s*=')
ENTRY_DEFINE = re.compile(r'^#\s*define\s+([A-Za-z_]\w*)\b')
CONCEPT_DECL = re.compile(r'^\s*concept\s+([A-Za-z_]\w*)')
TEMPLATE_LINE = re.compile(r'^\s*template\s*<')

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
    variants is what lets `//__TEST__` through while `// TODO` still reads as a comment: the
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
    engine uses begin with an underscore — 104 `#endif //__TEST__` labels — and it would have
    accepted `// TODO`, since a capitalised word matches `[A-Z][A-Z0-9_]*`. Comparing the label
    against what the line actually closes gets both right, and it is what stops the exemption from
    becoming a hole through which any short comment can pass.

    Comparing against what the line closes only works if the stack is a stack. `#else` and `#elif`
    belong to the guard still open, so they read the top and keep it; `#endif` alone closes a guard,
    so `#endif` alone pops. Reading without popping was the second bug in this function and the more
    damaging one: the first nested guard in a file stayed on top forever, every later `#else` and
    `#endif` was judged against the inner condition, and a permitted outer label such as
    `#else // PROFILE_ENABLED` in `Engine/Core/ScopedLock.h` was therefore reported as a violation and
    deleted by `--strip`. The pop is inside `if guard_stack` because an `#endif` with nothing open must
    be ignored rather than raise: a checker that crashes on a malformed file has lost its verdict, and
    `--selftest` pins the whole stack discipline.
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
            # A newline is a token boundary, not nothing. Accumulating nothing fused the last word of
            # one line onto the first of the next, so `#ifdef __TEST__` above `namespace hbe` put
            # `__TEST__namespace` in front of the keyword, `\bnamespace` refused to match, and the
            # namespace was never pushed — which cost its closing `} // namespace hbe` the structural-label
            # exemption and let `--strip` delete a label the standard protects. Any directive ending in a
            # word character did this, which is why a region that opens with an `#include` was unaffected:
            # the quote is not a word character, so the boundary survived by accident.
            pending = (pending + ' ')[-120:]
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
                    if keyword == 'endif':
                        guard_stack.pop()
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


def entry_name_of(line):
    """The entry name this line declares, or None when it declares none or cannot be read safely.

    Handles the grammar a caller can actually write: an attribute-specifier-seq between the keyword and the
    name (`alignas(std::uint64_t)`, `[[deprecated]]`, `__attribute__((packed))`), and `enum struct` beside
    `enum class`. It stops rather than guesses when the next token is a specifier word, which is how an
    export macro in that position is reported instead of invented into a class name.
    """
    head = TYPE_HEAD.match(line)
    if not head:
        for pattern in (ENTRY_ALIAS, ENTRY_DEFINE):
            found = pattern.match(line)
            if found:
                return found.group(1)
        return None
    rest = line[head.end():]
    while True:
        attribute = ATTRIBUTE_PREFIX.match(rest)
        if not attribute:
            break
        rest = rest[attribute.end():]
    found = re.match(r'\s*([A-Za-z_]\w*)', rest)
    if not found or found.group(1) in SPECIFIER_WORDS:
        return None
    if not AFTER_NAME.match(rest[found.end():]):
        return None
    return found.group(1)


def entry_names(text):
    """Every name this file declares as a documented entry: a type, an alias or a macro.

    The pointer exemption is granted by name, so the set of admissible names has to come from the same
    text the checker is reading. A pointer to `docs/Core/Task/index.html` inside `TaskProvider.h` is
    exactly the copy-paste this catches, and it can only be caught by asking what the file declares.
    """
    names = set()
    for line in text.split('\n'):
        name = entry_name_of(line)
        if name:
            names.add(name)
    return names


def declaration_below(lines, lineno):
    """The name declared after line `lineno`, skipping blank lines, comments and `template` headers.

    A doc comment sits above the `template` line as often as above the class, and the standard's own
    exemplars put the brief on the template header for `ScopedLock`. Skipping the template lines is what
    lets one rule cover both spellings without letting a pointer drift onto an unrelated member.
    """
    for index in range(lineno, min(lineno + 12, len(lines))):
        stripped = lines[index].strip()
        if not stripped or stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            continue
        if TEMPLATE_LINE.match(lines[index]):
            continue
        found = CONCEPT_DECL.match(lines[index])
        if found:
            return ('concept', found.group(1))
        name = entry_name_of(lines[index])
        if name:
            return ('entry', name)
        if TYPE_HEAD.match(lines[index]):
            return ('unreadable', lines[index].strip()[:40])
        return ('other', stripped[:40])
    return ('none', '')


def pointer_violation(path, target, lines, lineno, names):
    """None when the pointer line is admissible, else the category and what to do about it.

    Four things are checked, cheapest first, and each answers a question a prose brief could not: is the
    line shaped like a pointer, does the file it names exist, is it the module this header belongs to,
    and does the entry it names sit immediately below the line. The last one is what stops the pointer
    becoming a decorative comment on the wrong declaration.
    """
    shape = POINTER_SHAPE.match(target)
    if shape is None:
        anchor = POINTER_ANCHOR.match(target)
        if anchor is None:
            return ('POINTER-FORM', 'expected "/// API reference: docs/<Module>/<Entry>/index.html", got "%s"' % target)
        if not anchor_resolves(target):
            return ('POINTER-PATH', '%s does not name an id on that module page, so the pointer leads nowhere'
                    % target)
        module = engine_module(path)
        if module is None:
            return ('POINTER-MODULE', '%s is outside Engine/, so it has no module reference page to point at'
                    % path)
        stem = os.path.basename(path).rsplit('.', 1)[0]
        if anchor.group(2).lower() != stem.lower():
            # The same discipline the page form enforces: an address has to name what it addresses. Without it a
            # header could point at a neighbouring section and every checker would still be satisfied, which is
            # how a reference ends up with a page nobody can find from the code it documents.
            return ('POINTER-ENTRY', 'the section %s does not name %s, so this header is not what that address '
                                     'documents' % (anchor.group(2), stem))
        if anchor.group(1) != module:
            return ('POINTER-MODULE', 'names module %s while this file belongs to %s' % (anchor.group(1), module))
        # The address names a section of the module page, so there is no entry name to compare against and the
        # position test has to be a different question: is what sits below a member of that surface, rather
        # than a type that owns a page in its own right. Without that test, a class could be pointed at its
        # module page and the reference tree would lose the class without a checker ever objecting.
        for index in range(lineno, min(lineno + 12, len(lines))):
            probe = lines[index].strip()
            if not probe or probe.startswith('//') or probe.startswith('/*') or probe.startswith('*'):
                continue
            if TEMPLATE_LINE.match(lines[index]):
                continue
            if TYPE_HEAD.match(lines[index]):
                return ('POINTER-POSITION', 'is a module-page section address while the declaration below is a '
                                            'type, which owns a page under its own name')
            break
        return None
    if not pointer_resolves(target):
        return ('POINTER-PATH', '%s does not exist, so the pointer leads nowhere' % target)
    module = engine_module(path)
    if module is None:
        return ('POINTER-MODULE', '%s is outside Engine/, so it has no module reference folder to point at'
                % path)
    if shape.group(1) != module:
        return ('POINTER-MODULE', 'names module %s while this file belongs to %s' % (shape.group(1), module))
    named = shape.group(2)
    kind, declared = declaration_below(lines, lineno)
    if kind == 'unreadable':
        return ('POINTER-UNREADABLE', 'the declaration below it (%s) is a shape this reader cannot name, so '
                                      'neither the pointer nor the page can be trusted here' % declared)
    stem = os.path.basename(path).rsplit('.', 1)[0]
    if named == stem and named not in names:
        # A utility header — one whose surface is aliases, free functions and macros — is addressed by its own
        # stem, because there is no class name for a pointer to name and docs_coverage demands exactly that
        # page for such a file. The licence is deliberately narrow: the declaration below may not be a type,
        # since a class owns a page in its own right and a pointer wearing the file's name would quietly
        # document the wrong thing. The page's existence was already proved above, so this cannot aim a
        # pointer at nothing, and a header that does own a class is untouched by it.
        for index in range(lineno, min(lineno + 12, len(lines))):
            probe = lines[index].strip()
            if not probe or probe.startswith('//') or probe.startswith('/*') or probe.startswith('*'):
                continue
            if TEMPLATE_LINE.match(lines[index]):
                continue
            if TYPE_HEAD.match(lines[index]):
                return ('POINTER-POSITION', 'names the file %s while the declaration below is a type, which '
                                            'owns a page under its own name' % stem)
            break
        return None
    if named not in names:
        return ('POINTER-ENTRY', '%s declares no entry called %s' % (path, named))
    if kind != 'entry' or declared != named:
        return ('POINTER-POSITION', 'points at %s but the next declaration is %s%s' %
                (named, declared, ' (a concept, which owns no page)' if kind == 'concept' else ''))
    return None


def check_file(path, text):
    """Every comment in this file the ban does not exempt, as findings and as byte ranges.

    The ranges come back too because the strip step must delete exactly what was reported — not a
    re-scan of its own, which could disagree with the checker about what counted.
    """
    findings = []
    spans_kept = []
    if repo_relative(path) in EXEMPT_FILES:
        return findings, spans_kept
    starts = line_starts(text)
    try:
        spans = comment_spans(text)
    except ValueError as exc:
        return ['%s:1: LEXER  %s — the file cannot be lexed, so no verdict is possible' % (path, exc)], spans_kept
    lines = text.split('\n')
    expectations = label_expectations(text)
    names = entry_names(text)
    for begin, end, kind in spans:
        lineno = line_of(starts, begin)
        if lineno == 1 and COPYRIGHT.search(text[begin:end]):
            continue
        line_text = lines[lineno - 1]
        comment_col = begin - starts[lineno - 1]
        body = text[begin:end]
        if IGNORE_TAG in body:
            continue
        pointer = POINTER.match(line_text.strip())
        if pointer:
            bad = pointer_violation(path, pointer.group(1), lines, lineno, names)
            if bad:
                findings.append('%s:%d: %s  %s' % (path, lineno, bad[0], bad[1]))
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


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))


def repo_relative(path):
    """The spelling every rule in this file keys on: repository-relative, whatever the shell handed us.

    Three decisions are made from a path string rather than from its contents — whether the file is the
    exempt exemplar, which module owns its reference pages, and therefore whether a pointer address and a
    documentation guard apply at all. Reading them off the argument as typed made each of them an
    artefact of the working directory, which is how one module's total stopped equaling the sum of its
    files. A path outside the repository keeps its own spelling: the selftest fixtures name files that do
    not exist, and they must resolve to the same answer from any working directory.
    """
    absolute = os.path.abspath(path)
    relative = os.path.relpath(absolute, REPO_ROOT)
    if relative == os.pardir or relative.startswith(os.pardir + os.sep):
        return os.path.normpath(path)
    return relative


def anchor_resolves(target):
    """Whether `docs/<Module>/index.html#<id>` names an id that still exists on that page.

    An anchor is a promise held by a different file than the one carrying the pointer, so it rots silently in
    exactly the way a moved heading rots a wiki link. Checking the id rather than just the file is what keeps
    `#time` honest after somebody renames the section, which is the failure this address form would otherwise
    make invisible.
    """
    path, _, anchor = target.partition('#')
    for candidate in (path, os.path.join(REPO_ROOT, path)):
        if os.path.isfile(candidate):
            try:
                text = open(candidate, encoding='utf-8', errors='ignore').read()
            except OSError:
                return False
            return re.search(r'\bid=["\']%s["\']' % re.escape(anchor), text) is not None
    return False


def pointer_resolves(target):
    """Whether a pointer's path names a real file, tried against the working directory and the repo root.

    Every script in this skill is documented as running from the repository root, and every other check
    here assumes it. This one cannot, because it is the check that decides whether a comment survives a
    strip: run from elsewhere, a naive relative lookup would call every pointer broken and delete the
    addresses along with the prose they were meant to replace.
    """
    return os.path.isfile(target) or os.path.isfile(os.path.join(REPO_ROOT, target))


def engine_module(path):
    """The module a source file belongs to, or None for anything outside Engine/.

    Applications and Examples own no API reference — they are not modules under Engine/ — so their prose
    has no class page to land in and the caller must place it in a guide by hand. That is stated out loud
    below rather than treated as satisfied.

    The lookup is on the repository-relative spelling, so `Engine/Core/Task.h`, `./Engine/Core/Task.h`,
    `/Users/.../Engine/Core/Task.h` and `Task.h` read from inside `Engine/Core` are four spellings of one
    file with one verdict. Keying this on the argument as typed made the same header answer `Core` from
    the repository root and `None` from its own directory, and `None` means "no reference page needed": a
    module run launched from inside the module reported 31 findings `Core` does not owe, and a `--strip`
    launched the same way switched the reference-page guard off file by file.
    """
    parts = repo_relative(path).split(os.sep)
    if parts and parts[0] == 'Engine' and len(parts) > 2:
        return parts[1]
    return None


def docs_are_in_place(paths, force=False):
    """The files whose comments may be deleted, and why the rest may not.

    The ban is only safe after the prose has somewhere to live, and that ordering must be a property of
    the tool rather than of the operator's memory. It is asked per file, through
    `docs_coverage.py check-file`, which answers for the entries *this* file declares: a page exists,
    the header addresses it, and every method name has a page.

    Per entry, not per module. The module-wide question — "is Core documented?" — has the wrong shape
    for a strip: it made a header whose own reference was finished wait on every undocumented entry in
    the same module, so the guard meant to protect prose ended up deleting none of it anywhere, and the
    finished documentation bought nothing. What the invariant actually needs is per declaration: no
    comment is removed until the entry it documents owns the page it was moved into.
    """
    import subprocess
    outside = [p for p in paths if engine_module(p) is None]
    if outside:
        print('note: %d file(s) outside Engine/ own no API pages — move their prose into a guide under '
              'docs/ by hand' % len(outside))
    checker = os.path.join(SCRIPT_DIR, 'docs_coverage.py')
    allowed = []
    blockers = []
    for path in paths:
        if engine_module(path) is None:
            allowed.append(path)
            continue
        if not os.path.isfile(checker):
            blockers.append('%s: docs_coverage.py is missing, so this file\'s pages cannot be verified' % path)
            continue
        result = subprocess.run([sys.executable, checker, 'check-file', path], capture_output=True, text=True)
        if result.returncode == 0:
            allowed.append(path)
            continue
        detail = [line.strip() for line in result.stdout.splitlines()
                  if line.strip() and not line.startswith('file docs check')]
        reason = detail[0] if detail else 'the per-file documentation check failed'
        blockers.append('%s: %s' % (path, reason.replace('[BLOCKS STRIP] %s: ' % path, '')))
    if not blockers:
        return allowed
    print('REFUSED: comments are being deleted before their reference pages exist', file=sys.stderr)
    for row in blockers:
        print('  %s' % row, file=sys.stderr)
    if allowed:
        print('  %d file(s) in this call are not blocked and would still be stripped.' % len(allowed),
              file=sys.stderr)
    print('  Write the pages first (docs_page.py emits them with correct chrome), or pass --force if the '
          'prose is going somewhere else.', file=sys.stderr)
    return paths if force else allowed


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
    broken = [row for row in findings if ': POINTER-' in row]
    if broken:
        print('%s:1: STRIP-REFUSED  %d broken API reference pointer(s); a pointer is an address, so repair it '
              'rather than let a strip delete it' % (path, len(broken)), file=sys.stderr)
        for row in broken:
            print('  %s' % row, file=sys.stderr)
        return -1
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


SELFTEST_CASES = []

NESTED_GUARD = '\n'.join([
    '#if PROFILE_ENABLED',
    '\tvoid f()',
    '\t{',
    '#ifdef __DEBUG__',
    '\t\tx *= 2;',
    '#endif // __DEBUG__',
    '\t}',
    '#else // PROFILE_ENABLED',
    '\tvoid f()',
    '\t{',
    '\t}',
    '#endif // PROFILE_ENABLED',
    '',
])
SELFTEST_CASES.append(('nested guard: outer labels are labels', 'selftest.h', NESTED_GUARD, []))
SELFTEST_CASES.append(('nested guard: a label naming the inner guard on the outer close is prose',
                       'selftest.h', NESTED_GUARD.replace('#endif // PROFILE_ENABLED', '#endif // __DEBUG__'),
                       [12]))
SELFTEST_CASES.append(('a label that is not the construct is prose',
                       'selftest.h', '\n'.join(['#if PROFILE_ENABLED', '\tint x;', '#endif // TODO', '']), [3]))
SELFTEST_CASES.append(('namespace close is a label',
                       'selftest.h',
                       '\n'.join(['namespace hbe', '{', '\tvoid f();', '} // namespace hbe', '']), []))
SELFTEST_CASES.append(('nested namespace close is a label too', 'selftest.h',
                       '\n'.join(['namespace hbe::time', '{', '\tvoid f();', '} // namespace hbe::time', '']),
                       []))
SELFTEST_CASES.append(('a namespace label naming the wrong namespace is prose', 'selftest.h',
                       '\n'.join(['namespace hbe::time', '{', '\tvoid f();', '} // namespace hbe', '']), [4]))
SELFTEST_CASES.append(('doc comment is prose',
                       'selftest.h',
                       '\n'.join(['namespace hbe', '{', '\t/// @brief Acquires on construction.',
                                  '\tvoid f();', '}', '']), [3]))

POINTER_WORKITEM = 'Engine/Core/WorkItem.h'
SELFTEST_CASES.append(('api reference pointer above its own entry is exempt', POINTER_WORKITEM,
                       '\n'.join(['/// API reference: docs/Core/WorkItem/index.html',
                                  'class WorkItem final', '{', '};', '']), []))
SELFTEST_CASES.append(('api reference pointer naming another entry is reported', POINTER_WORKITEM,
                       '\n'.join(['/// API reference: docs/Core/Task/index.html',
                                  'class WorkItem final', '{', '};', '']), [1]))
SELFTEST_CASES.append(('api reference pointer to a page that does not exist is reported', POINTER_WORKITEM,
                       '\n'.join(['/// API reference: docs/Core/NoSuchEntry/index.html',
                                  'class WorkItem final', '{', '};', '']), [1]))
SELFTEST_CASES.append(('api reference pointer on a member is reported', POINTER_WORKITEM,
                       '\n'.join(['class WorkItem final', '{', 'public:',
                                  '\t/// API reference: docs/Core/WorkItem/index.html',
                                  '\tvoid Run() {}', '};', '']), [4]))

SELFTEST_CASES.append(('a namespace label inside a conditional is still a structural label',
                       'selftest.h',
                       '\n'.join(['#ifdef __TEST__',
                                  'namespace hbe',
                                  '{',
                                  '\tclass T final',
                                  '\t{',
                                  '\t};',
                                  '} // namespace hbe',
                                  '#endif //__TEST__', '']), []))
SELFTEST_CASES.append(('the exempt exemplar is exempt under every spelling of its path',
                       './Engine/CodingStandards.cpp',
                       '\n'.join(['// Copyright (c) 2025 Hansol Park', '// prose that would count', '']), []))
SELFTEST_CASES.append(('a header keeps its module when it is named from its own directory',
                       'Engine/Core/WorkItem.h',
                       '\n'.join(['/// API reference: docs/Core/WorkItem/index.html',
                                  'class WorkItem final', '{', '};', '']), []))
SELFTEST_CASES.append(('a guard whose name ends in a word character cannot swallow the keyword',
                       'selftest.h',
                       '\n'.join(['#if HBE_TRACE',
                                  'namespace hbe',
                                  '{',
                                  '} // namespace hbe',
                                  '#endif // HBE_TRACE', '']), []))

POINTER_PACKET = 'Engine/Core/ResultPacket.h'
SELFTEST_CASES.append(('alignas between the keyword and the name still names its entry', POINTER_PACKET,
                       '\n'.join(['/// API reference: docs/Core/ResultPacket/index.html',
                                  'class alignas(std::uint64_t) ResultPacket final', '{', '};', '']), []))
SELFTEST_CASES.append(('bracket attribute between the keyword and the name still names its entry',
                       POINTER_PACKET,
                       '\n'.join(['/// API reference: docs/Core/ResultPacket/index.html',
                                  'class [[deprecated]] ResultPacket final', '{', '};', '']), []))
SELFTEST_CASES.append(('enum struct is read as an entry name', POINTER_PACKET,
                       '\n'.join(['/// API reference: docs/Core/ResultPacket/index.html',
                                  'enum struct ResultPacket : std::uint8_t', '{', '};', '']), []))
SELFTEST_CASES.append(('a declaration name this reader cannot settle is reported, not guessed',
                       POINTER_PACKET,
                       '\n'.join(['/// API reference: docs/Core/ResultPacket/index.html',
                                  'struct HBE_API_EXPORT ResultPacket final', '{', '};', '']), [1]))
SELFTEST_CASES.append(('api reference pointer is not prose and survives a strip', POINTER_WORKITEM,
                       '\n'.join(['/// API reference: docs/Core/WorkItem/index.html',
                                  '/// @brief Prose that must go.',
                                  'class WorkItem final', '{', '};', '']), [2]))


def selftest():
    """Run the label rules against fixtures, so a stack regression cannot return silently.

    Every other gate in this skill compiles or compares files; none of them can see a mislabelled
    preprocessor close, because the code is valid either way. That makes this function the only place
    the distinction is checked, and the fixtures are the two shapes that were once wrong: the outer
    label deleted, and the inner label accepted.

    The pointer fixtures point at a page that exists in this repository, `docs/Core/WorkItem/index.html`,
    and at a module that exists, `Core`. A fixture pointing at a made-up page would prove only that the
    file is missing, which is one of the four failures being tested rather than the test itself.
    """
    failures = 0
    for name, source_path, source, expected in SELFTEST_CASES:
        findings, _ = check_file(source_path, source)
        got = sorted({int(finding.split(':', 2)[1]) for finding in findings})
        if got == sorted(expected):
            print('SELFTEST  ok    %s' % name)
            continue
        failures += 1
        print('SELFTEST  FAIL  %s: expected findings on lines %s, got %s' % (name, sorted(expected), got))
        for finding in findings:
            print('          %s' % finding)
    print('selftest: %d case(s), %d failure(s)' % (len(SELFTEST_CASES), failures))
    return 1 if failures else 0


def main(argv):
    if argv and argv[0] == '--selftest':
        return selftest()
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

    force = '--force' in argv
    argv = [a for a in argv if a != '--force']
    strip = '--strip' in argv
    argv = [a for a in argv if a != '--strip']
    if not argv:
        print('usage: comments.py [--strip] <file ...>', file=sys.stderr)
        return 3
    # Every argument must name a file before anything is asked about it. This is checked above the strip
    # guard on purpose: `docs_are_in_place` decides by path, and a directory reads to it as "outside
    # Engine/, so no reference pages are owed", after which the run opened nothing and still reported
    # `stripped 1 comment(s) from 0 file(s)` and exit 0. `blank_lines.py` and `includes.py` answer a
    # directory with a usage error and exit 3; so does this.
    for path in argv:
        label = path or '<empty argument>'
        reason = 'is a directory; this gate reads files, not directories' if path else 'names no file'
        if os.path.isdir(path) or not path:
            print('[NONE] comments — %s: %s' % (label, reason))
            return 3
    if strip:
        allowed = docs_are_in_place(argv, force=force)
        if not allowed:
            return 1
        # A guard that narrows the scope must not let the run report full success: the files it refused
        # are still standing, and a caller reading only the exit status would think the sweep finished.
        partial = len(allowed) != len(argv)
        argv = allowed
    total = 0
    scored = 0
    refused = 0
    for path in argv:
        try:
            text = open(path, encoding='utf-8', errors='ignore').read()
        except OSError as exc:
            print('[NONE] comments — %s: %s' % (path, exc))
            return 3
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
        return 1 if refused or partial else 0
    else:
        print('comments examined in %d file(s): %d violation(s)' % (scored, total))
    return 1 if (total and not strip) or refused else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
