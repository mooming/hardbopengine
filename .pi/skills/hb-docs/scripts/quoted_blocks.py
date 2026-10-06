#!/usr/bin/env python3
"""Compare every code block the reference quotes from the engine against the engine's current text.

  quoted_blocks.py [--allow-unresolved] [glob ...]      default: every page under docs/

A page shows engine code in two ways, and only one of them is a claim. A `<pre><code>` block followed
by a `<p class="meta">` caption that names a source file and a symbol says: this block is that symbol's
text, copied. A block with no such caption — a Signature section, a hand-written example, an
illustrative sketch — claims nothing about the source and is skipped, and the number skipped is
printed because the reader cannot weigh the check without knowing its coverage. Inline `<code>` in
prose is never a quotation: prose quotes a superseded form on purpose to explain a change, so reading
it as a copy would report every sentence about a change as a defect. Only content inside `<pre>` is a
copy candidate.

Verdicts, printed one line per block:

  verbatim           the block's lines are found, unchanged, inside that symbol's definition
  STALE              a line differs, and the report carries a unified diff of the two
  CAPTION UNRESOLVED the caption names something this checker cannot read: the file is gone, the file
                     defines no function of that name under the caption's qualifier nor the bare name the
                     file spells for it, the caption names a type or a macro rather than a function, or
                     several overloads exist and the caption's parameter list names none
  not a copy claim   the caption itself says the block is condensed, abridged, adapted or reshaped, so
                     a difference is not a defect; the block is still tried, and counts as verbatim if
                     it matches anyway

Both sides are normalised before they are compared, and the normalisation is deliberately narrow: HTML
entities decoded, markup tags dropped, trailing whitespace dropped, and leading indentation flattened to
spaces. Interior whitespace is left exactly as written. Nothing else is forgiven: a re-wrapped line, a
renamed local, a blank line added or removed, and a comment the copy lost all report as stale.

What a quotation is allowed to be, and what it is not:

  * any contiguous run of the symbol's lines, starting anywhere in the definition and ending at any later
    line. Pages quote a function's body and leave the signature line and the closing brace out, so those
    two are optional; a quotation may not be a subset drawn from all over the function.
  * placed anywhere horizontally. One shift applies across a run, decided by the run's own lines, because
    a body copied out of a function three tabs deep arrives at column zero. A run's internal indentation
    shape is still demanded, so a copy that re-indents one line by hand is stale.
  * missing the indentation of its own first line. A `<pre>` block's first line begins on the same line as
    the `<code>` tag that opens it and cannot be indented there: measured over every page under `docs/`,
    1562 of 1562 quoted blocks start flush with the tag. That one line's column is therefore unread, on
    both sides, and only that one.
  * shortened where the page says so. A block that carries an elision marker on one of its own lines is
    read as the sequence of runs the marker separates, and every run must appear in the source in order
    after the one before it. The marker vocabulary is the one the corpus writes: a line that begins with
    `...` or `…`, optionally after `//`, with an optional note saying what was dropped. An ellipsis
    inside a line of code is not a marker, and a block that thins itself without one stays reportable.

Exit code: 0 when nothing is stale, 1 when something is (or a caption could not be resolved, unless
--allow-unresolved), 2 when the arguments matched no page.
"""

import difflib
import glob
import html
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))
DOCS = os.path.join(REPO_ROOT, 'docs')

# A caption names a source file with the same root-relative address the header pointers use, so the
# same shape is recognised here. `.md` is kept in the list to be counted and refused, not compared.
SOURCE_FILE = re.compile(r'\b((?:Engine|Applications|docs|Platform|Build)/[\w./+-]*\.(?:h|hpp|inl|cpp|cc|md))\b')

# `PoolAllocator::Deallocate(Pointer ptr, size_t size)`, `hbe::Logger::AddLog`, `PoolAllocator::~PoolAllocator()`,
# `PoolAllocator::operator<(const PoolAllocator& rhs) const`. The parameter list is optional because half
# the corpus names a function without spelling its parameters; the qualifier is what makes a caption
# nameable at all, so `ComponentSystemTest` alone is not a symbol this script will guess.
CAPTION_SYMBOL = re.compile(r'\b((?:[A-Za-z_]\w*::)+(?:~?[A-Za-z_]\w*|operator[^\s(]*))(?:\s*\(([^()]*)\))?')

# A caption that says the block was rewritten says so in these words. The block is still compared;
# these words only decide what a difference means.
ADAPTED = re.compile(r'\b(condensed|condense|abridged|abbreviated|trimmed|shortened|adapted|simplified|'
                     r'simplified|paraphras\w*|reshaped|re-flowed|reformatted|stylis\w*|illustrative|'
                     r'the form used by|shape used by|hand-written|hand written)\b', re.I)

# The marker the corpus actually writes on its own line, measured over every page under docs/: a bare
# `...` or `…`, optionally followed by a note saying what it stands for (77 lines), the same after `//`
# (11), and `//` plus a note wrapped in markers (19). What such a line must do is begin with the marker:
# that is what separates a marker row from copied code that merely contains an ellipsis
# (`template<typename... Types>`) and from a copied comment whose prose happens to end in one. Nothing
# else counts, so a block that shortens itself without saying so, or that hides `...` inside a call's
# argument list, stays reportable.
ELISION = re.compile(r'^\s*(?://\s*)?(?:\.\.\.|…+)(?:\s+\S.*)?$')

TAB_WIDTH = 8

# The comment and string literal scanner, and the definition finder, both run over a masked copy of the
# source: every comment character and every string-literal body replaced by a space, newline for
# newline. Offsets found there index the real text, and a brace or a semicolon inside a comment or a
# message literal stops being structural. Same technique as docs_pass.py's declaration parser, one
# scanner stricter, because a quoted body is searched for braces and a `"}"` in a log message is real.
def mask_source(text):
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        ch = text[i]
        if ch == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                out[i] = ' '
                i += 1
        elif ch == '/' and i + 1 < n and text[i + 1] == '*':
            step = 2
            while i < n:
                if text.startswith('*/', i):
                    out[i] = out[i + 1] = ' '
                    i += 2
                    break
                if text[i] != '\n':
                    out[i] = ' '
                i += 1
            step = 1
        elif ch in '"\'':
            quote, i = ch, i + 1
            while i < n and text[i] != '\n':
                if text[i] == '\\':
                    out[i] = ' '
                    i += 1
                if i < n and text[i] != '\n':
                    out[i] = ' '
                if i < n and text[i] == quote:
                    i += 1
                    break
                i += 1
        else:
            i += 1
    return ''.join(out)


def block_text(raw):
    """The text a reader sees in one `<pre>` block, tags dropped and entities decoded."""
    text = re.sub(r'<br\s*/?>', '\n', raw)
    text = re.sub(r'<[^>]+>', '', text)
    return html.unescape(text)


def indent_columns(line):
    """(display columns, characters) of one line's leading whitespace.

    Both are returned because a tab is one character and up to eight columns, and a caller that slices
    the line by the column count eats the line itself instead of its indentation.
    """
    columns = width = 0
    for ch in line:
        if ch == '\t':
            columns += TAB_WIDTH - (columns % TAB_WIDTH)
            width += 1
        elif ch == ' ':
            columns += 1
            width += 1
        else:
            break
    return columns, width


def normalise(text, base=None):
    """Lines with leading indentation flattened to spaces and re-based, trailing whitespace dropped.

    `base` re-bases on a window rather than on the run itself, so a source window and the page fragment
    that came from it end up on the same footing. Interior whitespace is left exactly as written: an
    argument list aligned under its opener is part of the copy, and a copy that lost the alignment is
    not the same text.
    """
    lines = [line.rstrip() for line in text.split('\n')]
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    indents = [indent_columns(line) for line in lines]
    if base is None:
        filled = [columns for (columns, _), line in zip(indents, lines) if line.strip()]
        base = min(filled) if filled else 0
    flat = []
    for line, (columns, width) in zip(lines, indents):
        if not line.strip():
            flat.append('')
            continue
        flat.append(' ' * max(0, columns - base) + line[width:].rstrip())
    return flat


def strip_comments_only(text):
    """Lines of a source region with the comment lines it carries dropped.

    Used for the second attempt only, and reported when it succeeds: a quote that dropped a comment the
    source still carries is a copy that will match the day the comment is deleted, and the reader needs
    to know which of the two happened rather than see it fold into `verbatim`.
    """
    kept, depth = [], 0
    for line in text.split('\n'):
        stripped = line.strip()
        if depth == 0 and not stripped.startswith('/*') and '/*' not in stripped and stripped.startswith('//'):
            continue
        if depth > 0 or stripped.startswith('/*'):
            depth += stripped.count('/*')
            depth -= stripped.count('*/')
            continue
        kept.append(line)
    return '\n'.join(kept)


class Block:
    """One `<pre><code>` block on a page, with whatever caption the page put under it."""

    def __init__(self, page, line, text, caption):
        self.page = page
        self.line = line
        self.text = text
        self.caption = caption
        self.file = None
        self.symbol = None
        self.params = None
        self.adapted = False
        self.verdict = 'skipped'
        self.detail = ''
        self.diff = ''


def parse_caption(block):
    """Split a caption into the source file and symbol it names, or explain why it names neither."""
    caption = html.unescape(re.sub(r'<br\s*/?>', ' ', block.caption))
    visible = re.sub(r'\s+', ' ', re.sub(r'<[^>]+>', '', caption)).strip()
    files = list(SOURCE_FILE.finditer(visible))
    symbols = list(CAPTION_SYMBOL.finditer(visible))
    if not files:
        block.verdict, block.detail = 'skipped', 'caption names no source file'
        return False
    if not symbols:
        block.verdict, block.detail = 'skipped', 'caption names no qualified symbol'
        return False
    # "Declared in Engine/Config/ConfigFile.h, defined in Engine/Config/ConfigFile.cpp as
    # ConfigFile::Parse()" names two files: the body lives with the symbol, so the file taken is the one
    # that precedes the symbol most closely.
    symbol = symbols[0]
    chosen = None
    for found in files:
        if found.end() <= symbol.start():
            chosen = found
    if chosen is None:
        block.verdict, block.detail = 'skipped', 'caption names a symbol but the file follows it'
        return False
    block.file = chosen.group(1)
    block.symbol = symbol.group(1)
    block.params = re.sub(r'\s+', ' ', symbol.group(2) or '').strip()
    block.adapted = bool(ADAPTED.search(visible))
    block.detail = visible
    return True


# A definition's line may put a return type and an attribute in front of the qualified name, and nothing
# else may sit there: a call site is `auto& mmgr = MemoryManager::GetInstance();`, and `,` or `=` in the
# prefix is how a call announces itself. Attributes are in the class because the engine writes
# `[[nodiscard]] auto GetID() const`, and a checker that could not read past the attribute refused every
# page that quoted one. The shape is not enough on its own: `return GetBlockSize();` and
# `else if (IsMine(ptr))` have clean prefixes and are statements, so a statement keyword anywhere in
# the prefix rejects the candidate.
DECL_PREFIX = re.compile(r'^[\w\s:<>&*\[\]]*$')
DECL_PREFIX_STATEMENT = re.compile(
    r'\b(?:return|if|else|for|while|do|switch|case|goto|new|delete|throw|sizeof|decltype|co_return'
    r'|using|and|or|not|xor|assert|static_assert)\b')


def candidate_regions(masked, name_pattern):
    """Every span in the masked source that could be the definition of `Class::Name`.

    Returns (first line, last line exclusive, parameter text, has a brace body) with offsets into the
    real text, which the caller slices out of the unmasked copy.
    """
    regions = []
    for found in re.finditer(name_pattern, masked):
        line_start = masked.rfind('\n', 0, found.start()) + 1
        prefix = masked[line_start:found.start()]
        if not DECL_PREFIX.match(prefix) or DECL_PREFIX_STATEMENT.search(prefix):
            continue
        open_paren = masked.find('(', found.end() - 1)
        if open_paren < 0:
            continue
        depth, i = 0, open_paren
        while i < len(masked):
            if masked[i] == '(':
                depth += 1
            elif masked[i] == ')':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if i >= len(masked):
            continue
        params = re.sub(r'\s+', ' ', masked[open_paren + 1:i]).strip()
        j, brace, semicolon = i, None, None
        while j < len(masked):
            if masked[j] == '{':
                brace = j
                break
            if masked[j] == ';':
                semicolon = j
                break
            j += 1
        if semicolon is not None and (brace is None or semicolon < brace):
            regions.append((line_start, semicolon + 1, params, False))
            continue
        if brace is None:
            continue
        depth, k = 0, brace
        while k < len(masked):
            if masked[k] == '{':
                depth += 1
            elif masked[k] == '}':
                depth -= 1
                if depth == 0:
                    break
            k += 1
        if k >= len(masked):
            continue
        regions.append((line_start, k + 1, params, True))
    return regions


def symbol_pattern(symbol, bare=False):
    """Regex that finds the definition of a caption symbol, qualifier and all.

    `bare` drops the qualifier requirement: a template member defined inside its own class is spelled
    `Allocate(...)` in the header, never `InlineMonotonicAllocator::Allocate(...)`, and the negative
    lookbehind still refuses to match the qualified spelling or a member call.
    """
    parts = symbol.split('::')
    while parts and parts[0] == 'hbe':
        parts.pop(0)
    name = parts[-1]
    scope = parts[-2] if len(parts) > 1 else None
    name_re = re.escape(name)
    if name.startswith('operator'):
        spelling = name[len('operator'):].strip()
        name_re = r'operator\s*' + re.escape(spelling) if spelling else r'operator\S*'
    if bare:
        return re.compile(r'(?<![\w:.])' + name_re + r'\s*\(')
    scope_re = (r'(?:\b\w+\s*::\s*)*' + re.escape(scope) + r'\s*::\s*') if scope else r'(?:\w+\s*::\s*)*'
    return re.compile(r'(?<![\w:.])' + scope_re + name_re + r'\s*\(')


def canon_types(text):
    """Parameter text folded to what a caption and a source must agree on: qualifiers drop, spaces fold."""
    return re.sub(r'\s+', ' ', re.sub(r'\b(?:const|volatile|struct)\b', ' ', text)).strip().lower()


def bare_params(text):
    """Parameter text reduced to the types it names.

    A caption writes `(size_t)` where the source writes `(size_t blockIndex)`; both name one size_t
    parameter, and a gate that demanded the page copy parameter names verbatim would report a
    difference nobody cares about. Default arguments are dropped, which can only lose information.
    """
    text = re.sub(r'=[^,)]*', '', text)
    tokens = []
    for part in text.split(','):
        words = part.split()
        if len(words) > 1 and re.fullmatch(r'~?[A-Za-z_]\w*(?:\[\s*\])?', words[-1]):
            words = words[:-1]
        tokens.extend(words)
    return ' '.join(tokens)


def pick_region(regions, params):
    """Choose the overload a caption's parameter list names."""
    if len(regions) == 1:
        return regions[0], ''
    wanted = re.sub(r'\s+', ' ', params or '').strip()
    if wanted:
        bare = re.sub(r'\b(const|volatile|struct)\b', ' ', wanted)
        exact = [r for r in regions
                 if re.sub(r'\s+', ' ', re.sub(r'\b(const|volatile|struct)\b', ' ', r[2])).strip() == bare.strip()]
        if len(exact) == 1:
            return exact[0], ''
    return None, '%d candidate definition(s), the caption\u2019s parameter list (%s) names one of none' % (
        len(regions), wanted or 'not written')


def as_pairs(lines):
    """(indent columns, text) of already-normalised lines; a blank line is (0, '').

    The match works on pairs rather than on whole strings so the indent of a line can be forgiven
    by a uniform shift while the text of the line is demanded exactly: interior spacing, spelling
    and order stay strict, and only the run's horizontal placement is re-based.
    """
    pairs = []
    for line in lines:
        indent = len(line) - len(line.lstrip(' '))
        pairs.append((0, '') if not line.strip() else (indent, line[indent:]))
    return pairs


def render(pairs):
    """The inverse of as_pairs, for diff sides a reader can compare by eye."""
    return [' ' * indent + text for indent, text in pairs]


def find_run(haystack, needle, start, first_line_indent_free=False):
    """First index at or after `start` where `needle` sits as a contiguous run of lines.

    Both sides are (indent, text) pairs. A window of the source matches when its lines are the needle's
    lines, in order, unchanged, and every text line but possibly the first sits at its own source column
    shifted by one single amount: the run's horizontal placement is free, its internal indentation shape
    is not. The shift is read from the first line of the run that can carry one: a blank line's indent is
    always zero by construction, and with `first_line_indent_free` the first line joins it, because a
    `<pre>` block's first line begins on the same line as the `<code>` tag that opens it and no page in
    the corpus can indent it (measured over every page under `docs/`, 1562 of 1562 quoted blocks start
    flush with the tag). Reading the shift from a line that can carry it is what lets a body excerpt
    whose first tab the page could not keep, or a run that opens on a blank line, still be recognised as
    the copy it is, and it stays narrow: one shift for the whole run, decided by the run's own lines.
    Matching is whole-run, never line by line and never expression by expression, which is what keeps
    unchanged code that merely shares an expression with a nearby changed line from matching in the wrong
    place, and keeps a quotation from being a subset drawn from all over the function.

    A page that dedents an excerpt past the left margin, or that indents a `switch` body the engine keeps
    flush with its `case` labels, is stale here, and deliberately so: the alternative is a comparator that
    cannot see indentation at all, and a copy that rearranged its own columns is the same kind of defect
    as one that rearranged its words.
    """
    if not needle:
        return start
    anchor = next((offset for offset, (_, text) in enumerate(needle)
                   if text and not (first_line_indent_free and offset == 0)), None)
    limit = len(haystack) - len(needle)
    for index in range(max(0, start), limit + 1):
        shift = haystack[index + anchor][0] - needle[anchor][0] if anchor is not None else 0
        for offset, (indent, text) in enumerate(needle):
            window_indent, window_text = haystack[index + offset]
            if window_text != text:
                break
            if not text or offset == anchor or (first_line_indent_free and offset == 0):
                continue
            if window_indent != indent + shift:
                break
        else:
            return index
    return -1


def align_window(block_lines, source_lines):
    """Where in the source the block belongs, for a diff that stays small enough to read.

    A stale fragment is usually one changed line inside a run that still matches around it, so the
    window is anchored on the longest run the two texts still share. Without an anchor the diff would be
    the whole function against the whole block, which is how a one line change stops being reviewable.
    """
    matcher = difflib.SequenceMatcher(None, block_lines, source_lines, autojunk=False)
    blocks = [b for b in matcher.get_matching_blocks() if b.size >= 1]
    if not blocks:
        return 0, len(block_lines), 0, len(source_lines)
    biggest = max(blocks, key=lambda b: (b.size, -b.a, -b.b))
    a, b, size = biggest.a, biggest.b, biggest.size
    before = [x for x in blocks if x.b + x.size <= b and x.a + x.size <= a]
    after = [x for x in blocks if x.b >= b + size and x.a >= a + size]
    a0 = max([x.a for x in before], default=a)
    b0 = min([x.b for x in before], default=b)
    a1 = min([x.a + x.size for x in after], default=a + size)
    b1 = max([x.b + x.size for x in after], default=b + size)
    pad = 2
    return max(0, a0 - pad), min(len(block_lines), a1 + pad), max(0, b0 - pad), min(len(source_lines), b1 + pad)


def compare(block, source_text, regions, source_path):
    """Fill in the block's verdict by matching its runs against the chosen definition."""
    chosen, note = pick_region(regions, block.params)
    if chosen is None:
        block.verdict, block.detail = 'CAPTION UNRESOLVED', note
        return
    first, last = chosen[0], chosen[1]
    region = source_text[first:last]
    source_lines = normalise(region)
    block_lines = normalise(block.text)
    source_pairs = as_pairs(source_lines)
    block_pairs = as_pairs(block_lines)
    shift = block_pairs[0][0] if block_pairs else 0
    block_pairs = [(indent - shift if text else 0, text) for indent, text in block_pairs]
    fragments, run = [], []
    for pair in block_pairs:
        if ELISION.match(pair[1]):
            if run:
                fragments.append(run)
            run = []
        else:
            run.append(pair)
    if run:
        fragments.append(run)
    if not fragments:
        block.verdict, block.detail = 'not a copy claim', 'block holds nothing but an elision marker'
        return
    position, cursor = 0, 0
    for number, fragment in enumerate(fragments):
        position = find_run(source_pairs, fragment, cursor, first_line_indent_free=number == 0)
        if position < 0:
            break
        cursor = position + len(fragment)
    else:
        block.verdict = 'verbatim'
        block.detail = '%d line(s) of %d, %s:%d' % (len(block_lines), len(source_lines), source_path,
                                                    source_text[:first].count('\n') + 1)
        if len(fragments) > 1:
            block.detail += ', %d run(s) split by an elision marker' % len(fragments)
        return
    # The first attempt uses the source with its comments; the second drops them, because a copy that
    # omitted a comment the source has not yet lost is a preview of the migration, not a wrong copy.
    kept = strip_comments_only(region)
    if kept.strip():
        retry = as_pairs(normalise(kept))
        cursor2, whole = 0, []
        for index, fragment in enumerate(fragments):
            at = find_run(retry, fragment, cursor2, first_line_indent_free=index == 0)
            if at < 0:
                whole = None
                break
            cursor2 = at + len(fragment)
            whole.append(fragment)
        if whole is not None:
            block.verdict = 'verbatim'
            block.detail = ('%d line(s), %s:%d, after the comment lines the source still carries were '
                            'dropped' % (len(block_lines), source_path, source_text[:first].count('\n') + 1))
            return
    if block.adapted:
        block.verdict = 'not a copy claim'
        block.detail = ('caption declares the block adapted; %d line(s) do not occur verbatim in %s\u2019s %s, '
                        'which a declared rewrite makes not a defect' % (len(block_pairs), source_path, block.symbol))
        return
    block.verdict = 'STALE'
    # The anchor is found on texts alone so a genuine one line change is anchored on the run around it
    # rather than buried under the indentation the two sides legitimately differ by; each diff side is
    # then re-based on its own window so what remains in the diff is the difference itself.
    b0, b1, s0, s1 = align_window([text for _, text in block_pairs], [text for _, text in source_pairs])
    window = source_pairs[s0:s1]
    base = next((indent for indent, text in window if text), 0)
    window = [(max(0, indent - base) if text else 0, text) for indent, text in window]
    from_tag = '%s:%d quoted' % (block.page, block.line)
    to_tag = '%s:%d %s' % (source_path, source_text[:first].count('\n') + 1, block.symbol)
    block.diff = '\n'.join(difflib.unified_diff(render(block_pairs[b0:b1]), render(window), fromfile=from_tag,
                                                tofile=to_tag, lineterm='', n=2))
    block.detail = 'run %d of %d is not in %s\u2019s %s' % (number + 1, len(fragments), source_path, block.symbol)


def check_page(path, allow_unresolved):
    """Every `<pre>` block of one page, as Block objects."""
    text = open(path, encoding='utf-8', errors='ignore').read()
    blocks = []
    pattern = re.compile(r'<pre[^>]*>\s*<code[^>]*>(.*?)</code>\s*</pre>(\s*<p class="meta">(.*?)</p>)?',
                         re.S)
    for found in pattern.finditer(text):
        caption = found.group(3)
        block = Block(os.path.relpath(path, REPO_ROOT) if os.path.isabs(path) else path,
                      text[:found.start()].count('\n') + 1, block_text(found.group(1)), caption)
        if caption is None:
            block.verdict, block.detail = 'skipped', 'no caption under the block'
        elif not parse_caption(block):
            pass
        elif block.file.endswith('.md'):
            block.verdict, block.detail = 'skipped', 'caption names %s, not a source file' % block.file
        else:
            check_quotation(block, allow_unresolved)
        blocks.append(block)
    return blocks


def check_quotation(block, allow_unresolved):
    source_path = os.path.join(REPO_ROOT, block.file)
    if not os.path.isfile(source_path):
        block.verdict, block.detail = 'CAPTION UNRESOLVED', 'no such file: %s' % block.file
        return
    source_text = open(source_path, encoding='utf-8', errors='ignore').read()
    masked = mask_source(source_text)
    regions = candidate_regions(masked, symbol_pattern(block.symbol))
    if not regions:
        # The caption writes `BufferUtil::GenerateFileBuffer`; the source writes `GenerateFileBuffer`
        # inside `namespace BufferUtil {`, because a definition does not repeat the namespace it stands
        # in. The bare leaf name is then the only spelling the file carries, and the negative lookbehind
        # in symbol_pattern still refuses a qualified spelling or a member call. Only definitions with a
        # brace body are accepted here: a bare call site ends at its semicolon, and a quotation of a body
        # has no business matching a call.
        regions = [region for region in candidate_regions(masked, symbol_pattern(block.symbol, bare=True))
                   if region[3]]
    if not regions:
        block.verdict = 'CAPTION UNRESOLVED'
        block.detail = '%s defines no function matching %s' % (block.file, block.symbol)
        return
    compare(block, source_text, regions, block.file)


def main():
    argv = sys.argv[1:]
    allow_unresolved = '--allow-unresolved' in argv
    patterns = [a for a in argv if not a.startswith('--')] or [os.path.join(DOCS, '**', '*.html')]
    # A pattern may name a directory (`docs/Memory`) or a glob (`docs/Memory/**/*.html`); docs_pass.py
    # is handed folders and htmlcheck.py is handed globs, and both spellings must work here.
    expanded = [os.path.join(p, '**', '*.html') if os.path.isdir(p) else p for p in patterns]
    pages = sorted({p for pattern in expanded for p in glob.glob(pattern, recursive=True) if p.endswith('.html')})
    if not pages:
        print('no pages matched', file=sys.stderr)
        return 2
    counts = {'verbatim': 0, 'STALE': 0, 'CAPTION UNRESOLVED': 0, 'not a copy claim': 0, 'skipped': 0}
    stale = unresolved = 0
    for path in pages:
        for block in check_page(path, allow_unresolved):
            counts[block.verdict] = counts.get(block.verdict, 0) + 1
            if block.verdict == 'STALE':
                stale += 1
                print('%s:%d  STALE  %s' % (block.page, block.line, block.detail))
                print('%s\n' % block.diff)
            elif block.verdict == 'CAPTION UNRESOLVED':
                unresolved += 1
                print('%s:%d  CAPTION UNRESOLVED  %s' % (block.page, block.line, block.detail))
    skipped = counts['skipped']
    adapted = counts.get('not a copy claim', 0)
    print('quoted blocks: %d block(s) in %d page(s) checked, %d verbatim, %d stale, %d unresolved, '
          '%d skipped (no caption naming a file and a symbol)%s'
          % (sum(counts.values()), len(pages), counts['verbatim'], stale, unresolved, skipped,
             ', %d not a copy claim' % adapted if adapted else ''))
    if stale or (unresolved and not allow_unresolved):
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
