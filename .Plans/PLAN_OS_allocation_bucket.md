# PLAN: Count OS-image allocations as a separate bucket in the global memory monitor

## Goal

`WindowTest::TC0.Create Window` fails the 64 KB retained-global-heap ceiling because creating
the first window boots macOS system caches (Metal shader archive, AGX device registration,
CoreText font caches, AppleEvents tables) that live for the process lifetime. The engine
replaces global `operator new`, so these are accounted to whichever testlet triggers them.
The owner ruled this system behavior that cannot be freed, and directed that OS allocations
be **counted specially** rather than dropped or folded into the engine total.

## Design

Classify every `operator new` / `operator delete` call by the image its immediate caller
belongs to. On Apple platforms an allocation whose caller return address lies inside the
dyld shared cache (`_dyld_get_shared_cache_range`, one contiguous ~7 GB range holding every
system dylib including libc++) is an **OS-image allocation**. Everything else — the
executable, engine static libraries, third-party dylibs outside the shared cache — stays as
engine-accounted.

Verified by probe (`/tmp/classify_probe.cpp`):
- range lookup is one load + two comparisons; return-address read is a register read;
- engine-inlined call sites classify engine, libc++ out-of-line sites classify OS, at O0 and O2;
- `_dyld_get_shared_cache_range` is a libSystem export without an SDK header declaration —
  declare it `extern "C"` locally.

**Buckets.** Keep the existing global counters as the full total (no visibility lost) and add
four OS-side atomics incremented *in addition* when the caller is an OS image:
`globalOSAllocationBytes/Count`, `globalOSFreeBytes/Count`. The engine bucket is derived:
`global − OS`. New `MemoryManager` getters: `GetOSAllocationBytes`, `GetOSAllocationCount`,
`GetOSFreeBytes`, `GetOSFreeCount`.

**Deallocations** classify by the caller of `operator delete`, symmetric to allocation. OS
caches are released by OS code, so buckets stay consistent; cross-boundary frees drift bytes
between buckets but never corrupt the total. Documented, bounded.

**Ceiling guard.** `TestCollection.cpp` compares *engine-derived* retention against
`MaxRetainedGlobalBytes`; the OS split is printed on every per-testlet heap line and in the
process summary so nothing is hidden. `WindowTest::TC0` then measures its real ~7 KB engine
retention and passes without raising the ceiling.

**Documented bias.** Engine code whose immediate `operator new` caller is an out-of-line
libc++ frame (e.g. `std::string::__grow`) counts as OS. This moves bytes out of the engine
bucket only, so ceilings become looser, never stricter. Goes in the MemoryManager contract
page and a design document paragraph.

**Non-Apple platforms.** Classification returns "engine" — OS counters stay zero, behavior is
today's exactly. Windows equivalent (module snapshot denylist) explicitly deferred, noted in
the design doc.

## Steps

| # | Step | Verification |
|---|---|---|
| 1 | `GlobalAllocation.cpp`: caller plumbing through `AllocateAccounted`/`TryAllocateAccounted`/`AllocateAccountedAligned`/`Deallocate`, shared-cache range helper (Apple-only), four OS atomics, `MemoryManager` getters | three-config build |
| 2 | `TestCollection.cpp`: engine-derived retention for the ceiling guard; OS split on per-testlet log line; `TestEnv`/`TestMain` summary lines carry the OS total | EngineTest run |
| 3 | Reference pages: four new getter pages + amended contract on the global-counter pages, TestCollection ceiling-semantics update, Memory design-doc paragraph with the libc++ bias; `docs_coverage.py`, `docs_methods.py`, `htmlcheck.py` green | doc gates |
| 4 | Unit test in the Memory collection (Apple-only): engine container grows engine bucket; a >SSO `std::string` grows OS bucket and leaves engine bucket unmoved | EngineTest |
| 5 | Full gate: `./build.sh Applications/EngineTest -dev -debug -release -test`, run Debug and Dev, hb-standards scripts on touched files | `WindowTest` passes at the default 64 KB ceiling; no other collection regresses |
| 6 | JOURNAL.md entry, commit per step | git log |

## Acceptance

- `WindowTest::TC0` passes with the ceiling untouched at 65,536.
- Totals unchanged in meaning: `global = engine + OS` identically.
- All 58 previously passing collections still pass; exit code 0.

## Out of scope

- Windows/Linux classification.
- The TEMP-ALLOC-TRACE tool (left as-is; it captures everything regardless of bucket).
- The four performance warnings (allocator/containers slower than STL) — separate issue.
