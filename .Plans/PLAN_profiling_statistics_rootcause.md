# Profiling statistics: root cause and remaining defects

Owner request that opened this: profiling statistics do not work — diagnose fully, no leaks allowed, and
`SystemAllocator` must be the allocator that owns and reports `malloc`-backed fallback memory.
Standing instruction: do not stop until the problem is unsolvable; keep going to finish all items.

Status: **one item open.** All four configurations — Dev, Debug, Release and a `PROFILE_ENABLED 1` tree — run
**60 of 60** collections. Items 1, 2 and 3 below are closed by measurement and committed; item 4 (option C) stays
parked. The corrected facts are below, and the labels `#22`/`#25`/`#26` used in this file are plan shorthand only:
they are not issues or pull requests (`gh issue view 26` fails; the PR list stops at #17 and the only issue is #3),
so every item here is argued from source, not from a tracker.

## 1. What is closed, and by which commit

| Commit | Defect | Measured effect |
|---|---|---|
| `e28a2fd` | `hbe::source_location` forward-declared and defined nowhere; the real type is `hbe::SourceLocation` | profiling build went from 117 `error:` lines across 52 translation units to 0 |
| `dcc376d` | `MemoryManager::DeregisterAllocator` returned early from its thread-id branch and its leak branch, and never reached `proxyPool.Push(allocator)`; the second `stats.usage > 0` branch was unreachable dead code | each leak trip used to strand one of 256 allocator-proxy slots forever. Both detectors are now `Assert`, which costs nothing in a shipping build and does not seize control flow |
| `1de4b98` | `InlinePoolAllocator`'s immediate-block routes reported nothing, and the release path tested its cache slot **before** testing pointer ownership | under profiling the cached block now reports both ways, and a foreign block is routed to `parentID` instead of being swallowed. Measured: `InlinePoolAllocator` reaches teardown at `usage=0 allocCount=1 deallocCount=1` |
| `be5b586` | `PoolAllocator::Allocate` reported `ReportAllocation(id, ptr, size, blockSize)` without checking success, while `AllocateBlock()` returns `nullptr` on exhaustion | a failed claim credited 128 bytes, so `usage=1152` beat `capacity=1024` in `PoolAllocatorTest`'s "Reallocation After An Exhausted Free List". Profiling collections: 3 → 6 |
| `71565ea` | `ThreadSafeMultiPoolAllocatorTest`'s "Pool Buffer Returns To Its Recorded Parent" destroyed a `PoolAllocator` with a block still outstanding, purely to test buffer routing | the testlet now releases the block; the detector keeps its teeth and the buffer-routing claim is unaffected. Profiling collections: 6 → 57 |
| `a5611cb` | `SystemAllocator::DeallocateBytes` declared its `allocated` under `#ifndef MEMORY_INVESTIGATION_ENABLED`, a macro `BuildConfig.h` always **defines** (as `0`), so that line never compiled and the shipped path debited the caller's `nBytes` against a credit of `OS::GetAllocSize(ptr)`. `DefaultAllocator`'s id-0 bypass to `malloc`/`free` also went, so the registration at id 0 owns and reports the memory its `malloc` blocks come from | probe before: `allocEvents=829 deallocEvents=829 diff=32884`, `uniquePtrs=36 mismatchedPtrs=7 outstanding=0`, and those 7 summing to exactly the figure the assert printed. After: `allocEvents=1018 deallocEvents=1018 diff=0 mismatchedPtrs=0`, no abort. The residue was `malloc`'s size-class padding, never a leak |
| `e8f885f` | `InlinePoolAllocator::DeallocateBytes` tested pointer ownership only under `PROFILE_ENABLED`, so a shipping build kept a small block it did not own and handed it to a second owner; `deallocate` forwarded an element count where `allocate` forwarded a byte count | ownership is now the first guard in every configuration and the redundant second test is gone; the count becomes bytes. The testlet "Foreign Block Returns To Its Own Parent" fails three ways against the old order and passes against the new one, in all four builds. Cost on the one-slot cache route, 4M alloc+dealloc pairs: 1.14 ns/op before, 1.09 after — inside the run-to-run spread |
| `0efeed3` | with the bypass gone a zero-element request returns null on every id, and `DefaultAllocator::deallocate` then aborted on its own `Assert(ptr != nullptr)` | found by a scratch testlet before it shipped. The guard mirrors `MemoryManager::Deallocate`: null with a zero count is a no-op, null with a non-zero count still asserts |
| `1931cf6` | `MultiPoolAllocator::NewBankAllocate` and its thread-safe twin credited only the engine-wide `SystemStatistics` counter, so `AllocStats::fallbackCount` read 0 for the one allocator family whose fallbacks are bank generations | `ReportFallback` is public with three call sites — it was private only because `FallbackAllocate` had been its sole caller. Both collections' fallback testlets pass under profiling; both failed before the call was added |
| `a228add` | `InlineMonotonicAllocator` reported allocations under `#ifdef __MEMOR_STATISTICS__`, defined nowhere, while its `Deallocate` reported a release of `allocated = 0` | both reports are `#if PROFILE_ENABLED` now, `requested` is passed apart from the aligned `allocated`, and the zero-byte per-pointer release record is gone (it cancelled counts without cancelling bytes). Halving the teardown drain makes the manager say `leaks 8 of 2048 bytes`, a sentence the dead macro made unprintable |
| `fb8a983` | `StackAllocatorTest`: two testlets read `GetUsage() > 0` beneath a message demanding usage be non-zero, and one called any positive delta of global new/delete bytes a leak | that probe measured its own noise: +16 B of never-freed `StaticStringTable` intern, −8 B because an unsized `operator delete` records `malloc`'s size class (64) against a recorded 56. The testlet now asks the question its message asks — did the parent get back the pointer and count it handed out — and both failure branches were proved to fire by breaking the observer |
| `9d00565` | the reference described the bypass as a deliberate fast path and the dead macro as dead code | `quoted_blocks.py docs/Memory` 5 stale → 0 (two of those blocks older than this work), `htmlcheck` 0 problems, `docs_methods` 0 missing |

## 2. Facts that replaced four wrong hypotheses

Every line below was measured, not reasoned. The hypotheses were: slot ceiling, `InlineMonotonicAllocator`,
`String`/`HInlineString`, and the immediate-block pairing. All four were wrong.

| Question | Answer, as measured |
|---|---|
| Which check fires? | `MemoryManager::ReportAllocation`'s `stats.usage > stats.capacity` — **not** the leading `!IsValid(id)` guard, which cannot reject id 234 because `MaxNumAllocators` is 256 |
| Which test? | `PoolAllocatorTest`, testlet "Reallocation After An Exhausted Free List", `Engine/Memory/PoolAllocator.cpp:414`, pool `TestPoolAllocatorExhausted`, `blockSize=128`, `blockCount=8`, `allocSize=16` |
| Numbers at the trap | `usage=1152 capacity=1024 allocated=128 requested=16 name=TestPoolAllocatorExhausted id=234` — 9 blocks against a capacity of 8, the 9th being a `nullptr` claim |
| Where does `usage` come from? | `ReportAllocation` does `stats.usage += allocated`; `ReportDeallocation` does `-= allocated` and traps first if the subtraction would go negative. So returns **do** drain usage; `ReportFallback` touches only `fallbackCount` |
| Where does `capacity` come from? | `PoolAllocator` `stats.capacity = blockSize * numberOfBlocks`; `InlinePoolAllocator` registers `ActualBlockSize * ActualNumBlocks * sizeof(T)` at construction. Both fixed, both correct |

## 3. What is left, in the order worth attacking

Items 1, 2 and 3 of the previous version of this table are closed — section 1 carries the commit and the measured
effect of each. What remains is one design item and five findings that need a decision.

| # | Item | Evidence in hand | What to do |
|---|---|---|---|
| 1 | **Option C, the thread-sharded allocator** | parked on branch `wip/option-c-sharded` (head `9c2d964`); with it in the tree the Dev suite dies at `MultiPoolAllocator.cpp`'s bank assert and then at a `PoolAllocator` free-list bound check, and restoring the old allocator passes twice | root cause is named: the sharded free list stores a `uint32_t` next-link at block offset 0 while `PoolAllocator` stores `TSize` (8 bytes) there, and an extent is itself one block of its parent pool, taken from the ambient scope. Extents need a recorded parent id — `ThreadSafePoolState::state->parentID` exists but is never assigned |
| 2 | `InlinePoolAllocator::IsValidPointer` ends at the **start** of the last block | `block[0] <= ptr && ptr <= block[LastBlockIndex]`, so an address inside the final block reads as foreign and is released to `parentID` rather than joining the free list | documented as-is on `docs/Memory/InlinePoolAllocator/`'s `deallocate-bytes.html` and `is-valid-pointer.html`. Widening the bound changes which allocator a release is billed to, so it needs a decision, not a reflex |
| 3 | `InlineMonotonicAllocator`'s overflow branch reports no fallback | a request the arena cannot hold is forwarded to `parentID`, which moves the parent's counters, and nothing credits this registration with `fallbackCount` — the same class of gap `1931cf6` closed for the multi-pools | one `ReportFallback` call in the forward branch plus the page update. Not done: found while writing the page, outside the ruling that opened this plan |
| 4 | The byte-count fix in `e8f885f` has no assertion behind it in a shipping build | the two unit-dependent thresholds lead to the same pointer either way, so the only observable is the byte figure the manager records, and that exists only under `PROFILE_ENABLED` | a `#if PROFILE_ENABLED` testlet releasing `n` four-byte elements and reading `GetAllocatorStat`. Named on the page rather than papered over |
| 5 | `check.sh`'s build gate cannot reach `Examples/` | the root `CMakeLists.txt` adds `Applications`, `External` and `Engine` only, so `WindowExample` fails the gate for a reason no change can fix; `Examples/CMakeLists.txt` also still passes `-D__DEBUG__`, a macro nothing reads | gate defect worth its own small fix: either the script stops naming an unreachable target or the examples join the build |
| 6 | 27 unresolved quotation captions, and `build-prof/` is not ignored | `quoted_blocks.py docs/Memory`: 544 blocks, 233 verbatim, 0 stale, 27 unresolved. `git status` lists `build-prof/` untracked, so the scratch tree is one careless `git add .` away from history | captions: hb-docs skill, one page at a time. Ignore entry: one line in `.gitignore` |

## 3b. Facts only a measurement could produce

| Question | Answer, as measured |
|---|---|
| Does reading a value macro with `#ifdef` break anything else? | No. `MEMORY_INVESTIGATION_ENABLED`, `MEMORY_VERIFICATION_ENABLED`, `MEMORY_LOGGING_ENABLED` and `PROFILE_ENABLED` are always `#define`d as 0 or 1 in `Engine/Config/BuildConfig.h`, so only `#if`/`#if !` can test them. A repo sweep found `PLATFORM_*` and `RIGHT_HANDED_COORDINATE` as the other `#ifdef` sites, both legitimate presence tests, and `__MEMOR_STATISTICS__` only in `InlineMonotonicAllocator.h`, defined nowhere |
| How big is the residue a mismatched release leaves? | `4 mod 16` in every figure ever seen — 9012, 21620, 32884 — because it is `malloc`'s size-class padding. It is run-dependent, which is why no single number should be quoted as "the" leak |
| What does the ownership test cost on the hot route? | Nothing measurable: 4M alloc+dealloc pairs on the one-slot cache route, 1.14 ns/op mean without the guard, 1.09 ns/op with it. Ownership cannot be decided without at least one address comparison, and the new code performs exactly one per release |
| Why did three configurations miss the abort `0efeed3` fixed? | No container in the suite releases a zero-count allocation through `DefaultAllocator`, so the gate passed over it. The scratch testlet that found it is the cheapest form of that coverage |

## 4. Reproduction recipe, and the probe that produced these numbers

```bash
# 1. flip the switch: Engine/Config/BuildConfig.h:38 is `#define PROFILE_ENABLED 0`
#    A -DPROFILE_ENABLED=1 command line cannot win: the header redefines it to 0 afterwards.
python3 -c "import pathlib; p=pathlib.Path('Engine/Config/BuildConfig.h')
t=p.read_text(); p.write_text(t.replace('#define PROFILE_ENABLED 0','#define PROFILE_ENABLED 1'))"
cmake --build build-prof --target EngineTest -j 8 && ./build-prof/Applications/EngineTest/EngineTest

# 2. shipping configurations
./build.sh Applications/EngineTest -dev -debug -release -test
for c in Dev Debug Release; do ./build/Applications/EngineTest/$c/EngineTest; done
```

Nothing printed is usable at the trap: the detectors log through a buffered `std::cout`, and `debugBreak()`
kills the process before the next flush, so four runs gave four empty logs. `script -q /dev/null <binary>` gives
a pty, which line-buffers the harness banners and yields the testlet name. For the detector's **values**, flush
explicitly just above the check — this one line produced `usage=1152 capacity=1024 name=TestPoolAllocatorExhausted`
and then `name=SystemAllocator id=0 usage=21620 allocCount=829 deallocCount=829`:

```cpp
		std::cout << "PROBE leak name=" << stats.name << " id=" << static_cast<int>(id) << " usage=" << stats.usage
				  << " capacity=" << stats.capacity << " allocCount=" << stats.allocCount
				  << " deallocCount=" << stats.deallocCount << " isInline=" << stats.isInline << std::endl;
```

`lldb` alone is not enough here: it fails to load objfiles for `libHEngine.a`, so `MemoryManager.cpp` line
breakpoints and `frame variable` yield nothing, and only the symbol-level backtrace works. Reading the trap
signature `Assert<char const (&) [27], hbe::StaticString&, char const (&) [3], int&…>` is what identified the
firing check as the leak assert, because `"MemoryManager: allocator ["` is exactly 26 characters.

## 5. Decisions taken here, and why

| Decision | Reason |
|---|---|
| The detector stays strict; the **test** was fixed instead | the owner's ruling is "no leaks allowed", and weakening a detector to silence it converts a defect into a blind spot |
| Paired inline-pool reports were reverted once, then landed with the ownership test | reporting a stolen block as a legitimate release is a true-looking record of a false event, and it asserted at id 234; the accounting only becomes honest once only owned blocks can reach the fast path |
| `Assert` rather than `Log(Warning) + debugBreak` in `DeregisterAllocator` | an `Assert` costs nothing in Release and does not seize control flow, so the slot release at the tail of the function is reached on every path |
| Unmeasurable changes get reverted, not kept | nine quotation-caption edits moved the gate count by zero; five access-specifier deletions left `layout.py` at 21 both before and after |
| TaskSystem/TaskStream conflict resolved to `HEAD`, with the conflicted files copied to `.work/rescued/*.conflicted` | the "Stashed changes" side carried header-inline bodies from before the commit that moved those bodies into the source, so a union would duplicate declarations; nothing was discarded, and `.Plans/OUT_OF_SCOPE_taskstream_tasksystem_wip.patch` already owns that work |
| The allocator that owns a block reports the block's size, not the caller's request | `OS::GetAllocSize(ptr)` is what the credit side already used, and a debit of `nBytes` against a credit of the `malloc` size leaves size-class padding behind forever. Both sides now move by the same figure, which is the only rule under which `usage` can reach zero |
| The id-0 `malloc` bypass was deleted rather than special-cased | the owner's ruling is that `SystemAllocator` owns `malloc`-backed memory, and every non-system id already answers a zero-size request with `nullptr` — so the bypass was the one path that reported nothing, not a fast path that needed preserving |
| The inline-pool ownership guard runs in every configuration | the owner approved it after the cost was measured on the route that matters, not argued: one address comparison per release, 1.14 vs 1.09 ns/op across the change. A defect that only exists when the profiler is off is not a scoped defect |
| A bump arena records no per-pointer release | recording `allocated = 0` per release cancels counts without cancelling bytes, which is the exact signature this session spent itself removing; the arena comes back once, in full, at teardown |
| A test whose condition contradicts its own message is the bug, not the engine | `StackAllocatorTest` failed on working allocations for two sessions because `GetUsage() > 0` sat under "Usage should not be zero". Same rule as the detectors: fix the thing that lies |
| The reference moves in the same session as the code | sources carry no comments, so a page that still describes the old route is the only place the old contract survives. `quoted_blocks.py` is what makes that checkable rather than aspirational |
| Citations are file plus symbol, never line numbers, never `/tmp` | line numbers rot on the next edit, and a scratch path the reader cannot open is not a citation |

## 6. Environment

| Item | Value |
|---|---|
| Branch | `master`, clean at `9d00565`; nothing pushed |
| Profiling scratch tree | `build-prof/`, CMake-generated with `-DTEST_ENABLED=ON`; **not** in `.gitignore`, unlike `build/`, `build-asan/`, `build-tsan/` and `.work/`. No `-DPROFILE_ENABLED` define can win, because `Engine/Config/BuildConfig.h` defines the macro afterwards, so the switch is flipped in the header and must be flipped back before any `build/` work |
| Sanitizer verdicts already taken | ASan: nothing. TSan: 13 races, none with an `Engine/Memory` frame — which is how allocation overflow and a visible data race were ruled out early |
| Option C branch | `wip/option-c-sharded`, head `9c2d964` |
| Gate state | `check.sh --staged` 0 grep-rule failures, 0 blank-line and 0 include findings; the sweep backlog carries the `DefaultAllocator.h` and `MemoryManager.h` member-layout findings, proved identical at the parent commit. `comments.py` 0 violations; `docs_page.py check Memory` 0 problems of 236 pages; `docs_methods.py Memory` 0 missing; `quoted_blocks.py docs/Memory` 0 stale, 27 unresolved; `docs_coverage.py check` reports no `docs/Memory` finding (its 42 belong to `OSAL`, `Log`, `Renderer`, `Resource`, `String`, `Test`) |
| Cleanup owed | `rm -rf build-asan build-tsan` when the sanitizer evidence is no longer needed |
