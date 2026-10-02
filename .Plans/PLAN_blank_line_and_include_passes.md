# Plan — blank-line paragraphs and the include pass: a skill-LLM stage after the formatter pre-process

Status: **awaiting approval.** Nothing below is applied until you say go.

Two owner rulings started this. First, blank lines exist to separate logical paragraphs — and to
highlight — which is a human judgement, so no formatter can own it and a coding agent must. Second,
the include preamble must be deduplicated, sorted lexicographically with the own header excused, and
grouped by bracket type and directory, with the includes that a `.inl` or a test guard depend on left
exactly where they are.

Both rules describe a shape clang-format either refuses to produce or produces carelessly. The plan
therefore changes the pipeline, not just the rule list: `clang-format` and `autofix.py` become a
**pre-process**, and a new **skill-LLM stage** after them is the step that owns everything the
formatter cannot express. The formatter stops being the definition of clean.

## What each rule rests on (measured on this tree, clang-format 22.1.8)

Blank-line behaviour at every structural position, fed 1, 2 and 3 blank lines. `0` = always deleted,
`1` = forced to one whatever was written, `2` = author-controlled up to the `MaxEmptyLinesToKeep: 2`
ceiling.

| Position | 1 in | 2 in | 3 in | Owner of the seam |
|---|---|---|---|---|
| Copyright → `#pragma once` | 1 | 2 | 2 | author |
| `#pragma once` → first include | 1 | 2 | 2 | author |
| include block → include block | 1 | 2 | 2 | author |
| include preamble → `namespace hbe` | **1** | **1** | **1** | **formatter forces one** |
| include preamble → `class C` | **1** | **1** | **1** | **formatter forces one** |
| include preamble → function **definition** | **1** | **1** | **1** | **formatter forces one** |
| include preamble → function declaration | 1 | 2 | 2 | author |
| include preamble → `using namespace` | 1 | 2 | 2 | author |
| include preamble → comment, then body | 1 | 2 | 2 | author |
| after `{` of a namespace / class / function body | 1 | 2 | 2 | author |
| before `}` of a function or class body | 0 | 0 | 0 | formatter deletes |
| after `public:` / `private:` | 0 | 0 | 0 | formatter deletes |
| before `public:` / `private:` | inserts one when absent | 2 | 2 | formatter inserts |
| member → member, statement → statement, statement → `return`, nested `}` → statement | 1 | 2 | 2 | author |
| definition → definition | 1 | 1 | 1 | formatter forces one |
| last declaration → `} // namespace hbe` | 1 | 2 | 2 | author |
| comment → the declaration under it | 1 | 2 | 2 | author |

Two consequences that decide the design:

| Finding | Consequence |
|---|---|
| `BreakAfterIncludes` **does not exist** in clang-format 22.1.8 — `error: unknown key 'BreakAfterIncludes'`, and zero occurrences of the string in the binary | the two-blank seam after the include preamble can never be formatter-native. It lives in the skill-LLM stage, and the format gate must tolerate it |
| The formatter collapses 2 → 1 at exactly that seam whenever the first body is a namespace, a class or a function definition, which is 231 of 270 engine files | the pre-process destroys the rule, so the stage that restores it must run **after** the pre-process, and a later `check.sh --apply` must be followed by that stage again |
| The seam is format-clean today only where a comment follows the includes — which is why `Engine/CodingStandards.h` passes with two blanks there and every `namespace hbe` file passes with one | the existing gate cannot tell "one because the author wrote one" from "one because the formatter ate the second" |
| 83 files carry includes below the preamble: the in-class `.inl` include in 6 `Math` headers (`Engine/Math/Vector3.h:90`) and the trailing `#ifdef __UNIT_TEST__` regions (`Engine/Core/TaskSystem.cpp:787-794`) | those regions are immune. The include stage rewrites the preamble and nothing else |
| `Engine/Core/TaskSystem.cpp:791` writes `"../Engine/Engine.h"` | a relative project include is a finding in its own right |
| `Engine/OSAL/OSMemory.cpp` includes `Core/Debug.h`, `<cerrno>`, `<iostream>`, `<malloc.h>`, `<sys/mman.h>`, `<unistd.h>` twice | duplicates exist and no current layer reports them |

## Rule set A — blank lines

| # | Position | Rule | Enforced by |
|---|---|---|---|
| A1 | Copyright → `#pragma once`, `#pragma once` → first include | exactly one | lint |
| A2 | Between include blocks | exactly one | lint |
| A3 | Include preamble → first code body | **exactly two** — the only place two consecutive blanks are legal | skill-LLM stage writes it, format gate tolerates it |
| A4 | Immediately after any `{` | none | lint (the formatter tolerates a blank there, so a lint is required) |
| A5 | Immediately before any `}` — function body, class body, namespace body | none | formatter for function and class bodies, lint for the namespace close |
| A6 | Immediately after an access specifier | none | formatter |
| A7 | Immediately before an access specifier | exactly one | formatter |
| A8 | Two statements of one step, two declarations of one concern | none | judgement, skill-LLM stage |
| A9 | Two logical paragraphs — a change of step, of concern, or an emphasis the reader needs | exactly one | judgement, skill-LLM stage |
| A10 | Before `return`, when the return is not the only statement in its scope | exactly one | lint, both halves |
| A11 | After a nested block's `}`, before the next statement | exactly one | lint for the shape, judgement for whether the next statement begins a new paragraph |
| A12 | Between two definition blocks at namespace scope | exactly one | formatter |
| A13 | Between a doc comment or `/// API reference:` pointer and its declaration | none | lint |
| A14 | Before a trailing `#ifdef __UNIT_TEST__` region | exactly one | lint |
| A15 | Inside a constructor initializer list | none | lint |
| A16 | Anywhere else, two or more consecutive blanks | forbidden — this is the second pass the owner asked for | lint, tree-wide |

A3 and A16 together mean: exactly one seam in a file may hold two blanks, and it is the one after the
include preamble. Everything the judgement decides is *where the single seams go*, never how big they are.

## Rule set B — includes

| # | Rule | Enforced by |
|---|---|---|
| B1 | The file's own header first, excluded from sorting | lint |
| B2 | Three blocks — own header, `<standard>`, `"project"` — each sorted lexicographically on the written path, one blank between blocks. Sorting the project block by path is what groups it by directory: `"Core/Debug.h"`, `"Core/Task.h"`, `"Log/Logger.h"` | lint, and the formatter while it is a pre-process |
| B3 | No duplicated path inside the preamble | lint |
| B4 | `<…>` for the standard library, `"…"` for everything else | lint |
| B5 | Project includes root-relative — no `../` | lint |
| B6 | Remove an include whose entities the file does not itself name. A consumer that breaks gains its own include; the transitive one is never restored | skill-LLM stage, proven by the three-configuration build gate |
| B7 | Only the preamble is rewritten. In-class `.inl` includes, `#ifdef __UNIT_TEST__` includes, anything below the first code body: untouched | lint proves the region below the preamble is byte-identical |
| B8 | A deleted include is a token loss and must be declared in the fix manifest, which `prove_format.py` holds to | existing machinery, extended to include lines |

## Pipeline, before and after

| | Today | After |
|---|---|---|
| 1 | clang-format + `autofix.py`, and clang-format-clean is the post-condition | clang-format + `autofix.py`, declared a **pre-process** |
| 2 | — | **skill-LLM stage**: blank-line paragraphing, the two-blank seam, the include pass, then the sweep that deletes every unsanctioned consecutive double |
| 3 | member layout | member layout (unchanged) |
| 4 | reference pages | reference pages (unchanged) |
| 5 | strip comments | strip comments, **then the blank-line rules again** — a strip deletes the comment lines that used to mark a seam and leaves the blanks orphaned |
| 6 | build gate | build gate (unchanged) |

## Changes

| # | File | Change |
|---|---|---|
| 1 | `docs/CodingStandards.md` | replace the three *Readability* blank-line bullets with rule set A, and the include bullet with rule set B. This is where a convention is stated; the skill enforces it |
| 2 | `AGENTS.md` | same two clauses in its copy of the standards, which is what an agent reads first |
| 3 | `.pi/skills/hb-standards/SKILL.md` | new **Layer 2c — the skill-LLM stage**, the pipeline table, the measured matrix as a trap entry, `BreakAfterIncludes` recorded as nonexistent, the `.inl` and test-guard immunity, the helper-script rows |
| 4 | `.pi/skills/hb-standards/scripts/blank_lines.py` (new) | decides A1, A2, A3, A4, A5, A10, A13, A14, A15, A16. Says nothing about which seams are paragraphs — that is the judgement it must not fake |
| 5 | `.pi/skills/hb-standards/scripts/includes.py` (new) | B1-B5 and B7 from the preamble; B6 as an advisory candidate list read from the clang AST, never as an automated edit |
| 6 | `.pi/skills/hb-standards/scripts/check.sh` | wire the layer. Its format check gains exactly one sanctioned difference: a blank-line-only deviation at the A3 seam. Everything else stays byte-identical to clang-format's output |
| 7 | `.pi/skills/hb-standards/scripts/prove_regroup.py` | a `--whitespace-only` mode: non-blank lines identical **and** in place, which is the honest proof of a blank-line pass, since today it also accepts a reordering |
| 8 | `Engine/CodingStandards.{h,cpp}` | the exemplar follows A3/A4/A5, and the stale claim in its preamble comment — that clang-format collapses the post-include seam to exactly one, measured for 1, 2, 3 and 4 — is replaced with the measured matrix above. The exemplar is the proof of a rule; a wrong claim there teaches the wrong rule |
| 9 | `JOURNAL.md` | the rulings, with the measurements that forced the pipeline change |

**Not in scope.** The tree-wide sweep. 231 of 270 engine files write one blank where A3 now wants two, and
105 files already satisfy A5 while 163 do not. A bulk sweep is an owner decision on its own commit, which
is the stance the skill already takes on `NamespaceIndentation`. The new layer reports those as `[DEBT]`
under `--all` and as `[FAIL]` for the files a run owns, so a file enters conformance when it is touched.

## Verification

| Step | Check |
|---|---|
| 1 | `blank_lines.py --selftest` on fixtures, one per rule A1-A16, including the shapes the formatter rewrites |
| 2 | `includes.py` against `Engine/Core/TaskSystem.cpp` (relative include, below-preamble block), `Engine/OSAL/OSMemory.cpp` (duplicates), `Engine/Math/Vector3.h` (in-class `.inl`) |
| 3 | `prove_regroup.py --whitespace-only` proves the exemplar edit moved no line |
| 4 | `check.sh Engine/CodingStandards.cpp` — the format layer accepts the A3 seam and still rejects a real format error, tested by planting one |
| 5 | `build.sh EngineTest -dev -debug -release -test`, then `.pi/skills/hb-standards/scripts/gate.sh spawn --all --test` read with `gate.sh wait`, and `GATE_EXIT` quoted, never inferred from `no work to do` |
| 6 | `check.sh --all --no-build` reports the `[DEBT]` counts for A3 and A5, re-measured rather than carried from this plan |

## Step 0 — the tree is dirty before anything starts

`git status` holds 45 paths of uncommitted work: the header-minimality exemplar (`HeaderBodyExamples` in
`Engine/CodingStandards.{h,cpp}`), its copy in `docs/CodingStandards.md`, the matching SKILL.md section,
`AGENTS.md`, `Applications/EngineTest/TestMain.cpp` and 34 TaskSystem reference pages. Changes 1, 2, 3 and
8 of this plan land on four of those same files, so the work must land first or the two commits become
one story. Default: commit it as its own change before touching anything here.

## Status — executed 2026-10-02, and three of this plan's own claims corrected on the way

| Change | Outcome | Commit |
|---|---|---|
| 1 `docs/CodingStandards.md` | rule set A (A1-A16 table) and rule set B (B1-B7 table), each with its measurements | `8b15fc8` |
| 2 `AGENTS.md` | both clauses in the agent-facing copy, with pointers to the tables | `2ee85b1` |
| 3 `SKILL.md` | landed as **Layer 2b**, not 2c — the pass belongs before the comment sweep, so numbering follows position. Fix-cycle step 1b, module-cycle row, helper rows, judgement rows | `2ee85b1` |
| 4 `blank_lines.py` | 16 rules, 41 fixtures. **No `--fix` mode**: deleting a blank because a checker said so is the failure this layer exists to prevent, so the tool reports and the reader edits | `f0f77c5`, `9e1f244` |
| 5 `includes.py` | B1-B5 and B7 gated; B6 behind `--unused`, two measurements per candidate; 10 fixtures | `f0f77c5` |
| 6 `check.sh` | layer 2b wired, `[DEBT]` under `--all` and violations in a scoped run; format check now compares both sides through `blank_lines.py --collapse-seam` | `9e1f244` |
| 7 `prove_regroup.py` | `--whitespace-only`: non-blank lines identical **and** in place | `f0f77c5` |
| 8 `Engine/CodingStandards.{h,cpp}` | nine findings applied; the stale preamble claim went with the blanks | `4a95940` |
| 9 `JOURNAL.md` | the rulings, the reversal of `ea0f157`, and the three claims this plan began believing | `4a95940` |

**Three corrections to the plan itself, all found by measuring instead of reading notes.**
`.clang-format` never set `SortIncludes` or `BreakBeforeBinaryOperators`; it sets `IncludeBlocks: Preserve`
and leaves `SortIncludes` at the clang-format 22 default `{Enabled: true, IgnoreCase: false,
IgnoreExtension: false}`, which is why byte order is the standard rule B2 names and why
`SortIncludes: IgnoreCase` is not available as written (the key became a mapping). The access-label blank
the exemplar documented as unavoidable is not: `{`, no blank, `public:` survives a format run unchanged, so
rule A4 needed no exception. And `BreakAfterIncludes` was already recorded correctly as absent — re-checked
by running the formatter, not by trusting the note.

**One rule added during execution.** A10 stops at a preprocessor directive: the seam organising a
`#pragma clang diagnostic push` pair sits above the directive, so `CreateWithMove`'s `return` is not owed a
blank beneath its own pragma.

| Verification | Result |
|---|---|
| 1 `blank_lines.py --selftest` | 41 fixtures, 0 failures — one per decidable rule plus four that exist because the first version got them wrong |
| 2 `includes.py` on the named files | `TaskSystem.cpp:788` and `OSMemory.cpp:5` report B5 (`"../Engine/Engine.h"`); `Vector3.h` and `Matrix4x4.h` report nothing, which is the in-class `.inl` immunity holding. `OSMemory.cpp` no longer reports B3 — its duplicates went in the earlier header work |
| 3 `prove_regroup.py --whitespace-only` | proves both exemplar files changed only blank lines |
| 4 `check.sh HEAD` / `check.sh 6dad493` | the conforming pair reports `blank lines: 0`, `includes: 0`; the older commit prints its 132 findings and `[FAIL] rule set A and/or rule set B`, so the scoped path bites and the clean path is not vacuous |
| 5 gate with tests | `gate.sh spawn --all --test`, job `20261002-182032-91191` — the earlier job `20261002-180815-70218` reported build gate PASS 12/12 and `GATE_EXIT=1` from the standing comment/layout/docs backlog, with 0 occurrences of the `no recompile exercised` marker |
| 6 `check.sh --all --no-build` | `blank lines: 1648 finding(s) in 272 file(s)`, `includes: 23 finding(s) in 23 file(s)`, both `[DEBT]`. Re-measured in the gate run, not carried from this plan |

**Not in scope, and left as the owner's decision:** sweeping the 1648 seams and 23 include findings across
the tree, and switching any configuration value in `.clang-format`.
