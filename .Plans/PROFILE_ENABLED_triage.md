# PROFILE_ENABLED Triage

Probe of `HEAD = 60e171c`, Apple clang 21 / arm64, `Ninja Multi-Config`, `-Wall -Werror`. All experiments ran in a scratch copy of the committed tree at `/tmp/hbe-profile-probe` (deleted). The checkout was never configured or built.

## 1. Verdict

| Question | Answer |
|---|---|
| Does `HEAD` compile with profiling on? | **No.** Dev, Debug and Release fail identically: 117 `error:` lines in the Dev `EngineTest` build, 52 of 133 translation units dead, `-Werror` stops the link. |
| After the section-4 repair? | **Yes** — all targets, all three configurations, 0 errors, only the pre-existing `ld: ignoring duplicate libraries` note. |
| Can the profiling build run? | **No.** It traps 0.65 s into `EngineTest`, exit 133 `Trace/BPT trap: 5`. Section 5 is the real headline. |
| How profiling is enabled | `Engine/Config/BuildConfig.h:38` is a bare `#define PROFILE_ENABLED 0`: no CMake option, no `.project.config` define, no `customCMake.txt` hook. `docs/AllocatorGuide.md:315` tells the reader to edit that header, so the header *is* the documented switch. Measured: a `-DPROFILE_ENABLED=1` command line cannot win — the header redefines it to 0 afterwards (`static_assert(PROFILE_ENABLED == 1)` fails) and `-Wmacro-redefined` makes even that fatal under `-Werror`. Edited in the scratch copy only. |

First real errors, verbatim, from `Engine/OSAL/LinuxInputOutput.cpp` — any file reaching `Logger.h` dies here:

```
Engine/Memory/PoolAllocator.h:32:23: error: field has incomplete type 'hbe::source_location'
   32 |         hbe::source_location srcLocation;
      |                              ^
Engine/Memory/MemoryManager.h:24:8: note: forward declaration of 'hbe::source_location'
   24 | struct source_location;
      |        ^
Engine/Memory/PoolAllocator.h:38:44: error: incomplete type 'hbe::source_location' named in nested name specifier
   38 |                   hbe::source_location location = hbe::source_location::current());
      |                                                ^
```

## 2. Root causes, grouped by shape — 117 error lines, 21 sites, 7 files

| # | Shape | Errors | Owning file and function | Fix |
|---|---|---|---|---|
| A | `hbe::source_location` is forward-declared at `MemoryManager.h:24` and **defined nowhere**; the real type is `hbe::SourceLocation` (`OSAL/SourceLocation.h`). Profiling uses it as a by-value field and as `::current()`, both needing a complete type. | **111 of 117** | `PoolAllocator.h:32,38` (field, ctor default argument); `StackAllocator.h:29,46,57` (`TSrcLoc`, field, ctor); `ScopedLock.h:24,32,39` (`TSrcLoc`, `srcLoc`, ctor default) | Spell the real type: `SourceLocation` |
| B | Definitions drop the `noexcept` their header declares; `-Werror` makes it fatal. | 3 | `SystemStatistics.cpp:53,58,79` — `Report`, `ReportSysMemAlloc`, `ReportSysMemDealloc` | Add `noexcept` |
| C | Test body names members removed by an earlier rename. | 3 | `InlinePoolAllocator.cpp:145-146` — `inlineTimeAvg`, `stdTimeAvg`, `loopLength`, in `InlinePoolAllocatorTest::Prepare`'s `AllocDeallocTestFunc` lambda | Use surviving `inlineTimeDuration`, `stdTimeDuration`, `maxAllocSize` |
| D | *Cascade of A*: `no matching member function for call to 'DeregisterAllocator'` (2), `out-of-line definition of 'PoolAllocator' does not match any declaration` (1) | 3 | `PoolAllocator.cpp:14,143`; `StackAllocator.cpp:62` | None — cleared by A |
| E | *Cascade of A*: `lambda capture 'this' is not used [-Werror,-Wunused-lambda-capture]`; the body had already errored, so `this` looked unused. Proved: fixing A alone cleared it. | 1 | `ScopedLock.h:79`, `ScopedLock::~ScopedLock` timeout lambda | None — cleared by A |

## 3. Scope: not confined to `Engine/Memory` — 52 of 133 units fail, 12 modules plus the test app

| Module | Memory | Core | OSAL | Test | String | Resource | Renderer | Math | Container | Log | Engine | Config | EngineTest |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Failing TUs | 11 | 9 | 7 | 4 | 3 | 3 | 3 | 3 | 3 | 2 | 1 | 1 | 1 |

Mechanism: `ScopedLock.h:24`'s `using TSrcLoc = hbe::source_location;` is *not* guarded — the shipping build survives only because naming an incomplete type in a reference parameter is legal. The instant profiling declares `TSrcLoc srcLoc;` as a field, every unit including that header (all of the task system, and via `Logger.h` nearly everything) loses the build. Seven broken files understates the reach by an order of magnitude.

## 4. Repair — verified, then reverted. 8 files, 14 added / 13 removed lines

`diff -U0`, applied inside scratch copies only; the checkout never saw it.

```
--- a/Engine/Core/ScopedLock.h
+++ b/Engine/Core/ScopedLock.h
@@ -7,0 +8 @@
+#include "OSAL/SourceLocation.h"
@@ -24 +25 @@
-	using TSrcLoc = hbe::source_location;
+	using TSrcLoc = SourceLocation;
--- a/Engine/Core/SystemStatistics.cpp
+++ b/Engine/Core/SystemStatistics.cpp
@@ -53 +53 @@
-void SystemStatistics::Report(const AllocStats& stats)
+void SystemStatistics::Report(const AllocStats& stats) noexcept
@@ -58 +58 @@
-void SystemStatistics::ReportSysMemAlloc(size_t usage)
+void SystemStatistics::ReportSysMemAlloc(size_t usage) noexcept
@@ -79 +79 @@
-void SystemStatistics::ReportSysMemDealloc(size_t usage)
+void SystemStatistics::ReportSysMemDealloc(size_t usage) noexcept
--- a/Engine/Memory/InlinePoolAllocator.cpp
+++ b/Engine/Memory/InlinePoolAllocator.cpp
@@ -145,2 +145,2 @@
-		ls << "Performance: " << inlineTimeAvg << " msec vs STL: " << stdTimeAvg << " msec, rate = [" << rate
-		   << "], fallback count = " << stat.fallbackCount << " / " << (testCount * loopLength) << lf;
+		ls << "Performance: " << inlineTimeDuration << " msec vs STL: " << stdTimeDuration << " msec, rate = [" << rate
+		   << "], fallback count = " << stat.fallbackCount << " / " << (testCount * maxAllocSize) << lf;
--- a/Engine/Memory/MemoryManager.cpp
+++ b/Engine/Memory/MemoryManager.cpp
@@ -287 +287 @@
-void MemoryManager::DeregisterAllocator(TId id, const hbe::source_location& srcLoc)
+void MemoryManager::DeregisterAllocator(TId id, const SourceLocation& srcLoc)
--- a/Engine/Memory/MemoryManager.h
+++ b/Engine/Memory/MemoryManager.h
@@ -17,0 +18 @@
+#include "OSAL/SourceLocation.h"
@@ -24 +24,0 @@
-struct source_location;
@@ -164 +164 @@
-	void DeregisterAllocator(TId id, const hbe::source_location& srcLocation);
+	void DeregisterAllocator(TId id, const SourceLocation& srcLocation);
--- a/Engine/Memory/PoolAllocator.cpp
+++ b/Engine/Memory/PoolAllocator.cpp
@@ -15 +15 @@
-							 const hbe::source_location location)
+							 const SourceLocation location)
--- a/Engine/Memory/PoolAllocator.h
+++ b/Engine/Memory/PoolAllocator.h
@@ -32 +32 @@
-	hbe::source_location srcLocation;
+	SourceLocation srcLocation;
@@ -38 +38 @@
-				  hbe::source_location location = hbe::source_location::current());
+				  SourceLocation location = SourceLocation::current());
--- a/Engine/Memory/StackAllocator.h
+++ b/Engine/Memory/StackAllocator.h
@@ -29 +29 @@
-	using TSrcLoc = hbe::source_location;
+	using TSrcLoc = SourceLocation;
```

| Build | Tree | `PROFILE_ENABLED` | Result |
|---|---|---|---|
| Control | HEAD unrepaired | 0 | all targets, Dev: 0 errors — the baseline |
| Control | HEAD + repair | 0 | all targets, Dev: 0 errors — shipping path untouched |
| Probe | HEAD + repair | 1 | all targets, **Dev, Debug, Release: 0 errors**, 2 warnings (pre-existing `ld` duplicate-library notes) |
| Patch proof | HEAD + the patch text above, extracted from this document and applied with `patch -p1` | 1 | all targets, Dev, Debug, Release: 0 errors |

Restoration proof: the repair touched only `/tmp/hbe-profile-probe/src-*`; `git status --short` before and after lists the same 10 modified `Engine/` files, all another agent's comment migration. The only file created in the checkout is this document; nothing committed, nothing pushed. Three scratch-only diagnostics used in section 5 are deliberately
*not* in the patch: `LOG_FORCE_PRINT_IMMEDIATELY 1`, `debugBreak()` redefined to print `__FILE__:__LINE__`, and the
leak branch's `debugBreak()` wrapped in `#if 0`.

Not chosen: `using source_location = SourceLocation;` in `OSAL/SourceLocation.h` clears shape A in 3 lines rather than 13, but leaves three spellings of one idea alive (`hbe::source_location`, `hbe::SourceLocation`, and `std::source_location` in `Core/Debug.h`). The owner's call, not a probe's.

## 5. The leak-detector finding — the real headline

Profiling does compile; it cannot share a process with the test suite.

* **The trap is the leak detector.** `MemoryManager::DeregisterAllocator(TId)` at `MemoryManager.cpp:223-233`: when `stats.usage > 0` it logs a Warning, then `#if !RELEASE_BUILD` calls `debugBreak()` = `__builtin_trap()`. Debug and Dev are both `!RELEASE_BUILD`, so a detected leak is a `SIGTRAP`, not a report. Measured: exit 133 at 0.65 s, 4 testlets passed, 2 of 60 collections finished, trap inside `InlinePoolAllocatorTest` TC0.
* **It traps in silence.** That Warning goes through `MemoryManager::Log`, whose whole body is `#if MEMORY_LOGGING_ENABLED` — 0. The detector's only sentence is compiled away; its trap is not. With `LOG_FORCE_PRINT_IMMEDIATELY 1` the Warning still never appeared; locating the trip required redefining `debugBreak()` in scratch, which reported `MemoryManager.cpp:232`.
* **The accounting it traps on is wrong, not strict.** With that trap neutralised, 12 leak trips fired, **every one an `InlinePoolAllocator`**; the first read `name=InlinePoolAllocator id=196 usage=64 capacity=128` — exactly one 64-byte block of `InlinePoolAllocator<int,16,2>` still accounted after a matched `allocate`/`deallocate` pair. `InlinePoolAllocator::DeallocateBytes` returns at its `immediateBlock` fast path *before* the profiling `ReportDeallocation`, while `AllocateBytes` reports on the `availableBlock` path: usage ratchets up and never drains.
* **Removing that trap does not save the run.** 2.6 s later the second profiling-only detector fires — `MemoryManager.cpp:370`, `ReportAllocation`'s `stats.usage > stats.capacity` FatalError, same shape. Measured: 26 testlets pass, 5 of 60 collections finish, versus **389 testlets and 60 collections passing with profiling off**.
* Two defects sit behind them: the second `if (unlikely(stats.usage > 0))` at `MemoryManager.cpp:238` is unreachable dead code, and the leak branch's early `return` skips both the `stats.Reset()` bookkeeping and `proxyPool.Push(allocator)`, so each trip strands an allocator-proxy slot. Only `PoolAllocator` and `StackAllocator` pass a source location to deregistration; the other four allocator types call the 1-arg overload even under profiling.

To have both, cheapest first: (1) make the two detectors report instead of trapping — pair `MEMORY_LOGGING_ENABLED 1` with `PROFILE_ENABLED 1`, or route them through `Assert`, which calls `FlushLogs()` before it traps and here does not. One line, and 12 diagnosable warnings replace a silent abort. (2) Fix the unpaired `ReportDeallocation` on the `InlinePoolAllocator` immediate-block path — a real accounting bug, and the reason usage never reaches 0. (3) Only then let the remaining 363 testlets speak.

## 6. Consequence for numbers

Each statistic below sits inside `#if PROFILE_ENABLED`. **Before this probe, none of them had ever executed.**

| Statistic | Home | State with `PROFILE_ENABLED 0` |
|---|---|---|
| `usage`, `maxUsage`, `capacity`, `allocCount`, `deallocCount`, `totalRequested`, `maxRequested`, `totalFallback`, `maxFallback`, `fallbackCount` | `AllocStats` inside `AllocatorProxy::stats` | The `stats` member is itself profiling-only — no storage, nothing to trust |
| Engine-wide usage and peak capacity; `GetAllocatorStat`, `DeregisterAllocator(id, srcLocation)`, `ReportMultiPoolConfigutation` | `MemoryManager` `UsageRecord`; `MemoryManager.h:161-166` | Usage stays 0; the three entry points are not declared, so calling them is a compile error |
| Allocator history, its dump, system-memory high-water mark | `SystemStatistics::Report`, `ReportSysMemAlloc/Dealloc`, `PrintAllocatorProfiles` | Declared unconditionally, body guarded: `PrintAllocatorProfiles` prints nothing, silently |
| Per-pool peak block count; per-class bank peaks and `PrintUsage` | `PoolAllocator::maxUsedBlocks`, `MultiPoolAllocator` / `ThreadSafeMultiPoolAllocator::ReportConfiguration` | Not a member; `PrintUsage`'s body is entirely guarded, so the testlet meant to check it cannot fail (`JOURNAL.md` noted this) |
| Allocator memory configuration at shutdown; contended-lock hold-time report | `Logger::ReportMemoryConfiguration` from `Engine::PreShutdown`; `ScopedLock::~ScopedLock` vs `timeOutSec` | Not compiled; the profiling ctor is the only one that records `startTime`, so no timing is taken |

`JOURNAL.md:181` recorded three break sites and concluded the profiling paths were "unverified". The true state is worse and now bounded: 21 error sites, one phantom type behind 111 of 117 errors, and once compiled, two profiling-only detectors that trap instead of reporting. The strongest numbers the engine can report — leak detection and usage overflow — currently reject a correct program.

## Scratch and cleanup

`/tmp/hbe-profile-probe` (seven source snapshots of `HEAD`, five CMake trees, run logs, 1.2 GB) deleted after this
document was written; `ls` confirms it is gone. `./build` and `./lib` in the checkout were never touched, the `Engine/`
working-tree changes stayed at the same 10 files owned by the comment-migration agent throughout, and the `docs/`
edits visible in `git status` belong to that agent, not to this probe.

