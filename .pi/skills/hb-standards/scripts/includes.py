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
  B7  everything below the preamble is untouched, and `--prove-immutable <rev>` proves it was

B7 is the reason this checker exists next to clang-format rather than inside it. 83 files carry includes
below the preamble — `#include "VectorCommonImpl.inl"` inside a class body, where position decides what is
in scope, and the `#ifdef __UNIT_TEST__` regions that close a file — and a pass that "tidied" them would
break the build while looking like a cleanup. The proof is a byte comparison of the region under the
preamble against the revision the edit started from.

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


def below_preamble(text, path):
    """The bytes under the preamble — the region rule B7 forbids a pass to touch."""
    _includes, boundary = parse_preamble(text, path)
    return '\n'.join(text.split('\n')[boundary:])


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


def syntax_check(entry, source_path, directory):
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
    kept += ['-fsyntax-only', source_path]
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


def ast_names_of(header_path, entry):
    """The names a header declares itself, read from clang rather than guessed.

    This is the half of B6 that ablation cannot answer. Deleting `#include <atomic>` from a file that
    spells `std::atomic` still compiles when that file's own header drags `<atomic>` in transitively, and
    "it still builds" is not the rule — the rule is that a file keeps the include for every name it
    spells. A candidate therefore has to clear two tests, and this supplies the second one.

    Returns None when the question could not be answered. The caller reports that separately rather than
    folding it into the candidate list, because "could not tell" and "not a candidate" are different
    claims — the same distinction `layout.py` draws with `[NONE]`.
    """
    cache = ast_names_of.__dict__.setdefault('cache', {})
    if header_path in cache:
        return cache[header_path]
    wanted = os.path.abspath(header_path)
    scratch = os.path.join(os.path.dirname(wanted), '._includes_probe_names_%d.cpp' % os.getpid())
    with open(scratch, 'w', encoding='utf-8') as handle:
        handle.write('#include "%s"\n' % os.path.basename(wanted))
    names = set()
    try:
        command = layout.clang_argv(entry, scratch, header_name_filter(wanted))
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
    cache[header_path] = names
    return names


def walk_names(node, wanted):
    """Yield the declared name of every declaration of interest that lives in `wanted` itself.

    The location filter is what keeps a header's *own* names rather than everything its transitive
    includes drag in: clang puts the spelling location on the declaration, and a declaration spelled in an
    included file names a different file.
    """
    kinds = {'CXXRecordDecl', 'ClassTemplateDecl', 'FunctionDecl', 'FunctionTemplateDecl', 'TypeAliasDecl',
             'TypedefDecl', 'EnumDecl', 'VarDecl', 'NamespaceDecl', 'ConceptDecl', 'UsingDecl'}
    stack = [node]
    while stack:
        current = stack.pop()
        if not isinstance(current, dict):
            continue
        path = (current.get('loc') or {}).get('file') or ''
        name = current.get('name') or ''
        if current.get('kind') in kinds and name and os.path.abspath(path) == wanted:
            yield name.split('<')[0].split('::')[-1]
        for value in current.values():
            if isinstance(value, list):
                stack.extend(item for item in value if isinstance(item, dict))


def provided_names(item, entry, includer):
    """The names an include supplies, or None when that question has no cheap answer.

    A standard header is the unanswered case: with no per-header name table there is nothing sound to
    compare the file's spellings against, so it is reported as unverifiable instead of as a candidate.

    The including file's own directory is the first root, because a quoted include resolves against the
    file that writes it — `#include "Array.h"` in `Array.cpp` is the same directory, and leaving that out
    made every own header look unverifiable.
    """
    if item['bracket'] != '"':
        return None
    roots = [os.path.dirname(os.path.abspath(includer))] + [os.path.join(REPO_ROOT, root)
                                                           for root in PROJECT_ROOTS + ('.',)]
    for root in roots:
        candidate = os.path.normpath(os.path.join(root, item['path']))
        if os.path.isfile(candidate):
            return ast_names_of(candidate, entry)
    return None


def unused_candidates(path):
    """Includes this file neither needs nor names, as (candidates, error, unverifiable, needed).

    Only defined for a source file. A header's includes are consumed by whoever includes the header, so
    deleting one and recompiling the header's own translation unit proves nothing about the tree — the
    three-configuration build is the only instrument fine enough for that question, and the skill already
    runs it.

    Two measurements per include, and a candidate must clear both: compile the file with the line gone,
    and check that no name the include declares is spelled anywhere in the file's own code. The first
    alone would recommend deleting the include a file genuinely depends on and only gets transitively,
    which is the opposite of the rule.
    """
    if not path.endswith(('.cpp', '.cc', '.cxx')):
        return None, ("B6 is a translation-unit question and %s is a header: a header's include list is what "
                      'its consumers compile against, so the three-configuration build is the proof' % path), [], []
    entries, database = load_compile_database()
    if entries is None:
        return None, ('no compile database at %s — regenerate with cmake -S . -B cmake-build-debug '
                      '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' % database), [], []
    entry = database_entry(entries, path)
    if entry is None:
        return None, 'no compile database entry for %s' % path, [], []
    directory = entry.get('directory', os.getcwd())        # the database is authored against its own working directory

    try:
        with open(path, encoding='utf-8') as handle:
            text = handle.read()
    except OSError as error:
        return None, str(error), [], []
    items, _boundary = parse_preamble(text, path)
    inside = [item for item in items if not item['below']]
    lines = text.split('\n')
    spelled = set(re.findall(r'[A-Za-z_][A-Za-z_0-9]*', comments.code_only(text)))
    scratch = os.path.join(os.path.dirname(os.path.abspath(path)),
                           '._includes_probe_%d%s' % (os.getpid(), os.path.splitext(path)[1]))
    candidates, unverifiable, needed = [], [], []
    try:
        base_code, _stderr = syntax_check(entry, os.path.abspath(path), directory)
        if base_code != 0:
            return None, ('%s does not compile as it stands, so ablation would report every include as removable'
                          % path), [], []
        for item in inside:
            variant = list(lines)
            del variant[item['line'] - 1]
            with open(scratch, 'w', encoding='utf-8') as handle:
                handle.write('\n'.join(variant))
            code, stderr = syntax_check(entry, scratch, directory)
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
    return candidates, None, unverifiable, needed


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
    import shutil
    shutil.rmtree(root, ignore_errors=True)
    print('%d fixture(s), %d failure(s)' % (len(CASES), failures))
    return 1 if failures else 0


def main(argv):
    parser = argparse.ArgumentParser(description='Check the include rules of docs/CodingStandards.md')
    parser.add_argument('files', nargs='*', help='C++ sources to check')
    parser.add_argument('--selftest', action='store_true', help='run the built-in fixtures')
    parser.add_argument('--unused', action='store_true',
                        help='B6 candidates: compile this file once per include with that include deleted')
    parser.add_argument('--prove-immutable', metavar='REV',
                        help='B7: refuse unless the region under the preamble is byte-identical to REV')
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
    b7 = {'files': 0, 'regions': 0, 'lines': 0, 'changed': 0}
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
            was, now = below_preamble(before, path), below_preamble(text, path)
            b7['files'] += 1
            b7['lines'] += sum(1 for line in now.split('\n') if line.strip())
            if now.strip():
                b7['regions'] += 1
            if was != now:
                b7['changed'] += 1
                findings.append(Finding('B7', 0, 'the region under the preamble changed, and rule B7 forbids it'))

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
            candidates, error, unverifiable, needed = unused_candidates(path)
            if candidates is None:
                print('[NONE] B6 — %s: %s' % (path, error))
                continue
            print('[INFO] B6 — %s: %d candidate(s) this file neither needs nor names' % (path, len(candidates)))
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
        moved = 'unmoved' if not b7['changed'] else '%d changed' % b7['changed']
        print('%s — %d file(s) compared against %s; %d region(s) of %d line(s); %s; '
              '%d file(s) hold no region below the preamble'
              % (verdict, b7['files'], args.prove_immutable, b7['regions'], b7['lines'], moved,
                 b7['files'] - b7['regions']))

    detail = ', '.join('%s=%d' % (rule, counts[rule]) for rule in rules if counts.get(rule))
    print('includes: %d finding(s) in %d file(s)%s' % (total, dirty, ' — ' + detail if detail else ''))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
