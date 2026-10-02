#!/usr/bin/env python3
"""Check rule set B of docs/CodingStandards.md — the include preamble, and nothing below it.

The preamble is the region from the first line of the file to the line before the first body line, which
is the same boundary `blank_lines.py` uses for rule A3. Three blocks, sorted, holding nothing the file
does not name:

  B1  the file's own header first, and excluded from the sort
  B2  own header, then `<standard>`, then `"project"` — each block sorted lexicographically on the written
      path, which is also what groups the project block by directory (`"Core/A.h"`, `"Core/B.h"`, `"Log/C.h"`)
  B3  no path twice
  B4  `<…>` for the standard library, `"…"` for everything else
  B5  project includes written root-relative — no `../`
  B6  an include the file does not itself name is removed — reported as a candidate list, never applied
  B7  the preamble is the only region a pass rewrites, and `--prove-immutable <rev>` proves every include
      below it kept its place and the guard it belongs to

B7 is the reason this checker exists next to clang-format rather than inside it. 83 files carry includes
below the preamble — `#include "VectorCommonImpl.inl"` inside a class body, where position decides what is
in scope, and the `#ifdef __UNIT_TEST__` regions that close a file — and a pass that "tidied" them would
break the build while looking like a cleanup. The proof compares the list of below-preamble includes, each
with the guard text it sits under, against the revision the edit started from: that list survives a
blank-line pass and does not survive a hoist.

B6 is deliberately the weakest output here. Compiling one translation unit with one include deleted tells
you that file still builds; it says nothing about the consumer three modules away that names an entity it
never included itself and gets it transitively. That is why removal is reported as a candidate for a
reader and proved by the three-configuration build, and why header files are refused out right: a
header's include list is its consumers' contract, and no single-translation-unit compile can adjudicate
it.

Exit status: 0 clean, 1 findings, 3 could not run (no files, no compile database for --unused).
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))
sys.path.insert(0, SCRIPT_DIR)

import comments     # noqa: E402  (the same lexer the comment ban uses)
import layout       # noqa: E402  (clang_argv and the compile database are already correct here)
import blank_lines  # noqa: E402  (the preamble boundary and the line model are shared, not reimplemented)

COMPILE_DB = os.path.join(REPO_ROOT, 'cmake-build-debug', 'compile_commands.json')
RELATIVE = re.compile(r'^\.\.?/')
INCLUDE_LINE = re.compile(r'^#\s*include\s*([<"])([^">"]+)[>"]')

# The C++23 standard headers plus the C compatibility headers this tree actually uses. Membership decides
# B4 in one direction only: a quoted name in this set should be angle-bracketed. The reverse test needs the
# whole of every include path in the project, which a set cannot supply.
STD_HEADERS = set('''
	algorithm any array atomic bar beckoke bit bitset cassert ccomplex cctype cerrno cfenv cfloat
	chrono cinttypes complex ciso646 climits clocale cmath codecvt compare concurrent concepts
	csetjmp csignal cstdalign cstdarg cstdatomic cstddef cstdint cstdio cstdlib cstring ctgmath
	ctime cwchar cwctype deque execution filesystem forward_list fstream functional future initializer_list
	iomanip iosfwd iostream istream iterator latch map memory mutex new numbers numeric optional ostream
	queue random ranges ratio regex scoped_allocator semaphore set shared_mutex source_location span
	stack spanstream stacktrace streambuf string string_view syncstream system_error thread tuple type_traits
	typeindex unordered_map unordered_set valarray variant vector version
'''.split())


class Finding(object):
    def __init__(self, rule, line, message):
        self.rule = rule
        self.line = line
        self.message = message


COND_ANY = re.compile(r'^#\s*(if|ifdef|ifndef|else|elif|endif)\b')
COND_OPEN_DIRECTIVE = re.compile(r'^#\s*(?:if|ifdef|ifndef)\b')
COND_BRANCH_DIRECTIVE = re.compile(r'^#\s*(?:else|elif)\b')
PROJECT_ROOTS = ('Engine', 'Examples', 'Applications', 'External')


def resolves_in_project(written):
    """Whether an angle-bracketed path names a file this repository owns, with the case the file really has.

    The one-directional test B4 needs: `"atomic"` is provably wrong by membership in the standard header
    list, while `<sys/mman.h>` is provably fine only by not existing under Engine/, Examples/ or
    Applications/. Two wrong attempts are recorded here. A set of POSIX prefixes flagged `sys/mman.h` as a
    project header, which is how a checker gets ignored. A plain `os.path.exists` flagged `<string>` and
    `<memory>` because macOS resolves `Engine/string` to `Engine/String` — the case has to be matched
    against `os.listdir`, not against the filesystem's opinion.
    """
    for root in PROJECT_ROOTS:
        if _exact_file(os.path.join(REPO_ROOT, root), written):
            return True
    return False


def _exact_file(base, written):
    current = base
    for part in written.split('/'):
        try:
            entries = os.listdir(current)
        except OSError:
            return False
        if part not in entries:
            return False
        current = os.path.join(current, part)
    return os.path.isfile(current)


def parse_preamble(text, path):
    """The include lines of the preamble, in order, each carrying the condition that guards it.

    An include inside a conditional region is not a card in the sorted deck. `Engine/OSAL/OSMemory.cpp`
    picks its headers by platform, so `<unistd.h>` appears once per branch, and a checker that read those
    as one block reported the file as six interleaved blocks and a duplicate for every branch. The guard's
    own text is the identity of the branch, so a duplicate inside one branch is still a duplicate.
    """
    stem = blank_lines.own_stem(text, path)
    facts = blank_lines.line_facts(text, stem)
    boundary = blank_lines.first_body_index(facts)
    if boundary < 0:
        boundary = len(facts)

    items, stack = [], []
    for index, fact in enumerate(facts):
        if fact['directive'] and COND_ANY.match(fact['stripped']):
            if COND_OPEN_DIRECTIVE.match(fact['stripped']):
                stack.append(fact['stripped'])
            elif COND_BRANCH_DIRECTIVE.match(fact['stripped']) and stack:
                stack[-1] = fact['stripped']
            elif fact['stripped'].startswith('#endif') and stack:
                stack.pop()
        match = INCLUDE_LINE.match(fact['stripped'])
        if not match:
            continue
        bracket, written = match.group(1), match.group(2)
        kind = 'other' if index >= boundary else (
            'std' if bracket == '<' else ('own' if fact['kind'] == 'own' else 'proj'))
        items.append({'line': fact['index'] + 1, 'bracket': bracket, 'path': written, 'kind': kind,
                      'guard': ' / '.join(stack) if stack else None,
                      'below': index >= boundary})
    return items, boundary


def check_preamble(text, path):
    includes, _boundary = parse_preamble(text, path)
    sorted_block = [item for item in includes if not item['below'] and item['guard'] is None]
    findings = []

    kinds = [item['kind'] for item in sorted_block]
    if 'own' in kinds:
        if kinds[0] != 'own':
            findings.append(Finding('B1', sorted_block[kinds.index('own')]['line'],
                                    'the own header is included after another header, so it is not first'))
        if kinds.count('own') > 1:
            findings.append(Finding('B1', sorted_block[kinds.index('own', 1)]['line'],
                                    'two lines claim to be the own header'))

    order = {'own': 0, 'std': 1, 'proj': 2}
    highest, highest_line = -1, 0
    for item in sorted_block:
        rank = order[item['kind']]
        if rank < highest:
            findings.append(Finding('B2', item['line'],
                                    '"%s" sits after a %s block opened at line %d, so the blocks are interleaved'
                                    % (item['path'], ('own header', 'standard', 'project')[highest], highest_line)))
        elif rank > highest:
            highest, highest_line = rank, item['line']

    for kind in ('own', 'std', 'proj'):
        block = [item for item in sorted_block if item['kind'] == kind]
        paths = [item['path'] for item in block]
        for position in range(1, len(paths)):
            if paths[position] < paths[position - 1]:
                findings.append(Finding('B2', block[position]['line'],
                                        '"%s" precedes "%s" in its block, so the block is not lexicographic'
                                        % (paths[position], paths[position - 1])))
                break

    seen = {}
    for item in includes:
        key = (item['bracket'], item['path'], item['guard'])
        if key in seen:
            findings.append(Finding('B3', item['line'], 'duplicate of the include on line %d' % seen[key]))
        seen.setdefault(key, item['line'])

    for item in includes:
        base = item['path'].rsplit('/', 1)[-1]
        if item['bracket'] == '"' and '/' not in item['path'] and base in STD_HEADERS:
            findings.append(Finding('B4', item['line'], '"%s" is a standard header and should read <%s>'
                                    % (item['path'], item['path'])))
        if item['bracket'] == '<' and resolves_in_project(item['path']):
            findings.append(Finding('B4', item['line'], '<%s> is a project header and should read "%s"'
                                    % (item['path'], item['path'])))
        if item['bracket'] == '"' and RELATIVE.match(item['path']):
            findings.append(Finding('B5', item['line'], '"%s" is written relative to this file, not to the root'
                                    % item['path']))
    return findings


def below_preamble_includes(text, path):
    """Every include below the preamble, as (guard, bracket, written path) in file order — what rule B7 protects.

    An earlier version compared everything from the preamble boundary to end of file. That region is the
    body of the file, so a blank-line pass — whose whole job is moving blanks inside the body — failed the
    proof on every file it touched, and seven clean files came out dirty. The measure of an immune include is
    what it belongs to: `#include "Vector3CommonImpl.inl"` cannot leave the class body it sits in, and
    `#include "../Engine/Engine.h"` cannot leave the `#ifdef __UNIT_TEST__` region that exists to hold it, so
    the guard text is part of the identity alongside the written path, and adding, dropping, re-ordering or
    re-guarding any of them is a finding.

    Whether some *line* moved is a stronger claim and belongs to another tool: `prove_regroup.py
    --whitespace-only` proves no non-blank line moved at all.
    """
    return [(item['guard'], item['bracket'], item['path'])
            for item in parse_preamble(text, path)[0] if item['below']]


def revision_text(path, rev):
    result = subprocess.run(['git', 'show', '%s:%s' % (rev, path)], capture_output=True, text=True)
    return result.stdout if result.returncode == 0 else None


# ------------------------------------------------------------------ B6 candidates --
def load_compile_database():
    expanded = os.path.normpath(COMPILE_DB)
    if not os.path.exists(expanded):
        return None, expanded
    with open(expanded, encoding='utf-8') as handle:
        return json.load(handle), expanded


def database_entry(entries, path):
    wanted = os.path.abspath(path)
    for entry in entries:
        if os.path.isabs(entry.get('file', '')):
            candidate = entry['file']
        else:
            candidate = os.path.join(entry.get('directory', ''), entry.get('file', ''))
        if os.path.abspath(candidate) == wanted:
            return entry
    return None


TEST_MACRO_FLAGS = ('-D__TEST__=1', '-D__UNIT_TEST__=1')
TEST_GUARD = re.compile(r'^\s*#\s*ifn?def\s+__UNIT_TEST__\b', re.MULTILINE)


def text_macros(text):
    """The preprocessor definitions a measurement must add, judged from the bytes it is about to compile.

    `cmake-build-debug/compile_commands.json` is generated without `-D__TEST__ -D__UNIT_TEST__` — `build.sh`
    adds them to `CMAKE_CXX_FLAGS` only under `-test` — so any measurement taken from that database looks at
    a file with its unit-test surface preprocessed away. For a body that lives entirely behind the guard,
    like `main()` in `Applications/EngineTest/TestMain.cpp`, the measurement is not of the file at all: it
    compiles an empty translation unit, and an empty translation unit compiles with any include deleted.
    `layout.py` already meets the same trap and answers it by re-running with the macro defined; this is the
    same answer, and the caller prints which macros were active so a reader can tell a clean answer from an
    unmeasured one.
    """
    return TEST_MACRO_FLAGS if TEST_GUARD.search(text) else ()


def file_macros(path):
    """`text_macros` read off a file, and no answer at all when the file cannot be read."""
    try:
        with open(path, encoding='utf-8', errors='ignore') as handle:
            return text_macros(handle.read())
    except OSError:
        return ()


def syntax_check(entry, source_path, directory, macros=()):
    """Compile one translation unit for errors only, from a copy sitting where the original sits.

    The copy has to live beside the original, because the include paths in the database entry are what
    make the project's own headers reachable, and a copy in /tmp resolves them differently — the same
    trap clang-format probes fall into.
    """
    command = entry.get('args') or shlex.split(entry.get('command', ''))
    kept, skip = [], False
    for token in command:
        if skip:
            skip = False
            continue
        if token == '-o':
            skip = True
            continue
        if token in ('-c', '-Werror'):
            continue                             # -c is meaningless under -fsyntax-only, and a warning is not an answer
        if token.endswith(('.cpp', '.cc', '.cxx')):
            continue                             # the input is supplied below, from the scratch path
        kept.append(token)
    kept += list(macros) + ['-fsyntax-only', source_path]
    result = subprocess.run(kept, cwd=directory, capture_output=True, text=True)
    return result.returncode, result.stderr


def header_name_filter(header_path):
    """The `-ast-dump-filter` value for one header: the namespace it declares into.

    An empty filter is not an option. `-ast-dump-filter=hbe` against `Engine/Memory/MemoryManager.h` dumps
    1 MB in 0.3 s; unfiltered the same dump is 624 MB, which is the trap `layout.py` records and the
    reason this reuses its machinery instead of invoking clang again by hand.
    """
    with open(header_path, encoding='utf-8') as handle:
        body = handle.read()
    for name in re.findall(r'\bnamespace\s+([A-Za-z_]\w*)', body):
        if name != 'hbe':
            return name
    return 'hbe'


def ast_names_of(header_path, entry, macros=()):
    """The names a header declares itself, read from clang rather than guessed.

    This is the half of B6 that ablation cannot answer. Deleting `#include <atomic>` from a file that
    spells `std::atomic` still compiles when that file's own header drags `<atomic>` in transitively, and
    "it still builds" is not the rule — the rule is that a file keeps the include for every name it
    spells. A candidate therefore has to clear two tests, and this supplies the second one.

    Returns None when the question could not be answered. The caller reports that separately rather than
    folding it into the candidate list, because "could not tell" and "not a candidate" are different
    claims — the same distinction `layout.py` draws with `[NONE]`.

    The cache is keyed on the macros as well as the path: one header read two ways declares two different
    name sets, and a cached answer from the wrong one is a wrong answer about a file on disk.
    """
    cache = ast_names_of.__dict__.setdefault('cache', {})
    key = (header_path, tuple(macros))
    if key in cache:
        return cache[key]
    wanted = os.path.abspath(header_path)
    scratch = os.path.join(os.path.dirname(wanted), '._includes_probe_names_%d.cpp' % os.getpid())
    with open(scratch, 'w', encoding='utf-8') as handle:
        handle.write('#include "%s"\n' % os.path.basename(wanted))
    names = set()
    try:
        command = layout.clang_argv(entry, scratch, header_name_filter(wanted))
        if macros:
            at = command.index('-fsyntax-only')
            command = command[:at] + list(macros) + command[at:]
        result = subprocess.run(command, cwd=entry.get('directory', '.'), capture_output=True, text=True)
        decoder, dump, index = json.JSONDecoder(), result.stdout, 0
        while index < len(dump):
            while index < len(dump) and dump[index].isspace():
                index += 1
            if index >= len(dump):
                break
            node, index = decoder.raw_decode(dump, index)
            names.update(walk_names(node, wanted))
    except (OSError, ValueError):
        names = None                                       # could not read the header's names: say so, do not guess
    finally:
        if os.path.exists(scratch):
            os.unlink(scratch)
    cache[key] = names
    return names


def walk_names(node, wanted, inherited=''):
    """Yield the declared name of every declaration of interest that lives in `wanted` itself.

    The location filter is what keeps a header's *own* names rather than everything its transitive
    includes drag in — clang puts the spelling location on the declaration, and a declaration spelled in
    an included file names a different file.

    But clang writes `file` on a location only where the file *changes*. A declaration nested inside a
    namespace that this file also declares carries `{'offset', 'line', 'col', 'tokLen'}` and nothing else:
    the file is inherited, not repeated. Reading `loc.file` alone therefore yields nothing for a nested
    declaration, which in this engine means for **every** declaration, because every one of them sits in
    `namespace hbe`. Measured on `Engine/Test/UnitTestCollection.h` compiled as its own translation unit
    with the test macros on: the `NamespaceDecl` reported a file, its two `FunctionDecl`s did not, so the
    name set came back as `{'Test'}` and `RegisterSuite` was absent. The effect is not a missing nicety —
    `supplied` is the half of B6 that exists to stop an unsound deletion, and an empty `supplied` makes
    every include that ablation happens to survive look unnamed, which is exactly the recommendation the
    rule was written to refuse. So the file is inherited down the tree, as `layout.py` already does for
    the same quirk on a template's definition node.
    """
    kinds = {'CXXRecordDecl', 'ClassTemplateDecl', 'FunctionDecl', 'FunctionTemplateDecl', 'TypeAliasDecl',
             'TypedefDecl', 'EnumDecl', 'VarDecl', 'NamespaceDecl', 'ConceptDecl', 'UsingDecl'}
    stack = [(node, inherited)]
    while stack:
        current, from_above = stack.pop()
        if not isinstance(current, dict):
            continue
        path = (current.get('loc') or {}).get('file') or from_above
        name = current.get('name') or ''
        if current.get('kind') in kinds and name and os.path.abspath(path or '') == wanted:
            yield name.split('<')[0].split('::')[-1]
        for value in current.values():
            if isinstance(value, list):
                stack.extend((item, path) for item in value if isinstance(item, dict))
            elif isinstance(value, dict):
                stack.append((value, path))


def provided_names(item, entry, includer):
    """The names an include supplies, or None when that question has no cheap answer.

    A standard header is the unanswered case: with no per-header name table there is nothing sound to
    compare the file's spellings against, so it is reported as unverifiable instead of as a candidate.

    The including file's own directory is the first root, because a quoted include resolves against the
    file that writes it — `#include "Array.h"` in `Array.cpp` is the same directory, and leaving that out
    made every own header look unverifiable.

    A header whose declarations sit behind `#ifdef __UNIT_TEST__` declares **nothing** to a compiler run
    without that macro, and an empty name set intersects everything, so the include it owns is reported as
    one the file neither needs nor names. Measured on `Applications/EngineTest/TestMain.cpp`, whose only two
    calls are `hbe::Test::RegisterSuite` and `ScheduleSuiteOnBaseStream`, both declared solely inside that
    guard in `Engine/Test/UnitTestCollection.h`: the tool offered that header for deletion. It is the same
    blindness as the ablation half, and this half is the dangerous one, because the name test is the one
    this rule trusts to stop an unsound deletion.
    """
    if item['bracket'] != '"':
        return None
    roots = [os.path.dirname(os.path.abspath(includer))] + [os.path.join(REPO_ROOT, root)
                                                           for root in PROJECT_ROOTS + ('.',)]
    for root in roots:
        candidate = os.path.normpath(os.path.join(root, item['path']))
        if os.path.isfile(candidate):
            return ast_names_of(candidate, entry, macros=text_macros(candidate))
    return None


def unused_candidates(path):
    """Includes this file neither needs nor names, as (candidates, error, unverifiable, needed, macros).

    Only defined for a source file. A header's includes are consumed by whoever includes the header, so
    deleting one and recompiling the header's own translation unit proves nothing about the tree — the
    three-configuration build is the only instrument fine enough for that question, and the skill already
    runs it.

    Two measurements per include, and a candidate must clear both: compile the file with the line gone,
    and check that no name the include declares is spelled anywhere in the file's own code. The first
    alone would recommend deleting the include a file genuinely depends on and only gets transitively,
    which is the opposite of the rule.

    Both are run under the test macros when the file carries a `#ifdef __UNIT_TEST__` region, because the
    compile database does not define them and the measurement would otherwise be of an empty translation
    unit. `macros` says which were used, so the report can not read as a clean answer about code nobody
    compiled.
    """
    if not path.endswith(('.cpp', '.cc', '.cxx')):
        return None, ("B6 is a translation-unit question and %s is a header: a header's include list is what "
                      'its consumers compile against, so the three-configuration build is the proof' % path), [], [], ()
    entries, database = load_compile_database()
    if entries is None:
        return None, ('no compile database at %s — regenerate with cmake -S . -B cmake-build-debug '
                      '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' % database), [], [], ()
    entry = database_entry(entries, path)
    if entry is None:
        return None, 'no compile database entry for %s' % path, [], [], ()
    directory = entry.get('directory', os.getcwd())        # the database is authored against its own working directory

    try:
        with open(path, encoding='utf-8') as handle:
            text = handle.read()
    except OSError as error:
        return None, str(error), [], [], ()
    macros = text_macros(text)
    items, _boundary = parse_preamble(text, path)
    inside = [item for item in items if not item['below']]
    lines = text.split('\n')
    spelled = set(re.findall(r'[A-Za-z_][A-Za-z_0-9]*', comments.code_only(text)))
    scratch = os.path.join(os.path.dirname(os.path.abspath(path)),
                           '._includes_probe_%d%s' % (os.getpid(), os.path.splitext(path)[1]))
    candidates, unverifiable, needed = [], [], []
    try:
        base_code, _stderr = syntax_check(entry, os.path.abspath(path), directory, macros)
        if base_code != 0:
            return None, ('%s does not compile as it stands%s, so ablation would report every include as removable'
                          % (path, ' with ' + ' and '.join(macros) if macros else '')), [], [], macros
        for item in inside:
            variant = list(lines)
            del variant[item['line'] - 1]
            with open(scratch, 'w', encoding='utf-8') as handle:
                handle.write('\n'.join(variant))
            code, stderr = syntax_check(entry, scratch, directory, macros)
            if code != 0:
                if 'fatal error' in stderr and 'file not found' in stderr:
                    needed.append('%s%s> is needed: %s' % (item['bracket'], item['path'], stderr.splitlines()[0]))
                continue
            supplied = provided_names(item, entry, path)
            if supplied is None:
                unverifiable.append(item)
            elif not (supplied & spelled):
                candidates.append(item)
    finally:
        if os.path.exists(scratch):
            os.unlink(scratch)
    return candidates, None, unverifiable, needed, macros


# ----------------------------------------------------------------------- fixtures --
CASES = []


def case(name, source, expect, filename='Engine/Core/Foo.cpp'):
    CASES.append((name, source, expect, filename))


HEAD = '// Copyright (c) 2026 Hansol Park. All rights reserved.\n\n'


case('three clean blocks',
     HEAD + '#include "Foo.h"\n\n#include <atomic>\n#include <memory>\n\n#include "Core/Debug.h"\n'
     '#include "Log/Logger.h"\n\n\nnamespace hbe\n{\n}\n', [])

case('B1: the own header is not first',
     HEAD + '#include <atomic>\n\n#include "Foo.h"\n\n#include "Core/Debug.h"\n\n\nnamespace hbe\n{\n}\n',
     [('B1', 5), ('B2', 5)])

case('B2: a project block opened before the standard block',
     HEAD + '#include "Foo.h"\n\n#include "Core/Debug.h"\n\n#include <atomic>\n\n\nnamespace hbe\n{\n}\n',
     [('B2', 7)])

case('B2: a block that is not lexicographic',
     HEAD + '#include "Foo.h"\n\n#include <memory>\n#include <atomic>\n\n\nnamespace hbe\n{\n}\n',
     [('B2', 6)])

case('B3: the same include twice',
     HEAD + '#include "Foo.h"\n\n#include <atomic>\n\n#include "Core/Debug.h"\n#include "Core/Debug.h"\n'
     '\n\nnamespace hbe\n{\n}\n', [('B3', 8)])

case('B4: a standard header in quotes',
     HEAD + '#include "Foo.h"\n\n#include "atomic"\n\n#include "Core/Debug.h"\n\n\nnamespace hbe\n{\n}\n',
     [('B4', 5), ('B2', 7)])

case('B4: a project header in angle brackets',
     HEAD + '#include "Foo.h"\n\n#include <atomic>\n\n#include <Core/Debug.h>\n\n\nnamespace hbe\n{\n}\n',
     [('B4', 7), ('B2', 7)])

case('B5: a relative project include',
     HEAD + '#include "Foo.h"\n\n#include "../Engine/Engine.h"\n\n\nnamespace hbe\n{\n}\n', [('B5', 5)])

case('a platform block inside the preamble is its own group',
     HEAD + '#include "Foo.h"\n\n#include "Core/Debug.h"\n\n#if defined(HB_PLATFORM_LINUX)\n'
     '#include <unistd.h>\n#include <cerrno>\n#elif defined(HB_PLATFORM_OSX)\n'
     '#include <unistd.h>\n#include <cerrno>\n#endif\n\n\nnamespace hbe\n{\n}\n', [])

case('an in-class .inl include is not part of the preamble sort',
     HEAD + '#include <atomic>\n\n\nnamespace hbe\n{\nclass Vector3\n{\n\t#include "VectorCommonImpl.inl"\n'
     '};\n} // namespace hbe\n', [], 'Engine/Math/Vector3.h')


def run_selftest():
    """Run each fixture from a scratch file that carries the basename the fixture assumes.

    The path matters, not only the text: rule B1 identifies a file's own header by comparing the basename
    of the file being checked with the path it includes, so a scratch named `._includes_probe_Foo.cpp`
    would make `#include "Foo.h"` a project header and report the clean fixture as two violations. The
    scratch tree therefore mirrors the directory and the name.
    """
    root = os.path.join(SCRIPT_DIR, '._includes_selftest')
    failures = 0
    for name, source, expect, filename in CASES:
        scratch = os.path.join(root, filename)
        try:
            os.makedirs(os.path.dirname(scratch), exist_ok=True)
            with open(scratch, 'w', encoding='utf-8') as handle:
                handle.write(source)
            got = sorted((f.rule, f.line) for f in check_preamble(source, scratch))
        finally:
            if os.path.exists(scratch):
                os.unlink(scratch)
        if got != sorted(expect):
            print('FAIL %-58s want %s got %s' % (name, sorted(expect), got))
            failures += 1
        else:
            print('ok   %-58s %s' % (name, ','.join(sorted({rule for rule, _ in expect})) or 'clean'))
    # The name-extraction half, on recorded clang JSON shapes. Feed the node rather than compiling a
    # fixture: what broke was reading `loc.file` off a nested declaration, and clang writes that key only
    # where the file changes, so a compiled fixture of one's own making is likely to re-derive the shape
    # that already fooled the reader.
    header = os.path.join(REPO_ROOT, 'Engine', 'Core', 'Probe.h')
    other = os.path.join(REPO_ROOT, 'Engine', 'Core', 'Other.h')
    # `hbe` is in every expectation because the enclosing NamespaceDecl is itself located in the header and
    # is therefore a name the header supplies — which is exactly what the buggy reader reported as a whole
    # name set, mistaking the one node that carried a file for the only node that existed.
    shapes = [
        ('a declaration nested in a namespace inherits the file clang wrote on the namespace',
         {'kind': 'NamespaceDecl', 'name': 'hbe', 'loc': {'file': header, 'line': 5, 'col': 1},
          'inner': [{'kind': 'FunctionDecl', 'name': 'RegisterSuite', 'loc': {'line': 18, 'col': 6}}]},
         {'hbe', 'RegisterSuite'}),
        ('a declaration clang located in a different file still names that file',
         {'kind': 'NamespaceDecl', 'name': 'hbe', 'loc': {'file': header, 'line': 5, 'col': 1},
          'inner': [{'kind': 'FunctionDecl', 'name': 'Borrowed', 'loc': {'file': other, 'line': 3, 'col': 6}},
                    {'kind': 'FunctionDecl', 'name': 'OwnsIt', 'loc': {'line': 19, 'col': 6}}]},
         {'hbe', 'OwnsIt'}),
        ('a namespace with nothing in it supplies its own name',
         {'kind': 'NamespaceDecl', 'name': 'Test', 'loc': {'file': header, 'line': 11, 'col': 11}},
         {'Test'}),
    ]
    for name, node, expect in shapes:
        got = set(walk_names(node, header))
        if got != expect:
            print('FAIL %-58s want %s got %s' % (name, sorted(expect), sorted(got)))
            failures += 1
        else:
            print('ok   %-58s %s' % (name, ','.join(sorted(got)) or 'clean'))
    total = len(CASES) + len(shapes)
    import shutil
    shutil.rmtree(root, ignore_errors=True)
    print('%d fixture(s), %d failure(s)' % (total, failures))
    return 1 if failures else 0


def main(argv):
    parser = argparse.ArgumentParser(description='Check the include rules of docs/CodingStandards.md')
    parser.add_argument('files', nargs='*', help='C++ sources to check')
    parser.add_argument('--selftest', action='store_true', help='run the built-in fixtures')
    parser.add_argument('--unused', action='store_true',
                        help='B6 candidates: compile this file once per include with that include deleted, '
                             'under -D__TEST__ -D__UNIT_TEST__ when it carries a unit-test region')
    parser.add_argument('--prove-immutable', metavar='REV',
                        help='B7: refuse unless every include below the preamble kept its place and its guard')
    parser.add_argument('--summary-only', action='store_true', help='print counts, not findings')
    args = parser.parse_args(argv)

    if args.selftest:
        return run_selftest()
    if not args.files:
        print('[NONE] includes: no files given, so nothing was measured')
        return 3

    rules = ('B1', 'B2', 'B3', 'B4', 'B5', 'B7')
    counts = dict((rule, 0) for rule in rules)
    total, dirty = 0, 0
    b7 = {'files': 0, 'holders': 0, 'includes': 0, 'changed': 0}
    for path in args.files:
        try:
            with open(path, encoding='utf-8') as handle:
                text = handle.read()
        except OSError as error:
            print('[NONE] includes — %s: %s' % (path, error))
            return 3
        findings = check_preamble(text, path)

        if args.prove_immutable:
            before = revision_text(path, args.prove_immutable)
            if before is None:
                print('[NONE] includes — %s: not in %s, so there is nothing to compare' % (path, args.prove_immutable))
                return 3
            was, now = below_preamble_includes(before, path), below_preamble_includes(text, path)
            b7['files'] += 1
            b7['includes'] += len(now)
            if now:
                b7['holders'] += 1
            if was != now:
                b7['changed'] += 1
                findings.append(Finding('B7', 0, 'the includes below the preamble changed — added, dropped, '
                                                're-ordered, or moved into a different guard, which rule B7 '
                                                'forbids: %s -> %s' % (was or 'none', now or 'none')))

        for finding in findings:
            counts[finding.rule] = counts.get(finding.rule, 0) + 1
        total += len(findings)
        if findings:
            dirty += 1
            if not args.summary_only:
                print('[FAIL] includes — %s' % path)
                for finding in findings:
                    print('    %s:%s: [%s] %s' % (path, finding.line or '-', finding.rule, finding.message))
        if args.unused:
            candidates, error, unverifiable, needed, macros = unused_candidates(path)
            if candidates is None:
                print('[NONE] B6 — %s: %s' % (path, error))
                continue
            print('[INFO] B6 — %s: %d candidate(s) this file neither needs nor names%s'
                  % (path, len(candidates),
                     ' (measured with %s: the file\'s unit-test surface is otherwise preprocessed away)'
                     % ' and '.join(macros) if macros else ''))
            for item in candidates:
                closing = '>' if item['bracket'] == '<' else '"'
                print("    %s:%d: %s%s%s — deletion is a reader's call, proved by the build gate"
                      % (path, item['line'], item['bracket'], item['path'], closing))
            if unverifiable:
                print('    %d include(s) this file compiles without whose supplied names could not be listed, '
                      'so they are not candidates: %s'
                      % (len(unverifiable), ', '.join('%s%s%s' % (i['bracket'], i['path'],
                                                                  '>' if i['bracket'] == '<' else '"')
                                                      for i in unverifiable)))
            for note in needed:
                print('    %s' % note)

    if args.prove_immutable:
        # "No B7 finding" and "B7 was measured" are different claims, which is the same reason layout.py
        # prints MEMBER-WAIVED instead of saying nothing: a region that never needed comparing must not read
        # as a region that survived a pass. The counts are what a commit message cites.
        verdict = '[PASS] B7' if not b7['changed'] else '[FAIL] B7'
        moved = 'unmoved' if not b7['changed'] else '%d file(s) changed' % b7['changed']
        print('%s — %d file(s) compared against %s; %d include(s) below the preamble in %d file(s); %s; '
              '%d file(s) hold none'
              % (verdict, b7['files'], args.prove_immutable, b7['includes'], b7['holders'], moved,
                 b7['files'] - b7['holders']))

    detail = ', '.join('%s=%d' % (rule, counts[rule]) for rule in rules if counts.get(rule))
    print('includes: %d finding(s) in %d file(s)%s' % (total, dirty, ' — ' + detail if detail else ''))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
