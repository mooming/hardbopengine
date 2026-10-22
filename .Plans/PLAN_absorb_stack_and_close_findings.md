# PLAN: absorb the PR stack into #18 and close every open finding

Owner instruction (2026-10-21): 흡수 후 미수정 문제 전부 이 PR에서 수정. Push는 아직 승인 전.

## Phase A — absorb the stack

Merge `origin/docs/memory-investigation-records` (PR #23 tip; carries #20 `fix/memory-allocation-ledger`,
#21 `docs/memory-api-reference-pages`, #22 `refactor/memory-comment-strip`, #23 journal/plans) into
`fix/memory-allocator-defects`. The TEST_ENABLED rename exists on both sides with identical content, so it
auto-merges; hand-resolve `Engine/Memory/*` and `JOURNAL.md`.

Rules of resolution: the incoming side wins where it is the more advanced version of the same file
(prose-stripped headers, restructured `InlinePoolAllocator`, new docs pages); this branch's semantic
additions are re-applied on top (feature-macro layer 5b, the two refusals and their testlets, the `#endif`
sweep, the journal entries). After the merge, re-verify by measurement, not by assumption:

| Check | Expected |
| --- | --- |
| `InlinePoolAllocator.h` verification memset | names `block`, not `buffer` (the incoming side still has the bug) |
| `check.sh` layer 5b present | runs, 0 failing |
| The two refusal testlets present | `Zero Blocks Refuses…`, `Zero Minimum Block Size Is Clamped` |
| `#endif //TEST_ENABLED` | 0 files |
| Dev/Debug/Release build + `EngineTest` | green |

## Phase B — close the findings nothing had fixed

| Group | Items | Proof |
| --- | --- | --- |
| B1 `PoolAllocator` | O8 self-move guard, O24 clamp-vs-assert contradiction, O26 unconditional proxy repoint, O27 interior-pointer/double-free + unreachable guard (`>` → `>=`), O3 release-safe null-buffer handling | testlet per item, red before the fix where the defect is observable |
| B2 `MemoryManager` | O3 `New`/`NewArray` release-safe guard, O12 leak report visible outside `Assert`, O13 `FatalAssert`-in-Release policy recorded in the design record | suite green in all three configs; policy row in `docs/` |
| B3 multi-pools | O9 delete copy + anonymous namespace for the two `.cpp`-scope types, O11 tautological `ReportConfiguration` assert, O18 bank-generation log records every bank, O22 cold allocate path out of the lock, O23 capacity accounting: aligned stride, `statsLock`, grow `totalCapacity` | suite green; M4 p99 re-measured for O22 |
| B4 `AllocatorScope` | O17 constrain the converting constructor template | compiles, suite green |
| B5 tests | O6 destination observation in `Pool Buffer Returns To Its Recorded Parent`, O7 the three `Destroy In Own Scope` copies made destination-aware and de-duplicated | each red against a one-line revert |
| B6 docs/gates | O14 wire `quoted_blocks.py` as reported-not-gated in layer 5 like the other sweeps, and record the backlog it reports | `check.sh --all --no-build` shows it running |

Out of scope unless the owner asks: `Engine/OSAL/{Linux,OSX}*.cpp` format and include debt — pre-existing
master debt, in no PR of this stack.

## Phase C — gates and record

`check.sh --all --no-build` with no new findings over the pre-existing set, three-configuration build plus
`EngineTest`, `JOURNAL.md` entry, then report. No push without explicit approval.

## Status (2026-10-21)

- Phase A done at `5e41ae4` (merge), 66 collections / 426 testlets in all three configurations.
- Phase B done at `d7a8fad` and `739e97e`: O3, O8, O11, O12, O17, O23, O24, O26, O27 closed with testlets proved red;
  O9 and O25 were already closed by the absorbed `#20`; O6/O7 closed by the parent-forwarding testlet.
- O22 and the `M1` timeout closed as *not defects*, with reasons in `JOURNAL.md`.
- Phase C: three-configuration build plus `EngineTest` green (429 testlets), `htmlcheck` 904/0,
  `docs_methods Memory` and `docs_coverage check Memory` clean, feature-macro layer 141/141.
- Open: the OSAL format/include backlog stays outside this PR; nothing pushed without owner approval.
