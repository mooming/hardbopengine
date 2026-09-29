#!/usr/bin/env python3
"""Twelve-block member layout checker for HardBop Engine classes and structs.

The standard (docs/CodingStandards.md) makes the data layer of a class one visible block,
separated from the function layer. Every class body is ordered:

    block  0  nested types, aliases, enumerators        public -> protected -> private
    block  1  public   static   variables
    block  2  public            variables
    block  3  protected static variables
    block  4  protected        variables
    block  5  private   static variables
    block  6  private          variables
    block  7  public   static  functions
    block  8  public           functions   (constructors, destructors, operators included)
    block  9  protected static functions
    block 10  protected        functions
    block 11  private   static functions
    block 12 private           functions

So the block index sequence over a class body's members must never decrease.

The source of truth is the clang AST, not a regex. C++ declarator syntax defeats pattern
matching exactly where this rule lives: `std::function<void(int)> cb;` is data that
contains parentheses, `using TLogFunc = std::function<void(std::ostream&)>;` is a type
that looks like a call, and `explicit operator bool() const` is a function with no name.
clang answers "field or method, static or not, which access section" without guessing.

Members hidden behind `#if PROFILE_ENABLED` are not in the AST when the flag is 0, which
it is in Engine/Config/BuildConfig.h: such a file is reported PARTIAL rather than clean,
because a pass over a half-parsed class is the false green this script exists to prevent.

Output: `path:line: MEMBER-LAYOUT  ...`, exit 1 when anything was found.
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys

ACCESS_RANK = {'public': 0, 'protected': 1, 'private': 2}

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))

TYPE_KINDS = {'CXXRecordDecl', 'EnumDecl', 'TypeAliasDecl', 'TypedefDecl', 'ClassTemplateDecl',
              'ClassTemplatePartialSpecializationDecl', 'NamespaceAliasDecl', 'EnumConstantDecl'}
METHOD_KINDS = {'CXXMethodDecl', 'CXXConversionDecl', 'CXXDestructorDecl', 'CXXConstructorDecl'}
DATA_KINDS = {'FieldDecl', 'IndirectFieldDecl'}
EXEMPT_KINDS = {'AccessSpecDecl', 'EmptyDecl', 'FullComment', 'FinalAttr', 'FriendDecl',
                'FriendTemplateDecl', 'StaticAssertDecl', 'UsingDecl', 'UsingShadowDecl',
                'UsingDirectiveDecl', 'UnresolvedUsingValueDecl', 'UnresolvedUsingTypenameDecl',
                'ShapeInfoTemplateDecl', 'PRAGMA_CLASS_SCOPE', 'TypeAliasTemplateDecl',
                'TemplateTypeParmDecl', 'BuiltinTemplateDecl', 'MSGuidDecl', '__AttributeDecl'}

UNWRAP = {'TemplateDecl', 'ClassTemplatePartialSpecializationDecl', 'TypeAliasTemplateDecl', 'ClassTemplateDecl'}
GUARDED = re.compile(r'^\s*#\s*if(n?def)?\b')
CLASS_DECL = re.compile(r'^\s*(?:template\s*<[^>]*>\s*)?(class|struct)\s+[A-Za-z_]\w*', re.M)
NAMESPACE = re.compile(r'^\s*namespace\s+([A-Za-z_]\w*)', re.M)


def dump_filter(text, target):
    """The name substring clang should keep.

    -ast-dump-filter matches the qualified name, so the outermost namespace of the file keeps
    every declaration of its own project while dropping the whole standard library: 624 MB
    unfiltered against ~1 MB filtered, measured on Engine/Memory/MemoryManager.h. A file with
    no namespace has no such prefix, so its own class names are used instead.
    """
    namespaces = NAMESPACE.findall(text)
    if namespaces:
        return namespaces[0] + '::'
    names = re.findall(r'^\s*(?:template\s*<[^>]*>\s*)?(?:class|struct)\s+([A-Za-z_]\w*)', text, re.M)
    if not names:
        raise Skipped('no namespace and no class name to filter the AST by')
    return names[0]


class Skipped(Exception):
    def __init__(self, reason):
        Exception.__init__(self, reason)
        self.reason = reason


def read_compile_db(path):
    try:
        entries = json.load(open(path))
    except (OSError, ValueError) as exc:
        raise Skipped('compile database unreadable: %s' % exc)
    by_name = {}
    for e in entries:
        by_name[os.path.basename(e['file'])] = e
    return entries, by_name


def resolve_command(target, entries, by_name):
    """Pick the translation unit whose flags can parse this file.

    A header inherits the flags of its own .cpp when one exists, otherwise of the nearest
    .cpp in the tree by directory path. Those flags are the project's own, so the header is
    parsed under the same macros the build uses.
    """
    if target.endswith('.cpp') and os.path.basename(target) in by_name:
        return by_name[os.path.basename(target)]
    sibling = os.path.basename(target).rsplit('.', 1)[0] + '.cpp'
    if sibling in by_name:
        return by_name[sibling]
    absolute = os.path.abspath(target)
    best = None
    best_score = -1
    for e in entries:
        score = os.path.commonpath([os.path.dirname(absolute), os.path.dirname(e['file'])]).count(os.sep)
        if score > best_score:
            best, best_score = e, score
    if best is None:
        raise Skipped('no compile command available for this path')
    return best


def clang_argv(entry, target, name_filter):
    args = shlex.split(entry['command'])
    compiler = args[0]
    flags = []
    i = 1
    while i < len(args):
        a = args[i]
        if a == '-o':
            i += 2
            continue
        if a == '-c' or a.endswith(('.cpp', '.cc', '.m', '.mm')):
            i += 1
            continue
        flags.append(a)
        i += 1
    cmd = [compiler] + flags + ['-fsyntax-only', '-w', '-Xclang', '-ast-dump=json', '-Xclang', '-ast-dump-filter=' + name_filter]
    # clang runs in the compile database's own directory, so the target must travel as an
    # absolute path; a relative one resolves against that directory and is not found.
    absolute = os.path.abspath(target)
    cmd += ['-x', 'c++', absolute] if target.endswith(('.h', '.hpp', '.inl')) else [absolute]
    return cmd


def ast_of(target, entries, by_name, clang_override=None):
    """The clang AST of this file, plus how much of it clang was allowed to see.

    Returns (roots, has_preprocessor_guard, note).

    A file whose body sits behind `#ifdef __UNIT_TEST__` — Engine/Renderer/RendererTest.h and
    three Math sources, measured — preprocesses to nothing under the Dev configuration, and clang
    then exits 0 having dumped zero declarations. Reading that as clean would be a verdict about
    an unexamined class, so the pass is retried with the define the build itself uses, and the
    note says which macros were active.
    """
    entry = resolve_command(target, entries, by_name)
    text = open(target, encoding='utf-8', errors='ignore').read()
    guarded = bool(GUARDED.search(text))

    def run(extra_defines):
        cmd = clang_argv(entry, target, dump_filter(text, target)) + extra_defines
        if clang_override:
            cmd[0] = clang_override
        try:
            return subprocess.run(cmd, cwd=entry['directory'], capture_output=True, text=True)
        except OSError as exc:
            raise Skipped('clang could not run: %s' % exc)

    proc = run([])
    if proc.returncode != 0:
        detail = [d for d in (proc.stderr + proc.stdout).split('\n') if d.strip()]
        hint = next((d for d in detail if 'error' in d), detail[0] if detail else 'no output')
        raise Skipped('clang rejected this file: %s' % hint[:160])
    roots = parse_ast_stream(proc.stdout)
    if roots:
        return roots, guarded, None
    proc = run(['-D__UNIT_TEST__=1'])
    roots = parse_ast_stream(proc.stdout) if proc.returncode == 0 else []
    if roots:
        return roots, guarded, 'checked with -D__UNIT_TEST__=1, which the Dev build does not define'
    return [], guarded, 'no declaration is visible to this build configuration'


def unwrap(node):
    kind = node.get('kind')
    if kind in UNWRAP:
        inner = node.get('inner') or []
        for c in inner:
            if c.get('isImplicit'):
                continue
            return unwrap(c)
    return node


def member_loc(node, home):
    """(file, line, note) for a declaration, resolving clang's three location shapes.

    clang's JSON writes the file of a location in one of three ways, and only one of them names
    the file outright:
      loc.file .............. the declaration is in that file
      loc.includedFrom ...... the declaration is in a file included from the named file, and
                              `line` is relative to that *included* file, not the includer.
                              Engine/Math/VectorCommonImpl.inl and MatrixCommonImpl.inl are
                              #included inside a class body, so every member they contribute
                              arrives here; the line to fix is in the .inl
      neither ............... same file as the enclosing record
    Guessing wrong reports a line number past the end of the includer, which sends a reader to
    a file that has no such line.
    """
    loc = node.get('loc') or {}
    line = loc.get('line', 0)
    if loc.get('file'):
        return os.path.abspath(loc['file']), line, None
    via = (loc.get('includedFrom') or {}).get('file')
    if via:
        name = node.get('name') or ''
        resolved = resolve_included_file(via, line, name)
        if resolved:
            return resolved, line, None
        return via, line, 'line %d of a file %s includes' % (line, os.path.basename(via))
    return home, line, None


INCLUDE_LINE = re.compile(r'^\s*#\s*include\s+["<]([^">]+)[">]', re.M)
_include_cache = {}


def resolve_included_file(includer, line, name):
    """The included file a declaration came from, proven by the text at that line.

    The includer's own #include list is the only evidence available from the AST, so a candidate
    is accepted only when the reported line really names the declaration. A guess that fails the
    check returns None, and the caller says so instead of naming a file it cannot prove.
    """
    try:
        text = open(includer, encoding='utf-8', errors='ignore').read()
    except OSError:
        return None
    directory = os.path.dirname(includer)
    for candidate in INCLUDE_LINE.findall(text):
        for path in (os.path.join(directory, candidate), os.path.join(REPO_ROOT, 'Engine', candidate), os.path.join(REPO_ROOT, candidate)):
            if not os.path.isfile(path):
                continue
            lines = _include_cache.setdefault(path, open(path, encoding='utf-8', errors='ignore').read().split('\n'))
            if 0 < line <= len(lines) and name and name in lines[line - 1]:
                return os.path.abspath(path)
    return None


def classify(node, home):
    """Return (block_sort_key, description, file, line) or None when the construct is exempt.

    An unnamed union or struct in a class body is not a type declaration - it is the data layer
    written in place, because an unnamed member's fields become members of the enclosing class.
    Sorting it as a type would demand the union be hoisted above the constants, which is what
    Engine/Math/Vector3.h originally reported as a false violation.
    """
    node = unwrap(node)
    kind = node.get('kind')
    if kind is None or kind in EXEMPT_KINDS:
        return None
    if node.get('isImplicit'):
        return None
    name = node.get('name', '')
    is_static = node.get('storageClass') == 'static'
    access = node.get('access', 'private')
    rank = ACCESS_RANK.get(access, 0)
    loc = node.get('loc') or {}
    where = member_loc(node, home)
    unnamed_record = kind in TYPE_KINDS and not name and kind not in ('EnumDecl', 'ClassTemplateDecl')
    if unnamed_record:
        block = 2 + 2 * rank
        return (block, 'anonymous %s' % node.get('tagUsed', 'struct'), where, None)
    if kind in TYPE_KINDS:
        return (0, 'type %s' % name, where, None)
    if kind in DATA_KINDS or (kind == 'VarDecl' and is_static):
        block = 1 + 2 * rank + (0 if is_static else 1)
        what = 'static variable' if is_static else 'variable'
        return (block, '%s %s' % (what, name), where, None)
    if kind in METHOD_KINDS:
        block = 7 + 2 * rank + (0 if is_static else 1)
        kind_word = 'static function' if is_static else 'function'
        label = name if str(name).startswith('operator') or name else '(unnamed)'
        return (block, '%s %s' % (kind_word, label), where, None)
    if kind in ('TemplateTypeParmDecl', 'DeductionGuideList', 'OmpDeclareMapperList'):
        return None
    return None


def parse_ast_stream(text):
    """clang writes one JSON document per filtered declaration, back to back.

    json.loads accepts one document only, so the stream is decoded root by root. Feeding the
    whole buffer to json.loads raises "Extra data" at the second root, which is exactly what
    happened on the first run of this script against a namespace-wide filter.
    """
    decoder = json.JSONDecoder()
    roots = []
    index = 0
    n = len(text)
    while index < n:
        while index < n and text[index] in ' \n\t\r':
            index += 1
        if index >= n:
            break
        root, index = decoder.raw_decode(text, index)
        roots.append(root)
    return roots


def records_in(ast, target, text):
    """Every class/struct body defined in this file, with the file it belongs to.

    A templated class reports its definition on the inner CXXRecordDecl, and clang leaves the
    `file` off that node's loc - it is present on the enclosing ClassTemplateDecl only.
    Filtering on a node's own loc.file therefore silently skips every template in the engine:
    measured on Engine/Core/ScopedLock.h and Engine/Math/Vector3.h, both of which came back
    "no class body" on the first run. The file is inherited from the nearest ancestor that
    carries one, and the byte offset must land inside this file as a second lock.
    """
    absolute = os.path.abspath(target)
    size = len(text.encode('utf-8'))
    found = []
    seen = set()

    def walk(node, home):
        loc = node.get('loc') or {}
        own = loc.get('file')
        home = os.path.abspath(own) if own else home
        if node.get('kind') == 'CXXRecordDecl' and node.get('completeDefinition') and home == absolute:
            begin = ((node.get('range') or {}).get('begin') or {}).get('offset')
            if begin is None or begin >= size:
                return
            key = (node.get('name'), loc.get('line'), begin)
            if key not in seen:
                seen.add(key)
                found.append((node, loc.get('line', 0), home))
        for c in node.get('inner') or []:
            walk(c, home)

    for root in ast:
        walk(root, None)
    return found


def members_of(record, tag, home):
    """Sequence of (block, description, file, line) in source order, with access from AccessSpecDecl.

    The file is carried per member because Engine/Math/VectorCommonImpl.inl and
    Engine/Math/MatrixCommonImpl.inl are #included *inside* a class body: those members belong
    to the class, so they must be sorted with it, but the line to fix lives in the .inl.
    Attributing them to the includer reports a line number past the end of that file.
    """
    default = 'public' if tag == 'struct' else 'private'
    access = default
    out = []
    for child in record.get('inner') or []:
        if child.get('kind') == 'AccessSpecDecl':
            access = child.get('access', access)
            continue
        inner = unwrap(child)
        verdict = classify(dict(inner, access=inner.get('access') or access), home)
        if verdict is None:
            continue
        block, what, where = verdict[0], verdict[1], verdict[2]
        member_file, member_line, note = where
        if not member_file:
            member_file, member_line, note = home, (child.get('loc') or {}).get('line', 0), None
        out.append((block, what, member_file, member_line, note))
    return out


def first_disorder(seq):
    """Return [(file, line, note, what, block, prev_block, prev_what, prev_line)] for members out of order."""
    bad = []
    best_block = -1
    best = None
    for block, what, member_file, member_line, note in seq:
        if block < best_block:
            bad.append((member_file, member_line, note, what, block, best_block, best[0], best[1]))
            continue
        best_block = block
        best = (what, member_line)
    return bad


def check_file(target, entries, by_name, clang_override=None):
    text = open(target, encoding='utf-8', errors='ignore').read()
    if not CLASS_DECL.search(text):
        return [], [], 'no class or struct defined here'
    ast, guarded, config_note = ast_of(target, entries, by_name, clang_override)
    notes = []
    if guarded:
        notes.append('%s: [PARTIAL] a preprocessor guard may hide members from the AST' % target)
    if config_note:
        notes.append('%s: [PARTIAL] %s' % (target, config_note))
    records = records_in(ast, target, text)
    if not records:
        return [], notes, 'no class body visible to clang in this file'
    findings = []
    for record, line, home in records:
        name = record.get('name') or '(anonymous at line %d)' % line
        seq = members_of(record, record.get('tagUsed', 'class'), home)
        if not seq:
            continue
        bad = first_disorder(seq)
        for member_file, member_line, note, what, block, prev_block, prev_what, prev_line in bad:
            shown = os.path.relpath(member_file, REPO_ROOT) if member_file else target
            where = '%s  [%s]' % (shown, note) if note else shown
            findings.append('%s:%d: MEMBER-LAYOUT  %s — %s belongs in block %d, but %s at line %d already sets block %d'
                            % (where, member_line, name, what, block, prev_what, prev_line, prev_block))
    return findings, notes, ''


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('files', nargs='*')
    parser.add_argument('--db', default='cmake-build-debug/compile_commands.json')
    parser.add_argument('--clang', default=None)
    parser.add_argument('--quiet', action='store_true')
    opts = parser.parse_args(argv)

    if not opts.files:
        print('usage: layout.py <file ...> [--db compile_commands.json]', file=sys.stderr)
        return 3
    try:
        entries, by_name = read_compile_db(opts.db)
    except Skipped as exc:
        print('[SKIP] all — %s' % exc)
        return 0

    total = 0
    partial = 0
    checked = 0
    skipped = 0
    silent = 0
    for target in opts.files:
        try:
            findings, notes, reason = check_file(target, entries, by_name, opts.clang)
        except Skipped as exc:
            print('%s: [SKIP] %s' % (target, exc.reason))
            skipped += 1
            continue
        checked += 1
        for n in notes:
            print(n)
            partial += 1
        for f in findings:
            print(f)
        total += len(findings)
        if not findings:
            silent += 1
            if reason:
                print('%s: [NONE] %s' % (target, reason))
            elif not opts.quiet:
                print('%s: [PASS] member layout' % target)
    print('member layout: %d file(s) checked (%d clean, %d with findings), %d skipped, %d partial — %d violation(s)'
          % (checked, silent, checked - silent, skipped, partial, total))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
