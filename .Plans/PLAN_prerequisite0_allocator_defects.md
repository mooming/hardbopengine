# PLAN: prerequisite 0, the four correctness defects in the multi-pool allocators

## Goal

Fix the four defects recorded in `JOURNAL.md` under 2026-10-05 10:08, plus three trap-sized standards slips, so the
thread-safe multi-pool has a correct base for the option C redesign. No locking-model change belongs in this task:
the mutex stays exactly where it is, and the redesign is a separate task.

Files in scope: `Engine/Memory/ThreadSafeMultiPoolAllocator.cpp`, `Engine/Memory/ThreadSafeMultiPoolAllocator.h`
(includes only), `Engine/Memory/PoolAllocator.cpp`, `Engine/Memory/MultiPoolAllocator.cpp`.

Locate every site by function name, not by the line numbers below; the line numbers are from the tree before the
benchmark testlets were added and only the test block moved.

## The four fixes

| Id | Defect | Fix | Notes on how to do it |
|---|---|---|---|
| F1 | `ReportConfiguration` doubles every block size it introduces. `configs.reserve(banks.size())` prevents reallocation, so `emplace_back` constructs the new element at the address `found` already held as `end()`, and the following `found->numberOfBlocks += value` adds the value a second time. Proven by a standalone model: a true peak of 15 caches as 25 | Make the accumulate the `else` branch of the not-found test, or re-run `find_if` after `emplace_back` | Whichever form is chosen, the `reserve` that made the bug silent must stay, and a comment is not permitted: the shape of the `if` and `else` has to carry it |
| F2 | A destructor releases bank memory through the **ambient** scoped allocator instead of the recorded parent. `~PoolAllocator` calls the two-argument `mmgr.Deallocate(buffer, totalSize)`, which resolves to `GetScopedAllocatorID()`, and `~ThreadSafeMultiPoolAllocator` opens no `AllocatorScope(parentID)` around `banks.clear()` even though `GenerateBank` opens exactly that scope for the allocating half | `PoolAllocator::~PoolAllocator` releases through `parentID`: `mmgr.Deallocate(parentID, buffer, totalSize)`. `~ThreadSafeMultiPoolAllocator` opens `AllocatorScope scope(parentID);` as its first statement | `~MultiPoolAllocator` has the identical shape and gets the same one-line scope. Stated scope extension: leaving the twin knowingly wrong after touching the root cause would be worse than the three extra lines |
| F3 | Self-deadlock. The class logs while holding its own lock, and `Logger::Out` allocates its input buffer from its own `inputAlloc`, which **is** a `ThreadSafeMultiPoolAllocator` (`Engine/Log/Logger.h:98`), under `AllocatorScope(inputAlloc)` at `Logger.cpp:478`. The `Verbose` bank-creation log in `GenerateBank` and the `FatalError` foreign-pointer log in `Deallocate` are both issued inside the lock; `std::mutex` is not recursive | Move every diagnostic out of the locked region. In `Deallocate`, record the failure under the lock and log after the `lock_guard` scope ends. In `GenerateBank`, build the message into an `InlineStringBuilder` under the lock, since it allocates nothing, and let the caller log once the lock is released | The rule this establishes: **no call that can allocate while the lock is held.** `AllocatorScope(parentID)` does not protect these paths, because `Logger::Out` overrides the ambient scope with its own `inputAlloc`. Do not switch to `std::recursive_mutex`: it hides the reentrancy instead of removing it |
| F4 | `PrintUsage` allocates `std::map` nodes while holding the lock, through the caller's allocator scope, because its `AllocatorScope(MemoryManager::SystemAllocatorID)` is opened later and guards only the `StringBuilder` | Open `AllocatorScope scope(MemoryManager::SystemAllocatorID);` as the first statement of the function, before the map is touched | While there, the function's `using namespace std;` should go, since the rule set wants qualified names |

## Three trap-sized slips to fix in the same pass

| Slip | Where | Fix |
|---|---|---|
| `#ifdef PROFILE_ENABLED` where the other 13 guards are `#if PROFILE_ENABLED`, and `Engine/Config/BuildConfig.h` defines the macro to `0`, so `#ifdef` is always true and the block compiles in every configuration | `ThreadSafeMultiPoolAllocator.cpp` in `NewBankAllocate`, and the identical slip in `MultiPoolAllocator.cpp` | `#if PROFILE_ENABLED` |
| The header names `std::mutex`, `std::lock_guard` and `std::initializer_list` while including neither `<mutex>` nor `<initializer_list>`, so it compiles on a transitive include, which rule set B of `docs/CodingStandards.md` forbids | `ThreadSafeMultiPoolAllocator.h` include block | Add both, in written-path byte order inside the `<...>` block, one blank line before the project block. Adding an include adds no API entry, so the documentation gate is unaffected |
| `unlikely(requested <= 0)` on an unsigned parameter, which is `== 0` and always false otherwise | `Allocate` | `requested == 0` |

## Regression tests, and what each can honestly prove

| Fix | Test | Honest status of that proof |
|---|---|---|
| F1 | Assert inside the existing `#if PROFILE_ENABLED` path that a cached configuration equals the summed peak rather than the doubled one | Only compiles when profiling is on. Verify by temporarily setting `PROFILE_ENABLED` to 1 in `BuildConfig.h`, capturing before and after numbers, then reverting the file. That probe result belongs in `JOURNAL.md`, not in the permanent test, which stays compiled out |
| F2 | New testlet: construct allocator A with a known parent by holding `AllocatorScope` over the parent during construction, allocate, then set the ambient scope to a **different** allocator and destroy A. Before the fix, the parent's buffer is released to the unrelated allocator, whose foreign-pointer path fires a `FatalError` with a `debugBreak` in a Dev build. After the fix, silence | This one is genuinely runtime-provable, and it is the strongest of the four |
| F3 | **No runtime test.** The deadlocking instance is `Logger`'s private `inputAlloc`, which no testlet can reach, and a burst-based attempt to exhaust it from outside would be a flaky test masquerading as a proof. Verification is structural instead: no `Logger::` call and no other allocation-capable call sits inside any lock scope in the file, provable by reading every `lock_guard` scope | State this limitation in the report rather than shipping a flaky test |
| F4 | New testlet: call `PrintUsage` while the ambient scope is the allocator under test. Before the fix the map's node allocation re-enters `Allocate` on a mutex this thread already holds, which hangs; after the fix it returns | Runtime-provable. Demonstrate the hang once deliberately, then fix, so the committed testlet is a guard rather than a claim |

## Rules

| Rule | Source |
|---|---|
| No comments in `.cpp` or `.h`. The only exemptions are the line-1 copyright, a structural label that only names the construct its own line closes, and `hb-standards:ignore` | `AGENTS.md` |
| Blank-line size table A1 to A16, include grouping B1 to B5, tabs, Allman braces. Format first, apply the table second, and re-inspect after formatting because `clang-format` collapses legal seams | `docs/CodingStandards.md`, `.pi/skills/hb-standards/SKILL.md` |
| Never reorder data members relative to one another; initialization follows declaration order | `AGENTS.md` |
| Do not add, remove or rename any declaration in a header. A new header entry demands a reference page | `docs_coverage.py` |
| Testlet closures under 48 bytes, test names 128 bytes or less, 64 KiB retained global bytes per testlet, every wait carries a deadline | `Engine/Test/TestCollection.h`, `Engine/Test/Testlet.h` |
| Do not touch the four benchmark testlets or their output lines. They are the redesign's before-picture | `.Plans/PLAN_lockfree-contention-benchmark.md` |

## Verification

1. `./build.sh Applications/EngineTest -dev -debug -release -test` so all three configurations are proven, per the
   `hb-standards` gate that closes any engine change.
2. Run each built `EngineTest`; expect 60 collections `SUCCESS`, 388 testlets (386 plus the two new), exit 0, and the
   M1 to M4 lines still present and unchanged in shape.
3. The four style scripts on every touched file, compared with the pre-change baselines in `/tmp/base_*.txt` where they
   exist: `blank_lines.py`, `includes.py`, `comments.py`, `layout.py`. Zero new findings is the bar.
4. The F1 profiling probe, with `BuildConfig.h` reverted afterwards and `git diff` proving the revert.
5. `git status --porcelain` lists only the four files above, the two new testlets' file, plus `JOURNAL.md` and the
   `docs/` files already dirty from the benchmark task.

## Explicitly out of scope

The locking model, the deterministic size class, bank registration, alignment, `MaxNumAllocators`, the
`availables` member that `PoolAllocator`'s constructor leaves uninitialized on its `totalSize <= 0` early return, and
every performance claim. Each has its own task; this one makes the base correct.
