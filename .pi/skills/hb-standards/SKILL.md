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
.pi/skills/hb-standards/scripts/hb_standards.sh                 # files in HEAD
.pi/skills/hb-standards/scripts/hb_standards.sh --staged        # files about to be committed
.pi/skills/hb-standards/scripts/hb_standards.sh <rev>           # files in a given commit
.pi/skills/hb-standards/scripts/hb_standards.sh --staged --apply  # rewrite, then lint
.pi/skills/hb-standards/scripts/hb_standards.sh --all --no-build  # whole tree, no compile
.pi/skills/hb-standards/scripts/hb_standards.sh --test          # also run EngineTest
```

Exit status: `0` clean, `1` violations, `2` build failed, `3` usage error.

Default scope is the **files a commit touched**, not the whole tree. That is
deliberate: the tree currently has ~218 files that predate current rules, and a
bulk sweep is a separate owner decision, not something to fold into a feature commit.

Always run it **after** `--apply` and **before** committing. If `--apply` changed
files, re-run the lint from scratch rather than trusting the formatter's exit code.

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
Release**. Lint passing means nothing if the reformat broke compilation.

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

`NamespaceIndentation` is contradicted tree-wide: the rule and the exemplars say
`None`, ~218 engine files are written indented. The script reports this as `[DEBT]`
and does not fail on it. Flipping it is a ~14 000-line decision for the owner, not
something to slip into a formatting pass.

## Reporting

Report the violation count per layer, the 9-configuration build result, and
anything left as `[DEBT]` or `[WARN]`. Never claim the gate passed on the strength
of a "no work to do" build. Never push without explicit permission.
