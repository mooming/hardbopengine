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

Two independent layers, then a build gate. Both layers are required: on this tree
the standard's own exemplar files were **clang-format-clean but rule-non-clean**
(include layout), so neither layer alone is sufficient.

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
| `noexcept` | mark only what is provably exception-free; drop it where `new` is called |

## Traps already paid for — do not relearn them

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
no longer an open question. ~218 engine files are written indented and are legacy
debt awaiting a sweep. The script reports them as `[DEBT]` and does not fail on
them: failing every commit that happens to touch one of those files would block
unrelated work. When you `--apply` to such a file, expect its namespace body to be
de-indented to column 0 as part of bringing that file into conformance — that is
the rule working, not collateral damage.

The bulk sweep is a separate, owner-scheduled operation.

## Reporting

Report the violation count per layer, the 9-configuration build result, and
anything left as `[DEBT]` or `[WARN]`. Never claim the gate passed on the strength
of a "no work to do" build. Never push without explicit permission.
