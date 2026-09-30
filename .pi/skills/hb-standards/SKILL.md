---
name: hb-standards
description: >-
  Format and strictly lint HardBop Engine C++ sources touched by a commit against
  the project coding standards, then prove the tree still builds in Debug, Dev and
  Release. Use when asked to apply clang-format, check or fix coding standards,
  prepare or amend a commit, review a commit for style conformance, or when
  Main.cpp / engine sources need the Allman brace style, tab indentation, include
  ordering and the Engine/CodingStandards.h conventions enforced. Also use before
  declaring any engine change done, because the skill ends with a three-configuration
  build gate.
---

# hb-standards

Six layers, then a build gate. All are required: on this tree the standard's own exemplar files
were **clang-format-clean but rule-non-clean** (include layout), so no single layer is sufficient.

| Layer | Checks | How | Rewrites? |
|---|---|---|---|
| 1 | Allman braces, tabs, 120 columns, include order, blank lines | clang-format | yes, `--apply` |
| 2 | joined empty bodies, no exceptions, `m_` prefix, explicit `inline`, hygiene, include layout | greps | no |
| 3 | comment ban in `.h` and `.cpp` | `scripts/comments.py`, a lexer | no |
| 4 | twelve-block member layout | `scripts/layout.py`, clang AST | no |
| 5 | every declared entry owns a page under `docs/` | `scripts/docs_coverage.py` + `.Plans/DOCS_COVERAGE.md` | no |
| 6 | Dev, Debug, Release compile, `EngineTest` on request | cmake + ninja | no |

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

Default scope is the **files a commit touched**, not the whole tree. That is
deliberate: the tree currently has ~218 files that predate current rules, and a
bulk sweep is a separate owner decision, not something to fold into a feature commit.

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

## Layer 3 — build gate (mandatory, last)

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

## Layer 3 — the comment ban (`comments.py`)

No comments in `.h` or `.cpp`. The engine's prose belongs to `docs/`; see
`docs/CodingStandards.md` for the rule and the exhaustive exemption list. A grep cannot enforce
this: `http://` inside a string literal is not a comment, so the script lexes the file — line
comments, block comments, string and character literals, and line continuations. Raw string
literals are absent from this tree (measured: 0 files), and the lexer fails loudly rather than
mis-lexing if one appears.

Run the ledger before deleting comments from a module, and delete only once its pages exist:

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

## Layer 5 — docs coverage (`docs_coverage.py`)

Structural only, and honestly so: a script can prove a page exists and is linked, never that its
prose is right. The ledger lists every namespace-scope entry of every header. A nested type is
documented on its owner's page, and a `.cpp` declares implementation rather than API — both are
excluded from the ledger deliberately; the first version counted them and demanded pages no reader
could name (`docs/Container/Iterator/index.html` was wanted by three different `Iterator` types).

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

Report the violation count per layer, the 9-configuration build result, and
anything left as `[DEBT]` or `[WARN]`. Never claim the gate passed on the strength
of a "no work to do" build. Never push without explicit permission.
