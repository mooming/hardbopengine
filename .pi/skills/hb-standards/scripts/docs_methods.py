#!/usr/bin/env python3
"""Which declared methods have no page. complements docs_coverage.py, which only checks class pages.

docs_coverage.py answers "does this class own a page" and counts the HTML files in that page's
directory. Neither number means anything about coverage: docs/Log/Logger/ holds 18 method pages and
not one of them is SetIODriver, StopDriverThread or DriverLoop, so a class page plus 18 files read as
"documented" while the entire IO-driver mechanism — the thread's owner, the lock that makes stream
withdrawal safe, the shutdown order — existed only in `///` comments about to be deleted by a sweep
that trusts this gate.

Method names come from the clang AST through layout.py, not from a pattern over text, because the
question is "what can a caller call" and only the compiler knows: `operator bool` has no name a regex
can find, `using TValue = std::function<void(int)>` looks like a call and is not one, and the members
of Engine/Math/VectorCommonImpl.inl belong to the class that #includes them.

Page naming follows .Plans/AUTHORING_method_and_class_pages.md: a page per method, lowercase-hyphen,
`AddLog` -> `add-log.html`, `IsDrainTaskRunning` -> `is-drain-task-running.html`. The comparison
normalises punctuation away on both sides, so `set-io-driver.html` matches `SetIODriver` however the
acronym was hyphenated — a stricter comparison would report a gap that is only a spelling.

Constructors and destructors own `constructors.html` and `destructor.html` (one page each, however
many overloads), because that is what every existing page does. Operators are reported separately:
`operator<` becomes `operator-less.html`, and a class that documents its operators in a table on its
own page is answering the question, just not with a file per name. A conversion operator owns
`conversion.html`, because it has no name a reader could search for and a class whose entire API is one
conversion -- `hbe::EndLine`, which exists to become a newline -- would otherwise require no page at all.
A user-defined literal operator is still unrecognised and asks for nothing.

Usage: docs_methods.py [--db <compile_commands.json>] <Module> [<Module> ...]
Exit status: 1 when a public method of an entry with a page has no page of its own.
"""

import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))
DOCS = os.path.join(REPO_ROOT, 'docs')
sys.path.insert(0, SCRIPT_DIR)
import layout

# Spellings come from .Plans/AUTHORING_method_and_class_pages.md section 2 -- that table is the rule and
# this is its machine-readable copy, so the two must be edited together. `operator[]` is `index` and not
# `subscript` because that is what the contract says and what docs/Container/Array/operator-index.html
# already on disk uses. `<<` and `>>` are `left-shift` and `right-shift`, which an earlier copy of this
# table omitted entirely, and my own Resource pages went out named `shift-left` in the other direction.
OPERATOR_PAGES = {
    '<': 'less', '>': 'greater', '==': 'equal', '!=': 'not-equal', '<=': 'less-equal',
    '>=': 'greater-equal', '<=>': 'three-way', '()': 'call', '[]': 'index', '=': 'assign',
    '+=': 'add-assign', '-=': 'subtract-assign', '*=': 'multiply-assign', '/=': 'divide-assign',
    '*': 'multiply', '/': 'divide', '+': 'add', '-': 'subtract', '%': 'modulo',
    '<<': 'left-shift', '>>': 'right-shift', '<<=': 'left-shift-assign', '>>=': 'right-shift-assign',
    '&': 'and', '|': 'or', '^': 'xor', '~': 'complement', '!': 'not',
    '&&': 'logical-and', '||': 'logical-or', '++': 'increment', '--': 'decrement',
    ',': 'comma', '->': 'member-access', '->*': 'member-pointer-access', '.*': 'member-access',
    'bool': 'bool', 'new': 'new', 'delete': 'delete', 'new[]': 'new-array', 'delete[]': 'delete-array',
}
CONVERSION_PAGE = 'cast'        # the contract's `operator-cast.html` for `operator T()`


def norm(name):
    return re.sub(r'[^a-z0-9]', '', name.lower())


def member_name(description):
    """The declared name inside a member description like `function SetMesh` or `function ConfigParam<T, N>`.

    Qualifiers come first and the name may contain spaces, so the name is whatever follows the kind word
    `function`: `static function Get` is `Get`, and `function ConfigParam<T, IsAtomic>` is the class's own
    constructor — whose last whitespace token is `IsAtomic>`, which a plain comparison reads as a method
    the class does not have and then demands a page for.
    """
    if 'function' in description:
        name = description.rsplit('function', 1)[1]
    else:
        name = description.split()[-1] if description.split() else description
    return strip_templates(name.strip())


def strip_templates(name):
    """`ConfigParam<T, IsAtomic>` -> `ConfigParam`, repeatedly, because nested lists close in pairs.

    Only balanced `<...>` groups are removed, and the loop is bounded: an unterminated `<` in a macro or a
    `.inl` fragment must not spin.
    """
    for _unused in range(8):
        stripped = re.sub(r'<[^<>]*>', '', name)
        if stripped == name:
            return name
        name = stripped
    return name


def module_headers(module):
    root = os.path.join(REPO_ROOT, 'Engine', module)
    out = []
    for dirpath, _, filenames in os.walk(root):
        for name in sorted(filenames):
            if name.endswith('.h'):
                out.append(os.path.join(dirpath, name))
    return out


def page_names(module, class_name):
    folder = os.path.join(DOCS, module, class_name)
    if not os.path.isdir(folder):
        return None
    return [f for f in os.listdir(folder) if f.endswith('.html') and f != 'index.html']


def expected_file(description):
    """The page a method description should be satisfied by, or None when no page is expected."""
    name = member_name(description)
    if 'operator' in description:
        tail = description[description.index('operator') + len('operator'):].strip()
        symbol = tail.split('(')[0].strip() or name
        spelled = OPERATOR_PAGES.get(symbol)
        if spelled:
            return 'operator-' + spelled
        bare = strip_templates(symbol).replace('const', '').strip()
        if re.fullmatch(r'[A-Za-z_][\w:]*\s*\**', bare):
            return 'operator-' + CONVERSION_PAGE
        return None
    if name.startswith('~'):
        return 'destructor'
    return norm(strip_templates(name))


def is_constructor(description, class_name):
    """True when the declaration names the class itself, template parameter list and all.

    clang names a class template's constructor `ConfigParam<T, IsAtomic>`, so the last whitespace token is
    `IsAtomic>` and a plain comparison reads the constructor as a method called `IsAtomic` that owns no
    page. Stripping the template arguments first is what makes a templated constructor look like one.
    """
    name = member_name(description)
    return not name.startswith('~') and norm(name) == norm(class_name)


_DECL_CACHE = {}


def declaration_text(path, line, window=4):
    """The source text of a declaration, which the AST does not carry.

    clang reports `function operator=` and stops there, so whether a member is `= delete`, `= default` or
    hand-written can only be read from the file. Four lines is enough for any real declaration in this
    tree and keeps a stray `;` from swallowing the next member.
    """
    key = (path, line)
    if key in _DECL_CACHE:
        return _DECL_CACHE[key]
    try:
        lines = _DECL_CACHE.setdefault('__files__', {}).setdefault(path, open(path, encoding='utf-8', errors='ignore').read().splitlines())
    except OSError:
        text = ''
        _DECL_CACHE[key] = text
        return text
    text = '\n'.join(lines[line - 1:line - 1 + window])
    cut = len(text)
    for terminator in (';', '{'):
        at = text.find(terminator)
        if at >= 0:
            cut = min(cut, at)
    text = text[:cut]
    _DECL_CACHE[key] = text
    return text


def check_module(module, entries, by_name):
    """(rows with no page, operators to verify by hand, notes) for one module."""
    missing, operators, notes, seen, defaulted, deleted = [], [], [], set(), [], []
    for header in module_headers(module):
        rel = os.path.relpath(header, REPO_ROOT)
        try:
            records, _guarded, module_notes = layout.collect_records(rel, entries, by_name)
        except Exception as exc:
            notes.append('%s: [SKIP] %s' % (rel, exc))
            continue
        for note in module_notes:
            notes.append(note if ':' in note else '%s: [PARTIAL] %s' % (rel, note))
        for record, line, home in records:
            class_name = record.get('name')
            if not class_name:
                continue
            pages = page_names(module, class_name)
            if pages is None:
                continue  # the class owns no page directory; docs_coverage reports that gap
            # Strip the extension first: norm('out.html') is 'outhtml', and chopping five
            # characters off it asks for a page nobody could name.
            folded = {norm(os.path.splitext(p)[0]) for p in pages}
            for block, what, member_file, member_line, note in layout.members_of(record, record.get('tagUsed', 'class'), rel):
                if block < 7 or 'function' not in what:
                    continue
                if 'operator' in what:
                    want = expected_file(what)
                    if want is None:
                        operators.append('%s:%d: %s — %s' % (rel, member_line, class_name, what))
                        continue
                    if norm(want) in folded or (class_name, want) in seen:
                        continue
                    # clang tells us an operator exists and nothing about how it is defined, so the
                    # spelling of its definition comes from the source line itself. A `= delete` cannot be
                    # called and a `= default` has no behaviour of its own; what a class forbids or
                    # inherits by default is its ownership story, which the class page carries. Both are
                    # counted in the summary rather than dropped, so the exemption stays visible.
                    spelling = declaration_text(member_file, member_line)
                    if '= delete' in spelling:
                        deleted.append('%s:%d: %s' % (rel, member_line, class_name))
                        continue
                    if '= default' in spelling:
                        defaulted.append('%s:%d: %s' % (rel, member_line, class_name))
                        continue
                    seen.add((class_name, want))
                    missing.append('%s:%d: DOC-METHOD  %s — no %s.html in docs/%s/%s/'
                                   % (rel, member_line, class_name, want, module, class_name))
                    continue
                if is_constructor(what, class_name):
                    want = 'constructors'
                else:
                    want = expected_file(what)
                # norm() on both sides: `folded` is normalised, and `want` is a spelled-out operator name
                # like `operator-right-shift` whose hyphens norm() removes. Comparing them raw means an
                # operator page never matches the file that satisfies it, and only single-word pages such
                # as `constructors` pass by the accident of having no hyphen.
                if want and norm(want) not in folded and (class_name, want) not in seen:
                    # Ten overloads of Out are one page, and ten findings would be ten lines saying
                    # the same thing. Only the first call site is named.
                    seen.add((class_name, want))
                    missing.append('%s:%d: DOC-METHOD  %s — no %s.html in docs/%s/%s/'
                                   % (rel, member_line, class_name, want, module, class_name))
    return missing, operators, notes, defaulted, deleted


def main(argv):
    db = os.path.join(REPO_ROOT, 'cmake-build-debug', 'compile_commands.json')
    if '--db' in argv:
        at = argv.index('--db')
        db = argv[at + 1]
        argv = argv[:at] + argv[at + 2:]
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    entries, by_name = layout.read_compile_db(db)
    total = 0
    for module in argv:
        missing, operators, notes, defaulted, deleted = check_module(module, entries, by_name)
        for note in notes:
            print('    %s' % note)
        for row in missing:
            print('    %s' % row)
        for row in sorted(set(operators)):
            print('    [OPERATOR] %s' % row)
        total += len(missing)
        print('    method coverage %s: %d method(s) without a page, %d operator(s) to check by hand, '
              '%d defaulted, %d deleted'
              % (module, len(missing), len(set(operators)), len(set(defaulted)), len(set(deleted))))
    print('method pages missing: %d' % total)
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
