---
name: hb-standards
description: >-
  Bring HardBop Engine C++ sources into the project coding standards end to end: format, lint, migrate
  doc comments into the HTML reference, delete them, reorder members, and prove the tree still builds in
  Debug, Dev and Release. Use when asked to apply clang-format, check or fix coding standards, prepare or
  amend a commit, review a commit for style conformance, sweep or strip comments from a module, write or
  repair API reference pages under docs/, or when Main.cpp / engine sources need the Allman brace style,
  tab indentation, include ordering and the Engine/CodingStandards.h conventions enforced. Also use before
  declaring any engine change done, because the skill ends with a three-configuration build gate.
---

# hb-standards

Six layers, then a build gate. All are required: on this tree the standard's own exemplar files
were **clang-format-clean but rule-non-clean** (include layout), so no single layer is sufficient.

| Layer | Checks | How | Rewrites? |
|---|---|---|---|
| 1 | Allman braces, tabs, 120 columns, include order, blank lines | clang-format | yes, `--apply` |
| 2 | joined empty bodies, no exceptions, `m_` prefix, explicit `inline`, hygiene, include layout | greps | no |
| 3 | comment ban in `.h` and `.cpp` | `scripts/comments.py`, a lexer; `--strip` deletes | yes, `--strip` |
| 4 | twelve-block member layout | `scripts/layout.py`, clang AST | no |
| 5 | every entry owns a page, every method owns a page, every page is valid HTML | `scripts/docs_coverage.py`, `scripts/docs_methods.py`, `scripts/htmlcheck.py`; `scripts/docs_page.py` writes them | yes, `docs_page.py` |
| 6 | Dev, Debug, Release compile, `EngineTest` on request | cmake + ninja, `scripts/runtest.sh` | no |

Layers 3 to 5 never rewrite, and that is load-bearing rather than lazy. No tool can tell which
doc comment belonged to which member, and reordering data members against one another changes C++
initialisation order. A formatter that guessed either would corrupt documentation or behaviour
silently, so the layers report and a reader decides.

## Run it

```bash
.pi/skills/hb-standards/scripts/check.sh                 # files in HEAD
.pi/skills/hb-standards/scripts/check.sh --staged        # files about to be committed
.pi/skills/hb-standards/scripts/check.sh <rev>           # files in a given commit
.pi/skills/hb-standards/scripts/check.sh --staged --apply  # rewrite, then lint
.pi/skills/hb-standards/scripts/check.sh --all --no-build  # whole tree, no compile
.pi/skills/hb-standards/scripts/check.sh --test          # also run EngineTest
```

Exit status: `0` clean, `1` violations, `2` build failed, `3` usage error.

Default scope is the **files a commit touched**, not the whole tree. That is deliberate: a
bulk sweep is a separate owner decision, not something to fold into a feature commit.
Use `--all` when the whole tree is the intended subject.

Always run it **after** `--apply` and **before** committing. If `--apply` changed
files, re-run the lint from scratch rather than trusting the formatter's exit code.

## Running it off the main thread

The full gate takes minutes. Running it inline costs the caller its whole turn and
buys nothing, so run it as a separate process and read the verdict afterwards.

```bash
# one line, no wrapper — the model MUST be pinned (see the trap below)
.pi/skills/hb-standards/scripts/gate.sh spawn --all --test
.pi/skills/hb-standards/scripts/gate.sh wait                      # blocks, exits with the gate's code
```

`gate.sh spawn` launches `pi -p` detached, which loads this skill, runs `check.sh`
with the arguments it was given, and reports per the Reporting section. Under
`.pi/logs/gate/<job>/` you get `cmd`, `pi.log`, and `done`; `gate.sh status [job]`
reads the verdict, `wait` blocks on it, `list` shows jobs and slot occupancy,
`release` frees slots left behind by a killed job. Logs live under `.pi/logs/`,
which is gitignored.

**The verdict is the `GATE_EXIT=<n>` line inside `pi.log`, not the process exit code.**
`done` holds whether that assistant survived; `GATE_EXIT` holds whether the tree is
clean. Confusing the two reports a pass that never happened. `wait` re-exits with
`GATE_EXIT`, so scripts can treat it exactly like `check.sh`.

Raw form, if you want to see it without the wrapper:

```bash
pi -p --offline --no-session --approve --skill .pi/skills/hb-standards \
   --provider "$PI_PROVIDER" --model "$PI_MODEL" --tools read,bash,grep,find,ls \
   "Run .pi/skills/hb-standards/scripts/check.sh --all; echo GATE_EXIT=\$?; report per the skill's Reporting section; edit nothing."
```

Whole-tree fan-out also runs detached, in one command — note the `=` form, because
the space form swallows the prompt that follows it:

```bash
pi -p --offline --no-session --approve --subagents-workflow-file=<sweep>.js
```

### Concurrency: 4, and ask first

**Never run more than 4 concurrent subagents or detached gates without asking the
owner.** `PI_GATE_SLOTS` overrides the cap for `gate.sh`. This is not politeness:
the build tree is single-occupancy, so two gates race on `cmake-build-*` and each
reports a pass the other invalidated. `gate.sh` enforces it with `mkdir` slots
(macOS ships no `flock`) and refuses the fifth job with exit 3.

### Traps already paid for here

- **A detached `pi` must have its provider and model pinned.** Without
  `--provider`/`--model` it picks its own default and dies at startup with
  `401 Invalid bearer token`. `--offline` alone does **not** fix this — measured.
  Inside a pi session, inherit `PI_PROVIDER` and `PI_MODEL`; `gate.sh` does, and
  fails fast with a one-call preflight instead of burying the 401 in a log.
- **`pi auth check` can report `ready` while the token is dead**, so it is not a
  usable preflight. Ask the model to reply `OK` with `--no-tools` instead.
- **`--tools` is the safety belt.** A lint-only run gets `read,bash,grep,find,ls`
  so it cannot edit anything; `edit,write` are added only when `--apply` was asked for.
- **Never let a detached job push.** It is stated in the prompt and must stay there.
- `batch_ai_prompt.sh` is the older convention and is broken in two quiet ways: `local`
  is used outside a function, and its counters increment inside a piped `while`
  subshell, so its closing tally always prints `0 files processed, 0 failures`.

## Layer 1 — clang-format

Enforces things that need real C++ parsing: Allman braces (break before every `{`,
no exemptions, empty bodies included), tabs, 120 columns, blank-line placement.

## Layer 2 — mechanical rule checks clang-format cannot do

Tab-vs-space indentation · joined empty bodies that survived the formatter ·
no exceptions · no `std::move` on return (kills NRVO) · no `virtual` with
`override` · no `m_` prefix · copyright header, trailing newline, trailing
whitespace · include layout (own header → `<standard>` → `"project"`, each
alphabetical, exactly one blank line before the first code body).

`Engine/CodingStandards.{h,cpp}` are exempt from the behavioural checks only:
they carry deliberate BAD EXAMPLE blocks. Formatting, naming, hygiene and include
checks still apply to them. To silence a specific line elsewhere, end it with
`// hb-standards:ignore`.

## Layer 3 — the comment ban (`comments.py`)

No comments in `.h` or `.cpp`. The engine's prose belongs to `docs/`; see
`docs/CodingStandards.md` for the rule and the exhaustive exemption list. A grep cannot enforce
this: `http://` inside a string literal is not a comment, so the script lexes the file — line
comments, block comments, string and character literals, and line continuations. Raw string
literals are absent from this tree (measured: 0 files), and the lexer fails loudly rather than
mis-lexing if one appears.

Deleting them is `--strip`, and the ordering rule and its token proof live in
*Deleting the comments, with proof* under Layer 5, because what gates the deletion is the reference.
The ledger that schedules the work is the same file either way:

```bash
.pi/skills/hb-standards/scripts/docs_coverage.py ledger     # rewrite .Plans/DOCS_COVERAGE.md
.pi/skills/hb-standards/scripts/docs_coverage.py check Core # pages Core still owes
```

## Layer 4 — twelve-block member layout (`layout.py`)

Types, then all data, then all functions; each layer `public` → `protected` → `private`, `static`
first inside each. The block table is in `docs/CodingStandards.md`. The checker asks clang, because
C++ declarator syntax defeats patterns exactly here — see the traps below.

Needs a compile database for the project's own flags: `cmake-build-debug/compile_commands.json`.
Absent, the layer prints `[NONE]` and says so; a rule that could not run must never be readable as
a rule that passed. Regenerate one with
`cmake -S . -B cmake-build-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.

## Layer 5 — the reference under `docs/`

Three claims, three checkers, because "this module is documented" is not one question. Only the first
was wired into `check.sh` for a while, and the gap is exactly the failure it was meant to catch:
`docs/Log/Logger/` held 18 method pages while none of them was `SetIODriver`, `StopDriverThread` or
`DriverLoop`, so layer 5 read "documented" about a class whose entire IO-driver mechanism lived only in
`///` comments a sweep was about to delete.

| Check | Question it answers | Script | Fails the gate as |
|---|---|---|---|
| Coverage | does every namespace-scope entry own a page, and does the module index link it | `docs_coverage.py check <Module>` | `[DEBT]` — unauthored work |
| Method pages | does every declared method own a page of its own | `docs_methods.py <Module>` | `[DEBT]` — unauthored work |
| Validity | does every page parse, link something real, and use a class the CSS declares | `htmlcheck.py <page ...>` | `[FAIL]` — a defect in what exists |

A page that exists and is broken is a violation, not remaining work: an undeclared CSS class or a dead
anchor is a bug in what was just written, and the reader meets it before anyone reads the ledger.

`docs_coverage.py ledger` regenerates `.Plans/DOCS_COVERAGE.md`, the per-module worklist. Run it before
scheduling a module, not after — its first version counted only `.h` and `.hpp` for the comment column and
so understated `Core` by 295 lines, and a queue built on it sends the next worker to the wrong module.

### Method page naming, and why the checker asks clang

Names come from the clang AST through `layout.py`, because the question is "what can a caller call":
`operator bool` has no name a regex can find, `using TValue = std::function<void(int)>` reads as a call
and is not one, and members of `Engine/Math/VectorCommonImpl.inl` belong to the class that includes them.

File names follow `.Plans/AUTHORING_method_and_class_pages.md` section 2 — that table is the rule and
`docs_methods.py`'s `OPERATOR_PAGES` is its machine-readable copy, so the two are edited together. Both
sides of the comparison are `norm()`-ed, which is worth stating because `norm` deletes hyphens: comparing
the raw `operator-right-shift` against the normalised file list means **no operator page ever matches the
file that satisfies it**, and only single-word pages like `constructors` pass by the accident of having no
hyphen. Three things that bug hid, all found in one sitting:

- Operators were routed to a "check by hand" bucket before `expected_file()` was consulted, so a class
  whose entire API is one conversion — `hbe::EndLine`, which exists to become `"\n"` — required no page at
  all, and a reader could never reach its behaviour.
- clang names a class template's constructor `ConfigParam<T, IsAtomic>`, so taking the last whitespace
  token of the description yields `IsAtomic>` and reports a constructor as a method named `IsAtomic` that
  owns no page. The name is whatever follows the kind word `function`.
- The contract spells `operator[]` as `operator-index.html` and `operator<<` as
  `operator-left-shift.html`; a copy of the table said `subscript` and omitted `<<` entirely, and my own
  Resource pages went out as `operator-shift-left.html` — the third spelling in one tree.

`[[nodiscard]]` on getters, `= delete` and `= default` operators are exempt from needing a page and are
**counted in the summary line**, not dropped: what a class forbids or inherits by default is its ownership
story, which the class page carries, and an exemption nobody can see becomes a way to hide a gap.

### Writing the pages (`docs_page.py`)

`docs_page.py` owns the chrome — the sidebar module list, the `current` marker, the breadcrumb depth, the
prevnext footer, and the method list every page of a class must agree on. It does not own the prose: every
sentence comes from a fragment file you wrote after reading the header, because a tool cannot know what a
function was doing wrong.

```bash
docs_page.py class  String Letter --source Engine/String/Letter.h --summary "…" --sections spec.json
docs_page.py method String Letter is-lower-case --source Engine/String/Letter.h --summary "…" \
              --sections spec.json
docs_page.py renav  String Letter        # after adding a page, re-sync every page's method list
docs_page.py check  String               # htmlcheck over what was written
```

`spec.json` is a list of `{"anchor", "heading", "file"}` (or `"body"` for a short one) in page order, and
the anchors must be the contract's ids. Chrome is lifted from the module's own `index.html` and its
relative paths are deepened by one directory — the failure that proved this necessary was a lifted page
whose stylesheet resolved to `docs/String/assets/`, caught by the validator rather than by a reader.

A page is written to `<name>.html.new`, validated, and moved into place **only if `htmlcheck` accepts it**,
with one tolerated exception: a dead link pointing at a sibling page of the same class that has not been
authored yet, which is the normal state of the first page of a class. The count of tolerated links is
printed, so finishing the class stays a visible obligation.

### Deleting the comments, with proof

The ban is only safe after the prose has somewhere to live, and that ordering used to live in one sentence
of this file plus the operator's memory. It is now a precondition of the tool: `--strip` runs the same two
doc checks the gate runs and **refuses** if the module still owes class or method pages, naming the
command that lists them. `--force` overrides, for the case where prose is genuinely going somewhere other
than a class page — a guide, or nowhere.

```bash
git show HEAD:Engine/Module/Header.h > /tmp/pre_Header.h        # 1. snapshot, before anything
python3 scripts/comments.py Engine/Module/Header.h               # 2. read the findings
python3 scripts/comments.py --strip Engine/Module/Header.h       # 3. delete; refuses if code moved
python3 scripts/comments.py code_tokens Engine/Module/Header.h /tmp/pre_Header.h   # 4. prove equality
```

`code_tokens` is a real C++ tokenizer — comments blanked by the ban's own lexer, string and character
literals opaque, line continuations treated as whitespace, maximal munch — and step 4 compares token
multisets, so the proof is arithmetic rather than a reading of the diff. Two weak proofs came first: a
word-level comparison that claimed drift in 98 files that were byte-identical, and a whitespace-collapsed
one that still flagged 15 because it joined literals *with* their quote characters. The tokenizer's own
docstring example caught a bug in its first version: `++` and `--` were missing from the operator list, so
`a++b` and `a+ +b` tokenised identically.

## Layer 6 — build gate (mandatory, last)

Builds `EngineTest`, `VulkanExample` and `WindowExample` across **Dev, Debug and
Release**, plus the `CodingStandards` target. Lint passing means nothing if the
reformat broke compilation.

`CodingStandards` is compiled as a real engine target (see `Engine/CMakeLists.txt`)
precisely so the standard cannot drift from itself again — it belonged to no target
and quietly rotted. `build.sh` cannot reach it (it derives the CMake target from the
basename of an application directory), so the gate invokes `cmake --build` directly.

The touched files are `touch`ed first so ninja genuinely recompiles: without this,
"ninja: no work to do" would report a pass that exercised nothing. Treat a
`no work to do` line as a **suspicious** result and confirm it with
`touch <file>`.

`Examples/MacOSApp` is not covered: `Examples/` is not in the root `CMakeLists.txt`,
so formatting there cannot be compile-verified.

## Proving a commit moved no code (`prove_format.py`)

`clang-format --apply` on 169 files is not reviewable by reading 169 diffs, and "the build passed" does
not prove the formatter only moved whitespace. Run it against a revision and it classifies every file by
token comparison:

```bash
python3 scripts/prove_format.py HEAD~1     # exit 0 when every difference is explained
```

Verdicts: `whitespace` (nothing but layout), `include-order` (a re-sorted include block), `literal-split`
(a long string the formatter broke across lines), `reorder` (declarations moved — legal only for a member
layout commit), `unexplained`, and `reorder` with a lost token is a hard failure. The invariant is stated
once in the tool: nothing may be **lost** from the token multiset, and the only tokens a reorder may add
are access labels, which crossing an access section requires. An earlier draft demanded an identical
multiset and so would have failed the very reorder it was written to verify, and its first permitted set
included `template`, which is not an access label and can carry a change of behaviour.

## The module cycle

One module — or one header inside a module too large to review at once, which is the owner's decision for
`String`, `Core` and `Container` — lands as separate commits, in this order, each with its own proof:

| Commit | Content | Proof required before committing |
|---|---|---|
| `<mod> layout and naming` | twelve-block reorder, `m_` and snake_case removal, `[[nodiscard]]`, `explicit`, `= default`, `out`-prefixed write-only parameters, named constants | `layout.py` reports 0 for the files; the module target compiles; **if the docs already describe the code, do this first**, otherwise pages quote signatures that are about to change |
| `<mod> docs` | pages via `docs_page.py`, module index updated, `.Plans/DOCS_COVERAGE.md` regenerated | `docs_coverage.py check <mod>` and `docs_methods.py <mod>` at 0; `htmlcheck.py` clean |
| `<mod> format` | `clang-format --apply` only | `prove_format.py HEAD~1` exits 0 with every file explained |
| `<mod> comment ban` | `comments.py --strip` | `code_tokens` identical against the pre-strip snapshot; `comments.py` then reports 0 |
| ledger and plan | `.Plans/DOCS_COVERAGE.md`, `JOURNAL.md` | the numbers in them re-measured, not carried forward |

Three rules the cycle exists to enforce, each learned by being broken:

- **Snapshot before stripping.** The proof of a comment deletion is a token comparison against the exact
  bytes before it, and there is no way to recover that snapshot after the fact — one strip had to be
  undone and redone to get a real baseline.
- **Data member order never changes.** Member order is initialisation order. Only functions and types may
  move past data, and only into their own block.
- **A mover must never decide an access specifier.** Insert a labelled block, then re-open the access of
  the anchor it was inserted before. A mover that inserted `private:` plus members without re-opening
  `public:` produced a file that compiled and then failed three files away with "field of type `Buffer` has
  private default constructor" — after two days of notes said exactly this.

Applications and Examples are not modules: they own no API pages, and `Examples/` is outside the root
`CMakeLists.txt` so **nothing compiles it** — a strip there is provable only by `code_tokens`, never by the
build gate. Their prose goes to the top-level guides (`docs/RunningTests.md`, `docs/VulkanExampleGuide.md`
and siblings), which is the convention the tree already uses for prose that is not API.

## Helper scripts

| Script | What it is for |
|---|---|
| `check.sh` | the six layers plus the build gate; `--staged`, `<rev>`, `--all`, `--apply`, `--test`, `--no-build` |
| `gate.sh` | run the whole gate detached, `spawn` / `wait` / `status` / `list` / `release`, with 4 slots |
| `runtest.sh` | build then run `EngineTest` under a wall clock, and report even when it says nothing |
| `hang.sh` | build, run, and if the binary stalls, sample **its** stacks and say where |
| `verify-findings.py` | reject review findings whose cited line does not exist or does not contain the quoted evidence |
| `prove_format.py` | prove a commit moved no code, per revision |
| `docs_page.py` | emit reference pages with correct chrome, validated before kept |

`verify-findings.py` is not decoration. A model asked to audit files it never opened invents defects rather
than reporting none, and the invention is invisible in prose but cheap to detect: the line it "quoted" is
past the end of the file, or the file does not exist. Feeding findings through it turns that observation
into an exit code, so a reviewer that fabricates fails a gate instead of reaching a human.

## Judgement checks a script cannot do

Grep these in the changed hunks and fix by hand:

| Rule | What to look for |
|---|---|
| Log before failing | every early `return`/`Assert` on an error path needs a descriptive `Error:`/`Warning:` log first |
| `[[nodiscard]]` | on getters and value-returning functions |
| `explicit` | on every single-argument constructor |
| `= default` | instead of `{}` for trivial special members |
| `out` / `inOut` / `in` | prefix write-only, read-write, and name-colliding parameters |
| Pointer validation | validate before dereference; prefer references over raw pointers |
| `static_assert` | use it wherever a condition is compile-time evaluable |
| `constexpr` | no magic numbers; constants are PascalCase |
| Composition | prefer has-a; `final` on classes not meant as bases |
| `inline` keyword | redundant on an in-class member definition and on a template; **load-bearing** on a function or operator defined at namespace scope in a header, where dropping it makes every including translation unit emit the symbol and the link fails. Only the AST separates the two, so the lint reports `inline` as advisory |
| No snake_case member | a member is the token before `;`, and a type sits in the same position — `size_t MaxNameLength = 127;` is a type then a PascalCase name, so no grep separates them. The `m_` prefix is checked mechanically; this half of the rule is not |
| `noexcept` | mark only what is provably exception-free; drop it where `new` is called |

## Traps already paid for — do not relearn them

- **A ban on `inline` cannot be a grep.** Stage 0 raised this rule from advisory to FAIL on the strength
  of the standard's sentence "do not use the `inline` keyword explicitly". Applying it would have
  deleted the keyword from `Assert`, `FatalAssert`, `IdentityMatrix` and `operator<<` for `ComponentState`
  — namespace-scope definitions in headers, where `inline` is the only thing separating a working build
  from a duplicate-symbol link error. 16 of the 18 sites in the tree are that shape; exactly one, a
  constructor defined inside `Exception`, was noise. The standard's sentence is now qualified, and the
  layer is advisory again.

- **Member layout is not greppable.** All four of these were mis-classified by the first
  prototype of `layout.py`: `std::function<void(int)> cb;` is data containing parentheses;
  `using TLogFunc = std::function<void(std::ostream&)>;` is a type that reads as a call;
  `explicit operator bool() const` is a function with no name; and an unnamed `union` in a class
  body is a **data member written in place**, not a nested type — sorting it as block 0 tells you
  to hoist `Engine/Math/Vector3.h`'s union above its constants.
- **The AST dump needs a qualified filter.** `-ast-dump-filter=hbe::` against
  `Engine/Memory/MemoryManager.h` dumps 1 MB in 0.3 s; unfiltered it dumps **624 MB**. The filter
  matches the qualified name, so derive it from the file's own namespace.
- **clang's JSON emits one document per filtered declaration.** `json.loads` takes one document
  and raises `Extra data` at the second root; decode the stream root by root with `raw_decode`.
- **A templated class has no `file` on its definition node.** clang puts it on the enclosing
  `ClassTemplateDecl` only. Filtering on a node's own `loc.file` silently skips *every template in
  the engine* — measured on `Engine/Core/ScopedLock.h` and `Engine/Math/Vector3.h`, both reported
  "no class body". Inherit the file from the nearest ancestor that has one.
- **`#include`d `.inl` members belong to the class but not to the includer's line numbers.**
  `VectorCommonImpl.inl` and `MatrixCommonImpl.inl` are included *inside* a class body; clang
  marks their locs with `includedFrom`, and the line is relative to the `.inl`. Attributing them
  to the includer reports line 242 of a 151-line file. Resolve the real file by proving the
  declaration is on that line of a candidate include, and say so when you cannot prove it.
- **A `#ifdef __UNIT_TEST__` file compiles to nothing under Dev**, and clang then exits 0 having
  dumped zero declarations. That is not a pass. Re-run with `-D__UNIT_TEST__=1` and report which
  macros were active — `Engine/Renderer/RendererTest.h` and three Math sources need it.
- **The doc gate must check three things, not one.** `docs_coverage.py` asks whether a page exists; it
  cannot tell whether the page says anything about the method it names. Wiring only that one left `Log`
  passing with 18 method pages covering none of the driver-thread methods. Add `docs_methods.py` and
  `htmlcheck.py` to layer 5 and keep their exit codes distinct: unauthored pages are backlog, a broken page
  is a violation.

- **`htmlcheck` must count a page's own `<style>` block as a declaration.** Only reading the shared
  stylesheet reported seven styled blocks in `docs/RendererDesign.html` as undeclared classes, which
  teaches a reader to ignore the tool. A class consumed by a library rather than CSS — `mermaid` — is
  declared in the shared stylesheet with the reason written next to it, so the rule keeps meaning "nobody
  renders this".

- **A ledger column is a measurement, not a constant.** The comment column of `docs_coverage.py ledger`
  walked `.h`/`.hpp` only, which understated `Core` by 295 lines, and the same file still carried counts
  for two modules stripped weeks earlier. Re-measure before scheduling work from it; a plausible number is
  still a guess until something produces it.

- **Do not trust your own earlier claim that a check runs end to end.** `expected_file()` was verified with
  a unit test on its own inputs and reported as "the gate now requires a conversion page"; the caller
  routed every operator past it. Verify the whole path — generate the page, run the checker, read the
  verdict.

- **`docs_coverage.py` namespace detection needs the newline.** A pending-declaration buffer that
  drops newlines reads `...h"namespace hbe` and the pattern for a namespace brace can no longer
  see the keyword, so every class looked nested. Keep a separator when you strip whitespace.

- **`.mm` / `.m` are excluded.** clang-format classifies them as Objective-C, the
  repo config declares only `Language: Cpp`, so it aborts with exit 1 and writes
  nothing. Run where the config is *not* found, it silently rewrites them with
  LLVM defaults instead (measured 2249 → 2400 bytes, exit 0). Never add them to a
  `clang-format -i` batch without a `Language: ObjC` section in `.clang-format`.
- **`.inl` are excluded.** `MatrixCommonImpl.inl` / `VectorCommonImpl.inl` are
  `#include`d *inside a class body inside a namespace*; their one-tab indentation
  comes from the includer, so formatting them standalone de-indents everything.
- **Generated headers are skipped** when line 1-3 says auto-generated / do not edit
  (e.g. `Engine/Renderer/Vulkan/ShadersSpv.h`).
- **`AllowShortFunctionsOnASingleLine` must stay `None`.** With `Empty`, setting
  `SplitEmptyFunction: true` changes nothing at all — `Empty` wins and rejoins the
  braces.
- **`BraceWrapping` under a named `BreakBeforeBraces` is silently ignored.** Any new
  brace rule goes in the `Custom` table.
- **Two blank lines after includes is not achievable.** clang-format collapses any
  count to exactly one. `docs/CodingStandards.md` still says two and is stale.
- **`build.sh` takes `-test`, not `-notest`** as `AGENTS.md` and
  `docs/HelperScript.md` claim. Without `-test`, `__UNIT_TEST__` is undefined and
  `TestMain.cpp` compiles to an empty `main`, so the test sources are never
  compiled — pass `-test` when the gate is meant to cover them.

## Attributing a gate failure

The gate builds the whole tree, so it can fail on work that has nothing to do with
the commit being checked — including someone else's uncommitted edits in the same
working tree. Before "fixing" anything, decide whose breakage it is:

```bash
git status --short                                    # uncommitted edits?
git show HEAD:<file> | grep -c <missingSymbol>        # present at HEAD but not on disk?
grep -rln <missingSymbol> Engine/                     # who still references it
```

If the symbol exists at HEAD and is gone on disk, a refactor is mid-flight: report
it, do not repair it inside a formatting task. A style change confined to
`.clang-format` and `Engine/CodingStandards.*` cannot break compilation at all —
those files are referenced by no CMakeLists target, which is worth knowing before
spending time on a gate failure they caused.

## Known debt surfaced, not gated

`NamespaceIndentation` is **confirmed `None`** by the owner (2026-09-06) — this is
no longer an open question. ~218 engine files are written indented and are legacy debt awaiting a sweep. The script reports them as `[DEBT]` and does not fail on them: failing every commit that happens to touch one of those files would block
unrelated work. When you `--apply` to such a file, expect its namespace body to be de-indented to column 0 as part of bringing that file into conformance — that is the rule working, not collateral damage.

The owner scheduled the whole-tree sweep on 2026-09-29, module by module, each in its own commit
separate from any reordering commit. When the last module lands, flip this layer from `[DEBT]` to
`[FAIL]` — until then it stays advisory precisely so unrelated work can still be committed.

## Reporting

Report the count per layer — grep failures, advisories, sweep backlog — the
three-configuration build result, and anything left as `[DEBT]` or `[WARN]`. Never claim the gate passed on the strength
of a "no work to do" build. Never push without explicit permission.
