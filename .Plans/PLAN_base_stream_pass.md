# Plan: the base-stream pass — budget reopen, container swap, fold

Handoff item 2, chosen by the owner as "as written, skeleton and all" (2026-09-19).

## 1. Goal

One pass on the base thread, run on a cadence, that per stream:

| Step | Contract it implements | What it does |
|---|---|---|
| Reopen the budget | R1, R2 | Calls into `CPUBudget::Reset()`, which today has **no production caller** — a stream given an allowance stops dequeuing forever |
| Swap the pair | R3, R11 | Under a short lock, hand the filled container to the drainer and give the worker the other one |
| Fold what was taken | R3, R16 | Walk the drained slots without the lock, apply the invariants, rewind |

Plus the smallest thing that makes the fold testable with real traffic: a worker-side append entry point (R3's
worker half). Without it the fold drains empty containers and cannot be proven, which makes it the decoration the
project standards reject rather than a skeleton.

## 2. Design decisions taken here, and why

### 2.1 Cadence: elapsed quantum, not loop iteration

`Engine::Run()` (`Engine/Engine/Engine.cpp:139-143`) is a `while` over `ProcessMainThreadTasks()` + `yield()`. It
has no frame. Resetting a budget every spin iteration means a budget can never gate anything, so the pass is driven
by elapsed time: one pass per `TaskSystem::DefaultFrameQuantum`, a tunable atomic parameter, **default 1 ms**.
A 1 ms allowance then means what it says: one window per pass.

### 2.2 The swap takes its own lock, and `Append` takes that same lock

`queueLock` guards the two queues and the condition variable; taking it on the base thread would couple the swap to
workers sleeping in `cv.wait`. A separate `resultLock` guards the active-container index.

R11's "no lock inside the allocator" is about the bump allocator's internals — no free list, no atomics per slot.
One uncontended mutex acquire per result append is orders of magnitude cheaper than the 128-byte write that follows
it, and it is the only thing that makes a slot's claim indivisible against a swap. The alternative — an atomic
active-index read in `Append` — lets a worker load the old index and write into a container the base thread has
already taken for draining, which is a lost result with no diagnostic.

### 2.3 A task's declared results can still straddle a swap; that must be loud, not silent

Slot-level atomicity does not make a *group* of N appends atomic. The owner declined swap-only-at-quiescence as a
separate option, so the straddle is possible by choice, and the rule here is: detect it, do not lose data.

`TaskStream::AppendResult` captures the container epoch when a task claims its first slot. If the epoch has changed
before the declaration is complete, the remaining slots go into the new container — nothing is lost, they arrive one
window later — and the stream logs one error naming the task, the declaration and how many slots landed in which
window. The epoch exists for exactly this (R11: "a reference to the run that just ended is recognisable as stale").

### 2.4 The fold's invariants, chosen so each is checkable with no join counter

| Invariant | Why it holds without item 3 |
|---|---|
| Drained slot count equals the slots claimed in that epoch | `GetCount()` and `GetEpoch()` are the container's own; nothing else is needed |
| Every drained slot was claimed under a container that had room | `Append` asserts room; admission guaranteed it (R13) |
| A task declaring zero results costs nothing | R16: zero-output tasks are real and never capacity-blocked |
| A pass with nothing drained is a no-op that still rewinds | Makes an empty fold observable rather than indistinguishable from a skipped pass |

## 3. Checklist

Verification for each is a red test first, then green; every new check gets a mutation (a deliberate break of the
implementation) with per-test attribution, because several "passing" results in this subsystem were only ever proven
that way.

| # | Step | Verification |
|---|---|---|
| 1 | Measure `CPUBudget`: confirm `Reset()` has no production caller and `isMeasuring` is a plain `bool` | grep evidence in the commit message |
| 2 | `isMeasuring` becomes atomic; the single-owner note in `CPUBudget.h` is amended per R1 | `check.sh` + three-config build; existing `CPUBudgetTest` stays green |
| 3 | `CPUBudgetTest`: a spent budget reopens after `Reset()` — and a **stream** that had spent its allowance takes work again | New test, mutation: make `Reset()` a no-op → red |
| 4 | `TaskStream::ResetBudgetForNewFrame()` + `SwapResultContainers()` under `resultLock` | New tests: budget reopens; active container index flips; the container the drainer took is the one the worker filled |
| 5 | `TaskStream::AppendResult()` — the worker's entry point, appending under `resultLock`, asserting room | Mutation: drop the lock → TSAN-free but detectable by the straddle test going non-deterministic; assert-room mutation → red |
| 6 | Straddle detection via epoch, logged once, no slot lost | Test: append, rewind, append again → exactly one error line naming task and split; mutation: remove the epoch capture → red |
| 7 | `TaskSystem::BeginFrame()` iterates streams: reset, swap, fold; fold walks slots and rewinds | Tests: empty pass is a no-op that rewinds; a pass with slots folds them and rewinds the taken container's epoch; the worker keeps appending into the other container meanwhile |
| 8 | Cadence parameter `Task.FrameQuantumMS`, atomic, default 1 ms; `Engine::Run()` calls `BeginFrame()` when the quantum elapsed | Test: the figures read back the decided literals (written as literals, tied by `static_assert` — a comparison against the constant that produced the value cannot fail) |
| 9 | Guard (b) from my own guard list: a capacity-closed lane is logged with declared need and room remaining | Lands only if step 7 touches dequeue-time admission; otherwise stays open and is stated as open |
| 10 | Three-configuration gate: Debug/Dev/Release build 0 `error:`, runner exit 0, `all <N> collections passed`; `check.sh --staged --no-build` 0 violations | Actual command output quoted in the report |

## 4. Risks and what I will not do

- **The fold has no consumer.** Accepted by the owner's choice. The fold therefore validates and rewinds; it does not
  invent a delivery mechanism. When item 3's join counter lands, the fold grows the per-slot hand-off.
- **R12 growth on the reset pass stays unbuilt.** `Reset()` may grow a buffer by a figure from a `TaskDescriptor`,
  and no descriptor exists. Growing on a guess would be inventing the layer R19 explicitly refuses. Left open.
- **R14 lane closure stays unbuilt**, so guard (b) has nothing to log. Left open, named in the report.
- **The suite runs inside a task on the base stream.** Once folding happens on the base stream, a test that blocks
  waiting on its own stream's results stands on the folding thread. No `Wait` removal in this commit; if a test
  deadlocks because of the new pass, that is R8's known consequence and will be reported, not hidden with a sleep.
- **Not pushed.** Ever, without explicit permission.
