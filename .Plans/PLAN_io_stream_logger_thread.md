# Plan: the logger thread drives the IO stream

## Status

Proposed, not approved, not started. One decision is open (section 8) and it changes the shape of the budget work, so it
must be answered before commit 1.

## 1. Problem this closes

Measured, not theorised: the engine deadlocks when a log flush waits on work that a throttled stream has declined.

| Link | Where |
|---|---|
| A log flush blocks its thread until the logger's drain task runs | `Logger.cpp` flush waits for the drain task; it reports `the drain task is alive but has not written the queue` when the wait goes long |
| The drain task is enqueued on the IO stream | `Logger.cpp:208` takes `TaskSystem::GetIOTaskStreamIndex()`, `:247` enqueues onto it |
| A spent allowance declines general queue work | `CPUBudget::CanTakeWork`, and the suite asserts it: `A stream with a spent allowance declines the general queue and resumes after a window` |
| The IO stream's window re-opens only when the base thread runs the window pass | `TaskSystem::RunBudgetWindowPass` is the only writer of `RequestWindowAdvance`, and it runs on the engine loop thread |
| The thread running that pass is the thread blocked in the flush | cycle closed |

A first attempt at fixing this let each stream re-open its own window on its own clock. It was wrong and the suite proved
it within one run - `Worker1 charged a 1 ms allowance and took general queue work for 200 ms anyway` - because the
accounting window belongs to the base frame (R26/R27). Reverted. The window must stay owned by the base frame; the fix
has to move *who executes the IO stream*, not when windows turn over.

## 2. Target design

The logger owns a real thread that exists for the logger's whole lifetime, which starts before the task system and ends
after it. The IO stream stops being a thread-owning stream and becomes a driven stream, like the base stream. The logger's
thread loop calls a pump - a functor the task system installs - so the IO stream's queues are worked on by a thread that
can never be waiting on the IO stream.

```
Logger thread                         TaskSystem
-------------                         ----------
loop {                                BuildStreams:
  drain log queue inline;               Base -> no thread (unchanged in spirit)
  if (pump installed) pump(context);    IO   -> no thread, pump installed into Logger
}                                       Worker1..N -> own threads (unchanged)
```

Why this and not "logger uses the IO stream": a logger that depends on the subsystem it reports on cannot report that
subsystem failing. The bug above is exactly that case - an error line being flushed was itself unable to complete, which
is how a failing test turned into a five-minute wall-clock death instead of a red line.

## 3. Invariants to hold

| ID | Invariant | Why |
|---|---|---|
| I1 | The logger works before `TaskSystem::Init`, after the task system is gone, and with no pump installed. | "from the very beginning to very end of its lifecycle" - the pump is optional enrichment, never a dependency |
| I2 | Exactly one thread drives the IO stream, and it is the logger thread. | `threadID` claims, `IsIOThread()`, and the owner-thread budget asserts all rest on a single driver |
| I3 | The IO stream's queue is fully drained before the task system finishes tearing down. | There is no thread left to join, so nobody drains it implicitly |
| I4 | A thread blocked waiting on the task system keeps driving what it owns. | This is what the base thread already fails to do, and the direct cause of the deadlock |
| I5 | The accounting window still turns over on the base frame pass. | R26/R27, enforced by the tests named in section 1 |
| I6 | No allocation on the logger thread's drain path while the allocator is being torn down. | The logger outlives subsystems that own allocators |

## 4. The interface the logger thread calls, verified against the code

The shape is `ioStream.Update()` from the logger's loop. Read against the implementation, that call is sound but is not a
rename of the current loop body - four properties of `RunLoop` stop it from working as written.

| # | Finding in current code | Consequence for `Update()` | Design response |
|---|---|---|---|
| F1 | `RunLoop` obtains the task system through `Engine::Get()` (`TaskStream.cpp:302`). | The logger thread outlives `Engine`. An `Update()` that resolves the task system that way is a use-after-free at teardown, in exactly the window this design exists to protect. | `TaskStream` holds its own `TaskSystem*`, captured at registration. `Update` reads that and nothing global. |
| F2 | The idle path is `cv.wait_for(lock, 10ms)` *inside* the loop. | A driven `Update()` that blocks ten milliseconds stalls log draining by ten milliseconds, and the logger thread becomes the slowest thing in the process. | Split the two duties: `Update()` is one non-blocking pass and returns whether it did anything; `WaitForWork(budget)` is a separate, explicitly idle call the driver makes only when it chose to sleep. |
| F3 | `WakeUp()` is `cv.notify_one()` with no lock held. | Harmless today because the waiter times out at ten milliseconds anyway. Fatal for a driven stream, whose whole idle strategy is "sleep until woken": an unlocked notify racing a predicate check is a lost wakeup, and the symptom is log lines waiting out the sleep instead of being written. | The state change that a wake announces must be made under `queueLock`, and `WaitForWork` must wait on a predicate over that state - never a bare `notify_one` against a flag read outside the lock. |
| F4 | `streams` is an `HVector<TaskStream>` that is `Swap`ed in `BuildStreams` (`:532`) and `Clear`ed in `JoinAndClear` (`:144`). | A raw `TaskStream*` handed to a thread that outlives the task system dangles twice over: container mutation moves elements, and teardown destroys them. | Registration is bracketed: install the driver after the `Swap`, and withdraw it - waiting for an in-flight pass - before `streams.Clear()`. `TaskStream` must also be non-movable once registered, asserted rather than assumed. |

**One improvement on the waiting strategy.** The driver thread has two reasons to wake: log entries arrived, or the IO stream
has work. A thread can wait on one condition variable, so two waits with two timeouts would mean polling one of them - and
polling is where latency and lost wakeups hide. The clean form is to give the driven stream *the logger's* condition variable
for waiting purposes: the logger thread is the only waiter, so `WakeUp` on a driven stream notifies the logger's condition
variable, and every wake - log entry or IO item - lands on the same sleeping thread. Both producers then satisfy the same
predicate discipline under their own locks, and no timeout is load-bearing. This replaces F3's fix-up with a structure where
the failure cannot occur.

Two more that are not bugs but must be explicit, because a driver thread makes them visible for the first time:

| Concern | Requirement |
|---|---|
| Thread-local stream index | `RunLoop` calls `SetStreamIndex` once per thread. A shared driver must set the index before `Update` and **restore** it after, or the logger thread spends the rest of its life claiming to be stream 1 - which corrupts `GetCurrentStreamIndex()` for every task the IO stream runs, including the ones the delivery and drain tests read. |
| Allocator scope | `RunLoop` opens `AllocatorScope` once. `Update` must open and close it per pass, since the same thread also drains logs under different intent, and I6 forbids allocation on the drain path. |
| An unbudgeted IO stream lets a careless provider enqueue without limit. | The self-budgeting contract is stated on `TaskProvider::Produce` for the IO stream, `Update` takes at most one item per pass so an unbounded queue cannot starve the driver's log drain, and the assert on budget configuration forces the discussion into the open. |
| A reader mistakes the absence of an IO measurement for an IO measurement of zero. | Every per-stream budget report must distinguish "not budgeted" from "charged nothing" (section 10). |
| Diagnostic latency coupling | One thread now serves log writing and IO tasks, so a 500 ms file read delays the flush that a failing test is waiting on. `Update` handles at most one item per pass and returns, letting the driver drain logs between items; long IO tasks are then bounded by policy rather than by luck. |

## 5. Steps

### Commit 1 - `TaskStream::Update` and `TaskStream::WaitForWork`

- `Engine/Core/TaskStream.h`: declare `bool Update() noexcept` (one non-blocking pass, returns whether work happened) and
  `void WaitForWork(std::chrono::milliseconds patience) noexcept` (predicate wait on `queueLock`) public, documented as the
  driven-stream contract; keep `RunLoop()` as the thread entry for streams that own threads.
- `Engine/Core/TaskStream.cpp`: the once-per-thread setup - `SetThreadName`, the `TaskStreamDurationThreshold` `ConfigParam`,
  the scratch buffers, the window period - moves into a context object built by the driver of a thread-owning stream, and
  into stream members for a driven stream (there is no per-thread place to keep it). `Update` performs exactly one pass:
  window handling, pending allowance apply, lane takes, provider drain probe, at most one general queue item, re-add, inline
  delivery. It opens `AllocatorScope` and sets-and-restores the thread-local stream index around itself.
- `RunLoop` becomes the thread entry: claim `threadID`, build the context, loop calling `Update`, then `WaitForWork`.
  Behaviour must be identical to today - no thread is removed in this commit.
- `WakeUp` stops being a bare `notify_one`: whatever it announces is recorded under `queueLock` first (F3).
- `TaskStream` stores `TaskSystem*` instead of resolving `Engine::Get()` per loop (F1), and gains `Assert`-guarded
  non-movability once it has been driven (F4).
- Gate: three configurations green at 59 collections - this commit changes no thread ownership.
- New tests for the decision: configuring the IO stream's budget traps and names the self-budgeting contract; an unbudgeted
  stream never declines general work even after an enormous amount of work has run on it; the window pass leaves an
  unbudgeted stream alone.
- Proof this is not decoration: mutate `Update` to skip the general queue and confirm the suite fails by name.
- Gate: three configurations green, 59 collections. Behaviour must be identical - no thread yet is removed.
- Proof this is not decoration: mutate `PumpOnce` to skip the general queue and confirm the suite fails.

### Commit 2 - logger owns its thread

- `Engine/Log/Logger.h`: add the driver thread, a stop flag, and the hook pair `bool (*pumpHook)(void* context)` with
  `void* pumpContext`, both plain (not `std::function`) because the logger must not allocate here and the hook must be
  readable by a thread that outlives the task system.
- Install/withdraw API: `SetIOPump(hook, context)` and `ClearIOPump()`, the latter waiting for an in-flight pump to return
  - the same lock-held-across-call discipline R40 established for `Produce`.
- Thread body: drain the log queue inline; if a hook is installed, call it; brief sleep when both had nothing.
- Thread starts in logger initialisation, and stops in logger shutdown - after every other subsystem has had its final
  lines written, which is where the existing final flush has to move.
- Gate: three configurations, and a new test that the logger writes a line with the task system never started (I1).

### Commit 3 - the IO stream stops owning a thread

- `Engine/Core/TaskSystem.cpp` `BuildStreams`: for the IO index, do not call the thread-creating `Start`; register the IO
  stream with the logger *after* the `streams.Swap` that allocates the array, so the registered pointer cannot be
  invalidated by that reallocation (F4).
- `Engine/Core/TaskSystem.cpp` `JoinAndClear`: the existing `thread.joinable()` skip already avoids a bogus join. Withdraw
  the registration - which waits for an in-flight `Update` - **before** `streams.Clear()`, and drain the IO stream by
  calling `Update` inline until its lanes and general queue are empty. Withdrawal must precede the last log line teardown
  can emit only in the sense that nothing may need the IO stream afterwards; the logger's own final flush still has
  somewhere to run (I1).
- Ordering trap to respect: `ClearIOPump` must happen *after* the last log line the teardown will emit, or those lines
  buffer with nobody left to write them. Existing messages at `Logger.cpp:458/467` are the ones that describe this state;
  they must stay reachable as diagnostics, not become the normal outcome.
- Gate: three configurations, plus `IsIOThread()` true on the logger thread and false everywhere else, and the existing
  `IO has an unlimited allowance...` test still passing.

### Commit 4 - deadlock regression test, then the budget-gate test

- Test: throttle a worker and spend its allowance, then force an error-severity flush from a thread that is not the logger
  thread, and require the flush to return inside a bounded wait. Before commit 3 this test hangs - it is the reproduction
  already captured at `99f3634`; after, it must pass.
- Then re-attempt task #10's test (`A stream with a spent allowance does not ask its provider for work`), which is the
  sequence that uncovered the hang. Its acceptance proof is the mutant that swaps
  `if (!laneIsEmpty || !budget.CanTakeWork())` for `if (!laneIsEmpty)`; that mutant survives every test in the suite today.

### Commit 5 - documentation

- `docs/TaskSystemRedesign.md`: R41 (the IO stream is a driven stream, driven by the logger thread, which it never owns),
  R42 (the IO stream is not CPU-budgeted; tasks and providers on it budget themselves), R43 (why the logger cannot depend on
  the task system: it must survive the failure it reports), R44 (a thread blocked waiting on the task system keeps driving
  what it owns - the rule whose absence caused the deadlock), R45 (one waiting predicate for a driven stream: its wake
  notifies the driver's condition variable, so no timeout is load-bearing).
- Module pages: `docs/TaskSystem/index.html` for `TaskStream::PumpOnce` and the driven-stream lifecycle; the logger page for
  the driver thread and the hook contract.
- `.Plans/TODO_task_system.md`: #10 unblocked and closed, #12 closed as the deadlock itself.

## 6. Verification

| Check | Method | Pass condition |
|---|---|---|
| Build | `cmake --build build --config <Debug\|Dev\|Release> --target EngineTest` | 0 `error:` |
| Behaviour | `build/gate/gate.sh <Config> 300` | exit 0 and `EngineTest: all 59 collections passed`, plus the new tests |
| Stale-artifact guard | `gate.sh` refuses on build error or a source newer than the binary | never runs a binary that did not just build |
| Wall clock | runner reports `128 + signal` on signal death | a hang is visible as a hang, not as a pass |
| Test power | mutation per commit, in the file that owns the code, with substitution, ownership and relink all verified | every mutant killed by a named test |
| Style | `.pi/skills/hb-standards/scripts/check.sh --staged --no-build` | 0 mechanical violations |

## 7. Risks

| Risk | Mitigation |
|---|---|
| Log lines emitted *from* the IO stream re-enter the queue that the same thread is draining. | The drain path stays inline-append only, and the pump never waits on the logger's own queue. |
| IO work becomes as latency-sensitive as log writing, and log bursts delay IO tasks. | Bound work per pump pass for both sides, and measure: log drain and IO pump alternate rather than one draining unbounded. |
| Teardown ordering: a stream destroyed while the logger thread is one pass inside its `Update`. | Withdrawal waits for the in-flight pass, mirroring R40 where the queue lock is held across `Produce`, and precedes `streams.Clear()`. |
| Lost wakeup, so log lines wait out the driver's sleep instead of being written. | F3: the state a wake announces is recorded under `queueLock`, and `WaitForWork` waits on a predicate over it. |
| The logger thread permanently claims stream index 1 because a thread-local was set once and never restored. | `Update` sets and restores the index around every pass. |
| A long IO task delays a flush a failing test is blocked on, turning a red line into a slow red line. | One item per `Update` pass, and the driver drains logs between passes; long IO tasks are bounded by policy. |
| `PumpOnce` becoming a second execution path that drifts from `RunLoop`. | `RunLoop` keeps no logic of its own; it is the thread entry around `PumpOnce`, one context, one body. |
| The logger's thread priority starves real IO work, or vice versa. | The thread keeps the task system's worker priority baseline (`SetThreadPriority(thread, 0)` equivalent) and the choice is stated in the design document rather than discovered. |
| Allocator lifetime on a thread that outlives subsystems. | I6: no allocation on the drain path; the pump's scratch buffers belong to the stream and are created under its allocator while the allocator is alive. |

## 8. What this deliberately does not do

- It does not change when accounting windows turn over. That is the base frame's, and commit-time evidence says so.
- It does not make logging budget-exempt by classifying log work specially; the logger gets a thread instead, which is a
  weaker coupling rather than a special case inside the budget.
- It does not move asset or file IO off the IO stream; only the thread that drives it changes.

## 9. Decision: the IO stream is not budgeted

Decided by the owner. The IO stream runs completely asynchronously, and every task or `TaskProvider` attached to it handles
its own budget. Three consequences follow, and they are load-bearing rather than incidental:

| Consequence | Reason |
|---|---|
| The CPU allowance never applies to the IO stream. | The allowance measures CPU time spent inside tasks. The IO stream's work is waiting on devices, so charging it would ration the wrong resource - and rationing it is what deadlocked the logger. |
| Throttling is the task's own duty, not the stream's. | An async loader knows how many reads it may hold open; the stream cannot know that. `TaskProvider::Produce` on the IO stream is therefore documented as "return nothing when you have no capacity", which is what its `nullopt` contract already means (R39). |
| The budget window has no meaning for the IO stream. | The window re-opens an allowance. With no allowance there is nothing to re-open, so the window pass skips the IO stream rather than signalling it for show. |

## 10. Implementation the decision adds

- `TaskStream` gains a per-stream `isBudgeted` flag, set from the stream index at construction: true for worker streams,
  false for the IO stream. The general queue gate, the lane gate, and the provider drain probe all consult it, and
  `MayTakeNewWork()` answers unconditionally true for an unbudgeted stream.
- `TaskStream::ConfigureBudget` and `TaskStream::RequestBudget` assert when `isBudgeted` is false, naming the stream and
  pointing at the self-budgeting contract. This is the only new authority in the design: nothing may quietly throttle the
  stream the logger depends on.
- `TaskSystem::RunBudgetWindowPass` skips unbudgeted streams.
- Nothing about worker budgeting changes: the accounting window stays the base frame's (R26/R27), and every existing budget
  test keeps its meaning.
- Reporting: `CPUBudget::GetAccumulated` on an unbudgeted stream is meaningless, so any diagnostic that prints per-stream
  budget usage must say "not budgeted" rather than print a number that looks like an measurement.
