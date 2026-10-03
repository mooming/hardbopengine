# PLAN: apply hb-standards to Applications/ and Examples/

Status: **executed** on 2026-10-03, commits `8421628` `e88648b` `fca89a9` `1b28de5` `ea4c934` `50efdaa`.
Measured from a dirty tree at HEAD `b652a48`. Outcome at the bottom of this file.

One step changed in execution: the owner squashed the two pre-existing edits into a single commit rather
than one each, so the sweep opened from a clean tree in one commit instead of two.

## Goal

Bring every C++ source under `Applications/` and `Examples/` to the six layers of
`.pi/skills/hb-standards/SKILL.md`, and prove each edit with the layer's own checker.

Owner decisions taken before this plan was written:

| Decision | Ruling |
|---|---|
| `Examples/MacOSApp` | **delete the example entirely** |
| `Language: ObjC` in `.clang-format` | **do not add now** — record as an open item |
| Pre-existing uncommitted edits | **commit them as-is first**, on a separate commit each, so the sweep starts from a clean tree |
| Layer 3 prose destination | moot — every file that held a comment is inside the deleted directory |

## Measured state before any edit

Scope was `git ls-files` under both directories: 18 tracked C++/Objective-C++ files, 2 503 lines in the
16 that the toolchain can format, plus `Application.mm` and `Window.mm`.

| Layer | Checker | Applications (3 files) | `Examples/WindowExample` (1) | `Examples/MacOSApp` (14) |
|---|---|---|---|---|
| 1 format | `clang-format --dry-run --Werror` | clean | clean | clean |
| 2 mechanical | `autofix.py --dry-run` | 0 fixes | 0 fixes | 0 fixes |
| 2b blank lines | `blank_lines.py` | 5 | 5 | 73 |
| 2b includes | `includes.py` | clean | clean | clean |
| 3 comment ban | `comments.py` | 0 | 0 | 257 |
| 4 member layout | `layout.py` | 0 | 4 | 12 findings, 3 files `[SKIP]` |
| 5 reference | `docs_coverage.py` | not a module, no pages owed | same | same |
| 6 build | cmake | `EngineTest`, `VulkanExample` | `WindowExample` | **compiled by nothing** |

Two structural facts about these directories, both from the skill: they are **not modules**, so layer 5
owns no pages and their prose belongs in a guide rather than `docs/<Module>/`; and `Examples/CMakeLists.txt`
ends at `add_subdirectory (WindowExample)`, so `Examples/MacOSApp/CMakeLists.txt` is configured by nothing —
layer 6 cannot prove it and `layout.py` has no compile-database entry for its headers, which is the whole
reason `UI/Button.h`, `UI/ScrollBar.h` and `UI/TextPanel.h` answered `[SKIP] 'UI/UIComponent.h' file not found`.

## Why the deletion is the first move, not the last

`Examples/MacOSApp` holds **100 % of the layer 3 work** (257 of 257 comment lines), **76 % of the layer 2b
work** (73 of 83 blank-line findings), **75 % of the layer 4 work** (12 of 16 findings), and **both `.mm`
files that clang-format refuses**. Deleting it is not dodging the sweep; it removes the only parts of the
scope that the toolchain cannot prove, and leaves three files whose every layer is checkable.

What the deletion also settles, and must be reported rather than quietly gained:

| Removed with the directory | Consequence |
|---|---|
| 257 comment lines | layer 3 has nothing left to move into prose, and no guide needs authoring |
| 94 glyph-table labels `// 'A' (65)` | the one case the standard says position already encodes |
| `Application.mm`, `Window.mm` | the only in-scope reason to add a `Language: ObjC` block |
| `Design/WindowAppDesign.html` (79 KB) | MacOSApp's own design reference |
| 12 layout findings, 3 `[SKIP]` headers | and with them the need for a standalone compile database |

What the deletion does **not** fix: `Engine/OSAL/OSXApplication.mm`, `Engine/OSAL/OSXWindow.mm` and
`Engine/Renderer/Vulkan/VulkanRenderer.mm` remain unformattable, because `.clang-format` declares only
`Language: Cpp`. Owner ruled those files out of scope here; they become an open item.

## Steps

| # | Step | Files | Proof required before moving on |
|---|---|---|---|
| 0 | Commit the two pre-existing edits, as-is, one commit each. Both already verified with `-fsyntax-only` against the real compile-database flags — `TaskSystem.cpp` at exit 0 with `<limits>` and `Constants.h` gone, and **both** preprocessor halves of `TestMain.cpp` at exit 0 | `Engine/Core/TaskSystem.cpp`, `Applications/EngineTest/TestMain.cpp` | `git status --porcelain --untracked-files=no` empty |
| 1 | Delete `Examples/MacOSApp` — 18 tracked files | `Examples/MacOSApp/**` | `git grep -l MacOSApp -- ':!JOURNAL.md' ':!.Plans' ':!.pi/logs'` returns only `SKILL.md`, fixed in this same commit |
| 1b | Repair the sentence that becomes false in step 1: `SKILL.md:524` claims `Examples/` is outside the root `CMakeLists.txt`. It is not — line 35 adds it, and `WindowExample` compiles | `.pi/skills/hb-standards/SKILL.md` | re-read the paragraph; no claim about a deleted directory survives |
| 2 | Read all three surviving files in full and apply the judgement checks the scripts cannot: `explicit`, `[[nodiscard]]`, `= default`, named constants over magic numbers, `out`-prefixed write-only parameters, log-before-failing | 3 files | each fix quoted as file:line in the report, and each deliberate refusal stated |
| 3 | Layer 4: move the `WindowTickProvider` data block above its functions | `Examples/WindowExample/Main.cpp:81-84` | `layout.py` 0 **and** `layout.py --init-order` 0 — data moved, so the second is mandatory |
| 4 | Layer 2b: decide the 15 blank-line seams by reading each file, then re-check the include preamble | 3 files | `blank_lines.py` 0, `includes.py` 0, and `prove_regroup.py <file> --whitespace-only` per file |
| 5 | Layer 1 last-byte: `clang-format --dry-run --Werror` after the hand edits | 3 files | exit 0 |
| 6 | Layer 6: the full gate, detached | whole tree | `GATE_EXIT=0` read out of `pi.log`, never off `ninja: no work to do` |
| 7 | Ledger: `JOURNAL.md`, `.Plans/STANDARDS_PER_FILE.md` (15 MacOSApp rows), `.Plans/PLAN_no_cpp_comments.md:52`, the ObjC open item | 4 files | every number in them re-measured, not carried forward |

## Commits, and what each one owes

| Commit | Content | Proof attached |
|---|---|---|
| `EngineTest: the -test banner prints as a std::cerr chain` | step 0, pre-existing edit | `-fsyntax-only` both halves, exit 0 |
| `Core: the two includes TaskSystem.cpp no longer names` | step 0, pre-existing edit | `-fsyntax-only` exit 0 |
| `Examples: remove the MacOSApp example` | steps 1 and 1b | `git grep` clean; no target built it, so the gate is unaffected by construction |
| `Applications/Examples layout and judgement` | steps 2 and 3 | `layout.py` 0 + `layout.py --init-order` 0 + the targets compile |
| `Applications/Examples paragraphs` | steps 4 and 5 | `blank_lines.py` 0, `includes.py` 0, `prove_regroup.py --whitespace-only`, `prove_format.py HEAD~1 --manifest` |
| `journal: Applications and Examples conform, MacOSApp is gone` | step 7 | re-measured counts |

## The 15 blank-line seams, stated exactly

`blank_lines.py` sizes a seam and never decides that one exists, so every row below is a seam to look at,
not a line to drive to zero. A11, A9 and A8 are the rules the checker is deliberately silent on.

| File | Line | Rule | Says |
|---|---|---|---|
| `Applications/EngineTest/TestMain.cpp` | 10 | A3 | preamble seam wants 2 blanks, holds 1 |
| | 12 | A14 | wants 1 blank, holds 0 |
| | 31, 46, 67 | A10 | a `return` that is not alone in its scope wants 1 blank before it |
| `Applications/VulkanExample/Main.cpp` | 85 | A11 | wants 1 blank, holds 0 |
| | 86, 102, 110, 120 | A10 | as above |
| `Examples/WindowExample/Main.cpp` | 16 | A3 | preamble seam wants 2, holds 1 |
| | 19 | A4 | allows at most 0, holds 1 |
| | 87 | A5 | no closing brace behind a blank, holds 1 |
| | 98, 105, 114 | A10 | as above |

## Gate shape after the change

The three targets the gate builds — `EngineTest`, `VulkanExample`, `WindowExample` across Dev, Debug,
Release, plus `CodingStandards` — become **exactly** the set of targets that hold a file in this scope.
That is the strongest statement available here: after the deletion, every file in scope is compiled by the
gate that judges it. `Examples/` **is** in the root `CMakeLists.txt` at line 35, which is why
`WindowExample` is compile-provable and why the `SKILL.md` sentence is wrong.

`check.sh --all` will still exit 1 on this tree after the work is done, on engine backlog that this task
does not own: 1 903 comment lines, 225 layout findings, 362 method pages. The proof for these files is
therefore `check.sh <rev>` scoped to the two C++ commits, whose exit code describes only what they touched.

## Risks, and what refuses them

| Risk | What stops it |
|---|---|
| A data member moves past another, silently changing initialisation order | `layout.py --init-order`, which is clang's own `-Wreorder` computed statically; and the rule that only functions and types may pass data |
| A blank-line edit deletes or swaps a line | `prove_regroup.py --whitespace-only` compares non-blank lines position by position, so a swap is a refusal |
| The formatter's own exit code is mistaken for a verdict | layer 1 is re-run from scratch after the hand pass, and `prove_format.py HEAD~1 --manifest` classifies every difference by token comparison |
| A pass reads `ninja: no work to do` as a build | `check.sh` touches the files first; a `no work to do` line is treated as suspicious and re-probed |
| Deleting a directory breaks something that named it | `git grep MacOSApp` across the tree, run before the commit; the only hits are planning records and the one `SKILL.md` sentence |
| The gate passes on an empty lint scope | `check.sh` prints `lint scope: NONE` and refuses to let an empty scope carry a verdict |

## Out of scope, stated so it is not mistaken for done

- The three `Engine/*.mm` files, unformattable until `Language: ObjC` joins `.clang-format`.
- Every engine file's blank-line, include and comment backlog — `--all` counts, not this plan's work.
- `Examples/CMakeLists.txt` and `Applications/CMakeLists.txt` are CMake, not C++; nothing in this task
  reformats them. Their contents were read only to establish which targets compile.

---

## Outcome

Every step ran. The gate reports build gate PASS 12/12 with 0 `no work to do` lines, and `EngineTest`
pass=59 fail=0 of 59 collections in Dev, Debug and Release.

| Layer, on the 3 files left in scope | Result |
|---|---|
| 1 clang-format | 0 rewrites |
| 2 mechanical greps, `autofix.py` | 0 findings, 0 fixes, 0 advisories. Proved at the range: `prove_format.py 8421628^` — 19 files, 15 whitespace-only, 4 unexplained, and each of the 4 traces to the commit that declares it |
| 2b blank lines (rule set A) | 15 findings → 0. The last one was the refuted A14 false positive, gone with `6a48f9a` |
| 2b includes (rule set B) | 0 findings |
| 3 comment ban | 0 comment lines in scope before or after |
| 4 member layout | 4 findings → 0, `layout.py --init-order` 0 on the compiler's word |
| 5 API reference | not applicable — not modules |
| 6 build + tests | PASS 12/12, and 59/59 collections in all three configurations |

Two things the plan did not anticipate, both recorded in `JOURNAL.md`.

**The plan's own verification step was insufficient.** Step 0 checked the pre-existing `TaskSystem.cpp` edit
with `-fsyntax-only` on compile-database flags, which lack `-D__UNIT_TEST__`, so the region that names `Pi`
and `Epsilon` was preprocessed away and the check could not see the breakage it needed to see. The gate's
`--test` half found it; `50efdaa` restores `#include "Constants.h"`. Step 0's proof line should have read
"with `-D__UNIT_TEST__=1` where the file carries such a region".

**Four checker claims were refuted**, three of them false positives and one a direct contradiction between
`blank_lines.py` and `prove_regroup.py` on `Applications/EngineTest/TestMain.cpp`, where no file state
satisfies both. Each refutation, and the compiler or positive control that produced it, is in the journal
entry for 2026-10-03.

**All four are since fixed**, on the owner's ruling that the fixes were worth doing though they sat outside
this plan's scope: `a8793f7` (`includes.py`), `107ec20` (`prove_regroup.py`), `6a48f9a` (`blank_lines.py`),
`9a0182d` (`layout.py`). Every one was re-measured at the scale its checker runs at, not on a sample — the
first claim for `9a0182d` was drawn from 65 headers and missed all three files the fix changes. See the
journal entry for 2026-10-03 13:00.
