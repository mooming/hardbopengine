# PLAN: Move the drive-until wait out of TaskSystem into a TestHelper namespace

Date opened: 2026-10-02
Status: done — code in `3686c82`, reference in `e6eb256`

## Result

| Step | Outcome |
| --- | --- |
| 0 Settle the staged sweep | Committed alone as `b12c953` after Dev/Debug/Release builds and a 375-testlet run |
| 1 Baseline | `docs_coverage check-file TaskSystem.h` clean; `docs_methods Core` 197 missing; suite 59 collections / 375 testlets in three configs |
| 2-4 `TestHelper.{h,cpp}` + CMake | `Engine/Test/TestHelper.h`, `.cpp`, both registered in `Engine/Test/CMakeLists.txt` |
| 5 Engine deletions | `TaskSystem.h` lost the template, `NestedPumpGuard` and both declarations; `TaskSystem.cpp` lost the definition |
| 6-7 Call sites | 6 in `Window.cpp`, 8 in `TaskSystem.cpp`, all `TestHelper::DriveUntil(taskSystem…)`; `grep "\.DriveUntil("` over `Engine/` returns nothing |
| 8 Build and run | `check.sh 3686c82 --test`: build gate PASS 12/12, EngineTest 59/59 collections in Dev, Debug, Release — same totals as baseline |
| 9 Docs | `docs/Test/TestHelper/{index,drive-until,report-drive-timeout}.html` via `docs_page.py`; both `docs/Core/TaskSystem/` pages deleted; `renav` re-synced 44 pages |
| 10 References | Prose repaired in 6 Core/TaskSystem pages, `docs/Test/index.html`, `docs/TaskSystemGuide.md`, `docs/TaskSystemRedesign.md`; `htmlcheck` 99 pages, 0 problems |
| 11 Gate, ledger, journal | `grep rule failures: 0` on both commits; `.Plans/DOCS_COVERAGE.md` regenerated; `JOURNAL.md` 14:29 entry |

## What differed from the plan

| Assumption | What happened |
| --- | --- |
| The declarations could keep their order | They could not. `ReportDriveTimeout` is now declared above the template: the call inside it is non-dependent, so it resolves in the definition context. The first build failed with *use of undeclared identifier* |
| The unstaged 3-line lambda re-wrap at `Window.cpp:235` would have to be committed or argued about | clang-format's own pass rejoined it, so the code commit carries 6 call sites and 1 include and nothing else. Its tokens had already been proved identical to the index — formatting, not behaviour |
| `docs_coverage.py` would demand a page for the new API and accept an address line above it | It recognises no entry in `TestHelper.h` (`ENTRY_DECL` knows only class/struct/union/enum/using/define), so no page is demanded and no pointer is admissible. Documented anyway; the gap is recorded on the entry page and in the journal, next to the `EngineConfig` precedent |
| The Test module index was a clean source of chrome | Its legacy banner is lifted verbatim by `docs_page.py`, and only `href="../`-prefixed links are deepened, so a new page inherits twelve dead links. Generated with the banner lifted, then restored |

## Goal

`TaskSystem::DriveUntil` has 14 call sites and none of them is production code — 6 in
`Engine/OSAL/Window.cpp` (`hbe::WindowTest`) and 8 in `Engine/Core/TaskSystem.cpp`
(`hbe::TaskSystemTest`). Move the wait, and the timeout report it exists to serve, into a
`TestHelper` namespace so the shipped `TaskSystem` interface carries no member whose only
caller is a test.

## Confirmed decisions

| # | Decision | Choice |
| --- | --- | --- |
| 1 | Destination | `namespace hbe { namespace TestHelper { … } }` in a new `Engine/Test/TestHelper.h` — not `Examples/WindowExample/Main.cpp`, which is a separate executable holding no test code |
| 2 | `ReportDriveTimeout` | Moves too, into `TestHelper`, defined in a new `Engine/Test/TestHelper.cpp`; the `TaskSystem` member and its page are deleted |
| 3 | `WaitUntil` (`Engine/Test/TestCollection.h:32`) | **Out of scope.** Stays where it is. Optional follow-up: move it beside the new helper and update its 5 call sites in `Engine/Core/TaskSystem.cpp` |

## Preflight blocker — the index is not clean

`git status` shows work in flight that is not part of this task:

| State | Files | What it is |
| --- | --- | --- |
| Staged | `Engine/Core/TaskSystem.h` (+112/-84 region), `Engine/Core/TaskSystem.cpp` (+74) | The "declare in the header, define in the source" sweep: 14 bodies moved out of the header, including `GetBaseTaskStreamIndex`, `GetName`, `IsRunning`, `GetRegistry`, `HasStream`, `SetSuccessor`, `GetSuccessor`, `SetAbandonedNotice`, `GetNumBudgetWindowsAdvanced` |
| Unstaged | `AGENTS.md`, `.pi/skills/hb-standards/SKILL.md`, `docs/CodingStandards.md`, `Applications/EngineTest/TestMain.cpp`, `Engine/CodingStandards.{h,cpp}`, `Engine/OSAL/Window.cpp` | Standards-doc edits plus a formatting-only reformat of the `Window.cpp:235` lambda |
| Untracked | `.Plans/PLAN_engine_test_module.md`, `.Plans/PLAN_layout-and-format-exceptions.md`, `.Plans/fix-manifest.json` | Other sessions' artifacts |

The staged `TaskSystem` sweep touches the same header this task must edit. Proposal: build it, run
`check.sh`, and if green, commit it on its own first, so this change lands on a clean index and
each commit says one thing. Alternative if it is not green: leave it staged and commit only
`TestHelper` files, which forces a partial-index commit of `TaskSystem.h` — messy and error prone.

## Steps

| # | Step | Files | Verification |
| --- | --- | --- | --- |
| 0 | Settle the in-flight staged sweep (see Preflight) | — | `./build.sh Applications/EngineTest -dev -debug -release -test` then `.pi/skills/hb-standards/scripts/check.sh` |
| 1 | Baseline the gates before touching anything | — | `docs_coverage.py check-file Engine/Core/TaskSystem.h`, `layout.py`, `comments.py`, EngineTest run — record pass/fail so a later failure is attributable |
| 2 | Author `Engine/Test/TestHelper.h` | new | Copyright line 1, `#ifdef __UNIT_TEST__` guard, `namespace hbe { namespace TestHelper {`, template `DriveUntil(TaskSystem&, const char*, Predicate&&, ms = 30000ms) noexcept`, declaration of `ReportDriveTimeout(TaskSystem&, const char*, ms)` |
| 3 | Author `Engine/Test/TestHelper.cpp` | new | Body moved verbatim from `TaskSystem.cpp:181-193`, source name from `taskSystem.GetName()` |
| 4 | Add the source to the Test library | `Engine/Test/CMakeLists.txt` | Reconfigure finds no missing-source error |
| 5 | Delete the engine members | `Engine/Core/TaskSystem.h:91-138`, `Engine/Core/TaskSystem.cpp:181-193` | `grep -rn "DriveUntil\|ReportDriveTimeout" Engine/` finds no hit in `TaskSystem.*` |
| 6 | Rewrite the 6 window call sites | `Engine/OSAL/Window.cpp:123,235,348,461,566,671` | `TestHelper::DriveUntil(taskSystem, …)`; include `Test/TestHelper.h` in the file's `__UNIT_TEST__` region |
| 7 | Rewrite the 8 task-system call sites | `Engine/Core/TaskSystem.cpp:1248,1360,3401,3430,3472,3561,3627,3656` | Same form; `Test/TestHelper.h` included next to the existing `Test/TestCollection.h` at line 850 |
| 8 | Build all three configurations and run the suite | — | `./build.sh Applications/EngineTest -dev -debug -release -test`; `./build/Applications/EngineTest/{Debug,Dev,Release}/EngineTest` exits 0 and reports "all N collections passed" |
| 9 | Move the documentation | new `docs/Test/TestHelper/{index,drive-until,report-drive-timeout}.html`; delete `docs/Core/TaskSystem/{drive-until,report-drive-timeout}.html` | `docs_coverage.py` pairs the new header with the new page and no longer demands a `DriveUntil` page under `Core/TaskSystem` |
| 10 | Repair every reference to the deleted pages | the 46 files carrying the TaskSystem nav list; prose in `docs/Core/TaskSystem/{index,update,get-stream,get-stream-index,get-base-task-stream-index,dispatch-to-main-thread}.html`; `docs/TaskSystemGuide.md:74`; `docs/TaskSystemRedesign.md:719`; `docs/Test/index.html` gains the new module link | `htmlcheck.py` plus `grep -rn "drive-until.html\|report-drive-timeout.html" docs` returning only the new `docs/Test/TestHelper` paths |
| 11 | Standards gate and journal | `JOURNAL.md` | `.pi/skills/hb-standards/scripts/check.sh --test`, `layout.py`, `comments.py`, `docs_coverage.py`; one commit for the code, one for the docs |

## Behaviour that must not change

| Property | Why the move preserves it |
| --- | --- |
| The nested-pump permission is granted for the wait's duration and withdrawn on every exit path | `NestedPumpGuard` moves with the body; `TaskStream::Update`'s assert at `TaskStream.cpp:333` is untouched |
| The pump calls `TaskSystem::Update()`, not `TaskStream::Update()` | The helper holds a `TaskSystem&`, so the same call is made; the budget-window pass still runs |
| A caller that is not the base stream's thread sleep-polls and times out silently | Same branch, same predicate re-test; `baseStream.GetThreadID()` is public |
| Log line wording | Moved verbatim; the source string still comes from `taskSystem.GetName()` |
| Default patience 30 000 ms, 1 ms sleep per iteration, `remaining` decremented rather than measured | Copied, not rewritten |

## Risks

| Risk | Mitigation |
| --- | --- |
| Include cycle: `Core/TaskSystem.h:188` includes `Test/TestCollection.h`, and `TestHelper.h` includes `Core/TaskSystem.h` | `TestHelper.h` is never included from `TestCollection.h`; each test region includes it directly, the pattern `Engine/OSAL/OSInputOutput.cpp:11` already uses |
| The engine loses the one place a pump is legitimately re-entered, a guardrail commit `ba5f3a7` claims | `TaskStream::SetNestedPumpAllowed` is already public (`TaskStream.h:382`), so no new access opens. Recording this in the module design note is part of step 10 |
| A test-only log line now comes from a target compiled without `__UNIT_TEST__`... | It does not: `TestHelper.cpp` is added to the `Test` static library, which sets `__UNIT_TEST__` for itself, and the whole `-test` configure adds the flag tree-wide |
| Partial commit of in-flight work | Step 0 clears the index before step 5 edits the same header |

## Out of scope

- Moving `hbe::WaitUntil` (decision 3).
- Bounding the six `Window.cpp` calls' 30 s default patience.
- Relocating `hbe::WindowTest` out of the engine library.
- Adding a caller that exercises the foreign-thread branch.
- Teaching `docs_coverage.py` to treat a namespace of free functions as a documentable entry.
