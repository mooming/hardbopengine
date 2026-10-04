# PLAN: run hb-standards on Engine/Container

Cause: owner asked to bring `Engine/Container` through the hb-standards cycle; this is the second
measurement pass, taken after `git fetch` + rebase moved `master` from `3902663` to `0f48473`
(119 commits, a fast-forward — nothing of mine was replayed).
Status: **awaiting approval** — the tree is unmodified; only gitignored build artifacts were touched.

## What the rebase changed about this plan

| Change | Effect on the plan |
|---|---|
| `Engine/Container` moved 181 insertions / 102 deletions | every count below is re-measured, none carried forward |
| `docs_page.py` left `hb-standards`; a new **hb-docs** skill owns page authoring at `.pi/skills/hb-docs/scripts/{docs_page.py,docs_pass.py}` | commit 4 is a hand-off to `hb-docs`, not work `hb-standards` does |
| New layer **2b** (`blank_lines.py`, `includes.py`) and new steps **1b** / **4b** | two more gates before and after a strip; both are clean for Container today |
| New layer **4b** (`prove_regroup.py`) and new `layout.py` checks `ACCESS-EMPTY` / `ACCESS-REDUNDANT` | 1 gated empty section and 9 redundant labels now visible |
| `docs_coverage.py check-file <path>` gates the strip **per file** | the cycle can now ship pair by pair; the old module-wide precondition is gone |
| Build macros renamed to `DEBUG_BUILD` / `DEV_BUILD` / `RELEASE_BUILD` | `__DEBUG__` in my probe flags is dead; the `#ifdef` in `LinkedList.cpp` history note is stale prose |
| `.Plans/STANDARDS_PER_FILE.md` (Oct 4) still shows `Array.h` with 9 comment lines | stale; `file_ledger.py --write` regenerates it, never hand-edit |

## Measured state at `0f48473`

Scope: `git ls-files 'Engine/Container/*.h' 'Engine/Container/*.cpp'` → 20 files, 5930 lines.
Tool: `check.sh --all --no-build` plus the per-file scripts; layer 4 and 5 read a compile database
built at `/tmp/hbe-ast-db` and copied to the gitignored `cmake-build-debug/compile_commands.json`
so `check.sh` could run layer 4 at all.

| Layer | Check | Engine/Container | Whole-tree context |
|---|---|---|---|
| 1 | clang-format | ✅ **clean** | 11 offenders, all `Engine/OSAL/*.cpp` |
| 2 | mechanical greps | ✅ **clean** | hygiene, exceptions, `m_`, include layout all pass |
| 2b | `blank_lines.py` A1-A16 / `includes.py` B1-B7 | ✅ **0 / 0** | 23 A-findings in 8 files, 11 B-findings, all outside Container |
| 3 | `comments.py` | ⚠️ **55 lines in 6 files** | tree-wide 2283 |
| 4 | `layout.py` | ⚠️ **60 findings in 8 headers** (59 `MEMBER-LAYOUT` + 1 `ACCESS-EMPTY`), 9 `ACCESS-REDUNDANT` advice; `--init-order` 0 | tree-wide 210 |
| 4b | `prove_regroup.py` | not applicable before an edit | — |
| 5 | docs gates | ⚠️ **128 method pages, 2 entry pages, 9 header pointers**; 49 existing pages valid | tree-wide 167 method pages |
| 6 | build gate | ❌ **blocked — environment** | see blocker |

Layer-1 note: raw `clang-format --dry-run` flags all 10 headers for the two blanks after the include
preamble. That is rule **A16**, the one seam where the standard and the formatter deliberately disagree,
and `check.sh` compares both sides through `blank_lines.py --collapse-seam`. The gated verdict is clean;
the raw probe was the wrong question.

### Per-file worklist

| File | comments | layout findings | access advice | strip allowed by `check-file` | docs blockers |
|---|---|---|---|---|---|
| `Array.h` | 0 | 0 | 2 | **yes** | 0 |
| `Array.cpp` | 0 | 0 | — | no file of its own | 0 |
| `AtomicStackView.h` | 1 | 3 | 1 | no | 4 |
| `AtomicStackView.cpp` | 0 | 0 | — | no | 0 |
| `BoundedPriorityQueue.h` | 33 | 15 | 0 | no | 11 |
| `BoundedPriorityQueue.cpp` | 5 | 0 | — | no | 0 |
| `Deque.h` | 0 | 9 | 0 | no | 19 |
| `Deque.cpp` | 0 | 0 | — | no | 0 |
| `HashMap.h` | 0 | 11 | 0 | no | 21 |
| `HashMap.cpp` | 0 | 0 | — | no | 0 |
| `LinkedList.h` | 14 | 0 | 6 | no | 23 |
| `LinkedList.cpp` | 0 | 0 | — | no | 0 |
| `Map.h` | 0 | 9 | 0 | no | 17 |
| `Map.cpp` | 0 | 0 | — | no | 0 |
| `Queue.h` | 0 | 1 | 0 | no | 9 |
| `Queue.cpp` | 0 | 0 | — | no | 0 |
| `RingQueue.h` | 1 | 5 | 0 | no | 17 |
| `RingQueue.cpp` | 1 | 0 | — | no | 0 |
| `Vector.h` | 0 | 7 | 0 | no | 18 |
| `Vector.cpp` | 0 | 0 | — | no | 0 |

`Array` is finished: pointer line present, 14 method pages complete, layout clean, comments already
stripped by the upstream sweep. The `.cpp` rows say "no" because a translation unit declares no
documented entry, so its notes belong in a design document under `docs/` and `--strip` needs `--force`
plus a human decision about where the prose goes.

### Layer 5 detail

| Class | Method pages missing | | Class | Method pages missing |
|---|---|---|---|---|
| `Array` | 0 | | `Map` | 16 |
| `AtomicStackView` | 3 | | `Queue` | 8 |
| `BoundedPriorityQueue` | 10 | | `RingQueue` | 16 |
| `Deque` | 18 | | `Vector` | 17 |
| `HashMap` | 19 | | `LinkedList` | 21 |

Entry pages still missing: `EHashEntryState` (enum, `HashMap.h`), `LinkedListNode` (struct,
`LinkedList.h`). Header `/// API reference:` pointer missing on 9 classes: all but `Array`.

## Blocker: layer 6 cannot run on this machine as configured

`cmake-build-debug/CMakeCache.txt` pins `/usr/bin/c++`, which refuses to execute:

    You have not agreed to the Xcode license agreements.

Re-probed at `0f48473`:

| Probe | Result |
|---|---|
| `cmake --build cmake-build-debug --target Container` | license refusal |
| `sudo xcodebuild -license accept` | needs a password — owner-only |
| `DEVELOPER_DIR=/Library/Developer/CommandLineTools` | compiler runs |
| `#include <algorithm>` under that | **not found** — `/Library/Developer/CommandLineTools/usr/include/c++/v1` holds 3 leftover files; the real headers live in the SDK |
| `+ -nostdinc++ -isystem $CLT/SDKs/MacOSX.sdk/usr/include/c++/v1` | **10 of 10 Container translation units pass** `-fsyntax-only -Wall -Werror` |

The syntax-only pass is real compile evidence and is **not** the mandated gate: it is one configuration,
`-fsyntax-only` only, no link, no `VulkanExample` / `WindowExample` / `CodingStandards` / `EngineTest`.

## Commit sequence (module cycle, per-file strip now legal)

| # | Commit | Content | Proof before committing |
|---|---|---|---|
| 0 | `Container judgement review` *(optional)* | twelve judgement rules over two slicings, edits nothing | `verify-findings.py` passes each pass file; `review_merge.py` reports unique + corroborated |
| 1 | `Container twelve-block member layout` | reorder 8 headers; data block moves as a unit and never re-orders internally; mover re-opens the access of its anchor; settle the 1 `ACCESS-EMPTY`, take or leave the 9 redundant labels | `layout.py` 0 **and** `layout.py --init-order` 0 on all 10 headers; `Container` target compiles |
| 2 | `Container format` | expected no-op for layers 1/2/2b; `prove_regroup.py --whitespace-only` if 4b grouping is touched | `prove_format.py HEAD~1 --manifest .Plans/fix-manifest.json` explains every diff; `blank_lines.py` and `includes.py` at 0 |
| 3 | `Container reference pages` | hand-off to **hb-docs**: 128 method pages, `EHashEntryState` + `LinkedListNode`, 9 pointer lines, `docs/Container/index.html` rows | `docs_coverage.py check Container` 0, `docs_methods.py Container` 0, `htmlcheck.py` clean, no `[NO POINTER]` |
| 4 | `Container comment ban` | `comments.py --strip` on 6 files, 55 lines, each with its `check-file` gate green first | `code_tokens` identical vs `/tmp/pre_*`; `comments.py` then 0 |
| 5 | `Container paragraphs after the strip` | `blank_lines.py --paragraphs`, re-decide every seam by reading | no stranded run left; rule set A back to 0 |
| 6 | `ledger and journal` | `.Plans/DOCS_COVERAGE.md`, `.Plans/STANDARDS_PER_FILE.md` via `file_ledger.py --write`, `JOURNAL.md` | numbers re-measured |
| G | build gate | Dev, Debug, Release + `EngineTest` | `GATE_EXIT=0`, never read off `no work to do` |

Pair order for steps 1 and 3, smallest surface first: `Queue` → `AtomicStackView` → `RingQueue` →
`Vector` → `Map` → `Deque` → `HashMap` → `LinkedList` → `BoundedPriorityQueue`.

## Open questions for the owner

1. **Layer 6** — accept the Xcode license, or authorize Command Line Tools + `-nostdinc++ -isystem`
   for the gate? Without one of the two, steps 1 and 4 cannot be committed under the skill's own rule.
2. **Scope** — all nine remaining pairs in one pass, or land the first pairs and stop for review?
3. **Judgement review** — run step 0 first, or go straight to layout?
4. **`.cpp` prose** — `BoundedPriorityQueue.cpp` (5 lines) and `RingQueue.cpp` (1) need `--force` and a
   destination: a new design document under `docs/`, an existing page, or left in place?
5. **`Engine/Container/ReviewNote.txt`** — tracked prose with a verdict that no longer applies; leave,
   move to `docs/`, or delete?
