# Branch code review — `fix/memory-allocator-defects` at `213e7e8`

Reviewed 2026-10-20 by the session pair, at the owner's request: *"review the current code changes
on this branch, then list any issues or any items the commits fix."* Findings only — no engine file
was edited. This review is additive to `.Plans/review-memory-allocator/MEMORY_ALLOCATOR_REVIEW.md`
(the 27-finding judgement review): every row below is **RE-VERIFIED** (re-measured here), **NEW**
(not in that document), or an earlier row **UPGRADED** with a measurement of its own.

## 1. Scope

| Item | Value |
| --- | --- |
| Range | `master..HEAD` = 5 commits, 241 files, +2 845 / −667 |
| `34a0410` | `docs(skills)`: hb-docs guide rows for the new checkers |
| `626249e` | `fix(memory)`: 20 files — the allocator fixes + `quoted_blocks.py` (625 new lines) + `comments.py` POINTER-PATH gate |
| `9fa128f` | `docs`: the one test macro reaches README/AGENTS/pages/guides |
| `12a8715` | `build`: `TEST_ENABLED` replaces `__TEST__` + `__UNIT_TEST__`, 200 files (mechanical) |
| `213e7e8` | `review(memory)`: the 27-finding review document + its JSON, no code |
| Real code under review | `Engine/Memory/*`, `Engine/Core/{ScopedLock.h,SystemStatistics.cpp,TaskSystem.cpp}`, the build macro rename, two checker scripts |

## 2. Verification actually run here

| # | Check | Command | Result |
| --- | --- | --- | --- |
| V1 | Build, Dev + tests | `./build.sh Applications/EngineTest -dev -test` | ✅ 154/154, `EngineTest` links (only a duplicate-`lib` linker warning) |
| V2 | Suite, Dev | `./build/Applications/EngineTest/Dev/EngineTest` | ✅ exit 0, *"all 66 collections passed (423 testlets)"* |
| V3 | Feature-macro syntax layer | `MEMORY_VERIFICATION_ENABLED=1`, `PROFILE_ENABLED=1`, `-fsyntax-only` over 17 Memory TUs, in a `git worktree` at `/tmp/hbwt` so the checkout stayed clean | ❌ **12 of 17 fail**, one root cause: `InlinePoolAllocator.h:91` `std::memset(buffer, …)` — the member is `block` |
| V4 | Comment ban | `comments.py` on the three allocator headers | ❌ 3 × `POINTER-PATH`, **exit 1** |
| V5 | Page existence | `ls docs/Memory` | ❌ only `AlignedStorage/` + `index.html` — `PoolAllocator`, `MultiPoolAllocator`, `ThreadSafeMultiPoolAllocator`, `MemoryManager`, `StackAllocator`, `MonotonicAllocator`, `AllocatorScope` all absent |
| V6 | Formatter | `clang-format --style=file` on `Engine/Container/Array.h` | ❌ wants `#endif // TEST_ENABLED`; 104 files carry `#endif //TEST_ENABLED`. **Measured at `master` too:** `#endif //__UNIT_TEST__` is format-clean, `//TEST_ENABLED` is not → `12a8715` introduced it |
| V7 | New checker wired? | `grep -rn quoted_blocks .pi docs Scripts` | ❌ referenced only by `.pi/skills/hb-docs/SKILL.md`; `check.sh` calls `blank_lines`, `includes`, `comments`, `layout`, `docs_coverage`, `docs_methods`, `htmlcheck`, `autofix`, `prove_*` — never `quoted_blocks` |
| V8 | New checker by hand | `quoted_blocks.py` over all of `docs/` | ❌ `1386 blocks in 664 pages, 46 verbatim, **52 stale, 6 unresolved**, 1280 skipped … exit 1` |
| V9 | Copyability of the id-owning allocator | `static_assert` probe, same flags, in the worktree | ❌ `is_copy_constructible_v<MultiPoolAllocator>` **is true**, `is_copy_assignable_v` **is true**; an actual copy fails deep inside `allocator_traits::__construct_at` |
| V10 | Work policy | `head -20 JOURNAL.md`, `ls .Plans` | ❌ newest journal entry is *2026-10-19 (evening)*, the four code/doc commits are 10-19 23:39 → 10-20 12:11; no `PLAN_*` for the allocator work |
| V11 | Self-move, `PoolAllocator::operator=` | read `PoolAllocator.cpp:157-160` + `~PoolAllocator` | ❌ see N3 |
| V12 | Testlet can fail? | read `ThreadSafeMultiPoolAllocator.cpp:1344-1366` | ❌ vacuous, see O6 |

## 3. Items the commits genuinely fix (each re-checked in the code, not in the message)

| Fix | Where | Proof it is complete |
| --- | --- | --- |
| Free-list terminator off by one (`numberOfBlocks - 1` → `numberOfBlocks`) | `PoolAllocator.cpp` ctor `SetAs(cursor, i + 1)`, `Deallocate` writes `numberOfBlocks`, `ReadNextIndex`/`WriteNextIndex` assert `<= numberOfBlocks`, `AllocateBlock` ends the walk at `index >= numberOfBlocks` | Producer and all four consumers now agree on sentinel `== numberOfBlocks`; the last block is no longer handed out twice |
| Backing buffer returned to the wrong allocator | `PoolAllocator`/`StackAllocator`/`MonotonicAllocator` free to the recorded `parentID`; `MultiPoolAllocator`/`ThreadSafeMultiPoolAllocator` open `AllocatorScope scope(parentID)` for the whole destructor | `~PoolAllocator` → `mmgr.Deallocate(parentID, buffer, totalSize)` before `DeregisterAllocator`; the scope in `~ThreadSafeMultiPoolAllocator` covers the vector's own frees |
| Logging inside the pool's non-recursive `std::mutex` | `ThreadSafeMultiPoolAllocator::Deallocate` (fatal path now after the lock), `PrintUsage` (snapshot into `bankUsages` under the lock, log after), `GenerateBank` → `BankGenerationLog` thread-local record logged at `Allocate` exit | No `Logger::Get`/`log.Out` call remains inside a `lock_guard` region in that file |
| Move ctor left the registry pointing at the moved-from object | `PoolAllocator` move ctor repoints `mmgr.GetAllocatorProxy(GetID()).allocator = this` | Repoints `allocator`, `allocate`, `deallocate` (and `stats.capacity` under profiling) |
| Counter underflow path fell through | `MemoryManager::ReportDeallocation` gained `return` after the `usage < allocated` error branch | Confirmed against `ReportAllocation`, which does `usage += allocated` under `statsLock` |
| Failed allocation silently used | `MemoryManager::Allocate`/`Deallocate` return early when the proxy's function pointer is null | `return nullptr` / `return`, assert message kept |
| `#ifdef PROFILE_ENABLED` always true | `NewBankAllocate` now `#if PROFILE_ENABLED`; `GetAllocatorStat` guarded by `#if`, not `#ifdef` | `PROFILE_ENABLED` is always *defined* (`BuildConfig.h:50`), so `#ifdef` was a permanent true |
| An assert that named nothing | `NewBankAllocate`'s `FatalAssert(false)` gained the module's usual `"[" << GetName() << "::" << __func__ << …"` message; `SetScopedAllocatorID` logs `prevID = scopedAllocatorID` instead of printing `id` twice | The old message read `Previous ID = X, New ID = X` |
| `configs.end()` dereference | missing `else` added in **both** `ReportConfiguration` bodies | `InsertItem` now branches before `found->numberOfBlocks +=` |
| `configs.reserve` / bank walk outside the lock | `ReportConfiguration` does `reserve` and the walk inside `lock_guard` | bank count cannot change under the walk |
| Spelling + dead macro plumbing | `ReportMultiPoolConfigutation` → `…Configuration` (4 sites, 0 stale callers), `hbe::source_location` → `SourceLocation`, `megaBytes` → `MegaBytes`, `explicit` on the `AllocatorScope` ctors, member block above the functions per the twelve-block rule | `grep -rn ReportMultiPoolConfig` returns only the new spelling |
| One test macro instead of two | `.project.config`, all `CMakeLists.txt`, `build.sh`, `.module.config`, ~200 sources, README/AGENTS/pages | `git grep '__TEST__\|__UNIT_TEST__'` hits only historical records (`JOURNAL.md`, `.Plans`, docs prose, `.pi/logs`) |

## 4. Open issues

Severity = impact. **NEW** marks a row the earlier 27-finding review does not contain.

| # | Sev | Where | Defect | Evidence / truth test |
| --- | --- | --- | --- | --- |
| O1 | high → **closed here 2026-10-21** | `InlinePoolAllocator.h:91` | `std::memset(buffer, 0, length)` names a member that does not exist | V3: 12/17 Memory TUs stop compiling with the feature macros on. Closed by `buffer` → `block` on this branch, together with gate layer 5b (§7): 17/17 Memory TUs and 144/144 configured TUs now compile with both macros on |
| O2 | high | `PoolAllocator.h:13`, `MultiPoolAllocator.h:18`, `ThreadSafeMultiPoolAllocator.h:17` | API-reference pointers installed at pages that do not exist | V4/V5: `comments.py` exit 1, 3 × `POINTER-PATH`; the whole Memory module has 1 page today |
| O3 | high | `MemoryManager.h` `New`/`NewArray`; `PoolAllocator.cpp:48-56` | assert-then-use: `Assert` **is a function** and compiles to a no-op in Release (`Debug.h:59-67`), so a null allocation is still dereferenced — `availables = &buffer[0]` and the free-list build write through null | Read `Debug.h`; `MemoryManager::Allocate` can hand back `nullptr` on the very path this commit added |
| O4 | **NEW** high | `InlinePoolAllocator.h:198-215` | The guard that sends a foreign (fallback) pointer back to its parent is compiled **only** under `#if PROFILE_ENABLED`. With profiling off — every shipped configuration — `DeallocateBytes` caches that parent-owned block as `immediateBlock` and returns, so the pre-existing `if (!IsValidPointer(ptr))` below it is unreachable in exactly that case | Read of the full function: parent `usage` never drops, the pool's free cache gains a block it does not own, and it is leaked at destruction. This is the same defect class the commit set out to close |
| O5 | **NEW** medium | `InlinePoolAllocator.h:139-143` | `deallocate(T* ptr, size_t n)` calls `DeallocateBytes(ptr, n)` while `allocate(n)` uses `n * sizeof(T)` | Mixed units feed the `nBytes <= BlockSizeInBytes` test and the commit's new `ReportDeallocation(…, nBytes, …)`; `stats.totalRequested` drifts, and O4 becomes reachable for every `n < BlockSizeInBytes` |
| O6 | high | `ThreadSafeMultiPoolAllocator.cpp:1344` | *"Pool Buffer Returns To Its Recorded Parent"* never observes the destination allocator — it asserts only that an allocation happened, then prints its own success line | V12 read: with `pool.reset()` under `unrelatedScope`, a buffer freed to `unrelated` still passes. **RE-VERIFIED** prior #6 |
| O7 | **NEW** medium | `MonotonicAllocator.cpp`, `StackAllocator.cpp`, `InlineMonotonicAllocator.cpp`, each `"Destroy In Own Scope"` | All three measure *how much* was freed (`GetGlobalAllocationBytes`/`GetGlobalFreeBytes` are cumulative `fetch_add` counters, `GlobalAllocation.cpp:241`), never *to whom*; the InlineMonotonic copy cannot fail at all because that class owns no heap buffer — its own message says so | Pre-fix paths still free the same byte count, so the delta stays balanced; only a free that never happens at all would be caught |
| O8 | **NEW** medium | `PoolAllocator.cpp:157-160` | `operator=(PoolAllocator&&)` is `this->~PoolAllocator(); new (this) PoolAllocator(std::move(rhs));` with no self-move guard. On self-assignment the destructor nulls `id`/`buffer` first, so the placement-new rebuilds an empty allocator and its own `Assert(id != InvalidAllocatorID)` fires | V11 |
| O9 | **NEW** medium | `ThreadSafeMultiPoolAllocator.cpp` (namespace scope) + `PoolAllocator.h`/`MultiPoolAllocator.h` | `BankGenerationLog` and `BankUsageRecord` are namespace-scope types in a `.cpp` with no anonymous namespace (external linkage, ODR hazard); separately, V9 measures `MultiPoolAllocator` — the class that owns a registered id per bank — as **copy-constructible and copy-assignable today**, its copy failing only deep inside `allocator_traits`. Both are "the type does not forbid what it must forbid" | V9 probe output; `class BankGenerationLog final` at `.cpp` namespace scope |
| O10 | medium | `PoolAllocator.cpp:184-190` | `Allocate` reports a **failed** allocation (`ptr == nullptr` from `AllocateBlock`) to the profiler, and `ReportAllocation` does `usage += allocated` unconditionally | The inflation this commit's new `return` in `ReportDeallocation` was written to survive. **RE-VERIFIED** prior #10 |
| O11 | medium | both `ReportConfiguration` bodies | `Assert(reportedBlocks == peakBlocks)` is true by construction — both sides sum `GetUsedBlocksMax()` per bank — so it cannot fail and its message over-claims | Read of `InsertItem`: `found->numberOfBlocks += GetUsedBlocksMax()` vs `peakBlocks += GetUsedBlocksMax()`. **RE-VERIFIED** prior #11 |
| O12 | medium | `MemoryManager::DeregisterAllocator` | The leak report went from a Warning in every build to `Assert(stats.usage == 0, …)`, i.e. silent in Release, and profiling is off in all three configurations anyway | **RE-VERIFIED** prior #20 |
| O13 | medium | `MemoryManager::GetAllocatorProxy` | `FatalAssert` is *not* gated by `RELEASE_BUILD` (`Debug.h:72-99`), so an invalid id that used to log and return a dummy is now fatal in every configuration, Release included | **RE-VERIFIED** prior #5 — the `static AllocatorProxy dummy{}` return is the house convention and stays |
| O14 | high (process) | `quoted_blocks.py` | 625 new lines wired to no gate (V7), and by its own hand the tree it would police already carries 52 stale + 6 unresolved blocks (V8) | A checker outside the gate blocks nothing; landing it "green by absence of run" is worse than not landing it |
| O15 | high (process) | `JOURNAL.md`, `.Plans/` | No journal entry and no `PLAN_*` for `626249e`/`12a8715`/`9fa128f`/`34a0410` — work policy 2, 3, 10 | V10. **RE-VERIFIED** prior #8 |
| O16 | medium | 104 files | `#endif //TEST_ENABLED` — clang-format wants the space, and the `master` spelling was clean (V6), so the branch is gate-dirty at layer 1 | Also `Engine/Container/Array.h` shows clang-format and rule A1 disagreeing about the double blank after the includes; that one is pre-existing and by design |
| O17 | low | `AllocatorScope.h` constructor template | Still unconstrained after the siblings gained `explicit`: any type with a `GetID()` converts | **RE-VERIFIED** prior #23 |
| O18 | **NEW** low | `GenerateBank` / `BankGenerationLog` | `RecordBankGeneration` overwrites, so if one `Allocate` ever generates two banks the first bank's Verbose line is silently lost; the invariant "call `GenerateBank` holding `lock`, at most once per `Allocate`" is asserted nowhere | **RE-VERIFIED** in spirit from prior #14 |
| O19 | **NEW** high (process) | this review's own tooling | The subagent's **interim** list carried two fabricated citations: `Engine/Container/AtomicFixedRingMPMC.h:257-265` (a 234-line file this branch never touches — `git show 626249e --name-only` lists 0 Container files) and a symbol `IsPoolObject` that exists nowhere in `Engine/`. Both rows were dropped. Its **final** list dropped them too, and its six claims re-checked here all held (O20, O21, O22, O24, O25, O26) | An agent-authored finding list must pass a citation verifier (`verify-findings.py`) before it reaches the owner — an interim list without one is not evidence |
| O20 | **NEW** high | `PoolAllocator.cpp:19-29`, `:40-43` | `availables` is **absent from the constructor's initializer list**, and the `totalSize <= 0` early return leaves it unwritten — an indeterminate pointer that `AllocateBlock` later reads through (`if (!availables)` at `:296`, then `ReadNextIndex`). `PoolAllocator p("p", 64, 0)` is enough | Init list at `:19-29` names `id, parentID, name, blockSize, numberOfBlocks, numberOfFreeBlocks, buffer` (+ profiling pair); first write of `availables` is `:50`. The guard is pre-existing (`744ec75`), so this is exposed debt, not branch debt. Fix: `, availables(nullptr)` in the init list, plus a `numberOfBlocks > 0` precondition next to the `inBlockSize` assert |
| O21 | **NEW** medium | `MultiPoolAllocator.cpp:25`, `:63` vs `ThreadSafeMultiPoolAllocator.cpp:78`, `:116` | Half-applied fix: the `bankSize`/`minBlock` `std::max(…, sizeof(size_t))` clamp landed in the thread-safe sibling only. `MultiPoolAllocator` still divides by `minBlock` at `:434` (`(requested + minBlock - 1) / minBlock`), so `minBlockSize == 0` is SIGFPE there and the two siblings now disagree on identical input | Both ctors read side by side; `grep -n minBlock` shows the clamp on 2 of 4 constructor sites |
| O22 | **NEW** medium | `ThreadSafeMultiPoolAllocator.cpp:216`, `:221` | `return NewBankAllocate(requested);` runs **inside** `std::lock_guard lockGuard(lock)`. `NewBankAllocate` bumps engine statistics, calls `GenerateBank` (which registers an allocator and logs at Info) and `bank.Allocate` (which `LogError`s on an empty bank) — so the commit's own "no logging or callback under the non-recursive mutex" rule is unapplied on the cold allocate path, and it is the mechanism behind the M4 tail the branch itself measured (p99 24 875 ns vs 42 ns typical) | Read of `Allocate` at HEAD; compare the now-clean `Deallocate`/`PrintUsage` shapes in the same file |
| O23 | **NEW** medium (latent: profiling config) | `ThreadSafeMultiPoolAllocator.cpp:563-566`, `MultiPoolAllocator.cpp:478-481` | The bank-growth path adds the **pre-alignment** stride (`blockSize * numberOfBlocks`) to the parent `stats.capacity`, while the bank itself registers the aligned stride (`PoolAllocator::GetCapacity`), so the parent under-counts and `ReportAllocation`'s `usage > capacity` FatalError becomes reachable on a healthy allocator. That `capacity +=` is guarded by the bank `lock`, not `statsLock` like every other `AllocStats` write, and `usage.totalCapacity` is never grown for generated banks | `#if PROFILE_ENABLED` hunk in `GenerateBank`; `statsLock` is used by `ReportAllocation`/`ReportDeallocation`/`GetAllocatorStat`. Inert today because `PROFILE_ENABLED` is `0` |
| O24 | **NEW** medium | `PoolAllocator.cpp:22` vs `:36` | The init list clamps with `std::max(inBlockSize, sizeof(TSize))` while `Assert(inBlockSize >= sizeof(TSize))` aborts in Debug/Dev on the very input the line above proves is handled; the comment above the assert even says it is "a precondition, not something to clamp away" — and the code clamps it | Both lines read at HEAD |
| O25 | **NEW** low | `ThreadSafeMultiPoolAllocator.cpp:245-253`, `MultiPoolAllocator.cpp:173-181` | The new `Deallocate(nullptr, size != 0)` warning is followed by `Assert(size == 0)`, which aborts in Debug/Dev — so the warning can never be seen *and survived* there, and in Release both halves are inert | Read of the new hunk |
| O26 | **NEW** low | `PoolAllocator.cpp:119-126` + `MemoryManager.cpp:175` | The move ctor's new `allocProxy.allocator = this` is unconditional, so moving a **moved-from** pool calls `GetAllocatorProxy(InvalidAllocatorID)`, which the same commit turned into a `FatalAssert` → abort in every configuration, where it used to be a silent write into the shared dummy proxy. Two of this commit's own changes compose into a new fatal path | Read of the move ctor and of `GetAllocatorProxy`; the pre-existing `Assert(id != InvalidAllocatorID)` before it is inert in Release |
| O27 | **NEW** medium | `PoolAllocator.cpp:202-248`, `IsMine` `:249-255`, `GetIndex` `:257-263` | No double-free and no interior-pointer defence: `IsMine` is a pure range test and `GetIndex` floors `delta / blockSize`, so `block + 4` is accepted and re-linked as if it were `&buffer[k * blockSize]`, and a second free of the same block self-links it (`A → A`), handing one block to two callers. The only detector is `Assert(numberOfFreeBlocks <= numberOfBlocks)`, inert in Release, and `GetUsage` then wraps. The guard at `:228` reads the pool's **own** head (`GetIndex(availables)`), so it is unreachable and its `>` should be `>=` | Read of `Deallocate`, `IsMine`, `GetIndex`, `AllocateBlock` |
| O28 | note | 5 `Engine/Memory` files this branch touches | `comments.py` reports **24 pre-existing prose-comment violations** (`MultiPoolAllocator.cpp:146,153,336` …), exit 1. The branch neither adds nor removes them: `git show 626249e -- Engine/Memory/*.cpp` adds no prose comment line | Pre-existing module debt; the branch's only comment-rule regression is the 3 dangling pointers of O2 |

## 5. Recommendations, in order

| Step | Action | Proof |
| --- | --- | --- |
| 1 | `InlinePoolAllocator.h:91`: `buffer` → `block`; while there, move the `!IsValidPointer` forward **out** of `#if PROFILE_ENABLED` and delete the duplicate (O1, O4) | V3 goes green over 17/17 TUs |
| 2 | Add the feature-macro syntax layer to `check.sh` (`-DPROFILE_ENABLED=1 -DMEMORY_VERIFICATION_ENABLED=1`, `-fsyntax-only`) — it is the only check that tests the code this commit changed; the worktree trick used here keeps the checkout clean | A configuration nobody compiles stops being where defects live |
| 3 | `InlinePoolAllocator::deallocate` → `DeallocateBytes(ptr, n * sizeof(T))` (O5) | Fallback/free symmetry testable against `totalRequested` |
| 4 | Author the three `docs/Memory/…` pages with `docs_page.py`, or withdraw the pointers (O2) | `comments.py` exit 0; `docs_coverage.py check Memory` |
| 5 | Turn the four assert-then-use sites into release-safe guards (O3); keep the `return` after `FatalAssert`, and do **not** add `[[noreturn]]` | Suite green |
| 6 | Make the destination observable: compare the two allocators' usage (or per-allocator global bytes) in *"Pool Buffer Returns To Its Recorded Parent"*, then rewrite the three `Destroy In Own Scope` testlets the same way and de-duplicate them (O6, O7) | Red against a one-line revert, proven by reverting `parentID` |
| 7 | `PoolAllocator::operator=`: self-move guard or swap-and-reconstruct from a live source (O8) | A testlet that self-move-assigns and asserts capacity survives |
| 8 | `= delete` copy on `MultiPoolAllocator`/`ThreadSafeMultiPoolAllocator`; wrap `BankGenerationLog`/`BankUsageRecord` in an anonymous namespace (O9) | `static_assert(!is_copy_constructible_v<…>)` as a probe, and `layout.py`-clean |
| 9 | Do not report a null block to the profiler (O10); replace the tautological block-count assert with one that compares against the *registry's* capacity (O11); restore a Release-visible leak warning (O12) | Profiling-config testlet: allocate past exhaustion, assert `usage` unchanged |
| 10 | Decide `FatalAssert`-in-Release as a written policy (O13) | A row in `docs/CodingStandards.md` or the Memory design record |
| 11 | Wire `quoted_blocks.py` into layer 5, then triage its 52 stale + 6 unresolved hits page by page (O14) | `check.sh --all --no-build` shows it running and passing |
| 12 | Journal entry + `.Plans/PLAN_*.md` for the four commits (O15); sweep `#endif // TEST_ENABLED` (O16) | `check.sh --all --no-build` at 0 findings, then Dev/Debug/Release + `runtest.sh` |
| 13 | `, availables(nullptr)` in the `PoolAllocator` init list and a `numberOfBlocks > 0` precondition (O20) | A testlet that constructs `(name, 64, 0)` and asserts `Allocate` returns null instead of crashing |
| 14 | Carry the `bankSize`/`minBlock` clamp into `MultiPoolAllocator` too (O21), move the cold allocate path out of the lock the way `Deallocate` was moved (O22), and make the grow path add the **aligned** stride under `statsLock` (O23) | M4 p99 re-measured; `grep -n "lock_guard" …` shows no `NewBankAllocate` call inside a lock scope |
| 15 | Resolve the clamp-vs-assert contradiction (O24), the warn-then-assert pair (O25) and the unconditional proxy repoint (O26) | One testlet each, red before and green after |
| 16 | Decide whether `PoolAllocator` owes a double-free/interior-pointer guard, and if so make the detector survive Release (O27) | A testlet that frees one block twice and expects a rejection, in Release |
| 17 | Never accept a subagent finding list without `verify-findings.py` over it (O19) | Every citation resolves to a real `file:line` |

## 6. How the subagent's report was handled

| Stage | What it claimed | What this review did | Outcome |
| --- | --- | --- | --- |
| Interim list | `AtomicFixedRingMPMC.h:257-265` half-applied terminator; `IsPoolObject` leaking every pool block | Checked file length, `git show --name-only` for the commit, and grepped the symbol tree-wide | Both **rejected** — the file is 234 lines, the commit touches no Container file, the symbol does not exist |
| Interim list | `PoolAllocator::operator=` self-move | Read `:157-160` and `~PoolAllocator` | **Kept** as O8 |
| Final list | `availables` missing from the init list + `totalSize <= 0` early return | Read the init list and the guard; dated the guard to `744ec75` | **Kept** as O20, recorded as pre-existing debt |
| Final list | clamp in one sibling only; `NewBankAllocate` under the lock; capacity stride and lock; clamp-vs-assert; warn-then-assert; unconditional proxy repoint | Each re-read at HEAD | **Kept** as O21-O26 |
| Final list | capacity/`totalCapacity` accounting, `MEMORY_VERIFICATION_ENABLED` teardown loop, `IsMine` gaps | Marked latent or self-marked SUSPECTED by the agent; only the `IsMine`/`GetIndex` reasoning was re-traced here | **Kept** as O23 (latent) and O27; the teardown loop is left SUSPECTED, unmeasured |

## 7. Ownership check against the PR stack, and what closed on 2026-10-21

The rows above were written as if this branch were the whole delivery. It is not: `gh pr view` shows the PRs
are **stacked**, with `#18` (this branch) at the bottom, so a finding can be owned below rather than here.
`origin/master` is still `5bfab87`, which means nothing in the stack has landed — the `MERGED` states are
merges into the PR beneath them.

```
master (5bfab87)
└─ #18 fix/memory-allocator-defects        OPEN    ← this branch
   └─ #19 build/single-test-macro          MERGED → #18
      └─ #20 fix/memory-allocation-ledger  OPEN
         └─ #21 docs/memory-api-reference-pages   MERGED → #20
            └─ #22 refactor/memory-comment-strip  MERGED → #21
               └─ #23 docs/memory-investigation-records  OPEN  (stack tip)
```

| Row | Owned by | Measured at the stack tip | So on this branch |
| --- | --- | --- | --- |
| O1, F1 | **closed here 2026-10-21** | still `memset(buffer, …)` on every other ref | fixed, plus the gate layer that catches it |
| O2 / F2 (missing `docs/Memory` pages) | PR #21 | `comments.py` on the three headers → `0 violation(s)`, exit 0; 12 pages under `docs/Memory/ThreadSafeMultiPoolAllocator/` | **do not duplicate** — recommendation 4 belongs to #21 |
| O4 (foreign block swallowed, guard compiled only under `PROFILE_ENABLED`) | PR #20 | `DeallocateBytes` runs `if (!IsValidPointer(ptr))` unconditionally first, then the `immediateBlock` branch; new testlet `"Foreign Block Returns To Its Own Parent"` records `receivedPtr`/`receivedSize`, i.e. it observes the destination | **withdrawn** as a finding here |
| O5 (`requested` vs `allocated` units) | PR #20 | `ReportAllocation(id, ptr, size, size)` → `(id, ptr, requested, size)`, and the `ReportDeallocation(…, 0)` call removed | **withdrawn** here |
| O10 (failed allocation reported to the profiler) | PR #20 | two `if (likely(ptr != nullptr))` guards added in `PoolAllocator.cpp` | recommendation 9 reduced to O11/O12 |
| O6 (destination-blind testlet) | partly PR #20 | the pool testlet gained `pool->Deallocate(block, 32)` but still never reads the destination allocator | still open, and now cheap: copy the observation style of #20's new testlet |
| F3 (97 files, `#endif //TEST_ENABLED`) | partly PR #20, which fixes its own files | 104 → 97 | still a sweep, still nobody's; `JOURNAL.md:515` already records it as PR #19's |
| F4 (`quoted_blocks.py` unwired) | nobody | no ref calls it from `check.sh`; its findings grew to **58 stale, 33 unresolved** as #21 added pages | owner decision: triage the backlog before wiring it, or the gate goes red for another module's debt |
| F5 (journal/plan for these commits) | nobody | PR #23 documents the *2026-10-05 prerequisite-0 round*, and the newest entry at the tip is *2026-10-19 (evening)* | the 2026-10-21 entry added here covers the fix, not the four commits it reviewed |
| O20, O21, O22, O23, O27 | nobody | `availables` still absent from the init list; `MultiPoolAllocator` still has no `minBlock` clamp and still divides by it | remain, and remain this module's |

**Why the layer, not just the token.** `JOURNAL.md` at 2026-10-05 18:31 had already noticed the profiling
configuration did not compile and deferred it as "its own task", listing three causes — all three of which
`626249e` went on to fix. It never found the fourth (`memset(buffer, …)`), because its probe set
`PROFILE_ENABLED=1` alone and that line sits under `MEMORY_VERIFICATION_ENABLED`. A defect that survives
because the probe's shape matches the defect's location is exactly what a permanent gate layer is for:
`scripts/prove_feature_macros.py` now compiles the run's sources under both macros, failed 4 of 6 files
with exit 1 against the unfixed tree, and reports 17/17 for `Engine/Memory` and **144/144** for the whole
configured build with the fix in.
