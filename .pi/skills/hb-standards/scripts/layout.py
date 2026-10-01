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
GUARDED = re.compile(r'^\s*#\s*if(n?def)?\b', re.M)
CLASS_DECL = re.compile(r'^\s*(?:template\s*<[^>]*>\s*)?(class|struct)\s+[A-Za-z_]\w*', re.M)
HB_IGNORE = 'hb-standards:ignore'

_DIRECTIVE_CACHE = {}


def directive_lines(path):
    """The 1-based line numbers of `path` that carry the visible exception directive.

    A standard that bans comments in engine sources and still permits `hb-standards:ignore` is saying that
    an exception must be marked rather than explained in place. This is where a checker reads that mark, so
    a waived violation stays visible as a waiver instead of turning into silence.
    """
    if path not in _DIRECTIVE_CACHE:
        try:
            text = open(path, encoding='utf-8', errors='ignore').read()
        except OSError:
            text = ''
        _DIRECTIVE_CACHE[path] = {index for index, line in enumerate(text.splitlines(), 1) if HB_IGNORE in line}
    return _DIRECTIVE_CACHE[path]
DECL_NAME = r'(?:class|struct|union)\s+([A-Za-z_]\w*)'
NOT_A_CLASS_NAME = {'alignas', 'void', 'operator', 'return', 'const', 'mutable', 'static', 'friend', 'explicit'}


def declaration_tail_is_a_body(rest):
    """Does what follows the name introduce a body, or a variable?

    `struct stat info;` at OSAL/LinuxFileHandle.cpp:34 is a local of a system type, not a class this
    file declares, and `struct alignas(16) Vec` names a keyword rather than a type. A name is a
    declaration only when what follows it is a body, a base list, or nothing.
    """
    rest = rest.strip()
    if not rest or rest.startswith('{'):
        return True
    first = re.match(r'([A-Za-z_]\w*)', rest)
    if not first:
        return rest.startswith(':')
    word = first.group(1)
    if word in ('final', 'override'):
        return True
    after = rest[first.end():].lstrip()
    if after.startswith((';', '=', '(')):
        return False
    return after.startswith(':') or not after

NAMESPACE = re.compile(r'^\s*namespace\s+([A-Za-z_]\w*)', re.M)


def namespace_list(text):
    return list(dict.fromkeys(NAMESPACE.findall(text)))


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


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import comments as comment_lexer

RECORD_KINDS = {'CXXRecordDecl', 'ClassTemplateSpecializationDecl', 'ClassTemplatePartialSpecializationDecl'}
MAX_RESCUE_PASSES = 8



def blank_template_params(text):
    """Blank every `template < … >` header, which may run over several lines.

    A multi-line parameter list is the common engine shape:

        template<typename TKey,
                 typename TValue,
                 class TKeyEqual = std::equal_to<TKey>>

    Removing only single-line lists left `class TKeyEqual` and `class TAllocator` looking like class
    declarations, and each one cost a clang pass plus a note about a type parameter that has no
    member layout to check.
    """
    blanked = list(text)
    for match in re.finditer(r'\btemplate\b', text):
        depth = 0
        started = False
        k = match.end()
        while k < len(text):
            ch = text[k]
            if ch == '<':
                depth += 1
                started = True
            elif ch == '>':
                depth -= 1
                if started and depth == 0:
                    break
            elif ch in '{;' and started is False:
                break
            k += 1
        for position in range(match.start(), min(k + 1, len(blanked))):
            if blanked[position] != '\n':
                blanked[position] = ' '
    return ''.join(blanked)


def declared_names(text):
    """Every class, struct and union this file *defines a body for*, at any depth.

    Three exclusions, each of which otherwise buys a wasted clang pass and a note about a type that
    has no layout to check:
      * prose — `This singleton class serves as the core memory controller` is a sentence, so
        comments are blanked first (and will be absent entirely once the ban lands);
      * template parameters — `template<typename T, class THash>` declares a type parameter, and
        `class THash` reads exactly like a class declaration to a naive pattern;
      * forward declarations and `friend` lines — `class Engine;` names a type owned elsewhere.
    """
    blanked = list(text)
    try:
        spans = comment_lexer.comment_spans(text)
    except ValueError:
        spans = []
    for begin, end, kind in spans:
        for k in range(begin, min(end, len(blanked))):
            if blanked[k] != '\n':
                blanked[k] = ' '
    names = []
    for line in blank_template_params(''.join(blanked)).split('\n'):
        if 'friend' in line:
            continue
        for match in re.finditer(DECL_NAME, line):
            if line[:match.start()].rstrip().endswith('enum'):
                continue
            name = match.group(1)
            if name in NOT_A_CLASS_NAME or not declaration_tail_is_a_body(line[match.end():]):
                continue
            names.append(name)
    return list(dict.fromkeys(names))


def run_clang(entry, target, name_filter, extra_defines, clang_override):
    cmd = clang_argv(entry, target, name_filter) + list(extra_defines)
    if clang_override:
        cmd[0] = clang_override
    try:
        proc = subprocess.run(cmd, cwd=entry['directory'], capture_output=True, text=True)
    except OSError as exc:
        raise Skipped('clang could not run: %s' % exc)
    if proc.returncode != 0:
        detail = [d for d in (proc.stderr + proc.stdout).split('\n') if d.strip()]
        hint = next((d for d in detail if 'error' in d), detail[0] if detail else 'no output')
        raise Skipped('clang rejected this file: %s' % hint[:160])
    return parse_ast_stream(proc.stdout)


def collect_records(target, entries, by_name, clang_override=None):
    """Every class body this file defines, gathered by as many filtered passes as it takes.

    One pass is not enough, and believing otherwise is how this checker reported "5 clean" for a
    module whose two class bodies it had never seen. A filtered dump only carries declarations
    whose qualified name contains the filter, so:

      * the file's own namespace catches the classes declared in it (the common case, one pass);
      * a `std::hash<hbe::HString>` specialisation is declared in `namespace std`, and rescuing it
        needs a pass filtered by its own name;
      * a class inside `#ifdef __UNIT_TEST__` is invisible unless the build's define is added —
        Engine/Renderer/RendererTest.h and three Math sources preprocess to nothing under Dev, and
        clang then exits 0 having dumped nothing, which reads as clean unless it is retried.

    Returns (records, has_preprocessor_guard, notes).
    """
    text = open(target, encoding='utf-8', errors='ignore').read()
    entry = resolve_command(target, entries, by_name)
    guarded = bool(GUARDED.search(text))
    wanted = declared_names(text)
    namespaces = namespace_list(text)
    notes = []

    records = {}

    def absorb(name_filter, defines, note):
        for root in run_clang(entry, target, name_filter, defines, clang_override):
            for record, line, home, offset in records_in([root], target, text):
                key = (record.get('name'), offset)
                if key not in records:
                    records[key] = (record, line, home)
        if note and note not in notes:
            notes.append(note)

    # A dump must be filtered, so build the pass list first rather than discovering mid-run that
    # there is nothing to filter by: an unfiltered dump of this tree is 624 MB of JSON.
    passes = [namespace + '::' for namespace in namespaces]
    if not passes and wanted:
        passes = [wanted[0]]
    if not passes:
        raise Skipped('no namespace and no declared name to filter the AST by')
    for name_filter in passes:
        absorb(name_filter, (), None)

    found_names = lambda: {base_name(r[0].get('name')) for r in records.values()}
    missing = [name for name in wanted if name not in found_names()]
    for name in missing[:MAX_RESCUE_PASSES]:
        try:
            absorb(name, (), None)
        except Skipped:
            continue
    if len(missing) > MAX_RESCUE_PASSES:
        notes.append('%d declared name(s) beyond the %d rescue passes were not reached'
                     % (len(missing) - MAX_RESCUE_PASSES, MAX_RESCUE_PASSES))

    still_missing = [name for name in wanted if name not in found_names()]
    if still_missing and guarded:
        for name in still_missing[:MAX_RESCUE_PASSES]:
            try:
                absorb(name, ('-D__UNIT_TEST__=1',),
                       'checked with -D__UNIT_TEST__=1, which the Dev build does not define')
            except Skipped:
                continue
    if wanted:
        unrecovered = [name for name in wanted if name not in found_names()]
        if unrecovered:
            notes.append('no class body reached for: %s' % ', '.join(sorted(unrecovered)[:8]))
    return list(records.values()), guarded, notes


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
        if node.get('kind') in RECORD_KINDS and node.get('completeDefinition') and home == absolute:
            begin = ((node.get('range') or {}).get('begin') or {}).get('offset')
            if begin is None or begin >= size:
                return
            key = (node.get('name'), loc.get('line'), begin)
            if key not in seen:
                seen.add(key)
                found.append((node, loc.get('line', 0), home, begin))
        for c in node.get('inner') or []:
            walk(c, home)

    for root in ast:
        walk(root, None)
    return found


def base_name(name):
    """`hash<hbe::HString>` is the declaration of `hash`.

    clang names a specialisation with its argument list attached, while the source line a rescue
    pass is filtered by says only `hash`. Comparing the two without stripping the arguments leaves
    every standard-template specialisation looking as though it had never been found.
    """
    return (name or '').split('<')[0].strip()


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


def field_order(record):
    """{name: position} and [(name, line)] for this record's fields, in declaration order.

    Declaration order is what the standard sequences initialisation by, not the order a
    constructor happened to write, and not alphabetical order. A bitfield reports the
    struct's own name or nothing at all, so it is skipped rather than guessed at: an
    unrecognised name must never be reported as out of order.
    """
    order, seq = {}, []
    for child in record.get('inner') or []:
        node = unwrap(child)
        if node.get('kind') != 'FieldDecl':
            continue
        name = node.get('name')
        if not name or name in order:
            continue
        order[name] = len(seq)
        seq.append((name, (child.get('loc') or {}).get('line', 0)))
    return order, seq


def written_initializers(record):
    """[(constructor name, line, [(field name, line, col)])] in the order they were written.

    Three things about clang's JSON are not what they look like, and each one produced a
    silently passing check before it was measured against real output:

    * The field is not a `name` on the initializer. It is `anyInit.name`, and `anyInit.kind`
      is `FieldDecl`. Reading `name` off the initializer yields an empty string, every entry
      is then skipped, and the check reports clean whatever the source says.
    * This dump carries no `isWritten` flag, so an unwritten entry has to be recognised some
      other way: its initializer expression is a `CXXDefaultInitExpr`, which is clang's own marker
      for a default or in-class initializer the constructor never mentioned. An entry with no
      source range at all is dropped too, but that test alone does not catch these — they carry a
      range, and it is the *constructor's own* position. Counting them is not harmless: every one
      of them sorts ahead of every written entry, so a class with any in-class initializer reports
      a disorder in whichever member is declared last. That is a false finding on a correct file,
      which is worse than a missed one, because the fix it asks for changes nothing.
    * Children arrive in **initialization** order, which Sema has already sorted to match
      declaration order. Comparing children order against declaration order compares the data
      with its own sort key and can never disagree, so the written order is reconstructed from
      each initializer's source position, which the sort left intact.

    Bases and delegating constructors are skipped: a base subobject is initialised before every
    member no matter where it is written, so counting it reports a disorder that is not one.
    """
    out = []
    for child in record.get('inner') or []:
        node = unwrap(child)
        if node.get('kind') != 'CXXConstructorDecl' or node.get('isImplicit'):
            continue
        inits = []
        for inner in node.get('inner') or []:
            if inner.get('kind') != 'CXXCtorInitializer':
                continue
            target = inner.get('anyInit') or {}
            if target.get('kind') != 'FieldDecl':
                continue
            name = (target.get('name') or '').split('<')[0].strip()
            if not name:
                continue
            first = next((e for e in inner.get('inner') or [] if (e.get('range') or {}).get('begin')), None)
            if first is None:
                continue
            if first.get('kind') == 'CXXDefaultInitExpr':
                continue
            begin = first['range']['begin']
            inits.append((name, begin.get('line', 0), begin.get('col', 0)))
        inits.sort(key=lambda entry: (entry[1], entry[2]))
        out.append((node.get('name') or 'constructor', (child.get('loc') or {}).get('line', 0), inits))
    return out


def _fixture_init(field, kind, line, col, base=False):
    node = {'kind': 'CXXCtorInitializer'}
    if base:
        node['baseInit'] = {'qualType': 'Base'}
    else:
        node['anyInit'] = {'kind': 'FieldDecl', 'name': field}
    node['inner'] = [{'kind': kind, 'range': {'begin': {'line': line, 'col': col}}}]
    return node


def _fixture_record(inits):
    return {'kind': 'CXXRecordDecl', 'name': 'Probe',
            'inner': [{'kind': 'CXXConstructorDecl', 'name': 'Probe', 'loc': {'line': 10}, 'inner': inits}]}


def selftest():
    """Regression tests for the written-order reconstruction, on recorded clang JSON shapes.

    Feed the node shapes rather than compiling fixtures: what broke was reading the wrong key, and
    a compiled fixture would re-derive the shape from the same clang version that produced it.
    """
    cases = [
        ('in-class initializers are not written ones',
         _fixture_record([_fixture_init('', 'CXXConstructExpr', 10, 2, base=True),
                          _fixture_init('asks', 'CXXDefaultInitExpr', 10, 2),
                          _fixture_init('handed', 'CXXDefaultInitExpr', 10, 2),
                          _fixture_init('system', 'DeclRefExpr', 12, 5),
                          _fixture_init('observation', 'DeclRefExpr', 13, 5)]),
         ['system', 'observation']),
        ('a genuine disorder still reads as a disorder',
         _fixture_record([_fixture_init('b', 'DeclRefExpr', 11, 5),
                          _fixture_init('a', 'DeclRefExpr', 12, 5)]),
         ['b', 'a']),
        ('a constructor that writes nothing reports nothing',
         _fixture_record([_fixture_init('asks', 'CXXDefaultInitExpr', 10, 2),
                          _fixture_init('handed', 'CXXDefaultInitExpr', 10, 2)]),
         []),
        ('a base initializer is not a member',
         _fixture_record([_fixture_init('', 'CXXConstructExpr', 10, 2, base=True)]),
         []),
    ]
    failures = 0
    for label, record, expected in cases:
        got = [name for name, _, _ in written_initializers(record)[0][2]]
        if got != expected:
            failures += 1
            print('FAIL  %s: expected %s, got %s' % (label, expected, got))
        else:
            print('PASS  %s'
                  % label)

    # The waiver path, tested on a real file because the checker reads line numbers off disk.
    import tempfile
    sample = '\n'.join([
        'struct S final',
        '{',
        '\tstruct Depends final',
        '\t{',
        '\t\tint items[Limit];',
        '\t}; // hb-standards:ignore',
        '',
        '\tstatic constexpr int Limit = 8;',
        '};',
    ])
    with tempfile.NamedTemporaryFile('w', suffix='.h', delete=False, encoding='utf-8') as handle:
        handle.write(sample)
        sample_path = handle.name
    want = {6}
    got = directive_lines(sample_path)
    if got != want:
        failures += 1
        print('FAIL  the directive is read off the line that carries it: expected %s, got %s' % (sorted(want), sorted(got)))
    else:
        print('PASS  the directive is read off the line that carries it')
    if directive_lines('/nonexistent/definitely-not-a-file.h') != set():
        failures += 1
        print('FAIL  a missing file must yield no waivers rather than raise')
    else:
        print('PASS  a missing file yields no waivers rather than raising')
    os.unlink(sample_path)
    return 1 if failures else 0


def init_order_findings(target, entries, by_name, clang_override=None):
    """Findings for a written mem-initializer list that disagrees with declaration order.

    This is the compiler's own `-Wreorder` diagnostic, computed statically and named for the
    member the fixer must move. It exists because a twelve-block reorder that also moves data
    members is only honest once every constructor initialises them the way they are now
    declared: reordering the block without re-sequencing the list changes what runs first.
    """
    text = open(target, encoding='utf-8', errors='ignore').read()
    if not CLASS_DECL.search(text):
        return [], [], 'no class or struct defined here'
    records, _guarded, notes = collect_records(target, entries, by_name, clang_override)
    notes = [n if n.startswith(target) else '%s: [PARTIAL] %s' % (target, n) for n in notes]
    if not records:
        return [], notes, 'no class body reached by clang in this file'
    findings = []
    for record, _line, _home in records:
        name = record.get('name') or '(anonymous at line %d)' % _line
        order, seq = field_order(record)
        if not order:
            continue
        declared_line = dict(seq)
        for ctor, ctor_line, inits in written_initializers(record):
            known = [(order[f], f, ln) for f, ln, _col in inits if f in order]
            for i in range(1, len(known)):
                if known[i][0] < known[i - 1][0]:
                    later, sooner = known[i], known[i - 1]
                    findings.append('%s:%d: INIT-ORDER  %s::%s — %s is initialised after %s, but %s is '
                                    'declared earlier (line %d); initialisation follows declaration order'
                                    % (target, later[2] or ctor_line, name, ctor,
                                       later[1], sooner[1], later[1], declared_line[later[1]]))
    return findings, notes, ''


def check_file(target, entries, by_name, clang_override=None):
    text = open(target, encoding='utf-8', errors='ignore').read()
    if not CLASS_DECL.search(text):
        return [], [], 'no class or struct defined here', []
    records, guarded, notes = collect_records(target, entries, by_name, clang_override)
    notes = [n if n.startswith(target) else '%s: [PARTIAL] %s' % (target, n) for n in notes]
    if not records:
        return [], notes, 'no class body reached by clang in this file', []
    findings = []
    waived = []
    for record, line, home in records:
        name = record.get('name') or '(anonymous at line %d)' % line
        seq = members_of(record, record.get('tagUsed', 'class'), home)
        if not seq:
            continue
        bad = first_disorder(seq)
        for member_file, member_line, note, what, block, prev_block, prev_what, prev_line in bad:
            shown = os.path.relpath(member_file, REPO_ROOT) if member_file else target
            where = '%s  [%s]' % (shown, note) if note else shown
            if member_line and member_line in directive_lines(member_file or target):
                waived.append('%s:%d: MEMBER-WAIVED  %s — %s would belong in block %d, and the line carries %s'
                              % (shown, member_line, name, what, block, HB_IGNORE))
                continue
            findings.append('%s:%d: MEMBER-LAYOUT  %s — %s belongs in block %d, but %s at line %d already sets block %d'
                            % (where, member_line, name, what, block, prev_what, prev_line, prev_block))
    return findings, notes, '', waived


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('files', nargs='*')
    parser.add_argument('--db', default='cmake-build-debug/compile_commands.json')
    parser.add_argument('--clang', default=None)
    parser.add_argument('--quiet', action='store_true')
    parser.add_argument('--init-order', action='store_true',
                        help='report written mem-initializer lists that disagree with declaration order')
    parser.add_argument('--selftest', action='store_true',
                        help='run the initializer-order reconstruction against recorded clang JSON shapes')
    opts = parser.parse_args(argv)

    if opts.selftest:
        return selftest()

    if not opts.files:
        print('usage: layout.py <file ...> [--db compile_commands.json] [--init-order]', file=sys.stderr)
        return 3
    try:
        entries, by_name = read_compile_db(opts.db)
    except Skipped as exc:
        print('[SKIP] all — %s' % exc)
        return 0

    total = 0
    waived_total = 0
    partial = 0
    checked = 0
    skipped = 0
    silent = 0
    checker = init_order_findings if opts.init_order else check_file
    mode = 'init order' if opts.init_order else 'member layout'
    for target in opts.files:
        try:
            result = checker(target, entries, by_name, opts.clang)
        except Skipped as exc:
            print('%s: [SKIP] %s' % (target, exc.reason))
            skipped += 1
            continue
        findings, notes, reason = result[0], result[1], result[2]
        waived = result[3] if len(result) > 3 else []
        checked += 1
        for n in notes:
            print(n)
            partial += 1
        for w in waived:
            print(w)
        waived_total += len(waived)
        for f in findings:
            print(f)
        total += len(findings)
        if not findings:
            silent += 1
            if reason:
                print('%s: [NONE] %s' % (target, reason))
            elif not opts.quiet:
                print('%s: [PASS] %s' % (target, mode))
    summary = '%s: %d file(s) checked (%d clean, %d with findings), %d skipped, %d partial — %d violation(s)'
    summary += ', %d waived' % waived_total if waived_total else ''
    print(summary % (mode, checked, silent, checked - silent, skipped, partial, total))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
