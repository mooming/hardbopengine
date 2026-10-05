# PLAN: contended benchmark for ThreadSafeMultiPoolAllocator

## Goal

Produce measured contention numbers for the current single-mutex design so the choice between the five locking
designs recorded in `JOURNAL.md` (2026-10-05 10:08, "Open decision recorded") is made on data. Today the only
number in existence is single-threaded: 0.020381 s against `std::malloc` 0.019169 s.

Deliverable is measurement, not a fix. No production (non-`__UNIT_TEST__`) line changes.

## Where the code goes

`Engine/Memory/ThreadSafeMultiPoolAllocator.cpp`, inside the existing `#ifdef __UNIT_TEST__` block at the bottom,
as new `AddTest` testlets in `ThreadSafeMultiPoolAllocatorTest::Prepare`.

**Do not edit `ThreadSafeMultiPoolAllocator.h`.** A new member function in a header is a new API entry, and
`docs_coverage.py` then demands a reference page for it. Shared helpers go in the unit-test block as `static`
free functions in `namespace hbe`, not as class members.

## Measurements

| Id | What it measures | Method | Why it decides something |
|---|---|---|---|
| M1 | Shared-instance scaling | For N in {1, 2, 4, min(hardware_concurrency, 8)}: N threads allocate `OpsPerThread` blocks through **one shared** `ThreadSafeMultiPoolAllocator`, then free them in a permuted order. Report ns/op and scaling efficiency `(opsPerSec(N) / opsPerSec(1)) / N` | The mutex hypothesis: efficiency collapsing toward `1/N` means the lock, not the allocator, is the cost |
| M2 | Two baselines at the same N and the same workload | (a) each thread uses `std::malloc` and `std::free`; (b) each thread uses **its own private** `MultiPoolAllocator` that nothing else touches | (a) is the platform's per-thread cache, (b) is the sharded upper bound. If (b) is much faster than M1, thread sharding has real headroom and option C is worth its risk |
| M3 | Cost of a cross-thread free | Thread A allocates a batch, a barrier hands it over, thread B frees it. Compare ns per free against the same-thread free of the same batch | Every lock-free option must pay this on a foreign free. If it is already cheap, a shared list is affordable; if it is the dominant cost, it is not |
| M4 | Latency spike from bank growth | With the size class exhausted, one thread creates a bank while the others allocate. Time **each individual** allocate and report the maximum and the worst 1 percent | The current lock is held across `GenerateBank`, which takes a whole `DefaultBankUnit` (1 MiB) from the parent at `ThreadSafeMultiPoolAllocator.cpp:446-455`. This is the price the other threads pay for that |

## Workload definition, fixed so runs are comparable

- `OpsPerThread = 20000`. Total work stays near a few million operations, which keeps the whole addition well
  under five seconds.
- Size mix is a fixed round-robin over `{16, 24, 32, 48, 64, 96, 128, 256}`. No random number generator inside a
  timed region: sizes are an index function, and the free permutation is a fixed coprime stride such as 7, so
  runs are reproducible without an RNG.
- Initial configurations for the allocator under test: `{{16, 16}, {32, 16}, {64, 16}, {128, 16}, {256, 16}}` so
  every size in the mix has a home and growth is exercised only where the measurement wants it.
- Pointer slots are one `std::vector<void*>` of `numThreads * OpsPerThread` elements, allocated and resized by the
  testlet thread **before** any timed region, then handed to each worker as a `std::span<void*>` slice. Nothing in
  a timed region may allocate memory for the test itself.
- Timed region calls `allocator.Allocate(n)` and `allocator.Deallocate(p, n)` **directly**, not through
  `MemoryManager`. The number must be attributable to this class; `MemoryManager`'s proxy dispatch is an extra hop
  that the log line must state it excludes.
- Each worker opens `AllocatorScope` only where the test needs the ambient scope to be the allocator under test;
  for direct calls no scope is needed, and worker threads otherwise default to `MemoryManager::SystemAllocatorID`
  through `MemoryManager::scopedAllocatorID`.

## Rules that must not be broken

| Rule | Source | Note |
|---|---|---|
| No testlet may exceed 64 KiB of retained global heap at its end | `TestCollection::MaxRetainedGlobalBytes` | Free every block, `join` every thread, destroy the allocator before the testlet returns |
| A testlet closure must fit 48 bytes | `TestCollection::MaxClosureBytes`, `static_assert` in `Testlet.h:100-110` | Keep captures to a few pointers; call `static` helpers instead of inlining large bodies |
| Test names are `char` arrays of at most 128 bytes | `Testlet::MaxNameBytes` | Names must be string literals |
| Every wait carries a deadline | `WaitUntil` in `Test/TestCollection.h:35` | A stall must become a reported failure, never a hang |
| No comments in `.cpp`, tabs, Allman braces, blank-line table A1 to A16, include grouping B1 to B5 | `AGENTS.md`, `docs/CodingStandards.md` | The trailing `#ifdef __UNIT_TEST__` include region stays where it stands; add new includes to that region |
| Log through the testlet stream `ls` with `lf`, `lfwarn`, `lferr` | existing testlets in the same file | |

## Pass and fail policy for these testlets

Today these are **measurements, not assertions**. Report numbers, and use `lfwarn` when scaling efficiency drops
below 0.5 or when the shared instance is slower than `std::malloc`. Do not add `lferr` thresholds: a ceiling
invented before the redesign exists is a number with no basis, and it would make the suite red for a known
condition. The redesign task owns the thresholds, once a target is measured.

## Verification

1. `./build.sh Applications/EngineTest -dev -test` then run `./build/Applications/EngineTest/Dev/EngineTest`, and
   the same for Release, because `-test` is what makes the unit-test sources compile at all.
2. Capture the new testlet output lines plus the suite's own `Collection Result` line.
3. Style gates on the touched file, reporting new findings against the HEAD baseline:
   `.pi/skills/hb-standards/scripts/blank_lines.py`, `includes.py`, `comments.py`, `layout.py`.
4. Full suite must still end `SUCCESS` with the same collection count as before (60 collections, 382 testlets,
   plus the testlets added here).

## Recorded decision this benchmark feeds

Option A deterministic size class, B tagged-index lock-free shared stack, C thread-sharded chunks with
return-to-owner, D wait-free lease by `fetch_add`, E bounded spin lock. Only C has a fast path that touches no
atomic, so the single number that matters most is **M2(b) against M1**: it is the headroom sharding buys.
