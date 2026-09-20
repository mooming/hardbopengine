# Plan B — Task system refactor: provider/stream job graph

**Task B of two.** Companion: [`PLAN_defect_board_partial_init.md`](PLAN_defect_board_partial_init.md).
Design and rationale live in [`docs/TaskSystemRedesign.md`](../docs/TaskSystemRedesign.md) — **read it first; this plan does not duplicate it.**
Parent record: [`PLAN_single_executable_app_registry.md`](PLAN_single_executable_app_registry.md).

---

## 0. Scope

**In.** `Engine/Core` task internals: `Task`, `RangedTask`, `TaskStream`, `TaskSystem`, `TaskStreamAffinity`, and `Engine::Run()`. Replace the range-splitting model with the provider/stream job graph.

**Out.** The application registry, descriptors, catalogue, MakeBuild, per-application arenas, CLI, the SPIR-V tool — all parent-plan steps 3–9. None of them depend on how tasks are scheduled.

**Depends on:** Task A, for one thing only — `EInitLevel`, so tests and tools can construct an engine without the window server. Everything else is parallel.

---

## 1. The one seam with the registry work

The two workstreams touch at exactly one point: **how an applet is driven.** Everything else is orthogonal, which is why they split cleanly.

| Registry layer | Driven by tasks? | Survives the refactor untouched |
|---|---|---|
| `ApplicationRegistry`, `ApplicationDescriptor`, catalogue | no | yes |
| `.module.config` `application =` key, generated `.def` | no | yes |
| Per-application arena + `AllocatorScope` | no (but see risk R4) | yes |
| CLI / TUI dispatch | no | yes |
| **`EngineApplication::Run()`** | **yes** | **no — becomes a `TaskProvider`** |
| **Host start/pump/stop order (parent plan §6)** | **yes** | **no — re-derived here** |

**Consequence for ordering.** Do the registry work against the *current* `EngineApplication::Run()` = "the applet owns its loop" model (which is what the examples already do), and accept that this plan replaces the **driver** and nothing above it. Rework surface = one class's method set plus the host's start/stop path. Rewriting the applet interface before the registry lands would mean designing it twice. **Refinement (2026-09-14):** steps 6/7 should target **B6a**, not today's semantics — see §2.2.

---

## 2. Fact that blocks the current design (fix first)

`Engine::Run()` is **not a frame pump** — it blocks until shutdown is requested:

```cpp
while (taskSystem.GetMainThreadTaskQueue().HasPendingTasks() || taskSystem.IsRunning()) { ...yield(); }
taskSystem.JoinAndClear(); ... application.reset();
```

`isRunning` is set at `TaskSystem.cpp:271` and cleared by `RequestShutDown()` at `:101`. Only `EngineTest/TestMain.cpp:15` calls it; the two windowed examples never do — they run their own loops and call `ShutDown()`.

Any design that says "the host pumps `Engine::Run()`" *and then* issues `RequestShutdown()` on the same thread deadlocks. This is corrected in the parent plan §6 already; **B6a/B6b (§2.2) are where the semantics actually change.**

### 2.1 Target semantics — `Engine::Run()` pumps ticks to major systems

Owner decision (2026-09-14): `Engine::Run()` **is** the per-frame tick pump, and it drives the major systems — **task system, renderer, and log flush**. Each row is today's mechanism and its target.

| System | Today | Target |
|---|---|---|
| Task system | `Engine::Run()` spins on `IsRunning()` until shutdown; streams self-drive on their own threads | `Engine::Run()` advances one budget frame and dispatches the tick to providers/streams |
| Renderer | `VulkanRenderer::Render(float deltaTime)` (`VulkanRenderer.h:72`) is called from the **application's** own loop (`VulkanExample/Main.cpp`) | ticked by the frame pump, which supplies the delta; heavy render work still runs on the Render Task Stream |
| Log flush | a **self-rescheduling runnable on the IO stream** (`Logger.cpp:205-229` — the runnable calls `ProcessBuffer()` and returns `0`, so it is re-added forever) | one of the systems ticked per frame |

**Evidence this is a real gap, not a preference:** `Applications/VulkanExample/Main.cpp:106` calls `Logger::Get().Flush()` by hand every frame. An application manually flushing engine logs *is* the symptom of flush not being an engine-ticked system — that line is meant to disappear.

**`Render(float deltaTime)` needs a frame delta**, which is exactly what G1/G2 must produce. A tick pump with no clock cannot tick a renderer.

> **Constraint on B6 — do not delete the inline drain.** `WaitForFlush` and the inline-drain path from step 2 (`5677fce`, `95053e5`) exist for the windows where the pump is **not** running: before `Initialize`, after the pump exits, and on the fatal path (`Debug.h` calls `FlushLogs()` at five assert/fatal sites). Frame-ticked flush *adds* a periodic driver; it does not remove the need for an out-of-pump flush. Corollary: once flush is ticked, D6's `GetIOTaskStream().WakeUp()` becomes a flush tick rather than a stream wake — see Task A §A1.

### 2.2 Sequencing consequence — B6 is split

Parent-plan steps **6 and 7 need to know whether an applet owns its loop or is ticked**, and that is a Task B answer. To avoid writing the host launch order twice:

- **B6a (early, small):** `Engine::Run()` becomes a tick pump over the *existing* systems while applets still own their loops — an applet's loop calls the tick. **Parent-plan steps 6/7 target this model.**
- **B6b (later):** frame ownership moves to the pump, applets become `TaskProvider`s, applet-owned loops are removed. Parent-plan §6's either/or table becomes obsolete here and is re-derived once.

---

## 3. Decision gates — resolve before the step that needs them

These are open in the design doc (§10). Do not silently pick one; each is recorded as a decision where it is made.

| # | Must be decided before | Options / note |
|---|---|---|
| G1 ✅ | B1 | **Decided 2026-09-16** — see G1 below for the semantics |
| G2 ✅ | B1 | **Decided 2026-09-16** — one `steady_clock` epoch owned by `Engine`, read by all; see the G2 section below |
| G3 ✅ | B2 | **Decided 2026-09-16**, with sub-gate **G3b** also decided — context struct (stream handle + engine clock reading, no budget); `Produce` returns whether it produced anything. See below. |
| G4 ✅ | B5 | **Decided and acted 2026-09-16** — mutex where there really are several producers (D10 fixed now), SPSC lock-free only where one producer is provable; see below |
| G5 ✅ | B7 | **Decided 2026-09-16** — not retired as a capability: `TaskSystem` gains `ParallelFor`; see below |

### G1 — decided: measured CPU duration, accumulated per stream, budgeted against the base stream's frame period

Four parts, each load-bearing:

1. **Configured in seconds, consumed in CPU cycles.** A caller states a budget as a duration - a wall-clock-shaped quantity people can reason
   about and put in a config file - and the scheduler translates it into CPU cycles at the point of use. No one configures cycles, and nothing
   compares a cycle count against a wall-clock deadline.
2. **Budgets are decoupled from frames.** A budget is not "this frame's allowance", so missing a budget does not drop a frame's work; the stream
   simply stops taking more until its accumulation falls back inside the window.
3. **Each `TaskStream` measures the duration of the tasks it runs.** The accounting is measured, not estimated from task count or a static cost
   table - which is the only version that stays honest when task costs vary.
4. **The stop-dequeueing rule:** when a stream's accumulated measured duration exceeds the target frame period of the **base stream**, it stops
   dequeueing. The yardstick is one engine-wide number derived from the base stream's target framerate; the accumulation is per stream.

Consequences B1 has to carry, recorded so they are not discovered later:

- Measuring a task's CPU duration needs per-thread CPU time, not `steady_clock`: `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` on macOS/Linux, and the
  Windows equivalent belongs behind an OSAL wrapper rather than a call site `#ifdef`.
- Measured duration is only meaningful when the thread that ran the task is the thread being measured, so the measurement and the dequeue live on
  the same stream thread. A task that blocks does not accumulate cycles - which is the point of choosing cycles over wall-clock.
- The base stream's target frame period becomes a first-class value the other streams read. Where it lives, and who may change it, is a B1 design
  question; it is the same epoch/clock surface as G2.
- "Stops dequeueing" must still leave already-produced outcomes flowing: guard on *taking new work*, never on *delivering results*, or a stalled
  stream becomes the deadlock the design rules exist to prevent.

### G2 — decided: one `steady_clock` epoch, owned by `Engine`, read by everyone

One engine epoch, expressed over `std::chrono::steady_clock`, and every consumer reads that same value: task deadlines, `Render`'s `deltaTime`,
and the log-timestamp baseline A2 introduced. No module keeps a private epoch and no caller converts between clocks.

Why not the alternatives, so the reason outlives the person who chose it:

- *Keep `SystemStatistics` as the source* — it stores its start as `time::TTime` over `high_resolution_clock`, which is `system_clock` on some
  standard libraries, while the log module is uniformly `steady_clock`. Converting across the two is a unit error that compiles and is silently
  correct on whichever toolchain wrote it; that is precisely why A4 took the instant from the log's own clock instead of converting.
- *`high_resolution_clock` everywhere* — would retype the log module onto a clock that is not monotonic on every standard library, trading a real
  correctness property for one fewer alias.
- *A Core singleton independent of `Engine`* — a second thing that gets to decide when the engine began.

Consequences B1 has to carry:

- **Ownership is `Engine`; storage must be reachable without it.** `Engine::Get()` asserts an instance exists, and logging has to keep working when
  none does — the D1/D2 constraint. So the epoch is reachable without `Engine::Get()`, is set once by `Engine`, and falls back to "captured on
  first use" when nothing set it, which is already the shape `LogUtil`'s A2 default has.
- `SystemStatistics::startTime` derives from the epoch instead of sampling `high_resolution_clock` in its constructor. `time::TStopWatch` stays as
  the elapsed-time clock; it simply stops being the origin of the engine epoch.
- **This makes D8 fixable rather than merely noted:** with one readable epoch, `Engine::Log`'s private re-derivation of the timestamp has nothing
  left to be private about and folds onto `LogUtil`.
- `deltaTime` for `Render` is the difference between successive pump ticks on this clock, not a number a stream invents.
- Distinct from G1 by design: the epoch is a monotonic clock for deadlines and deltas, budgets accumulate **CPU cycles**. Reading a deadline off
  the same counter as a budget is the mistake this separation exists to prevent.

### G3 — decided: `Produce` is handed a context struct, and nothing about the budget

`Produce` takes one context value per call carrying two things: a handle identifying the stream it produces into, and a reading of the engine
epoch from G2. Deliberately **not** handed: remaining budget.

- The stream already decides whether to call `Produce` at all — that is G1's stop-dequeueing rule. Handing the provider a remaining-budget figure
  invites a second layer to self-limit with the same number, and when two layers gate on one quantity only one is the truth while both look
  authoritative.
- The stream handle is what B5 needs: an outcome must know the stream it is delivered back to, and a provider that cannot name its origin cannot
  enqueue a cross-stream successor.
- The clock reading is passed rather than pulled so a provider never reaches for `Engine::Get()`. G2 records that constraint for logging; it
  applies with more force to providers, which run on stream threads.
- One struct rather than loose parameters, so B2 can add a field without rewriting every provider. The struct's name is B2's to choose — naming it
  here would be deciding an interface twice.

**Owner's follow-up constraint, load-bearing:** the task system **cannot stop a single task**. Once taken, a task runs to completion; there is no
per-task cancel or preemption. The unit of control is therefore the **provider drain**, and a provider is drained until it reaches its budget.
Consequences, all binding:

- Budget is checked **between** tasks, never during one. A task already running overshoots rather than being truncated.
- Overshoot is bounded by the longest single task, not by the budget: 2 ms of budget attached to a 50 ms task buys nothing. Task granularity is a
  caller obligation and the budget's meaning depends on it — recorded as **R8**.
- Nothing may assume "budget exhausted ⇒ nothing in flight". Any teardown, detach or abandon path that assumes it is wrong in precisely the way
  the design's abandonment rules are careful not to be.
- **G3b — decided:** `Produce` returns whether it produced anything, and `false` ends the drain for this tick. The budget says *may I take more
  work*; the return says *is there work*. Two different questions, so unlike remaining budget there is no second layer gating on one quantity.
  The rejected alternative was a `HasWork()` the stream asks first: that is two calls where the first answer is already advisory by the time the
  second runs, since provider state can change between them — the stream would be steering on a reading it cannot trust. Idle providers therefore
  pay one call per drain, which is the honest price of not maintaining a second source of truth about a provider's intent; if that cost ever
  matters, guardrail 5's "no preemption" means the fix is a registration surface measured against a real stall, not a guess now.

### G4 — decided: mutex where producers are several, lock-free SPSC only where one is provable

- **Policy for what Task B introduces:** a stream's enqueue path is lock-free single-producer/single-consumer **only when the single producer is
  provable**, and a mutex otherwise. MPMC lock-free is not the default in either case.
- *Provable* means provable by construction and stated where the queue is declared — "only the owning stream thread enqueues here", or "exactly one
  named producer thread, by design". It does not mean "it looked single-producer in the run I did". That distinction is the whole content of D10:
  `MainThreadTaskQueue` claimed thread safety in its header and had no synchronisation, and the producer really was a task-stream thread while the
  main thread drained it.
- **The live race was fixed immediately (`567a987`) rather than left for B5**, because it is reachable today and B5 does not remove it. Proven with
  ThreadSanitizer on a two-thread repro before the change (`Push` racing `IsEmpty`/`Pop` on the same object) and clean after; the fix keeps the
  container under the lock but invokes task functions outside it, since queued work may itself schedule more main-thread work.
- **The fork this forces on B5, recorded so it is not rediscovered there:** an outcome delivered across streams is an enqueue into a queue whose
  producer set is then *owner plus deliverer* — at least two producers, so not SPSC, so a mutex unless B5 changes the shape. The alternative shape
  is a per-stream intake that only the owning thread drains from a structure written by one thread. B5 must pick one deliberately; stumbling into
  whichever one is convenient mid-implementation is how D10 happened.

### G5 — decided: `TaskSystem::ParallelFor`, which waits asynchronously and collates

The call sites were found before choosing, as this gate required, and they separated two things the gate's wording had lumped together:

| Measured | Evidence | Consequence |
|---|---|---|
| Every range-splitting call site is test-only | `GenerateSubTask(i, i + Increment)` at `TaskSystem.cpp:318, 364, 422`, all inside `#ifdef __UNIT_TEST__` | zero production users of data-parallel splitting |
| `RangedTask` is load-bearing as a *carrier* | `Logger.cpp:234` enqueues `GenerateSubTask(0, 1, 0)` — a one-unit range holding the drain runnable | retiring the **type** would break the logger; retiring **splitting** would not |

Decision: splitting stops being a property of the task container and becomes a **primitive on `TaskSystem`** — `ParallelFor`, which creates a task
that waits asynchronously for all of its subtasks, collates their results, and presents one final result. In the provider/stream model
data-parallel work is a provider producing N tasks, so this relocates the capability rather than deleting it.

What that requires, recorded so B7 builds it rather than discovers it:

- **The wait is asynchronous, not a spin.** Today's tests use `task.BusyWait()`, which occupies a thread. The collator is a successor that fires
  when the last child completes — a completion count plus one enqueue — so no stream thread is parked waiting for its own children.
- **It depends on B5.** "Collate their results" means children must carry results, which is exactly B5's outcome-delivery shape. B7 cannot land
  `ParallelFor` before B5 defines what a result is; the step order below is now wrong about that and must be read with this dependency.
- **Abandonment must not strand the collator.** Guardrails 1 and 2 mean a child can be abandoned on deadline while never having run. An abandoned
  child still counts as completed-for-joining, or a single deadline miss hangs the parent forever — which is a worse failure than the one the
  deadline was for.
- **Collation order is not the completion order.** Children finish in whatever order their streams get there, so results are collated into indexed
  slots, and any combine the caller supplies has to be order-independent or explicitly ordered by index.
- **Ordering constraint on B7, unchanged:** `ParallelFor` has to exist and be tested before the two splitting tests stop using `GenerateSubTask`, or
  the coverage is silently lost instead of migrated. That is R3, and it is the reason B7 is a step rather than a cleanup.
- **Unchanged by this decision:** `RangedTask` as a carrier survives until B5/B7 moves `Logger` off it. `ParallelFor` does not address that use.

---

## 4. Steps

Ordered so the `EngineTest` baseline holds at **every** commit; each step is independently revertable. The baseline is 53/53 up to B1, and
**54/54 from B1 onward**, because B1 adds `TimeTest` — the number moves from coverage growing, not from anything regressing. If a later step
drops it below 54, that is a regression. **As of B3a the count is 56** (`TimeTest`, `CPUBudgetTest`, `TaskProviderTest` added; each step that adds a
collection states so in its commit message).

### Measured facts that constrain these steps

Established by running things, not by reading interfaces. Each one invalidates a plausible guess:

| Fact | Evidence | Constrains |
|---|---|---|
| Task priority is unused. Every caller passes 0 — the default at `Task.h:43`/`RangedTask.h:50` — and the only explicit production argument is `Logger.cpp:234` passing 0 | grep of all `GenerateSubTask`/`Start` call sites | **B3b**: replacing the priority queue with FIFO breaks no promise, because nothing sets a priority. It replaces an *unspecified* order with a deterministic one |
| `BoundedPriorityQueue` has no tie-break — no sequence or insertion field | `Container/BoundedPriorityQueue.h` | **B3b**: today's ordering among the all-equal-priority tasks is whatever the heap shape happens to yield, so a test that appeared to depend on priority order was depending on heap layout |
| The entire `EngineTest` suite runs **inside a task on the base stream** | `UnitTestCollection.cpp:147` enqueues the `TestEnv` subtask to `BaseStreamIndex`; a 20-minute hang was sampled with the base stream's `RunLoop` parked in `TestEnv::Start` | **any test needing a stream**: the base stream is occupied for the whole suite, so target a worker (`GetIOTaskStreamIndex() + 1`) |
| `TaskSystem::Enqueue(task)` is a **general** queue that whichever stream asks first claims | `TaskSystem.cpp:130` pushes to `taskQueue`; `TaskSystem::Dequeue` serves any asking stream, with affinity | **any measurement test**: assert on a specific stream only after `Enqueue(streamIndex, task)` |
| A `Task` whose subtasks were never enqueued never satisfies `HasDone`, and `BusyWait` is `while (!HasDone());` | `Task.h:53` requires `numSubTasks > 0`; `Task.cpp` `BusyWait` spins | **B7/B8 tests**: waiting on a task nobody will run hangs the suite with no diagnostic. Use `Wait(interval)` and a bounded body |

| Step | Work | Gate |
|---|---|---|
| **B1** | ✅ **done 2026-09-18** — epoch (`57336b2`) + budget primitive (`f751a8f`): `OS::GetThreadCPUTime` on all three platforms, `hbe::CPUBudget`, `time::Set/GetBaseFrameRate` and `GetBaseFramePeriod`. Suite 55/55 ×3, mutation-proven both ways, build gate 12/12. Nothing consumes them yet; wiring is B3. | G1 ✅, G2 ✅ |
| **B2** | ✅ **done 2026-09-18** (`e5fb359`): `TaskProvider`, `TaskHandle`, `TaskProduceContext` + `ForStream`, `TStreamIndex` its own header. 7 tests, suite 56/56 ×3, mutation-proven. Two forced deviations from the sketch (bounded attach set because engine `Array` cannot grow; `Stop` is a request the stream applies) recorded in the design doc. | G3 ✅ |
| **B3a** | ✅ **done 2026-09-18** (`65944f1`): `TaskStream` holds a `CPUBudget`, charges each task it runs, and answers `MayTakeNewWork`. Nothing gates on it yet — deliberately not the existing pop, which holds already-accepted work. | G1 ✅ |
| **B3b** | Invert `BoundedPriorityQueue`: highest number = most urgent, oldest first within a tie, buckets become `hbe::Deque`. Update `BoundedPriorityQueueTest` to the new contract and `MainThreadTaskQueue`'s documented convention and default with it. | O1, O2 |
| **B3c** | Dual lanes in `TaskStream`: FIFO + priority queue, per-stream rate from the constructor, two enqueue functions, lane-aware sweep and re-add, lane budget shares with free borrowing and a round that ends on allowance exhaustion. | L1-L3, B1-B3 |
| **B3d** | Task max age: steady-clock stamp on the queued entry, evaluated in the existing sweep, per-task choice of abandon (reports the subtask finished-cancelled) or escalate (front of the priority lane). | A1-A3 |
| **B3e** | Register providers per stream and drain them, gated on `MayTakeNewWork()` before taking work and on `Produce` returning false to end the drain. | G1, G3b |
| **B4** | Named streams: Engine / IO / Render / Base Application (`UserThread[0]`) / Custom (`UserThread[1..N]`). Replace `BaseStreamIndex = 0` / `IOStreamIndex = 1` constants and `baseTaskThreadID`-captured-at-construction identity (`TaskSystem.cpp:70`). Update its users, incl. the `Assert(IsBaseThread())` at `TaskSystem.cpp:232`. | — |
| **B5** | Outcome delivery: size-class payload (inline copy vs heap `unique_ptr` via a thread-safe allocator), ownership following the holder, cross-stream successor enqueue. | G4 |
| **B6a** | `Engine::Run()` pumps a frame tick to the major systems (task system, renderer, log flush) while applets still own their loops and call the tick. Early — parent-plan steps 6/7 target this. | G1, G2 |
| **B6b** | Frame ownership moves to the pump; applets become `TaskProvider`s; applet-owned loops removed. **Parent-plan §6 is re-derived here.** | G3 |
| **B7** | Replace `Task`/`RangedTask` range splitting with `TaskSystem::ParallelFor` (asynchronous join, indexed collation, one final result); migrate the two test call sites. `RangedTask`-as-carrier is retired only once `Logger` has moved. | G5, **and blocked by B5** — collation needs results, which B5 defines |
| **B8** | Land the four guardrails as tests, not prose (below). | — |
| **B9** | Docs: update `docs/TaskSystemGuide.md` to describe the new model (it currently documents the range-splitting one), and mark `docs/TaskSystemRedesign.md` as implemented. | — |

---

## 5. The four guardrails become tests

Stated in `docs/TaskSystemRedesign.md` §9; here they get execution, because a guardrail nobody runs is a comment.

| # | Guardrail | Test |
|---|---|---|
| 1 | Abandonment always destroys — a dropped task still runs RAII | inject past-deadline tasks with heap outcomes; leak/heap check shows zero outstanding |
| 2 | Deadlines are engine-clock relative, stream-independent | same deadline on a fast custom stream and the engine stream abandons at the same threshold |
| 3 | Enqueue is SPSC lock-free or mutex — never MPMC lock-free | hammer enqueue from many producers under **ThreadSanitizer** |
| 4 | Cross-stream isolation: communicate only via outcome delivery | a provider sharing mutable memory across two streams must fail under TSan |

Additional: budget enforcement over many frames; cancellation safety — stopping a provider mid-frame must not corrupt a task already executing (the handle must distinguish *not-yet-run* from *running*).

---

## 6. Risks

| ID | Risk | Mitigation |
|---|---|---|
| R1 | Replacing the scheduling core while `EngineTest` must stay 53/53 throughout | steps ordered to keep both models coexistent until B7; no big-bang commit |
| R2 | A holder of a heap outcome escaping into a never-destroyed slot leaks | abandonment funnels into a container that is drained and destroyed (design §6.2) |
| R3 | Losing range-parallel data-parallelism if B7 retires `RangedTask` too early | G5 is a gate, not an assumption |
| R4 | Arena `AllocatorScope` is `thread_local` (`MemoryManager.h:51`), so a provider running on a stream must have its scope opened **on that thread** | coordinate with the parent plan's step 4; the trampoline already opens it on a worker thread |
| R5 | Build tree is single-occupancy (see `JOURNAL.md`), so this cannot be parallelised with other build-heavy work | schedule serially |
| R6 | Flush ends up driven twice per frame — the new tick *and* the self-rescheduling IO runnable | one owner only (§2.1): move flush to the tick, or keep the IO runnable — not both |
| R7 | Deleting the step-2 inline drain once flush is ticked re-introduces the hang on fatal-with-no-pump | §2.1 constraint is binding; keep the `repro_d1.cpp` scenario in the suite |
| R8 | G1's budget cannot bound a task already running — the system cannot stop one — so overshoot is bounded only by the longest single task | budget gates *taking* work between tasks; task granularity becomes a caller obligation stated in the provider contract (G3) |

---

## 7. Commit sequence

1. `feat(core): expose the engine epoch and a per-stream budget primitive`
2. `feat(core): TaskProvider and TaskHandle alongside the range-splitting task model`
3. `feat(core): FIFO queue and budget enforcement inside TaskStream`
4. `refactor(core): named task streams replace base-thread identity captured at construction`
5. `feat(core): cross-stream outcome delivery with holder-owned lifetime`
6. `refactor(engine): B6a — Engine::Run pumps a frame tick to task system, renderer and log flush`
7. `refactor(core): retire/bridge range-splitting RangedTask`
8. `refactor(engine): B6b — the pump owns the frame and applets become TaskProviders`
8. `test(core): the four task-system guardrails as executable tests`
9. `docs: TaskSystemGuide describes the provider/stream model` + `JOURNAL.md`

---

## 8. Deliberate non-goals

| Not doing | Because |
|---|---|
| Registry, descriptors, catalogue, arenas, CLI, SPIR-V tool | parent plan steps 3–9; orthogonal to scheduling |
| Redesigning `EngineApplication` before the registry lands | would design the same interface twice (§1) |
| MPMC lock-free enqueue | correctness first; only under a profiling requirement, behind a seam |
| A hard per-frame barrier | the model deliberately has none; correctness comes from explicit outcome delivery |

## B3c migration steps

Worked out against the code at `246794a`, so the next session does not re-derive it. This is one
coherent change - two lanes, per-lane re-add, lane-aware sweep - and should not be split.

CORRECTION: `hbe::Deque` does **not** support `Erase`, and neither does `hbe::Queue` - the compiling I
remembered had been done on a different container, and `hbe::Queue` is a ring buffer with no iteration at
all. What the FIFO lane actually has is `PushBack`, `PopFront`, `Front`, `Back`, `IsEmpty`, `Size`, `begin`,
`end`, so the sweep ROTATES the lane: pop `Size()` entries, push the unfinished ones back, drop the finished
ones. That preserves arrival order and costs the same O(n)-per-loop `BoundedPriorityQueue::Remove` already
pays, so there is no asymptotic regression - which is the point the false claim was reaching for.

1. `RangedTask` gains `StreamDrainPolicy::ELane lane` plus a constructor parameter, so a re-added task
   returns to the lane it came from instead of guessing.
2. Members: `Queue<RangedTask> fifoQueue;`, the existing `taskQueue` renamed `priorityQueue;`, and
   `StreamDrainPolicy drainPolicy;`. `ConfigureBudget` configures the allowance into BOTH: `CPUBudget`
   measures, the policy decides. Two independent gates would double-account and then disagree.

   AS BUILT THIS DOES NOT HOLD, and the cause is a missing decision rather than a bug: closing a round at
   exhaustion needs a rule for when the next round OPENS. Resetting instantly in the charge path was built,
   and it reopened the gate the instant it shut - `TaskSystemTest`'s budget test failed with "A stream that
   has spent its allowance still reported that it may take more work", which is that test doing its job. So
   `MayTakeNewWork()` is back to `CPUBudget::CanTakeWork()` and `StreamDrainPolicy` selects lanes only.
   OPEN QUESTION FOR THE OWNER: what reopens a round - a period of some kind, or an explicit engine tick?
   Until that is answered the policy cannot own the take decision.
3. `Enqueue(const RangedTask&)` splits into `EnqueueFifo` and `EnqueuePriority`, each taking `queueLock`
   and notifying. Existing callers (`Logger`, `UnitTestCollection`, `TaskSystem::Enqueue`) name FIFO
   explicitly - none of them sets a priority today, so that is the faithful mapping: a call-site change,
   not a behaviour change.
4. `Dequeue` asks `drainPolicy.ChooseLane(!fifoQueue.IsEmpty(), !priorityQueue.IsEmpty())` and pops the
   chosen lane. `ChooseLane` is const and mutates nothing, which is what makes it safe to consult under
   the lock without committing to a take.
5. On completion the measured nanoseconds go to `ChargeFifo`/`ChargePriority` for the lane that ran, and
   `IsRoundExhausted()` -> `EndRound()` replaces the single `budget.EndFrame()`.
6. The sweep becomes per lane: `priorityQueue.Remove(HasFinished)` as today, FIFO swept with `Erase`.
   Finished tasks land in `reAddFifo` or `reAddPriority` by their `lane`, and each buffer returns to its
   own lane.
7. `MayTakeNewWork()` becomes `ChooseLane(true, true) != ELane::None`, keeping the "estimate, not
   reservation" wording. `TaskStreamTest`'s existing budget assertions still hold: the stream total is
   still enforced.
8. The rate arrives as a `BuildStreams` argument. Both streams stay 1:1 until a caller needs otherwise.
9. Verification gate: 57 collections in three configs, plus a new assertion that a task enqueued on the
   priority lane is taken before an earlier FIFO task when the rate favours priority, and that a re-added
   task returns to its own lane.

## B3c reopen rule, and what result delivery became (2026-09-18 second round)

The open question "what reopens a round" is answered: the base stream calls into each stream to reset
its budget (decision R1) and the pre-existing hole is that nothing ever reopened it before (R2). That
makes `isMeasuring` atomic and amends `CPUBudget`'s single-owner note. All ten decisions, their
consequences and the still-open list are in `docs/TaskSystemRedesign.md`; B5 is now specified rather
than sketched, and B7 ships as several `ParallelFor` functions.

New work this creates, in order, because R7 and R8 are prerequisites rather than details:

1. `TaskRegistry` with index plus generation - Tasks stop being stack objects (6 call sites).
2. Thread-safe 128-byte packet pool, header as decided, banks from a cross-thread-safe allocator.
3. Two result containers per stream plus the base-stream swap, fold, and budget reset in one pass.
4. Successor and join counter on the registry record; then remove `Wait`, `BusyWait` and public
   `HasDone`, which rewrites five test call sites that currently wait or poll on the base stream.
5. Then `ParallelFor` overloads, which is where the blocked B7 finally unblocks.

## Result container, final shape (R11-R14)

Not a pool allocator. Each stream owns two bump-allocated containers of fixed 128-byte slots; the base
stream swaps them and a worker rewinds the rewound one by zeroing its slot count. Growth happens only in
the `Reset()` pass from a rate carried by a `TaskDescriptor`, and a task declares how many packets it can
produce - the stream refuses to run a task it has no room for, and an oversized head closes its own lane
until the next `Reset`. Consequences for implementation, in `docs/TaskSystemRedesign.md`: allocation never
touches the hot path, overproduction is an assert, and three guards are recorded as my additions - refuse
at enqueue what can never fit, log a closed lane, and stamp a container epoch into every handle.

## Completion is optional, and a closed lane falls back (R15, R16)

A lane closed by declared capacity takes from the other lane when that task needs no slots, and completion
is opt-in: fire-and-forget jobs emit nothing, callers needing a join bring their own collation. The engine
contract is now dispatch, admit by capacity, swap, deliver - which is what makes R8's removal of `Wait` and
a public `HasDone` coherent, since there is no engine-wide completion promise left to spin on. Note for B3d:
"an abandoned task still counts as completed" now binds only tasks that opted into completion.

## First cut, sized down (R17-R19)

`growBy` is a stream property set where the stream is built. `ResultCapacity` is now `NumResults`, and
`NumResults > 0` is what makes a task report completion - no separate flag, so the contradictory combination
cannot be written. The first implementation carries only `NumResults`, capacity admission, two bump
containers, swap, fold, rewind. No `TaskDescriptor` object exists until a second field needs one.

## Sizes fixed (R20)

Initial result container is 1024 slots and `growBy` is 1024 slots, one number for every stream. At 128 bytes
per slot that is 131,072 bytes per container, so 256 KiB per stream for the swap pair before any growth, and
one eighth of a 1 MB bank.

---

# HANDOFF — read this first on a fresh session

## Authoritative sources, in the order to read them

1. `docs/TaskSystemRedesign.md` — decisions **R1 through R20** are the design. They are binding and are not
   re-derived here. Sections before "Owner decisions, 2026-09-18 (second round)" are older; where they
   conflict, the higher R number wins.
2. `JOURNAL.md` top entries — records WHY, plus every error made, including several of mine.
3. `AGENTS.md` — no comments in `.cpp`; contracts go in `.h` Doxygen; commit each task; never push.
4. `README.md` § Documentation → **Documentation Policy** — the same self-documented rule for whoever is
   writing rather than an agent bound by `AGENTS.md`, plus the fixed Module / Class / Function outline an API
   reference under `docs/` must follow. Added in `358d7f6`.

Everything below that is not in those three files is **superseded**, including parts of this plan. Two known
stale spots: the "B3c migration steps" section's opening claim about `hbe::Deque` supporting `Erase` (false,
corrected below it), and step 2's "the policy decides" (does not hold — see R1).

## Verified state of the tree

Suite: **57 collections passing in Debug, Dev and Release** as of `2d9b5d3`. Everything committed since then
is documentation only, so nothing has compiled in the last several commits — run the gate before trusting
that number. **It is 58 as of `1f7e777`**, which registered `ResultContainerTest`; every gate line below that
says `all 57 collections passed` now expects 58, and a run printing 57 means the new collection was not
registered rather than that the tree regressed.

Already built and working:

| Piece | Where |
|---|---|
| Engine epoch, CPU budget primitive, base frame rate | `Core/Time.h`, `Core/CPUBudget.h`, `OS::GetThreadCPUTime` |
| Providers and handles | `Core/TaskProvider.h` |
| Priority queue: highest number first, oldest first within a tie, buckets created lazily | `Container/BoundedPriorityQueue.h` |
| Drain policy: lane selection only, tested | `Core/StreamDrainPolicy.h` |
| Two lanes on a stream | `Core/TaskStream.h`: `fifoQueue` (`Deque`), `priorityQueue`, `drainPolicy`, `EnqueueFifo`, `EnqueuePriority`, `ConfigureRate` |

Not true, despite how it may read: `MayTakeNewWork()` is still `budget.CanTakeWork()`. The policy does **not**
gate dequeuing, because R1's reopen rule is not implemented yet.

## Measured container facts (all of these cost me a build to learn)

* `hbe::Queue` is a ring buffer: no `begin`/`end`, no `PopFront`, no `Erase`.
* `hbe::Deque` has `PushBack`, `PopFront`, `Front`, `Back`, `IsEmpty`, `Size`, `begin`, `end` — and **no
  `Erase`**. Sweeping it means rotating: pop `Size()` entries, push the survivors back, drop the rest.
* `MultiPoolAllocator` contains **no mutex, no atomic, no thread_local** — not thread-safe. Per-stream pools
  cannot be freed from another thread.
* Test lambdas calling `lferr` need `[this]`; `lferr` is a `TestCollection` member.

## Commit 1 landed as `1f7e777` — what is true now

`NumResults` on `Task`, two fixed-slot containers per `TaskStream`, nothing consuming either. Behaviour
preserving because nothing declares results yet. Preceded by `2dca103`, a formatting-only commit on the three
legacy files it had to touch (`Array.h` 243 lines, `Task.h` 45, `Task.cpp` 7 off the house format at HEAD),
proven code-identical by a digest taken after stripping comments, `#include` lines and whitespace.

| Name as built | Where |
|---|---|
| `TNumResults GetNumResults()`, `SetNumResults()`, default `0` | `Task.h` — `TNumResults` is `uint32_t`, not `uint8_t`: the count must be able to state a whole container |
| `ResultContainer` — `Append`, `GetSlot`, `Count`, `Capacity`, `Rewind`, `Epoch`, `Grow` | `Core/ResultContainer.h`. `Append` is `slots[count++]` and never grows; `Rewind` is `count = 0` plus `++epoch` |
| `TResultSlot`, `SizeBytes = 128`, 8-byte aligned | `Core/ResultContainer.h`. Stride 128 is asserted from `GetSlot(1) - GetSlot(0)`, not assumed from `sizeof` |
| Two containers per stream, `GetResultContainer(index)`, `InitialResultCapacitySlots = 1024`, `DefaultGrowBySlots = 1024` | `TaskStream.h` — R20 sizes are stream properties, and R17's `growBy` is one stream property, not a per-container argument |
| `Array::Resize(newSize)`, exact, no growth policy | `Container/Array.h`. This is the growth API `Array` never had |
| `Array(const TAllocator&, TIndex)` and `NamedPoolAllocator<T>` | `Container/Array.h`, `Memory/NamedPoolAllocator.h` — the only way a `TaskStream` member can name the stream's own pool, since `DefaultAllocator` captures the allocator scope open at construction |

Facts a later commit depends on, all measured:

* A container's buffer is **2 MiB, not 131,072 bytes**. `CalculateNumberOfBlocks` takes `ceil(1 MB / 131072) = 8`
  and floors it at `MinNumberOfBlocks = 16`. Twelve streams on this machine, so about 24 MiB reserved for 3 MiB
  of slots; blocks 2–16 absorb later same-size allocations, so growth is free until the seventeenth block.
  Computed, not observed — `PROFILE_ENABLED` is 0, so no allocator stats reach the run logs.
* `Grow` asserts the capacity moved by exactly what it was asked, so a container that ever doubles would be
  caught rather than guessed. It does **not** grow the way `hbe::Vector` does (`max(4, capacity * 2)`,
  `Container/Vector.h`) and that is the point — which is why the test capacities are small and
  non-power-of-two (8 initial, grow by 3): a doubling bug cannot hide behind a 1024/1024 pair, where doubling
  and `growBy` give the same number.
* **Overproduction aborts; it is not a value a caller can ignore.** `Append` asserts `count < capacity` (R13),
  and `Array::operator[]` `FatalAssert`s its index in every configuration (`Container/Array.h:139`), so a task
  producing more than it declared stops the run in Release as well as Debug — in Debug the container's own
  assert names the cause first. Consequence for tests: nothing may append past capacity, and a zero-capacity
  container can only be checked for capacity/count/epoch, never appended to.
* Nothing proves a container's buffer came from the pool named in its constructor. `MultiPoolAllocator` exposes
  no usage or address-range query, so that regression surfaces as the pool's own "is not allocated by this
  allocator" fatal log, not a red test. Needs `PROFILE_ENABLED` or an accessor — a separate decision.

## Registry landed as `3be27c3` — what is true now

`Task` is no longer a stack object: `Engine/Core/TaskID.h` and `Engine/Core/TaskRegistry.h/.cpp` are new, every
task is a registry record addressed by `{index, generation}`, `RangedTask` carries a `TaskID` plus the declared
result count instead of `std::reference_wrapper<Task>`, and a stream resolves the task at dispatch and drops the
work item with a warning when the generation does not match. `Logger`'s drain task and the suite's task are
registry-tracked. **The suite is 59 collections now**, not 58: `TaskRegistryTest` joined. Gate was green in all
three configurations with 0 `error:` and runner exit 0.

Two things the next commit inherits from this one:
* Records never move, because `Record` embeds `Task`, which embeds three `std::atomic` and cannot be moved. The
  table is banks, grown one bank per `Grow`. Any future container holding a `Task` has the same constraint, and
  `Array` cannot satisfy it.
* `TaskSystem::CreateTask` / `FindTask` / `ReleaseTask` are the only ways a `Task&` may be obtained. A raw
  `Task task(...)` on the stack no longer compiles usefully — the default-constructed task has no registry record,
  so `TaskStream` will drop it with a warning rather than run it. That is deliberate; do not "fix" it by making
  the drop quiet.

## Next commit to make — do only this, then verify

Item 2 of the ordered list below: the **base-stream pass in `Engine::Run()` → `TaskSystem::BeginFrame()`** — reset
each stream's budget (R1, which makes `CPUBudget::isMeasuring` atomic and amends its single-owner note), swap
containers, fold. Gate: Debug, Dev and Release each with zero `error:`, then the runner printing
`all 59 collections passed` with exit status `0`, then `.pi/skills/hb-standards/scripts/check.sh --staged`.

Item 3 (`Wait` / `BusyWait` / public `HasDone` removal) stayed blocked by this handoff's own ordering, not by the
registry: the successor and join counter must land on the registry record first, and that record now exists.

`2e087a0` landed the R21 ceiling and the dispatch guard since this handoff was written. It changed the enqueue
signatures - `TaskStream::EnqueueFifo`, `EnqueuePriority` and `TaskSystem::Enqueue(TIndex, task)` return
`[[nodiscard]] bool` - so any path the registry adds that enqueues must decide what a refusal means rather than
drift past the compiler. The five `(void)` casts mark those decisions still outstanding - measured: `Logger.cpp:246`,
`Test/UnitTestCollection.cpp:168` and three in the test section of `TaskSystem.cpp` at `:663`, `:666` and `:731`.
Earlier notes here, and the message of `2e087a0`, said six; that count was wrong by one and was never re-counted
before being repeated.

Touching legacy files again means another proven
code-identical formatting commit first; `TaskSystem.cpp` and `UnitTestCollection.cpp` are conformant, so the
exposure is whichever files are still indented-namespace legacy (`grep` before assuming).

## Then, in this order — each is its own commit with the same gate

1. ~~`TaskRegistry` with index plus generation.~~ **Landed as `3be27c3`.** The blast radius this list
   carried was wrong twice over: 12 construction call sites, not 6, and `Logger.h:103`'s `Task task` **member**
   is invisible to the grep that counted them.
2. Base-stream pass in `Engine::Run()` → `TaskSystem::BeginFrame()`: reset each stream's budget (R1, which
   makes `CPUBudget::isMeasuring` atomic and amends its single-owner note), swap containers, fold.
3. Successor and join counter on the registry record; **then** remove `Task::Wait`, `BusyWait` and a public
   `HasDone` (R8). That rewrite touches **5** test call sites that wait or poll on the base stream — the
   suite runs inside a task on that stream, so waiting there becomes illegal once folding moves there.
4. `ParallelFor` as several functions: explicit stream list plus lane plus sub-job count, and an N-streams
   variant (R10). Unblocks B7.
5. B3d task max age (abandon or escalate per task; wall time from enqueue; no max age means never expires;
   an abandoned task still counts as completed **only** for tasks that opted into completion, per R16).
6. B3e provider drain, B4 named streams replacing `BaseStreamIndex` and `IOStreamIndex`.

## Two guards that are MINE, not the owner's — do not silently drop either

* **Refuse at dispatch** any task whose `NumResults` could never be admitted. R14 closes a lane at dequeue,
  so an unreachable capacity closes it forever on a configuration value. **Still unimplemented after `1f7e777`,
  and it cannot be written yet**: it needs a maximum reachable capacity, R20 fixes an initial size and a
  `growBy` and no ceiling, growth is therefore unbounded, and no declared count is unreachable — so the check
  could never fire. **Resolved by R21 and implemented in `2e087a0`: `TaskStream::CanAdmitResults` against a
  per-stream ceiling, default `0` meaning unlimited, and `EnqueueFifo` / `EnqueuePriority` /
  `TaskSystem::Enqueue(streamIndex, task)` returning `[[nodiscard]] bool`, refusing before the queue is touched
  and logging the task, its declaration and the ceiling.** With the default the guard is dormant, exactly as R21
  decides; opting a stream in is one call. What is still missing is R15's side: a refused task is not queued,
  not retried and not run, and the five `(void)` casts left at the existing call sites are the current shape of
  "no fallback exists yet" - each should become real handling when the capacity-free fallback lands. Cost of
  opting in is measured and is not zero: see R21.
* **Log a capacity-closed lane** with the declared need and the room remaining, rate-limited per closure.
  Without it, a config mistake is indistinguishable from the dropped-task hang that reached a human by
  watching the close test stall.

## Verification traps that already bit once each

* Grep build output for **`error:`** — `grep -c " error "` cannot match clang's format and reported success
  through a failed build. A failed build leaves the **previous binary on disk**, which then runs and passes.
  Always check the runner's exit status separately.
* **Assert the count of every scripted replacement.** Two edits "succeeded" silently because guessed
  indentation did not match; one shipped as a hang.
* `grep -c` prints `0` and exits `1` on no match — it breaks `&&` chains.
* `BoundedPriorityQueue.cpp`'s collection could not fail until 2026-09-18: 27 assertions logged without
  `lferr`. Audit other collections for the same pattern; some earlier green runs are weaker than claimed.
* `hbe::Assert` calls `std::abort()`, so a test must not deliberately trip a bound.
* macOS has no `timeout`/`gtimeout`. For a possibly-hanging run: launch, `sleep`, `kill -0`, `kill -9`.
* Proving a check works means mutating the implementation and watching the test go red. Several "passing"
  results in this subsystem were only ever proven that way.
* **A comparison against the constant that produced the value cannot fail.** My first per-stream test checked
  each stream against `TaskStream`'s own `InitialResultCapacitySlots`/`DefaultGrowBySlots`; changing the
  decided 1024 to 512 left all of it green. Found only by running the mutation. State decided numbers as
  literals and tie them to the engine's with `static_assert`, which covers both directions: changing the
  decision breaks the build and names it, building it differently turns the test red.
* **Run clang-format inside the repo.** With no `.clang-format` above the file it silently applies LLVM
  defaults — the fallback `check.sh`'s own header warns about. Measuring a copy in `/tmp` reported
  213/85/107/763/147 differing lines where the real figures were 243/45/7/0/0, which is a wrong answer rather
  than a noisy one.
* **Never run clang-format on `CMakeLists.txt`.** It does not know CMake: it joined line 1 into
  `cmake_minimum_required(VERSION 3.12) project(Core)`, and CMake then failed to configure with "Parse error.
  Expected a newline". Caught before any commit by a build that exited 1 with **zero** `error:` lines - which is
  also the lesson that a failed configure prints nothing clang-shaped, so grep for `FAILED:` and read the tail.
  22 tracked CMakeLists were checked afterwards; only the file being edited was damaged.
* **A test must not consume the general task queue.** A test that calls `TaskSystem::Dequeue` to inspect
  behaviour pops the queue's top item, and that queue is shared with every other collection - it took another
  collection's work item, never ran it, and `RHICapabilities` aborted with "is not allocated by this allocator"
  one log line later. The failure looked like a memory bug in a subsystem the test had nothing to do with. The
  queue has no test seam and no "is it empty" accessor, so this behaviour cannot be asserted from a test at all;
  assert the predicate at its point of use instead.
* **Something else in this checkout runs `EngineTest`.** A Debug run took SIGTERM (exit `143`) after the suite
  printed its farewell without the summary line, while a second `EngineTest` process was alive. The identical
  binary re-ran green. Do not read a `143` as a defect in the change under test - check for another instance
  and rerun.
* A `git add` naming one nonexistent path adds **nothing**, and `clang-format -i` then leaves the worktree
  conformant while the index still holds the old bytes. `check.sh --staged` fails on content you no longer
  have; its index/worktree guard (`b629166`) is what surfaced it here.

## Commits

Newest first. `3be27c3` is the last commit green in all three configurations (59 collections), and the commit
whose message carries this handoff update and the JOURNAL entry follows it immediately:
`9e4b69e` ceiling checklist closed · `2e087a0` the R21 ceiling and dispatch guard ·
`2dca103` formatting-only, proven code-identical · `1f7e777` NumResults and result containers ·
`358d7f6` README documentation policy · `202d4b8` the lint trap · `b629166` the index guard · `1b376b0` the
stale plan sentences · `7b7f172` sizes · `70e5878` growBy and NumResults · `4db7d93` optional completion ·
`a8c3d0c` stack allocator · `bde79e9` registry and continuation · `2d9b5d3` both-lane test (last green build
before the rewrite) · `dbfb181`, `4ecd68d` dual lanes · `246794a` priority ordering.
