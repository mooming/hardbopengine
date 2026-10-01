#!/usr/bin/env python3
"""Documentation coverage ledger for the HardBop Engine API reference.

Source comments are banned in this engine, so docs/ is the only place a contract can live.
A rule that removes the documentation from the code without proving the replacement exists
loses information, so this script is what makes the ban safe: it pairs every class, struct,
enum and macro set in a module against its page, and refuses to call a module conformant
while a page is missing.

The approved site shape, from .Plans/AUTHORING_method_and_class_pages.md:
    docs/index.html                      module index, links every module page
    docs/<Module>/index.html             module page, links every entry of that module
    docs/<Module>/<Entry>/index.html     class page
    docs/<Module>/<Entry>/<method>.html  method page, one per method name

Usage:
    docs_coverage.py ledger            write .Plans/DOCS_COVERAGE.md
    docs_coverage.py check [MODULE ...]  exit 1 while an entry has no page
    docs_coverage.py missing MODULE    list entries of one module that need a page

`check` exits 0 with a printed notice when no ledger exists, because an empty tree that was
never measured must not read as a pass.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import comments as comment_lexer

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
DOCS = os.path.join(REPO_ROOT, 'docs')
LEDGER = os.path.join(REPO_ROOT, '.Plans', 'DOCS_COVERAGE.md')

SOURCE_EXT = ('.h', '.hpp')
# The comment column must walk more than the headers. With SOURCE_EXT alone the ledger reported Core
# holding 1,181 comment lines while the module actually holds 1,476 — the 295 in its .cpp and .inl files
# were invisible. That number is what the sweep is scheduled from, so an undercount does not merely
# misreport, it sequences the work wrong: a module reads nearly finished while half its prose is still
# in implementation files. Class enumeration stays header-only, because declared entries come from
# headers; only the comment tally widens.
PROSE_EXT = ('.h', '.hpp', '.hh', '.cpp', '.cc', '.cxx', '.inl', '.mm', '.m')
SKIP_DIR = ('build', 'cmake-build')


def blank_out(text):
    """Source text with comments and string literals replaced by spaces.

    Brace depth is what separates a class a reader can link to from one nested inside it, and
    a brace inside a comment or a string would move that line. The comment ban's lexer is
    already in this directory, so the same precise scan is reused rather than written twice.
    """
    blanked = list(text)
    for begin, end, kind in comment_lexer.comment_spans(text):
        for k in range(begin, min(end, len(blanked))):
            if blanked[k] not in '\n':
                blanked[k] = ' '
    return ''.join(blanked)


def unit_test_guarded_lines(text):
    """Lines whose code exists only when __UNIT_TEST__ is defined.

    The authoring contract puts test-only code in the Coverage section of the page that owns it,
    not on a page of its own, so a test class declared inside `#ifdef __UNIT_TEST__` must not be
    demanded as `docs/HSTL/HUnorderedMapTest/index.html`. The first ledger asked for exactly that.

    Only the branch the guard opens is treated as test-only: an `#else` branch is the production
    code, so a stack entry is cleared there rather than kept.
    """
    guarded = set()
    stack = []
    for number, line in enumerate(text.split('\n'), 1):
        directive = line.strip()
        if directive.startswith('#'):
            if re.match(r'#\s*if(n?def)?\b', directive):
                stack.append(bool(re.search(r'__UNIT_TEST__|__TEST__', directive)))
            elif re.match(r'#\s*(else|elif)\b', directive) and stack:
                stack[-1] = False
            elif re.match(r'#\s*endif\b', directive) and stack:
                stack.pop()
        if any(stack):
            guarded.add(number)
    return guarded


def brace_depths(text):
    """Class-body nesting at the start of each line, where namespace braces are transparent.

    A namespace adds no nesting for this purpose: a class written inside `namespace hbe { … }`
    is still an entry a module page links to. Counting its brace put every engine class one
    level too deep and the first run of this script reported a single entry for the whole tree.
    """
    clean = blank_out(text)
    depths = {}
    line = 1
    depth = 0
    max_depth = 0
    stack = []
    pending = ''
    for ch in clean:
        if ch == '\n':
            line += 1
            # A newline becomes a space in the pending text rather than being dropped: without
            # the separator the previous line's trailing token runs into `namespace`, and the
            # pattern that is supposed to recognise the namespace brace then cannot see it.
            pending += ' '
            continue
        if line not in depths:
            depths[line] = depth
            max_depth = max(max_depth, depth)
        else:
            depths[line] = min(depths[line], depth)
        if ch == '{':
            is_namespace = bool(NAMESPACE_HEAD.search(pending))
            stack.append(is_namespace)
            if not is_namespace:
                depth += 1
            pending = ''
        elif ch == '}':
            if stack and not stack.pop():
                depth -= 1
            pending = ''
        elif ch == ';':
            pending = ''
        else:
            pending = (pending + ch)[-120:]
    # Blank lines are never visited by the character loop above, so they used to be absent from the
    # map — and callers read a missing line as depth 1, "nested, therefore not an entry". That was
    # latent until a comment sweep turned the line before every class into a blank one, and Config
    # then reported zero entries and passed coverage unchecked. Every line gets its shallowest depth.
    for index in range(1, line + 1):
        depths.setdefault(index, max_depth)
    return depths

ENTRY = re.compile(r'^\s*(?:template\s*<[^>]*>\s*)?(class|struct|union|enum)\s+(?:class\s+)?([A-Za-z_]\w*)'
                   r'\s*(?:final\b)?\s*(?::[^;{]*)?(?=[;{])', re.M)
NAMESPACE_HEAD = re.compile(r'(^|[^\w.])\bnamespace\b')
ENUM_ANON = re.compile(r'^\s*enum\s+(?:class\s+)?[A-Za-z_]\w*\s*(?=\{|$)')
MACRO_SET = re.compile(r'^\s*#\s*define\s+(HB_[A-Z0-9_]+|[A-Z][A-Z0-9_]{2,})\b')
NAMESPACED_FUNCS = re.compile(r'^\s*(?:namespace\s+(\w+))', re.M)
EXCLUDE_FILE_LINE = re.compile(r'auto-?generated|do not edit', re.I)


def sources(module=None, ext=SOURCE_EXT):
    root = os.path.join(REPO_ROOT, module) if module else os.path.join(REPO_ROOT, 'Engine')
    for directory, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if not any(s in os.path.join(directory, d).split(os.sep) for s in SKIP_DIR)]
        for f in sorted(files):
            if f.endswith(ext):
                yield os.path.join(directory, f)


def module_of(path):
    parts = os.path.relpath(path, REPO_ROOT).split(os.sep)
    if parts[0] == 'Engine' and len(parts) > 2:
        return parts[1]
    return None


def entries_in(path):
    """Namespace-scope entries of a header — the unit the site links to.

    Two exclusions, both deliberate:
      * a nested class or struct is documented on its owner's page (`UsageRecord` belongs to
        MemoryManager, `Iterator` to HashMap), so requiring `docs/Container/Iterator/index.html`
        would invent a page for something a reader cannot even name unambiguously — three
        different `Iterator` types in this module alone collided on that one path.
      * a .cpp declares implementation, not API. Everything the engine exposes to a caller is
        in a header, and a file-local struct in AtomicStackView.cpp is not a reference page.
    """
    try:
        text = open(path, encoding='utf-8', errors='ignore').read()
    except OSError:
        return []
    if EXCLUDE_FILE_LINE.search('\n'.join(text.split('\n')[:3])):
        return []
    depths = brace_depths(text)
    guarded = unit_test_guarded_lines(text)
    found = []
    for match in ENTRY.finditer(text):
        kind, name = match.group(1), match.group(2)
        # Measured from the keyword, not from match.start(): `^\s*` in MULTILINE happily swallows a
        # blank line, so a class written under one reports a start on the blank line above it.
        line = text[:match.start(1)].count('\n') + 1
        if depths.get(line, 1) != 0:
            continue
        if text[match.end()] == ';':
            # A forward declaration names a type owned elsewhere. Logger.h:27 declares
            # `class TaskSystem;` to break an include cycle, and an earlier revision of this script
            # billed a docs/Log/TaskSystem/ page for it — while Core's real backlog grew with rows
            # for Task in WorkItem.h and TaskRegistry in Task.h, none of which is a definition.
            continue
        if line in guarded:
            found.append((name, 'test-only'))
            continue
        if name.startswith('I') and name[1:2].isupper():
            found.append((name, 'interface'))
        elif kind == 'enum':
            found.append((name, 'enum'))
        elif kind in ('struct', 'union'):
            found.append((name, 'struct'))
        else:
            found.append((name, 'class'))
    if MACRO_SET.search(text) and not found:
        found.append((os.path.basename(path).rsplit('.', 1)[0], 'macro set'))
    return found


def has_pointer(path, module, name):
    """Whether the header carries the API reference pointer for this entry.

    Presence of the exact line is the question here, because adjacency and path validity are already
    enforced by `comments.py` when it decides whether a pointer is an exemption or a violation. Splitting
    the two checks the way the standard splits them means neither tool can quietly disagree: one asks
    "is this comment a valid address", this one asks "does every page have an address".
    """
    wanted = '/// API reference: docs/%s/%s/index.html' % (module, name)
    try:
        text = open(path, encoding='utf-8', errors='ignore').read()
    except OSError:
        return False
    return any(line.strip() == wanted for line in text.split('\n'))


def comment_count(path):
    """Comment lines in this file that the ban requires to move, per comments.py.

    Counted by the same lexer that will enforce the rule, not by a line-start grep, so the
    copyright notice and structural labels are not tallied as work that does not exist.
    """
    try:
        text = open(path, encoding='utf-8', errors='ignore').read()
    except OSError:
        return 0
    findings, _spans = comment_lexer.check_file(os.path.relpath(path, REPO_ROOT), text)
    return len(findings)


def modules():
    root = os.path.join(REPO_ROOT, 'Engine')
    return sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))


def ledger_rows():
    rows = []
    for path in sources():
        module = module_of(path)
        if module is None:
            continue
        for name, kind in entries_in(path):
            page = os.path.join(DOCS, module, name, 'index.html')
            rows.append({
                'module': module,
                'entry': name,
                'kind': kind,
                'source': os.path.relpath(path, REPO_ROOT),
                'page': os.path.relpath(page, REPO_ROOT),
                'exists': os.path.isfile(page),
                'test_only': kind == 'test-only',
                'method_pages': len([f for f in os.listdir(os.path.dirname(page)) if f.endswith('.html') and f != 'index.html']) if os.path.isdir(os.path.dirname(page)) else 0,
                'pointer': has_pointer(path, module, name),
                'comments': comment_count(path),
            })
    return rows


def site_links_ok(wanted=None):
    """docs/index.html must reach every module page, and every module page must reach its entries."""
    problems = []
    index = os.path.join(DOCS, 'index.html')
    if not os.path.isfile(index):
        return ['docs/index.html is missing — the module index is the site start page']
    text = open(index, encoding='utf-8', errors='ignore').read()
    for module in modules():
        if wanted and module not in wanted:
            continue
        page = os.path.join(DOCS, module, 'index.html')
        if not os.path.isfile(page):
            problems.append('docs/%s/index.html is missing' % module)
            continue
        if 'href="%s/index.html"' % module not in text:
            problems.append('docs/index.html does not link %s/index.html' % module)
        module_text = open(page, encoding='utf-8', errors='ignore').read()
        for path in sources(os.path.join('Engine', module)):
            for name, kind in entries_in(path):
                if kind == 'test-only':
                    continue
                if os.path.isdir(os.path.join(DOCS, module, name)) and 'href="%s/index.html"' % name not in module_text.replace('%s/' % module, ''):
                    if ('%s/index.html' % name) not in module_text:
                        problems.append('docs/%s/index.html does not link %s/index.html' % (module, name))
    return problems


def write_ledger():
    rows = ledger_rows()
    by_module = {}
    for r in rows:
        by_module.setdefault(r['module'], []).append(r)
    out = ['<!-- generated by .pi/skills/hb-standards/scripts/docs_coverage.py — do not hand-edit -->',
           '# Documentation coverage ledger',
           '',
           'Every entry the engine declares, and whether the HTML reference has a page for it.',
           'Source comments are banned, so a missing page means the contract is nowhere.',
           '',
           '| module | API entries | with a page | missing | test-only | pages addressed from the header | comment lines still in sources |',
           '|---|---|---|---|---|---|---|']
    for module in sorted(by_module):
        group = by_module[module]
        api = [g for g in group if not g['test_only']]
        have = sum(1 for g in api if g['exists'])
        addressed = sum(1 for g in api if g['exists'] and g['pointer'])
        # Comments are counted over every header of the module, not over the files that own an
        # entry. Counting only those hid Engine/Config/BuildConfig.h — a header of macro switches
        # whose whole content is documentation and which declares no class — and reported 12 lines
        # for a module that actually owed 102, which is the difference between a small pass and a
        # documentation migration.
        comments = sum(comment_count(path) for path in sources(os.path.join(REPO_ROOT, 'Engine', module), PROSE_EXT))
        out.append('| %s | %d | %d | %d | %d | %d | %d |'
                   % (module, len(api), have, len(api) - have, len(group) - len(api), addressed, comments))
    out += ['', '## Entries', '', '| module | entry | kind | source | page | method pages | header pointer | status |', '|---|---|---|---|---|---|---|---|']
    for r in sorted(rows, key=lambda r: (r['module'], r['entry'])):
        out.append('| %s | %s | %s | `%s` | `%s` | %d | %s | %s |' % (r['module'], r['entry'], r['kind'], r['source'],
                                                                     r['page'], r['method_pages'],
                                                                     'yes' if r['pointer'] else ('—' if not r['exists'] else '**no**'),
                                                                     'documented' if r['exists'] else ('test-only — owns a Coverage row' if r['test_only'] else 'MISSING')))
    os.makedirs(os.path.dirname(LEDGER), exist_ok=True)
    open(LEDGER, 'w').write('\n'.join(out) + '\n')
    api = [r for r in rows if not r['test_only']]
    documented = sum(1 for r in api if r['exists'])
    test_only = len(rows) - len(api)
    print('ledger written: %d API entries (%d documented, %d missing) + %d test-only -> %s'
          % (len(api), documented, len(api) - documented, test_only, os.path.relpath(LEDGER, REPO_ROOT)))
    return 0


def check(argv):
    if not os.path.isfile(LEDGER):
        print('[NONE] no coverage ledger exists — run `docs_coverage.py ledger` first, an')
        print('       unmeasured tree is not a passing tree.')
        return 0
    rows = ledger_rows()
    wanted = set(argv)
    # `check Core` and `check Engine/Core` are not the same question. Rows are keyed by module
    # name, so the path form matches nothing and used to print "0 missing page(s)" for a module
    # that owed twenty-three — a gate that reports a clean verdict over zero checked files is
    # worse than no gate, because the reader stops looking. Refuse instead.
    known = set(r['module'] for r in rows)
    unknown = sorted(wanted - known)
    if unknown:
        for name in unknown:
            guess = name.split('/')[-1] if name.startswith('Engine/') and '/' in name else None
            hint = ' — pass the module name, which is %s' % guess if guess in known else ''
            print('[ERROR] no ledger rows for %r%s' % (name, hint))
        print('        known modules: %s' % ', '.join(sorted(known)))
        return 3
    missing = [r for r in rows if not r['exists'] and not r['test_only'] and (not wanted or r['module'] in wanted)]
    by_module = {}
    for r in missing:
        by_module.setdefault(r['module'], []).append(r)
    for module in sorted(by_module):
        names = ', '.join(sorted(r['entry'] for r in by_module[module]))
        print('[MISSING PAGE] docs/%s/ — %d entry(ies): %s' % (module, len(by_module[module]), names))
    problems = site_links_ok(wanted if wanted else None)
    for p in problems:
        print('[SITE LINK] %s' % p)
    unpointed = [r for r in rows if r['exists'] and not r['test_only'] and not r['pointer']
                 and (not wanted or r['module'] in wanted)]
    by_pointer = {}
    for r in unpointed:
        by_pointer.setdefault(r['module'], []).append(r)
    for module in sorted(by_pointer):
        names = ', '.join(sorted(r['entry'] for r in by_pointer[module]))
        print('[NO POINTER] docs/%s/ — %d page(s) whose header carries no address: %s'
              % (module, len(by_pointer[module]), names))
    total = len(missing) + len(problems) + len(unpointed)
    scope = ' '.join(sorted(wanted)) if wanted else 'all modules'
    print('docs coverage: %s — %d missing page(s), %d site-link problem(s), %d page(s) without a pointer'
          % (scope, len(missing), len(problems), len(unpointed)))
    return 1 if total else 0


def main(argv):
    action = argv[0] if argv else None
    if action == 'ledger':
        return write_ledger()
    if action == 'check':
        return check(argv[1:])
    if action == 'missing':
        for r in ledger_rows():
            if r['module'] == (argv[1] if len(argv) > 1 else '') and not r['exists']:
                print('%s\t%s\t%s\t%d comment lines' % (r['entry'], r['kind'], r['source'], r['comments']))
        return 0
    print(__doc__)
    return 3


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
