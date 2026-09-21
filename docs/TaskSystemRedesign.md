# TaskSystem Redesign — Provider/Stream Job Graph

**Status:** DRAFT — design proposal for review.
**Owner:** (author)
**Replaces:** the current `Task` / `RangedTask` / `TaskStream` range-splitting model (`Engine/Core/Task.h`, `RangedTask.h`, `TaskStream.h`, `TaskSystem.h`).
**Supersedes in plan:** `PLAN_single_executable_app_registry.md` §1 (base-thread guard) and §3 (`EngineApplication::Initialize/Run/Shutdown` tick model) — those steps must be re-derived against this model.

---

## 0. Why this redesign

The current model has three structural weaknesses this design removes:

| Weakness (current) | Problem | This design fixes |
|---|---|---|
| `baseTaskThreadID` captured at `TaskSystem` construction | "The base stream" is *whichever thread built the system*, not a named identity. Wrong once the engine is not built on the render/application thread. | Each stream is a first-class **named** identity. The engine task stream is a role, not a side effect of construction. |
| Range-splitting `Task` / `RangedTask` (`Task.h`) | A single `Runnable` splits into `[start,end)` sub-tasks and re-schedules itself. This is data-parallel range work — it cannot express a *job graph* where one job produces an outcome delivered to another stream. | A `Task` is an **ephemeral one-frame job** that can enqueue a **successor** on any stream, carrying its **outcome**. Cross-stream data flows through the graph. |
| `TaskStream` = thread + `BoundedPriorityQueue` with no budget | A hot task can monopolize a stream for an unknown time. | Each `TaskStream` runs within a **per-frame budget**; stale tasks are **abandoned** by deadline. |

The net effect: the task system becomes a **job-graph engine** rather than a range splitter.

---

## 1. Glossary

| Term | Meaning |
|---|---|
| **TaskStream** | A thread running a **priority queue + FIFO queue** of tasks, work bounded by a **per-frame budget**. Owns its thread, its queues, its clock. |
| **Task** | An **ephemeral unit of work for a single frame**. Carries a payload and optionally an **outcome**. |
| **Outcome** | The data a task produces for a downstream task. Copied inline if it fits the payload, otherwise a heap `unique_ptr`. |
| **TaskProvider** | A **consistent producer** of tasks. Attaches to **one or more** task streams. Can stop itself or be stopped via a **handle**. |
| **Handle** | Control token that stops a provider, either by the provider itself or by a task stream. |
| **Frame** | One **budget epoch** of the **engine task stream** (the master clock). |
| **Budget** | The time an engine task stream may spend advancing one frame. |
| **Deadline** | Optional per-task time limit (engine-clock relative); exceeded → task **abandoned**. |

---

## 2. Topology — streams to threads

Each task stream runs on a dedicated OS thread. Naming is the *thread's role*; the stream is the logical work it runs.

```
Host Thread      ─ Engine Task Stream   (master clock; orchestrates renderer + task system)
IO Thread        ─ IO Task Stream
Render Thread    ─ Render Task Stream
UserThread[0]    ─ Base Application Task Stream
UserThread[1..N] ─ Custom Application / Custom User Task Stream
```

| Stream | Thread | Rate | Purpose |
|---|---|---|---|
| Engine Task Stream | Host | one frame per budget epoch | Master clock; schedules and gathers; runs core engine features. |
| IO Task Stream | IO | independent | Blocking I/O; must not starve the engine. |
| Render Task Stream | Render | independent (e.g. ~120 Hz) | Draw / surface work. |
| Base Application | UserThread[0] | independent | Base/first-class application tasks. |
| Custom Application / User | UserThread[1..N] | independent | Per-app or arbitrary work (physics, sound, …). |

**Key rule:** the engine task stream governs **only its own queue**. Custom streams run at their **own independent rates**; the engine does not drive their clocks. A frame is therefore a **time unit** measured off the engine clock, not a synchronization barrier that halts the other streams.

---

## 3. Frame and budget model

A **frame** is one budget epoch of the engine task stream.

```
Engine Task Stream (master clock):
  frame N:  run engine tasks until budget N exhausted
            └─ any task may Enqueue(successor, outcome, targetStream)
            └─ gather is implicit: outcomes flow forward, no barrier
  frame N+1: …
```

Properties:

- **No per-frame barrier.** Streams run continuously at their own rate. Correctness does **not** depend on a global fence; it depends only on **explicit outcome delivery**.
- **Heavy results may be delivered late.** A heavy task's outcome can arrive 3 (or more) frames after production. This is acceptable; the frame is a unit of time, not a consistency point.
- **Budget = frame time.** The engine task stream's budget bounds how long one frame may run. Other streams self-regulate their own budgets.
- **Independent clocks.** A task on the render stream (120 Hz) and one on the engine stream (60 Hz) share only the **engine clock**, which is what deadlines reference.

**Deadline policy.** Each task may declare an optional deadline (engine-clock relative). If a task has not been executed within its deadline, it is **abandoned**. Abandonment still destroys the task (RAII frees its allocations — see §6).

---

## 4. The task model

A `Task` is ephemeral — it exists for the frame(s) needed to produce its outcome.

```cpp
namespace hbe
{
class Task
{
public:
	// Ephemeral one-frame job. Payload + optional outcome.
	Task(TaskPayload payload, OutcomeSink sink);

	// Enqueue a successor on another (or the same) task stream, embedding this task's outcome.
	// Thread-safe; may be called from any stream.
	void EnqueueSuccessor(TStreamIndex targetStream, TaskPayload payload) const noexcept;

	// Optional deadline (engine-clock relative). Exceeded → abandoned.
	void SetDeadline(uint64_t engineEpochMillis) noexcept;

private:
	TaskPayload payload;
	mutable Outcome outcome; // inline copy, or heap unique_ptr
};
}
```

### 4.1 Outcome size classes

An outcome is delivered by enqueuing a successor task that **embeds** it. Two size classes:

| Fits payload | Exceeds payload |
|---|---|
| Copy the value into the successor's task payload. | `new` a `unique_ptr` via a thread-safe allocator; **move** it into the successor. |
| Owner = task's RAII. | Owner = the new holder's `unique_ptr`/RAII. |

Ownership **follows the holder**: whoever holds the `unique_ptr` (or the owning container) frees it. There is no separate "reclaim abandoned tasks" subsystem — destruction of the holder cascades.

### 4.2 Concurrency of enqueue

Enqueuing onto a task stream is **thread-safe**. Prefer, in order:

1. **SPSC lock-free queue per stream** — exact when a stream has one producer.
2. **Mutex-guarded queue** — otherwise.

**Do not** build multi-producer/multi-consumer (MPMC) lock-free cross-stream enqueue unless profiling demands it, and then only behind a seam.

---

## 5. The provider model

A `TaskProvider` is a consistent producer of tasks. It is the unit of lifecycle: apps, subsystems, and tools are providers.

```cpp
namespace hbe
{
class TaskProvider
{
public:
	explicit TaskProvider(StaticString name);
	~TaskProvider();

	// Attach to one or more task streams; a provider may feed several.
	void AttachTo(TStreamIndex stream) noexcept;
	void AttachTo(const Array<TStreamIndex>& streams) noexcept;

	// Produce tasks into the stream named by the context. Called on that stream's schedule and
	// possibly more than once per tick: the stream drains a provider until its budget is spent (G1),
	// and the task system cannot stop a task once taken - so the drain, not the task, is the unit of
	// control, and a task already running overshoots the budget rather than being truncated.
	// Returns whether anything was produced. The stream drains a provider while the budget allows AND
	// this returns true, so an idle provider costs one call per drain rather than a spin (G1, G3b).
	// Deliberately not a HasWork() the stream asks first: by the time Produce runs, a HasWork answer is
	// advisory, since provider state can change in between and the stream would be steering on a
	// reading it cannot trust.
	// The context carries the stream being produced into and a reading of the engine epoch (G2, G3).
	// It carries no remaining budget: the stream alone decides whether to call this at all, and a
	// provider that also gates on the same number makes two layers look authoritative where one is.
	// The task system cannot stop a task once taken, so a task already running overshoots the budget
	// rather than being truncated - guardrail 5.
	virtual bool Produce(const TaskProduceContext& context) = 0;

	// Self-stop, or stop via the handle. Either detaches all streams and stops Producing.
	void Stop() noexcept;
private:
	TaskHandle handle;
	Array<TStreamIndex> attached;
};
}
```

Lifecycle:

```
create provider → AttachTo(streams) → Produce() per attached stream → Stop()/handle → detach → destroy
```

- A provider may attach to **1..N** streams (e.g. a physics provider feeding both a physics stream and, on completion, the engine stream).
- **Stopping** (self or via handle) detaches the provider from all streams and halts `Produce()`. In-flight tasks on each stream are handled as abandoned tasks (§6).
- A **handle** is the control token a stream uses to stop a provider.

### B2 as built — where the sketch met the codebase

Implemented in `Engine/Core/TaskProvider.h` / `.cpp`, both models now coexisting. Four places where the sketch above could not stand as drawn:

| Sketch | As built | Why |
|---|---|---|
| `Array<TStreamIndex> attached` + an `AttachTo(Array)` overload | Bounded inline set, `MaxAttachedStreams = 8`, single-stream `AttachTo` only | The engine's `Array` has **no growth API** — it is fixed at construction and non-copyable, so a member `Array` could not accumulate attachments at all. Attachments are a lifecycle event whose realistic figure is one or two, so an inline set removes the allocator question entirely: a provider that cannot allocate still gets to exist. The array overload existed only to seed that container. |
| Overflow behaviour unstated | Asserts, and is dropped in a release build | Silent truncation would read as a stream that never hears from the provider. The cap is stated, and raising it is a one-line change. |
| `Stop()` "detaches all streams" immediately | `Stop()` sets an atomic request; the owning stream applies it when it next looks | Streams iterate the attachment list on their own threads. Detaching from the caller's thread would edit a container while another thread iterates it — the exact class of defect D10 was. Mutation on the list now happens only on the thread that owns the list. |
| Context "carries the stream and a reading of the epoch" | `TaskProduceContext::ForStream(stream)` builds `{stream, now}`, where `now` is an instant in the epoch's time base | A factory shared by the drain and the tests, so a test cannot pass while production hands out something else. It also fixes one reading per drain: if each provider read the clock itself, two providers in one drain would reason about two different times. |

`TaskHandle` is a plain token: a raw provider pointer, `RequestStop() const` (stopping does not alter the token, so copies are equivalent), and an **empty handle that is inert** — a registry slot never filled can be stopped on a teardown path without every caller writing a guard. It does not own the provider, and no check can detect a handle outliving it; the owner's lifetime rule is stated in the header instead.

Two contracts pinned by tests, because both are the kind of thing that reads as obvious and breaks silently: a repeated `AttachTo` of the same stream must not produce two entries (a duplicate silently doubles that provider's output rate), and `Stop()` must leave the attachment list intact. Verified by mutation — deleting the dedup scan turns the test red with "Attaching stream 2 twice left 3 attachments".

---

## 6. Outcome delivery, abandonment, and ownership

### 6.1 Delivery

A task on stream `A` enqueues a successor on stream `B` embedding its outcome:

```
Task A (stream A)  ──EnqueueSuccessor(B, outcome)──▶  Task B (stream B)
                          └─ inline copy OR moved unique_ptr ─┘
```

Outcomes may form a chain: `A → B → C`, where `C` may be the engine stream, render stream, physics, sound, or a custom stream.

### 6.2 Abandonment and RAII

A task is **abandoned** when it exceeds its deadline (or its provider is stopped before it runs). Critical invariant:

> **An abandoned task is still destroyed.** RAII frees the allocations the task possesses — including a heap `unique_ptr` outcome in flight.

Consequence: there is no leak-on-abandonment **as long as the holder of any allocation is itself guaranteed destroyed** (stack object or managed member). If a holder could escape into an un-destroyed slot, its `unique_ptr`/container would leak — so abandonments must funnel into a container that is drained and destroyed.

### 6.3 The one cross-stream invariant

> **The only sanctioned communication between task streams is outcome delivery via successor tasks.**

Shared mutable memory between streams is forbidden. If a provider is attached to two streams, it must not assume ordering between its own tasks on those streams except where an outcome was explicitly delivered. This is the rule that keeps the barrier-less model race-free.

---

## 7. Scheduling and ordering

Within a stream's budget:

- **Priority queue** orders tasks (0 = highest priority).
- **FIFO** orders tasks of equal priority.
- **Deadline** is a time limit (engine-clock relative), orthogonal to priority; a task past its deadline is abandoned regardless of priority.

Ordering policy (reference): enqueue priority, then FIFO, then deadline. Documented so schedulers are deterministic and reviewable.

---

## 8. Mapping to the current system

| Current (`Engine/Core`) | New design | Note |
|---|---|---|
| `TaskSystem` (`TaskSystem.h`) | `TaskSystem` owns `TaskStream`s and `TaskProvider`s | Retires `MainThreadTaskQueue` / `BoundedPriorityQueue<RangedTask>` general queue (or bridges to a stream). |
| `TaskStream` (`TaskStream.h`) | `TaskStream` = priority queue + **FIFO** queue + **budget clock** per thread | Adds FIFO + budget; keeps dedicated thread + `MultiPoolAllocator`. |
| `Task` / `RangedTask` (`Task.h`, `RangedTask.h`) | `Task` = ephemeral one-frame job with **outcome**; `TaskProvider` = producer | **Paradigm shift**: range-splitting → job graph. The range-splitting model is data-parallel and is superseded. |
| `TaskStreamAffinity` (`TaskStreamAffinity.h`) | Explicit **provider attachment** + optional affinity | Attachment replaces affinity for provider-level routing; affinity may remain as "any of these streams" for individual tasks. |
| `baseTaskThreadID` captured at construction | Named engine task stream | Removes construction-order fragility. |

---

## 9. Guardrails (correctness invariants)

These five must hold for the model to be correct. They are the design's contract.

| # | Guardrail | Why |
|---|---|---|
| 1 | **Abandonment always destroys.** A dropped task still runs RAII; abandoned tasks funnel into a container that is drained and destroyed. | No leak on abandoned in-flight outcomes. |
| 2 | **Deadlines are engine-clock relative**, stream-independent. | A task on a fast custom stream is abandoned by the same common clock as one on the engine stream. |
| 3 | **Enqueue: SPSC lock-free or mutex, never MPMC lock-free** unless profiling justifies it behind a seam. SPSC is permitted only where the single producer is *provable by construction* and stated where the queue is declared — not inferred from a run that happened to look single-producer. | Correctness first; lock-free is an optimization. The failure mode is real: `MainThreadTaskQueue` claimed thread safety in its header with no synchronisation at all, while a task-stream thread produced and the main thread consumed (D10, fixed `567a987`). |
| 4 | **Cross-stream isolation:** communicate only via outcome delivery. No shared mutable state across streams. | Keeps the barrier-less model race-free. |
| 5 | **No per-task preemption.** The task system cannot stop a task once taken; a budget gates *taking* work between tasks and never interrupts one in flight. | Everything that reasons about budgets, teardown or abandonment must treat "budget spent" as compatible with work still running. Assuming otherwise is a teardown race, and a provider cannot be asked to cooperate around a cancellation primitive that does not exist. |

---

## 10. Open questions and risks

| Item | Risk | Resolution needed |
|---|---|---|
| Abandoned-task container | A holder that escapes destruction leaks. | Define the abandon/destroy funnel (§6.2). |
| Deadline clock source | A common engine clock must be exposed to providers. | **Resolved — G2, 2026-09-16.** One `steady_clock` epoch owned by `Engine` and read by every consumer — deadlines, `Render`'s `deltaTime`, the log baseline. Storage reachable without `Engine::Get()` so engine-less logging survives; `SystemStatistics` derives from it instead of originating an epoch on `high_resolution_clock`. Full reasoning in `.Plans/PLAN_task_system_refactor.md` §G2. |
| Budget vs wall clock | Budget must be measured (wall or CPU cycles) consistently across streams. | **Resolved — G1, 2026-09-16.** Configured in seconds, consumed in CPU cycles; budgets decoupled from frames; each stream accumulates the *measured* CPU duration of the tasks it ran and stops dequeueing once that accumulation exceeds the base stream's target frame period. Full semantics and consequences in `.Plans/PLAN_task_system_refactor.md` §G1. |
| Cancellation safety | Stopping a provider must safely abandon in-flight tasks across **all** its streams without corrupting tasks executing mid-frame. | Handle must distinguish *not-yet-run* (drop) from *running* (finish or force-terminate). |
| Provider attached to multiple streams | Ordering between its tasks on different streams is undefined except via delivered outcomes. | Document; require outcome delivery for cross-stream dependencies. |
| Range-splitting tasks | The current `Task`/`RangedTask` model is data-parallel and is being retired. | **Resolved — G5, 2026-09-16.** Splitting becomes `TaskSystem::ParallelFor`: a task that waits asynchronously for all subtasks, collates their results into indexed slots, and presents one final result. Asynchronous join, not `BusyWait`; an abandoned child still counts as completed or the parent hangs. Depends on §7 outcome delivery, since children must carry results. Measured: splitting has no production callers (test-only), while `RangedTask` survives as a single-runnable carrier for `Logger`. |

---

## 11. Test strategy

| Check | Method |
|---|---|
| Budget enforcement | A high-priority task stream must not exceed its budget frame over many frames. |
| Cross-stream outcome delivery | Task on stream A delivers an outcome to stream B; verify the successor runs and sees the value. |
| Abandonment is leak-free | Inject tasks past deadline; run under leak/heap checks; assert zero outstanding `unique_ptr` outcomes. |
| Cross-stream isolation | A provider attached to two streams that shares mutable memory must fail a sanitizer/UB check (guards the §6.3 invariant). |
| Enqueue concurrency | Hammer enqueue from many producers into one stream under ThreadSanitizer; SPSC path lock-free verified. |
| Deadline clock | A task with a deadline on a fast custom stream is abandoned at the same engine-clock threshold as an engine-stream task. |
| Cancellation safety | Stop a provider mid-frame; no executing task is corrupted; in-flight tasks abandon cleanly. |

---

## 12. Notes on the current `Task`/`RangedTask` model (for reference)

The existing model (still present today) is a data-parallel range splitter:

- `Task` (`Task.h`) wraps a `Runnable` (`std::size_t(*)(void*, size_t, size_t)`), splits into `RangedTask`s over `[start,end)`, and tracks sub-task completion via `std::atomic`.
- `RangedTask` (`RangedTask.h`) carries `priority`, a `TaskStreamAffinity` bitmask, and re-schedules itself if it does not finish its range in one call.
- `TaskStream` (`TaskStream.h`) is a thread over a `BoundedPriorityQueue<RangedTask>` with a `MultiPoolAllocator`.

This redesign replaces that paradigm with the job-graph model of §4–§6. If any caller relies on range-splitting semantics, those usages must be converted to a provider or a bridge before retirement (§10).

---

## 13. Owner decisions 2026-09-18 — dual lanes, budget shares, max age

Decided in answer to explicit owner direction, each with the consequence that follows from it. Where a
decision reverses something documented, that is stated rather than left for the next reader to trip over.

| # | Decision | Consequence accepted with it |
|---|---|---|
| L1 | A stream holds **two queues**: a FIFO lane and a priority lane. | A drain policy between them is now mandatory, and it is the only thing standing between a busy lane and the starvation of the other. |
| L2 | Each stream carries a **FIFO : priority rate**, given at construction and set per stream class in `BuildStreams`; default **1:1**. | One place states what every stream does. Runtime tuning would need a rebuild, which is honest for a ratio nothing has measured yet. |
| L3 | **Two enqueue functions**, one per lane. | Every existing call site must name a lane. The routing decision is at the call site, not derived from a number. |
| O1 | Priority lane drains **highest number = most urgent**, and **oldest first within a tie**; buckets become `hbe::Deque`. | **Reverses a documented contract.** `MainThreadTaskQueue.h` states "0 = highest, 255 = lowest. Default is 128", so inverting the direction makes that false and makes 128 a mid-range value rather than an urgent one. Updated in the same commit as the container. Cheapest moment it will ever be: no caller in the tree sets a non-zero priority. |
| O2 | Within-tie order becomes oldest-first. | Today `Pop` takes `bucket.back()` — newest first — so two equally urgent tasks currently run in reverse arrival order. This is the actual defect the ordering decision was reaching for. |
| P1 | Priority lives on the **`Task`**, and the **existing per-loop sweep re-buckets** entries whose priority changed. | No search, no handle, no id allocator, and the walk is already paid for. An update takes effect at the next sweep, not instantly — one loop iteration of the stream holding it. |
| P2 | A re-bucket keeps **age** (front of the destination group), and **raising and lowering are both allowed**. | Escalation feels like escalation. A lowered task keeps its seniority in its new group, so demotion is not a reward. |
| B1 | Each lane gets a **budget share** = stream allowance x rate share, with its own accumulation. | The rate is now a share of CPU budget, not a take-count quota — this revises the original reading of L2. |
| B2 | **Borrowing is free each round**: a lane may keep taking while the stream total is inside budget and either share remains. | Long-run ratio is not preserved under sustained back-pressure from both lanes; whoever has work gets the CPU. Deficit accounting was the rejected alternative that would have preserved it. |
| B3 | The **round ends when the stream allowance is exhausted**, which resets the stream total and both lane accumulators. | A stream that never reaches its allowance never refreshes, so a lane that borrowed early stays borrowed while the engine is quiet. Accepted: the alternative tied shares to a clock that G1 keeps deliberately decoupled from frames. |
| A1 | A task may carry a **max age** (timeout duration). Per task, the caller chooses **abandon** or **escalate** at expiry. | Two behaviours, one field, and a default meaning "never expires" — absent must not be a large number, or a default-constructed task times out by accident. |
| A2 | Age is **plain wall time from enqueue**, stamped on the queued entry with the steady clock. | A stream that stalls or is budget-blocked ages its backlog: a pause costs you the backlog. The alternative (an age clock the stream advances only while its loop runs) was declined. |
| A3 | Abandoning a task **must report its subtask finished-cancelled**. | Non-negotiable: `Task::HasDone()` is `numSubTasks > 0 && finished >= numSubTasks`, so an abandoned subtask that reports nothing hangs anyone waiting on the parent — the same silent-hang class as waiting on a task nobody enqueued. |

### What this costs in code, stated before writing it

- `BoundedPriorityQueue` changes direction *and* within-tie order, so its own test must assert the new
  contract, and `MainThreadTaskQueue`'s header comment plus its default priority move with it.
- `TaskStream` gains a lane pair, a rate, lane-aware sweeping and re-adding (a re-added task must return to
  the lane it came from, so unfinished work does not silently change lanes between frames), and two lane
  accumulators.
- The sweep grows from "remove finished" to "remove finished, re-bucket changed priorities, expire aged
  tasks" — one walk, three decisions, and it is the step where an O(n) mistake would be invisible until the
  queue is deep.

---

## Owner decisions, 2026-09-18 (second round: budget windows and result delivery)

Asked and answered one at a time. Each row states the decision and the consequence the owner was shown
before choosing it.

| # | Decision | Consequence accepted |
|---|---|---|
| R1 | **The base stream resets each stream's budget by calling into it.** The base stream is the sync point, and one base-stream pass reopens every window. | `CPUBudget`'s "belongs to exactly one thread" note is amended: `isMeasuring` becomes atomic. A reset landing between a worker's `BeginTask` and `EndTask` drops that task's charge, which is fail-open and already the documented behaviour for an unpaired `EndTask`. |
| R2 | There was **no reopen rule at all before this.** `CanTakeWork()` is `accumulated < allowance` and `Reset()` had no caller, so a stream with a configured allowance stopped dequeuing permanently after spending it. | Pre-existing defect, inherited rather than introduced. The test that appeared to cover it only proved the latch is permanent. |
| R3 | **Result delivery is two containers per stream.** The worker appends results to its own; the base stream swaps the pair under a short lock and then drains what it took without holding the lock. | The worker's blocking window is one swap, not the whole drain. A result is not delivered until the base stream next pumps: stall the base thread and results accumulate, they do not get lost. | **[Superseded by R23 - two containers per stream no longer exists.]**
| R4 | **Payloads are fixed 128-byte packets from a thread-safe pool allocator**, populated on worker streams and released on the base stream. A delivered packet stays valid **for one frame**. | `MultiPoolAllocator` is not thread-safe - no mutex, no atomic, no thread_local anywhere in its header or implementation - so results get their own pool. The free list is deliberately a mutex over pre-allocated banks rather than an atomic Treiber stack, which has an ABA defect; this path sees one push per completed task, far cheaper than the CPU-time syscall already paid per task, so lock-free buys nothing measurable. | **[Superseded by R23 - 128-byte packets from a thread-safe pool, released on the base stream, valid for one frame no longer exists.]**
| R5 | **The 8-byte header sits inside the 128**, leaving a 120-byte payload. | Power-of-two slot: index to address is a shift by 7, one `DefaultBankUnit` bank of 1 MB holds exactly 8192 packets, every slot is 8-byte aligned. |
| R6 | **`kind` is split**: 0-63 engine, 64-255 application, boundary a named constant. | The app owns the top of the range and the engine can never allocate an app's kind by accident. |
| R7 | **Task identity is an index plus generation in a task registry, and Tasks stop being stack objects.** | API break. Blast radius measured at the time: 6 construction sites. Re-measured before implementation, still incomplete: `grep -rnE "(^|[^a-zA-Z:])Task +[a-zA-Z_][a-zA-Z0-9_]*\("` gives **11** function-local constructions in `TaskSystem.cpp`'s `__UNIT_TEST__` section plus `static Task task("TestEnv", ...)` at `UnitTestCollection.cpp:145` - 12 sites - and the grep cannot see `Logger.h:103`'s `Task task` **member** at all, which is the one that lives for the engine's whole life. Files: `Task.h`, `Task.cpp`, `TaskSystem.cpp`, `UnitTestCollection.cpp`, `Logger.h` (not `Logger.cpp` - the member is in the header), and now also `RangedTask.h/.cpp` and `TaskStream.cpp`, which hold the references that identity replaces. Test count: 57 collections with 208 tests at the time of this decision, 59 after the result-container and registry collections. A packet naming a dead task is recognised by generation and dropped rather than followed. |
| R8 | **`Task::Wait`, `BusyWait` and a public `HasDone` are removed. A continuation job is the only way to observe completion.** | Because completions fold on the base thread, waiting there is illegal - and the suite runs inside a task on the base stream, so every wait in it stands on the folding thread. The five test call sites that wait or poll, including the lane test added today, must be rewritten. **[Rationale corrected by R23d: the original reason blamed folding on the base thread, and there is no fold.]** |
| R9 | **A pipeline is a series of sequential jobs managed by its caller. TaskSystem does not know pipelines exist.** | TaskSystem supports only: dispatch a job, an optional successor recorded on the registry record, and a join counter on that record. Routing therefore lives in the registry, which is why the 8-byte packet header did not have to grow. |
| R10 | **`ParallelFor` is a splitter for a heavy job, spreading sub-jobs across requested streams, offered as several functions**: one taking an explicit stream list plus lane plus sub-job count, one asking for N streams and letting TaskSystem choose. | Explicit placement keeps "not the IO stream" expressible; the N-streams variant reads better but the same call can behave differently frame to frame, which makes a performance regression harder to reproduce. Both are provided rather than picking one. |

### Still open

* semantics of the header's `flags` byte (named rather than left as padding, but not yet defined)
* task registry capacity and what happens when it is full
* result pool bank growth policy and behaviour on exhaustion
* which allocator the pool's banks come from, given per-stream pools cannot be freed cross-thread
* how stage-to-stage payload binding works when two of a caller's sequential jobs want the same slot

## Owner decisions, 2026-09-18 (third round: the result container is a stack allocator)

| # | Decision | Consequence accepted |
|---|---|---|
| R11 | **The per-stream result container is a stack (bump) allocator over fixed 128-byte slots, not a pool.** The base stream only swaps the two containers; a worker never deallocates to reuse the buffer - reuse is rewinding the slot count. | With every slot 128 bytes, allocation is `index = count++` and release is `count = 0`: one store, no free list, no atomics, no lock inside the allocator. Memory stays owned by the thread that grew it. | **[Superseded by R23 - bump allocator over fixed slots, reuse by rewinding no longer exists.]**
| R12 | **`Reset()` may grow the buffer by a rate or size declared in the task object, supplied by a `TaskDescriptor`.** | Growth happens only on the base stream's reset pass, so no allocation ever lands on the hot path. Cost: buffer size is set by descriptors, so a badly declared descriptor shows up as waiting rather than as an error. | **[Superseded by R23 - Reset grows the buffer from a TaskDescriptor no longer exists.]**
| R13 | **Capacity is declared per task. A stream does not proceed with a task whose declared result capacity is not available.** | Admission control moves into the stream. A task that produces more packets than it declared becomes an assert, because admission already guaranteed the room, so a shortfall is a programming error and not a runtime condition. | **[Superseded by R23 - capacity declared per task and admitted by the stream no longer exists.]**
| R14 | **Admission is checked at dequeue, and when the head task does not fit, that lane stops taking work until the next `Reset`, with the head left in place.** | Arrival order from B3b survives untouched - nothing overtakes the head. Cost, stated before choosing: a lane idles behind one oversized task while queued tasks that would fit wait behind it, and if a grow policy is never set the lane closes permanently. | **[Superseded by R23 - a lane closes at dequeue when the head does not fit no longer exists.]**

### Guards I am adding under R14, flagged as mine rather than the owner's

1. **Refuse at enqueue anything that can never fit.** R14 stops a lane at dequeue, so a task whose declared
   capacity exceeds the maximum reachable capacity would close that lane forever. That must be rejected where
   the caller can still hear about it, which is the dispatch call, not the worker thread.
2. **A closed lane says so.** The stream logs the refused task, its declared need and the room remaining,
   rate-limited to once per closure. Without this, a configuration mistake is indistinguishable from the
   dropped-task hang that has already happened once in this subsystem today.
3. **Handles carry a container epoch**: `{streamIndex, containerEpoch, slotIndex}`, with the epoch advanced
   on every rewind. A bump allocator cannot detect a use-after-rewind on its own - a stale slot index reads
   whichever packet landed there next - so the epoch is what keeps R7's "recognise a recycled task, do not
   follow it" promise true against the container as well.

## Owner decisions, 2026-09-18 (fourth round: capacity fallback and optional completion)

| # | Decision | Consequence accepted |
|---|---|---|
| R15 | **When a lane is closed by capacity, the stream takes from the other lane** if that task needs no result slots. | A capacity-closed lane no longer idles the stream. Arrival order still holds, because nothing overtakes a head - the other lane is a different queue, not a jump the queue. | **[Superseded by R23 - a closed lane falls back to the other lane no longer exists.]**
| R16 | **Task completion is optional.** A fire-and-forget job reports nothing; a caller that needs results collated brings its own mechanism. | The engine's contract reduces to: dispatch, admit by declared capacity, swap and deliver packets. A task that declares zero outputs is therefore real and can never be blocked by capacity, which is the population R15 falls back to. It also completes the removal of `Wait` and a public `HasDone` (R8): there is no engine-wide completion promise left to wait on, and a join is a caller-side stage as R9 already ruled. |

R16 has one consequence worth naming before anyone writes code against it: the earlier rule that an
abandoned task still counts as completed for a join (the max-age decision in B3d) now applies only to tasks
that opted into completion. A fire-and-forget job that expires is simply gone, and nothing is waiting for it.

## Owner decisions, 2026-09-18 (fifth round: what each field is called, and where it belongs)

| # | Decision | Consequence |
|---|---|---|
| R17 | **`growBy` is a task stream property**, not a per-task descriptor field. | One growth number per stream, so a stream's memory ceiling is decided where the stream is built and every task on it inherits it. A task needing more waits for repeated grows rather than asking for its own size. | **[Superseded by R23 - a stream no longer owns a result buffer to grow.]**
| R18 | **No `reportsCompletion` field. `ResultCapacity` becomes `NumResults`, and `NumResults` is what decides whether completion is reported.** | One field instead of two, so the contradiction where a task declares zero outputs but asks to report completion cannot be written down. `NumResults > 0` reports; zero is fire-and-forget. R15's capacity-free fallback population is therefore exactly the tasks with `NumResults == 0`. | **[Superseded by R23 - NumResults is gone; one packet per record says nothing about completion.]**
| R19 | **Start simple and let it grow.** | The first implementation carries the minimum that works: `NumResults`, capacity admission, two containers, swap, fold, rewind. No `TaskDescriptor` object until something actually needs a second field, since inventing a descriptor to hold one integer is the kind of layer this project's standards reject. | **[Partly superseded by R23: of the minimum it lists, only capacity admission survives as a concept, and that concept went too. The principle - do not invent a TaskDescriptor to hold one field - is vindicated: the descriptor was never built and never will be.]**

R17 slightly retracts R12 as written: growth was described as coming from the task object. It comes from
the stream instead. R18 collapses R13 and R16 onto one integer.

## Owner decisions, 2026-09-19 (sizes)

| # | Decision | Consequence |
|---|---|---|
| R20 | **Initial result container holds 1024 slots; `growBy` is 1024 slots.** | Arithmetic that follows: a slot is 128 bytes, so one container starts at 131,072 bytes and each grow adds the same. Two containers per stream means 256 KiB per stream before any growth. A 1 MB bank fits 8192 slots, so an initial container is one eighth of a bank. | **[Superseded by R23 - container initial capacity and growBy no longer exists.]**

This answers the open item that asked for a ceiling number, and it is one capacity for every stream rather
than a per-stream tuning value - consistent with R17, which moved `growBy` onto the stream.

## Owner decisions, 2026-09-19 (second round: the ceiling on declared results)

| # | Decision | Consequence |
|---|---|---|
| R21 | **The ceiling on declared results is a per-stream constant, and `0` means no ceiling.** | A stream refuses at dispatch any task whose `NumResults` exceeds its ceiling, and refuses to grow past it. The default of `0` is unlimited, so on a default-configured stream no task can ever be refused for capacity and the guard stays dormant; opting a stream in is one line where the stream is built, which is the place R17 already put `growBy`. | **[Superseded by R23 - per-stream result ceiling with 0 meaning unlimited no longer exists.]**

R20 named an initial size and a `growBy`, and its own note claimed that answered the open item asking for a
ceiling. It did not: with growth available and nothing capping it, no declared count is unreachable, so a
dispatch-time refusal could never fire and would be decoration. R21 supplies the missing bound and makes the
guard writable at last, with the default deliberately being the inert value.

**What opting a stream in costs, measured rather than estimated.** Reserved memory per stream is not the
ceiling in bytes. `MultiPoolAllocator::Allocate` rounds the request to a block size, then sizes a bank as
`blockSize * numberOfBlocks` where `numberOfBlocks` is `ceil(bankSize / blockSize)` raised to
`MinNumberOfBlocks = 16` for any block under 1 MB (`MultiPoolAllocator.cpp:306-307`, `:397-421`, bank created at
`:424`). So a 128-byte slot ceiling of 1024 reserves a 2 MiB bank, 2048 reserves 4 MiB, 4096 reserves 8 MiB:
above the 16-block floor the ceiling multiplies reserved memory roughly one-for-one, and the slots actually
in use are a small fraction of what is reserved. A ceiling is therefore not a free safety knob, and the
inert default is the right default for streams that will never declare much.

**Correction to R20's arithmetic, which this section inherited.** "A 1 MB bank fits 8192 slots, so an initial
container is one eighth of a bank" is true of a bank that is exactly 1 MB. No bank for this block size is:
the 16-block floor makes a 131,072-byte block produce a 2 MiB bank holding 16,384 slots, so an initial
1024-slot container is one sixteenth of a bank, and the second container per stream lands in the same bank
rather than buying another. The per-stream reservation on this engine is 2 MiB per stream, 24 MiB across the
12 streams built here, not the 512 KiB a naive reading of R20 suggests. That figure is computed from the
constants and the call path above; `PROFILE_ENABLED` is 0, so no run log carries allocator statistics to
observe it directly.

## Owner decisions, 2026-09-19 (fourth round: the result lives in the task)

| # | Decision | Consequence accepted |
|---|---|---|
| R23 | **The result is embedded in the task object. Exactly one 128-byte packet per record; a task that has more than one packet worth of result handles it itself.** | Measured, not estimated: `Task` 48 bytes becomes 176, the registry record 64 becomes **192**, and the default 4096-record table 256 KiB becomes **768 KiB**. In exchange the **24 MiB** of per-stream container reservation (2 MiB pool bank across 12 streams) disappears along with R3, R11, R12, R13, R14, R15, R20 and R21, and with both guards I had been carrying: refusal-at-dispatch and closed-lane logging, whose subject - a shared per-stream buffer - is gone. A bigger result is a handle in the payload, which is the size-class rule this document already carried. |
| R23a | **Padding the packet to a cache-line boundary was measured and rejected.** | Doing so makes the record **208** bytes, and `208 % 64 == 16`, so records stop starting on line boundaries and neighbours share lines - worse than the overlap it was meant to prevent. Appended loose, the record is 192 = 3 lines and line-aligned. The only shared line is the packet's 8-byte header with `numFinishedSubTasks`, and both belong to the same task, whose counter is quiescent by the time anyone else reads it. |
| R23b | **The destination stream id rides on the result, inside R5's 8-byte header, and the producing task fills the value.** | R9 already recorded that the header did not have to grow for routing, and this is the field it grew for. `kind == 0` marks "no packet was written"; destination `0xFF` marks "no destination", which is R16's fire-and-forget and the caller that polls its own `TaskID` instead. |
| R23c | **The base stream runs a pass each quantum: reopen budgets, drain the completion list, triage by destination, enqueue a deliver work item.** | This is the payload that "fold" named and never had; the word is retired. The pass copies nothing and rewinds nothing - a result lives until its declaring task is released, and a deliver task naming a released task hits the generation check that landed in `3be27c3` and drops the work item with a warning. |
| R23d | **R8's rule stands and its reason is replaced: blocking on the base stream is still illegal, because the pass that reopens every stream's budget and performs every delivery runs there.** | Leaving the old sentence would leave a rule whose stated premise is false, which is how a rule gets quietly ignored later. |
| R23e | **Completion is published through a bounded list - a mutex over a pre-allocated array sized from the registry capacity - and it cannot overflow.** | Proven rather than retried: a record is live at most once, and a task publishes only as it finishes, so outstanding entries never exceed live tasks, which never exceed capacity. R4's own argument transfers verbatim - one push per completed task is far cheaper than the CPU-time syscall the stream already pays per task, so lock-free buys nothing measurable and an atomic Treiber stack contributes an ABA defect for free. |
| R24 | **Stream 0 is named "Base". The thread that drives `Engine::Run()` is named "EngineLoop".** | Three different things were called base: stream 0 (named "Main"), the OS thread named "Base" at `TaskSystem.cpp:266`, and `baseTaskThreadID`, which means "whoever constructed the TaskSystem". Each now has one name, and `baseTaskThreadID` becomes `engineLoopThreadID`. |
| R24a | **A thread that is not a stream has a stream index of `NonStreamIndex`, not 0.** | `thread_local TIndex StreamIndex = 0` combined with `BaseStreamIndex == 0` made `IsBaseThread()` true on **every thread that never called `SetStreamIndex`**, application threads included, and made `Assert(IsBaseThread())` in `BuildStreams` pass vacuously; the general-queue drain at `:169` charged such a thread's sighting of a task to **stream 0**. Safe to change because `TaskStreamAffinity` guards `bitIndex >= NumBits` on Get, Set and Unset, so an out-of-range index is dropped rather than shifting past the unit. One behaviour changes deliberately: a non-stream thread no longer takes work off the general queue at all, which is more honest than letting it pose as stream 0. |
| R25 | **The superseded machinery was deleted in the same round rather than left behind.** `ResultContainer.h/.cpp`, the `NamedPoolAllocator` adapter written for it, `TaskStream`'s two container members and their whole sizing chain (`InitialResultCapacitySlots`, `DefaultGrowBySlots`, `DefaultMaxResultCapacitySlots`, `growBySlots`, `maxResultCapacitySlots` and their accessors), `CanAdmitResults` and `ReportRefusal`, `Task::numResults` with its accessors and the `TNumResults` alias, `RangedTask::declaredResults`, and the two `Array` members that `1f7e777` added for nothing else - all gone. The three lane-enqueue entry points went back to `void` with the five `(void)` casts that had been marking "no fallback exists yet". | 1,126 lines out, 26 in. Peak resident set of the test binary measured **418,955,264 bytes before and 407,584,768 after** (a second run gave 406,028,288), so roughly 11-12 MiB rather than the 24 MiB the reservation arithmetic suggests - RSS counts *touched* pages, and a buffer with zero producers never had its pages written. The clean claim is the one a grep makes: nothing in the engine asks a pool for result storage any more. Suite 60 -> 59 collections. |


## Owner decisions, 2026-09-19 (third round: the registry's identity table is sized like a container)

| # | Decision | Consequence accepted |
|---|---|---|
| R22 | **The task registry's capacity is tunable, adjustable and growable exactly like a result container's: initial capacity 4096 records, `growBy` 4096 records, ceiling defaulting to `0` = no ceiling.** | Answers the open item asking what happens when the registry is full. Same three knobs as R20 and R21, so there is one shape to reason about: a record is 64 bytes as the class is built, so a registry starts at 256 KiB and every explicit grow adds another 256 KiB, and with the default ceiling growth is unbounded. All three are settable where the registry is built rather than baked in as a constant. |
| R22a | **`Create` never grows the table.** Growth is its own call, as it is for a result container. | A creation that finds no free record returns an invalid ID and says so in the log, instead of reallocating on whichever thread happened to dispatch first. That keeps R12's rule - no allocation on the hot path - true for identity as well as for results, and it makes exhaustion a size to tune rather than a rare race. |

Measured demand the sizing was chosen against, not assumed: **one** task record is alive in engine code today -
`Logger.h:103` holds a `Task` as a member for the engine's whole life, and nothing else outside tests constructs
one. The suite has twelve sites: one `static` living for the suite, and eleven function locals, at most two
concurrent. So 4096 is on the order of four thousand times this tree's current demand, chosen for the game rather
than for the engine as it stands. `sizeof(Task)` measured 32 bytes by template instantiation, and `StaticString`
is an interned 8-byte ID rather than a buffer, which is why a record - the task, a `uint32` generation and an
in-use flag - does not come close to a name-sized object. The record as built measures **64 bytes**, published as
`TaskRegistry::RecordSizeBytes` and printed by the sizing test rather than computed by hand: identity added a
16-byte `TaskID` to the task, and the record adds that ID's generation, an in-use flag and the free-list link. The
40 bytes quoted above was the record as *designed*, before the identity was added to `Task` - an estimate written
before the class existed, and wrong by exactly the width of the thing R7 exists to add.

The three figures are `TAtomicConfigParam`, not `TConfigParam`: `initialize()` runs on the booting thread and the
first test that reads a figure runs on a task, so `ConfigParam::Get`'s cross-thread assert fires immediately - and
every other parameter in this engine is already atomic for the same reason.

**Why the table lives in banks rather than one array or one pool.** Two obvious shapes were measured out before
this one was built. `Array<Record>` with a grow-by cannot work at all: `Array::Resize` memmoves when its allocator
answers `AllocateAligned`, and `std::atomic_is_always_lock_free` is true on this toolchain, so a table of records -
each embedding a non-trivially-destructible `Task`, which embeds three `std::atomic` - would compile, run, and
silently dangle every reference handed out before a growth. Handing the records to `MultiPoolAllocator` works but
is wasteful and wrong in its teardown: 4096 records of 64 bytes is 256 KiB, which becomes a 2 MiB bank plus a second
block to split, so 4 MiB is reserved for a 256 KiB table; and the registry would free through the pool while the
blocks it took came from `AllocateBlock`, a teardown bug by construction. Banks from the engine allocator give the
same grow-by-exactly-N semantics with neither problem, at the cost of a bank that cannot be returned before
`Shutdown`.

Why the generation is 32 bits: a stale ID aliases only after the same record is reused four billion times, which
is not a window anyone reaches; a 16-bit generation makes aliasing an everyday hazard in a table this size, and
the whole point of R7's identity is that a recycled record is recognised rather than followed.

