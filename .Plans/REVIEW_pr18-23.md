# Review record — PR #18…#23 (the regrouped `Engine/Memory` stack)

Date: 2026-10-12. Method: `hb-standards` skill (deterministic layers per commit, then two independent
read-only judgement passes, citations machine-checked) plus `hb-docs` gates for the reference pages.
**Outcome: 55 findings delivered as 35 inline review comments across the six PRs; no commit, branch or
PR content was changed.** Form of delivery is documented in section 4 — the first pass posted six long
body comments and was replaced by these.

## 1. Gate layers, run per commit (`check.sh <rev> --no-build`, 12 runs)

| Rev | clang-format | greps | blank lines / includes | layout | docs coverage |
|---|---|---|---|---|---|
| `8bdfbb6` | PASS | 0 | 0 / 0 | 4 viol (backlog) | 22 pages missing¹ |
| `a8b4226` | PASS | 0 | 0 / 0 | — | 22¹ |
| `ca6ab76` | PASS | 0 | 0 / 0 | 0 | 22¹ |
| `7763b02` | **FAIL 2 files** | 0 | 0 / 0 | 10 viol | 22¹ |
| `532c669` | **FAIL 1 file** | 0 | 0 / 0 | 10 viol | 22¹ |
| `23a2d9c` | PASS | 0 | 0 / 0 | 5 viol | 22¹ |
| `7e08cc9` | **FAIL 104 files** | 1 (OSMemory.cpp comment) | **2 findings** | 117 viol | 64 missing, 32 unpointed |
| `f86f49e` | **FAIL 1 file** | 0 | 0 / 0 | 16 viol | 22¹ |
| `5c4affc` | **FAIL 1 file** | 0 | 0 / 0 | 16 viol | 22¹ |
| `9d776ec` | **FAIL 1 file** | 0 | 0 / 0 | 16 viol | **0 missing**, 17 unpointed² |
| `d5e6cf7` | **FAIL 5 files** | 0 | 0 / 0 | 11 viol | **0 / 0 / 0** ✓ |
| `c399742` (head) | **FAIL 5 files** | 0 | 0 / 0 | 11 viol | **0 / 0 / 0** ✓ |

¹ Deferred by design to `9d776ec` (PR #21). ² Correct: the header pointers arrive with the strip in PR #22.

Docs gates at the head: `docs_coverage.py check Memory` 0/0/0 · `docs_methods.py Memory` 0 ·
`htmlcheck.py docs/Memory` 0 problems · `quoted_blocks.py docs/Memory` 0 stale of 544.

## 2. The one systemic gate defect

`7e08cc9` (PR #19) wrote the closing-guard label as `#endif //TEST_ENABLED`; the repo's clang-format
rejects a comment with no space after `//`, so layer 1 fails **104 files** at that commit and **97 files
still carry it at the head**. It survives because the gate is scoped to the files each commit touched —
no later commit touches them, so no later gate sees it. One space fixes 96 of 97 files; the 97th
(`PoolAllocator.cpp`) needs one continuation-line indent as well.

Reproduced with the gate's own comparison. Two of my own probes were wrong before that: piping
`clang-format` on **stdin** loses the main-header identity (≈140 phantom offenders), and a **raw** diff
counts rule A3's sanctioned double blank — the gate compares through `blank_lines.py --collapse-seam`.

## 3. Judgement review

Two read-only passes, separate contexts, no edits: **pair-1** (pool family, 8 files), **pair-2** (rest of
the module, 32 files), plus my own pass over the changed code (10 files). 57 inputs → 56 unique from
`review_merge.py`, 55 after folding a row my two passes had split across two rule names; confidence
23 high / 27 medium / 5 low, 1 corroborated by more than one track.
Every citation resolved by `verify-findings.py`; attribution to a PR by `git blame` at the stack head.

**Introduced by the stack (4 rows, PR #18 / #19 / #20):** the `#endif //TEST_ENABLED` sweep,
`PoolAllocator.cpp`'s over-indented assert continuation (still FAIL at head), a provably dead
`if (likely(ptr != nullptr))` added in `PoolAllocator::Deallocate`, and the thread-safe bank path
asserting with no diagnostics while its single-threaded twin got a six-argument message from the same PR.

**Pre-existing, in code this stack touches or documents (top of 32):** `Shareable::operator=` falls off
the end without `return *this` and releases before inspecting `rhs`; `Optional`'s copy constructor reads
`hasValue` before initialising it and `CopyValue(False_t)` assigns into never-constructed storage;
`PoolAllocator`'s move constructor refreshes the proxy's function pointers but not `proxy.allocator`,
which `std::sort` of the bank vector makes live; `ThreadSafeMultiPoolAllocator::NewBankAllocate` grows
and re-sorts `banks` holding no lock; bank growth credits `stats.capacity` without crediting the usage
record deregistration debits it from; `MEMORY_VERIFICATION_ENABLED` builds cannot compile
(`std::memset(buffer, …)` names a member that does not exist); `ReportDeallocation` wraps `size_t` after
logging an underflow instead of returning.

**Dropped rather than posted:** one pass reported "a cache file loads unsorted, so every look-up misses
silently". `Serialize` calls `Normalize()`, which sorts, so a file this engine wrote is sorted; reduced
to a narrow robustness gap in the loader, medium confidence.

## 4. Comment form: what GitHub will and will not anchor

The six initial comments were one 20 KB body per PR, listing `file:line` in prose. They were deleted and
replaced by inline comments, which exposed a hard platform rule:

> A review comment can only be pinned to a line the PR's diff shows. `POST /pulls/{pr}/comments` with a
> `commit_id` and an off-diff line returns **422 `pull_request_review_thread.line could not be resolved`**
> — verified directly, not inferred.

Only **6 of 55** findings sit on a line any PR in the stack adds; the other 49 are pre-existing code no
diff touches. Policy applied, in order, with the machine-readable plan in `.Plans/review/anchor2.py`:

| Case | Rule | Count |
|---|---|---|
| Line in the diff | pinned exactly | 6 |
| Nearest shown line ≤ 25 lines away, same file | pinned there, body states the distance and the true defect line | 18 |
| Introduced finding | forced onto the PR that wrote it, however far, always labelled | 3 |
| Line nowhere in the diff, file shown by a PR | **file-level** comment (`subject_type: file`) | 11 |
| File in no PR's diff | named in that PR's review body | 2 |

Per PR: **#18** 12 line + 5 file · **#19** 6 + 3 · **#20** 3 + 1 · **#21** body only (adds no C++ line) ·
**#22** 3 + 2 · **#23** body only.

Two blemishes worth knowing about: `subject_type: file` is rejected *inside* a review payload, so
file-level comments go in as separate calls (the script's fallback); and two probe reviews on #21 made to
learn the pinning rule cannot be deleted — GitHub deletes only *pending* reviews — so their bodies were
rewritten to say they were probes.

## 5. Build evidence, and what it does and does not cover

Three-configuration builds and `EngineTest` are **inherited, not re-run**: each PR head SHA cited above is
the same SHA that was built green in Debug/Dev/Release (393 testlets) during the regroup, so the claim is
evidenced by an earlier run of identical trees, not by a fresh compile in this session. Any fix applied
later must re-run the gate at every commit it rewrites.

## 6. Deliberately not done

*Superseded for PR #18 on 2026-10-12: the review was answered and the branches were force-pushed. See §8.*

* **No fixes applied, nothing pushed** — user's choice. All six branches are byte-identical to their
  remotes; `JOURNAL.md` was left alone because it lives inside PR #23's diff.
* **Not covered:** the prose contracts of PR #21's 242 pages, page by page. Structure, signatures and
  quoted blocks are proven by the gates; truth of the English is not. PR #21's comment names three
  places where a clean-sounding page would be documenting behaviour the code cannot keep.
* Nine parallel review passes were killed by the runtime (one subagent at a time); breadth was
  recovered by running passes sequentially. Two sequential passes completed.

## 7. To continue

Apply tiers T1/T2 from the review (gate failures, then the cheap rule conformance) as `--fixup` commits
against `7e08cc9`, `7763b02` and `f86f49e`, autosquash, then re-run `check.sh` at every commit plus the
three-configuration build and `EngineTest`, and force-push the affected branch plus its four descendants
in the stack. The two behaviour changes (stats-lock capacity accounting; locking around `GenerateBank`)
belong in a separate PR with a stress test. Machine-readable finding set:
`.Plans/review/regroup/merged.json`; pass inputs and renderer beside it.

## 8. Resolution of the PR #18 review (2026-10-12)

The reviewer answered 15 of the 17 comment threads on #18 and directed a fix in each. Fourteen were applied;
one was refused because the finding behind it was wrong. Everything landed as one commit,
`70ead88 fix(memory): the review findings on the allocator family`, on `fix/memory-allocator-defects`; the
five descendant branches were rebased onto it and force-pushed, so each PR still shows only its own diff.
Pre-fix state: tag `backup/pre-review-fixes` (= `c399742`).

| thread (as reviewed) | directive | landed at |
| --- | --- | --- |
| `AllocatorScope.h:27` | add `explicit` | `:27`, `:30` |
| `MemoryManager.h:188` | `Assert`, failure is unrecoverable | `:189`, `:200` (header now includes `Core/Debug.h`) |
| `MultiPoolAllocator.h` ×3 | the trivial set | `:5` include, `:40`/`:42` `inName`, `:34` seam |
| `PoolAllocator.cpp:123` | carry the reviewer's own change | `:126`, as written |
| `PoolAllocator.cpp:268` | fix by hand | `:272` re-indented by hand, no `--apply` |
| `ThreadSafeMultiPoolAllocator.cpp:30` | drop `noexcept` | `:30`, `:49` |
| `ThreadSafeMultiPoolAllocator.cpp:245` | log before failing | `:250` Warning with size + name |
| `ThreadSafeMultiPoolAllocator.cpp:416` | add a minimized lock guard | **refuted** — see below |
| `ThreadSafeMultiPoolAllocator.cpp:420` | `FatalAssert` message | `:428` |
| `ThreadSafeMultiPoolAllocator.cpp:587` | add the `static_assert` | `:599`, `<numeric>` at `:580` |
| `ThreadSafeMultiPoolAllocator.h:40` | `inName` | `:40`, `:42` |
| `MemoryManager.h:192` | early return | `:211`, `:223` |
| `MultiPoolAllocator.cpp:170` | add a log | `:178` |
| `MemoryManager.cpp` ×6 | fix all | `:175`, `:434`, `:605`, `:646`, `:716`, `:911` |
| `PoolAllocator.cpp:41` | `Assert`; buffer is never null in life | `:47` |
| `ThreadSafeMultiPoolAllocator.cpp:509`/`:521` | fix as suggested | clamped in both initializer lists `:77`/`:115`; `MegaBytes` `:529`, `MultiPoolAllocator.cpp:446` |

### The refuted finding, and why it mattered

The comment claimed `NewBankAllocate` grew and re-sorted `banks` holding no lock. It does hold one:
`Allocate` opens `std::lock_guard lockGuard(lock)` at `ThreadSafeMultiPoolAllocator.cpp:210` and reaches
`return NewBankAllocate(requested)` at `:215` and `:221` from inside that scope, and `Deallocate` holds the
same guard from `:263`. `lock` is a plain `std::mutex`, so the guard added on the strength of that comment
would have self-deadlocked on the first bank growth. The fix that was applied instead is the one genuinely
unlocked read the sweep found: `ReportConfiguration` reserved `configs` from `banks.size()` before its guard
opened, and now reserves inside it (`:391`-`:393`). The invariant `NewBankAllocate` relies on — *caller holds
`lock`* — is still undocumented, and #21's page is where it belongs.

### Evidence at `70ead88`

| gate | result |
| --- | --- |
| clang-format, seam-aware, 8 changed files | residue is `#endif //TEST_ENABLED` only — PR #19's defect, one per swept file |
| mechanical greps, rule set A, rule set B, comment ban | 0 findings |
| member layout | 14 findings, identical to the same file set at the pre-fix tip `c399742` — backlog, not regression |
| docs coverage / method pages / html | 0 missing, 0 problems |
| EngineTest, Debug · Dev · Release | all 60 collections, 393 testlets, each configuration |
| `VulkanExample` + `EngineTest`, Debug · Dev · Release | link clean (`MarchingCubes` is not in the build graph) |
| content gate, old tip → new tip | the eight files of `70ead88`, nothing else |

### Left where it was

* `StackAllocator.cpp:38` and `:91` drew no directive, so both stay; the fixes are in their thread bodies.
* `MultiPoolAllocator.cpp:344`-`:345` carried the same assert-continuation defect as `PoolAllocator.cpp:268`
  and was fixed unasked, because it is the same line of the same rule in a file the commit already touched.
* PR #19's `#endif //TEST_ENABLED` in 97 files is untouched: the format layer will keep reporting two lines
  per swept file until that rename is corrected.
* No ThreadSanitizer run. The concurrency surface this commit touches is `ReportConfiguration`'s reserve and
  the clamped constructor; `build-tsan/` exists, so a TSAN pass over the M1-M4 contention testlets is the
  missing proof.
