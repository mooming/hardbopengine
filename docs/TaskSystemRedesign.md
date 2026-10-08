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

**Status (2026-09-25) - what is and is not implemented.** The per-task optional deadline below is a **design that
was not built**: no caller in the tree declares one, and it would cost a field in `Task` plus a second comparison
path against a clock that can disagree with the drain clock. What ships instead is the narrower mechanism that
prevents the documented harm: a **per-stream max age**, `TaskStream::SetMaxAge`, judged against `Task::offerTime`
(stamped when the registry loads the record, carried by every item and every sub-slice) and evaluated where work is
taken. Over-age work is dropped, counted by `GetAgedOutWorkCount()`, reported in the same words as work dropped for
a released task, and notified through `SetAbandonedNotice`. It is off by default, and it is per stream because
staleness is a property of what the stream is for. Reopen the per-task deadline when a caller exists that needs a
deadline independent of its stream.

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

A task's work is **abandoned** when its stream declines it as older than that stream's max age (implemented, see `TaskStream::SetMaxAge`), when its task is released while it is still queued, or when a closing stream discards what it still holds; the per-task deadline that would add a fourth cause is designed but not implemented. Critical invariant:

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
| R1 | **The base stream resets each stream's budget by calling into it.** The base stream is the sync point, and one base-stream pass reopens every window. | `CPUBudget`'s "belongs to exactly one thread" note is amended: `isMeasuring` becomes atomic. A reset landing between a worker's `BeginTask` and `EndTask` drops that task's charge, which is fail-open and already the documented behaviour for an unpaired `EndTask`. | **[Mechanism superseded by R26/R27: the pass signals and the stream reopens itself, so no foreign thread writes a budget field and the atomic `isMeasuring` this row anticipated was never needed. The decision - one pass reopens every window - stands.]**
| R2 | There was **no reopen rule at all before this.** `CanTakeWork()` is `accumulated < allowance` and `Reset()` had no caller, so a stream with a configured allowance stopped dequeuing permanently after spending it. | Pre-existing defect, inherited rather than introduced. The test that appeared to cover it only proved the latch is permanent. |
| R3 | **Result delivery is two containers per stream.** The worker appends results to its own; the base stream swaps the pair under a short lock and then drains what it took without holding the lock. | The worker's blocking window is one swap, not the whole drain. A result is not delivered until the base stream next pumps: stall the base thread and results accumulate, they do not get lost. | **[Superseded by R23 - two containers per stream no longer exists.]**
| R4 | **Payloads are fixed 128-byte packets from a thread-safe pool allocator**, populated on worker streams and released on the base stream. A delivered packet stays valid **for one frame**. | `MultiPoolAllocator` is not thread-safe - no mutex, no atomic, no thread_local anywhere in its header or implementation - so results get their own pool. The free list is deliberately a mutex over pre-allocated banks rather than an atomic Treiber stack, which has an ABA defect; this path sees one push per completed task, far cheaper than the CPU-time syscall already paid per task, so lock-free buys nothing measurable. | **[Superseded by R23 - 128-byte packets from a thread-safe pool, released on the base stream, valid for one frame no longer exists.]**
| R5 | **The 8-byte header sits inside the 128**, leaving a 120-byte payload. | Power-of-two slot: index to address is a shift by 7, one `DefaultBankUnit` bank of 1 MB holds exactly 8192 packets, every slot is 8-byte aligned. |
| R6 | **`kind` is split**: 0-63 engine, 64-255 application, boundary a named constant. | The app owns the top of the range and the engine can never allocate an app's kind by accident. |
| R7 | **Task identity is an index plus generation in a task registry, and Tasks stop being stack objects.** | API break. Blast radius measured at the time: 6 construction sites. Re-measured before implementation, still incomplete: `grep -rnE "(^|[^a-zA-Z:])Task +[a-zA-Z_][a-zA-Z0-9_]*\("` gives **11** function-local constructions in `TaskSystem.cpp`'s `__TEST__` section plus `static Task task("TestEnv", ...)` at `UnitTestCollection.cpp:145` - 12 sites - and the grep cannot see `Logger.h:103`'s `Task task` **member** at all, which is the one that lives for the engine's whole life. Files: `Task.h`, `Task.cpp`, `TaskSystem.cpp`, `UnitTestCollection.cpp`, `Logger.h` (not `Logger.cpp` - the member is in the header), and now also `RangedTask.h/.cpp` and `TaskStream.cpp`, which hold the references that identity replaces. Test count: 57 collections with 208 tests at the time of this decision, 59 after the result-container and registry collections. A packet naming a dead task is recognised by generation and dropped rather than followed. |
| R8 | **`Task::Wait`, `BusyWait` and a public `HasDone` are removed. A continuation job is the only way to observe completion.** | Because completions fold on the base thread, waiting there is illegal - and the suite runs inside a task on the base stream, so every wait in it stands on the folding thread. The five test call sites that wait or poll, including the lane test added today, must be rewritten. **[Rationale corrected by R23d: the original reason blamed folding on the base thread, and there is no fold.]** |
| R9 | **A pipeline is a series of sequential jobs managed by its caller. TaskSystem does not know pipelines exist.** | TaskSystem supports only: dispatch a job, an optional successor recorded on the registry record, and a join counter on that record. Routing therefore lives in the registry, which is why the 8-byte packet header did not have to grow. |
| R10 | **`ParallelFor` is a splitter for a heavy job, spreading sub-jobs across requested streams, offered as several functions**: one taking an explicit stream list plus lane plus sub-job count, one asking for N streams and letting TaskSystem choose. | Explicit placement keeps "not the IO stream" expressible; the N-streams variant reads better but the same call can behave differently frame to frame, which makes a performance regression harder to reproduce. Both are provided rather than picking one. |

### Still open

Three of the five items this list carried were retired by the decisions that removed the containers they were about,
and leaving them here read as an invitation to rebuild deleted machinery (R23 embedded the result in the task,
`ac496e6` deleted `ResultContainer` and the pool with it - recorded as R31):

* semantics of the packet header's `flags` byte - named rather than left as padding, still undefined
* how stage-to-stage payload binding works when two of a caller's sequential jobs want the same slot, given R23 gives
  a task exactly one packet and larger payloads are caller-owned memory
* the priority band of the one work item the engine makes for a successor (R30): R18 has a producer declare a band for
  work it queues itself, and here no producer exists, so the item runs at the default. Closing it needs a field on the
  record, and R28 priced the record's room without one
* the ordering "record the successor before queueing the first item" is a contract in `ParallelFor`'s header with **no
  test behind it** (R32): a mutation that records it afterwards came back green, because witnessing the race needs a
  sub-job to reach a stream, finish, and close its join before the call returns. Losing it is silent - the join closes,
  finds no routing, and nothing is dispatched

Closed since the list was written, with where the answer lives: registry capacity, growth and full-table behaviour are
R22 with the refusal measured in `TaskRegistryTest`; the result buffer's growth policy and its allocator question died
with the buffer (R23, R31).

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
| R23c | **The base stream runs a pass each quantum: reopen budgets.** *(jobs 2-4 - drain a completion list, triage, enqueue a deliver work item - are superseded by R30, which delivers from the thread that closed the join; job 1 stands and is the whole pass.)* | This is the payload that "fold" named and never had; the word is retired. The pass copies nothing and rewinds nothing - a result lives until its declaring task is released, and a deliver task naming a released task hits the generation check that landed in `3be27c3` and drops the work item with a warning. |
| R23d | **R8's rule stands and its reason is replaced: blocking on the base stream is still illegal, because the pass that reopens every stream's budget runs there.** *(R30 moved delivery off the base stream, so this reason now covers budgets only; the rule is unchanged, and a blocked base thread no longer stalls outcomes - it still stalls every budget window, which is the half that matters.)* | Leaving the old sentence would leave a rule whose stated premise is false, which is how a rule gets quietly ignored later. |
| R23e | ~~**Completion is published through a bounded list.**~~ *Superseded in full by R30: a task carries its own outcome, so there is nothing to publish and no place to publish it to. Kept as a row because the overflow proof it recorded is the reason the container looked safe, and safe-looking is how it would have come back.* | Proven rather than retried: a record is live at most once, and a task publishes only as it finishes, so outstanding entries never exceed live tasks, which never exceed capacity. R4's own argument transfers verbatim - one push per completed task is far cheaper than the CPU-time syscall the stream already pays per task, so lock-free buys nothing measurable and an atomic Treiber stack contributes an ABA defect for free. |
| R24 | **Stream 0 is named "Base". The thread that drives `Engine::Run()` is named "Main".** | Three different things were called base: stream 0 (named "Main"), the OS thread named "Base" at `TaskSystem.cpp:266`, and `baseTaskThreadID`, which means "whoever constructed the TaskSystem". Each now has one name, and `baseTaskThreadID` becomes `mainThreadID`. |
| R24a | **A thread that is not a stream has a stream index of `NonStreamIndex`, not 0.** | `thread_local TIndex StreamIndex = 0` combined with `BaseStreamIndex == 0` made `IsBaseThread()` true on **every thread that never called `SetStreamIndex`**, application threads included, and made `Assert(IsBaseThread())` in `BuildStreams` pass vacuously; the general-queue drain at `:169` charged such a thread's sighting of a task to **stream 0**. Safe to change because `TaskStreamAffinity` guards `bitIndex >= NumBits` on Get, Set and Unset, so an out-of-range index is dropped rather than shifting past the unit. One behaviour changes deliberately: a non-stream thread no longer takes work off the general queue at all, which is more honest than letting it pose as stream 0. |
| R25 | **`RangedTask` is deleted in the commit that lands `ParallelFor`, not before.** | The type is not a caller convenience, it is what every queue stores: each stream's FIFO `Deque<RangedTask>` and `BoundedPriorityQueue<RangedTask>`, the shared general queue, and the item `TaskQueueItem` embeds. Deleting it needs a replacement item type and a producer in the same commit, and the producer is `ParallelFor`. Doing it earlier means rewriting the same call sites twice: measured today, 15 `GenerateSubTask` sites across 6 files and 19 enqueue or dequeue sites, all in nine production files. What the deletion will buy is measured too: `RangedTask` is 128 bytes (a name copy, a `TaskID`, an affinity mask, two atomics, four index fields) against 40 bytes for the `{TaskID, start, end, priority}` item R11 describes, so queue slots and every enqueue copy get 3.2 times cheaper. Until then nothing new may be written against the caller-side range protocol `Task::Start` and `GenerateSubTask` expose. *(Two things in this row are now dead in the ways that make it easier: the 40-byte item is described here rather than by another row - R11 is the retired container row, cited here in error - and there is no completion list to carry anything, R30 having removed it. The deletion still needs `ParallelFor` first, for the reason G5 gives: the splitting tests have to migrate onto it before the protocol they use goes.)* | *Deletion carried out later than this row originally read: `ParallelFor` landed in `74bd776` and the type was deleted in its own commit afterwards, on the owner's instruction, because the call-site census had to be re-taken once rather than twice; see `JOURNAL.md`.*


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


## Owner decisions, 2026-09-20 (budget window: what the pass owns before it can own anything else)

The pass R23c names has three jobs - reopen budgets, drain completions, deliver results. Asked which of them
to build, the owner chose **the budget window alone**. That choice is R26, and it was made on a call-site
census rather than a preference. Measured at `28a0cbc`:

| Thing | Production callers | Test callers |
|---|---|---|
| `CPUBudget::Reset()` | 0 | 1 - its own test, `CPUBudget.cpp:155` |
| `TaskStream::MayTakeNewWork()` | **0** - `TaskStream::RunLoop` never consults the budget before acquiring | 3, all inside `__TEST__`: `TaskSystem.cpp:581`, `:622`, `:629` |
| `StreamDrainPolicy::EndRound()` | 0 | 0 |
| `StreamDrainPolicy::IsRoundExhausted()` | 0 | 0 |
| Successor task (`EnqueueSuccessor`) | 0 - the name exists only in this document, at line 99-101 and section 6.1 | 0 |

Two consequences, and they point the same way. An allowance configured on a stream gates **nothing** today, so
the budget primitive is inert in the running engine. And the moment the acquire gate is wired without a window
boundary, a stream that spends its allowance stops dequeuing for the rest of the process's life - which means
R2 is not a defect that has been sitting there doing damage, it is a defect waiting for the wiring that will
make it bite. The window is therefore simultaneously the smallest useful increment and a prerequisite for the
gate that gives an allowance any meaning.

The deliver half is blocked by a dependency, not by effort. This document defines delivery as *"An outcome is
delivered by enqueuing a successor task that **embeds** it"* (section 6.1, and R9 restricts TaskSystem to
"dispatch a job, an optional successor"). The work item the pass would enqueue onto the destination stream
**is** a successor, and no successor field exists on a task or a record. A pass that drains completions with
nothing lawful to enqueue would be a pass that finds work and discards it, which is the stranding failure this
document refuses everywhere else.

| # | Decision | Consequence accepted |
|---|---|---|
| R26 | **The pass is split by dependency: the budget window lands first, and triage plus delivery land with the successor field.** | The rejected alternative was to finish delivery now by declaring that the first 16 bytes of a payload are a `{TRunnable, void*}` pair the engine runs on the destination stream. That works, and it invents an engine-wide calling convention whose only job is to be replaced one round later by the successor field - the same mistake this document already caught itself making with a `TaskDescriptor` built to hold one integer (R19). The second rejected alternative was to build the successor field first and land the whole pass at once, which leaves an inert budget and no window while the record grows. Cost of the chosen split: this round changes no observable behaviour for a caller who never configures a budget, so the proof is tests and measurement rather than a demo. |
| R27 | **The budget window is one base frame period wide, and the pass signals rather than writes.** `TaskSystem::RunBudgetWindowPass` advances at most once per `time::GetBaseFramePeriod()`, called from stream 0's own loop; each stream then reopens itself. | No new tunable was added, because the yardstick already exists: `ConfigureBudget` states an allowance as CPU time per base frame period, so a window narrower or wider than that period would make the two numbers mean different things and drift. The pass writes one atomic per stream and nothing else, which is R26's rule kept exactly - `CPUBudget` charges the CPU time of its owning thread, and a reset executed elsewhere races that thread's BeginTask-EndTask pairing on `taskStart` and `isMeasuring`, where the cost of a broken pairing is either a lost charge or the thread's whole life billed to the budget. **Consequence: R1's amendment is not needed.** R1 anticipated `isMeasuring` becoming atomic; under the signal-and-self-reopen mechanism no foreign thread writes any budget field, so it stays a plain owner-thread member. Second consequence, and the one a caller should know: **the window cadence is the base stream's loop**, so a work item that blocks that thread freezes every budget window in the engine. That is R8's rule with a new cost attached - blocking the base stream used to stall deliveries, and now it also stalls every reopen. |
| R28 | **The task registry record is padded to 256 bytes - four cache lines - at the moment the successor field lands.** | Measured layout today, taken from the compiler rather than from arithmetic: `Record` is 192 = `Task` 176 (id 16, name 8, two 1-byte counters, 6 pad, function pointer 8, userData 8, result packet 128) + generation 4 + inUse 1 + 3 pad + free-list link 8. Adding the successor the delivery half needs costs a full `TaskID`, measured at 16 bytes, and 192+16 = 208 is a multiple of 8 but not of 64 - records would stop starting on cache lines and two streams writing results into neighbouring records would invalidate each other through a shared line. Rejected alternatives: aliasing the 8-byte free-list link, which is unused while a record is live and is overwritten on release, rejected because it caps the successor at a compressed 8-byte identity and leaves a field overlap a reader has to hold in their head; a side table outside the record, rejected because it adds a lifetime rule (clear the entry when the record dies) and a second read on the delivery path for the same memory it saves; and shipping 208 unwrapped. Cost accepted: 64 bytes per record, so the default 4096-record table goes from 768 KiB to **1 MiB**. The padding lands in the same commit as the field that costs it, never before - padding for a field that does not exist is the same mistake as a descriptor built to hold one integer (R19). `RecordSizeBytes` keeps its static_assert, re-pinned at 256 and at the multiple of 64. |
| R29 | **The subtask count is declared before any work item is queued.** `Task::ReserveSubTasks(n)` states the join; `GenerateSubTask` no longer counts anything, and `ReportFinishedSubTask` returns whether *this* call closed the join. | Handing out an item used to increment the count, so a producer that queued item 1 before generating item 2 could have a worker finish item 1 while `numSubTasks` still read 1 - the task reported itself done on one item of three, and anything waiting on that was woken before the results existed. Declaring first is the only order that makes the counter mean "the whole of it", and it is the primitive `ParallelFor` sits on (R10). The join closes on an **equality**, not a threshold: past the last item a threshold makes every later finisher a winner too, and a successor would be dispatched twice. Costs, measured: every producer has to state its count, which is 9 call sites in this tree (7 in the suite, the logger's drain task, the suite's own task); over-production asserts, which is a debug-only bound with no other consumer, so it is stated rather than tested - a test that trips it takes the engine down with `std::abort`; and `Task::Start` was deleted in the same commit, being dead (no caller in `Engine/` or `Applications/`), broken (`length / numSubTasks` divides by the member the caller has not set yet, so a fresh task divides by zero), and impossible to keep honest once the count is declared. Price on the record: **zero** - the generated-item counter went into padding `Task` already had at offset 26, so the task stays 176 bytes and only the successor R28 already priced moved the record to 256. |
| R30 | **Delivery is performed by the thread that closed the join: it copies the finished task's packet onto the successor recorded for it and queues that successor on the stream the packet names. There is no completion list, and the base stream's pass keeps only the budget window.** | A task carries its own outcome, so the outcome is already in the hands of the thread holding the task, and one push is the whole delivery. Section 6.1 drew the arrow from producer to B's stream in the first place; sending it through a buffer the base stream drains was an addition of this document's, and what it bought was a container whose lifetime outlasts the task it describes - the third such container refused here, after the result buffer (R23) and `ResultContainer` (`ac496e6`). Costs, measured and stated: the delivering worker takes the destination stream's queue lock for the length of one push, and a stream holds that lock across the lane rotation that picks its next work item, so a worker can wait on a stream it was told to write to - bounded by a push and a rotation, and only ever for a stream somebody named out loud. Chains cost one push per link and no stack, because dispatch queues rather than runs (measured: first 1, middle 1, last 1, last link reading the middle's byte). A successor dispatched this way receives the producer's packet and whatever it held before is overwritten: the successor's packet is the outcome slot, and parameters for a successor have to travel somewhere the successor reads them. That one work item is made by the engine, not by a producer, so it carries the default priority - R18's producer-declared bands apply to items a producer queues itself, and a successor wanting a band has to be split by its own runnable. Every path that declines to dispatch is reported naming the pair that caused it: no destination named, destination is not a stream, successor no longer tracked, successor reserved no subtask under R29. |
| R31 | **The superseded machinery was deleted in the same round rather than left behind.** `ResultContainer.h/.cpp`, the `NamedPoolAllocator` adapter written for it, `TaskStream`'s two container members and their whole sizing chain (`InitialResultCapacitySlots`, `DefaultGrowBySlots`, `DefaultMaxResultCapacitySlots`, `growBySlots`, `maxResultCapacitySlots` and their accessors), `CanAdmitResults` and `ReportRefusal`, `Task::numResults` with its accessors and the `TNumResults` alias, `RangedTask::declaredResults`, and the two `Array` members that `1f7e777` added for nothing else - all gone. The three lane-enqueue entry points went back to `void` with the five `(void)` casts that had been marking "no fallback exists yet". | 1,126 lines out, 26 in. Peak resident set of the test binary measured **418,955,264 bytes before and 407,584,768 after** (a second run gave 406,028,288), so roughly 11-12 MiB rather than the 24 MiB the reservation arithmetic suggests - RSS counts *touched* pages, and a buffer with zero producers never had its pages written. The clean claim is the one a grep makes: nothing in the engine asks a pool for result storage any more. Suite 60 -> 59 collections. | *(Renumbered from a second R25: the row collided with R25 above, which is a different decision, and an ambiguous decision number is a defect in the thing decisions get cited from. It is recorded out of order because it documents `ac496e6`, whose neighbours were renumbered when the registry rows went in.)*
| R32 | **`ParallelFor` is the splitter, it is asked to split rather than told to, and a split states where its successor runs.** Two forms as R10 required: one names the streams, one asks for a number and gets workers. Neither waits. | G5's requirement that the join be asynchronous is met by R30: the last sub-job to report in closes the join and dispatches the collator, so no stream thread is parked on its own children, and a chain of stages costs one push per stage. Collation follows the rule the shape forces - one task, one packet (R23) - so per-item output is caller-owned memory addressed by index, and the combine happens in the successor; the test that migrated onto this sums ten slots claimed by `fetch_add` rather than accumulating into a shared double, which is what the retired version did under a data race that passed. Routing is the part this commit had to discover: R30 routes an outcome by the destination byte of the *producing* task's packet, and a split has no producing task - nobody writes its packet, because the sub-jobs own slices of the work and the combine happens downstream. Measured before the fix, in three tests at once: every sub-job ran, the join closed, and the engine logged that the record had closed a join whose packet named no destination stream. So `ParallelFor` takes the successor's stream and fills the byte before the first item is queued, and naming a successor with no stream refuses the call - the same rule R30 applies to a result with nowhere to go. |
| R33 | **Counts handed to a splitter are refused at zero *or below*, and a split is clamped twice: to the number of items, then to what the join counter can count.** | These counts arrive as the engine's signed stream-index type, so a negative is not a smaller job - it is a value that becomes a fourteen-digit range the moment it is used as an unsigned index, which is how `GenerateSubTask` reads it. Clamping to the item count is what keeps every work item non-empty (an empty item reports in for a unit of work that does not exist); clamping to 255 is what keeps the declared join equal to the number of items queued, because the join counter is 8 bits and 300 asked for silently becomes 44 kept, which closes a join while most of the work is still queued. Measured: a split of 1000 items into 300 sub-jobs is declared as 255, runs 255 items, and covers all 1000 units. |
| R34 | **The engine does not provide a task provider.** *(Narrowed by R35: what this row withdraws is the engine inventing work for streams nobody asked it to fill - it never withdraws draining a provider a user attached. `TaskProvider` has zero production callers today, measured: `Engine/Core/TaskProvider.{h,cpp}` reference each other and its only consumer is its own test collection at `Engine/Test/UnitTestCollection.cpp:92`.)* |
| R35 | **A user implements a task provider and attaches it to a lane of a stream; the stream drains work items from that provider under that lane's policy - which lane it is attached to is what defines the behaviour.** | This is the shape the owner stated after R34 was written, and it corrects R34's overreach: `AttachTo(stream)` was too coarse, because a stream has two lanes with different policies, and "give me work when you can" means something different in each. So attachment carries a lane - FIFO or priority - and the drain sits where the lane is served: a lane empty of queued items asks its attached provider for one, under the same budget gate that governs taking new work (R27's window is what makes that gate mean anything), and a provider returning false ends the drain for that stream rather than being asked again this pass. Stop is not cancellation and stays as documented: work already taken runs out. Two consumers are named for it, both user-side - `Applications/WindowExample`, whose loop is today only `app->PollEvents(); window->PollEvents()` and never touches the task system, and the window test, with the engine suite covering the mechanism itself. **Ordering:** the item a provider hands over is the queue's item type, so this lands in the same commit as the queue item type change (`.Plans/PLAN_queue_item_type.md`), or `Produce` gets written twice against two different item shapes. |
| R36 | **One attachment slot per stream, lanes held as a two-bit mask in that slot, and the capacity set well above the worker count. A duplicate is per (stream, lane), and an attachment that does not fit is reported.** | Asked whether any reason exists for the old `MaxAttachedStreams = 8`, the honest answer is that the reason for *a* fixed inline cap is real - "a provider needs no allocator to exist, and a provider that cannot allocate still gets to be constructed" - and the number is not. Measured: `TStreamIndex` is `int` so the old array cost 32 bytes, `hw.ncpu` on this machine is 12 giving **10 worker streams**, so feeding every worker already exceeded 8 before R35's lanes were counted, and both lanes on every worker would want 20. Doubling the array for lanes would have paid for slots that a per-stream slot with a lane mask gets for one byte. Duplicate suppression moves from per-stream to per-(stream, lane) because that is the rule R35 actually needs - the old rule existed to stop a provider being called twice per drain, and two lanes is the same hazard taken deliberately. Capacity is raised to cover every stream a machine of this kind has, with headroom, for a couple of hundred inline bytes on an object an application owns one or two of. And a refused attachment now **logs the provider, the stream and the capacity**: the old behaviour dropped it in release builds, and the header itself named the symptom - "a stream that never hears from the provider" - which is a hang with no diagnostic, produced where the information existed at the moment the mistake was made. |
| R37 | **A provider is owned by the user who made it. It may only be destroyed after detaching, `DetachFrom` and `DetachAll` take the target stream's lock and wait for a drain already in flight, and `~TaskProvider` asserts nothing is still attached.** | Ownership measured before deciding: no engine class holds a `TaskProvider*`, `&` or smart pointer anywhere - `TaskProvider.{h,cpp}` reference each other and nothing else does - so the user owns the object, `TaskHandle` is a non-owning stop token whose own doc says a handle outliving its provider dangles and "no check here can detect that", and there was **no detach entry point at all**: `Stop()` documents that it does not detach, and `TaskProvider.cpp:254-261` asserts exactly that. Harmless while nothing drains, and reachable the moment R35's drain exists - a stack provider going out of scope while a worker is inside its `Produce` is a use-after-free no current check catches. Chosen over streams holding a strong token because that would make every provider a shared-allocated object and give up the construct-without-allocator property, and over a documented honour with a bare destructor assert because an assert in the destructor cannot see a drain in progress, which is the one window that matters. Cost: a drain-in-progress count per stream, detach blocking for the length of a `Produce` call, and the rule stated where a user will read it - the destructor that fires when it is broken. |
| R38 | **A provider's attachment list is bookkeeping; the stream's lane list is the truth, and `AttachTo` registers with a live stream while still recording the slot.** | The drain has to be pulled by the stream, so the stream needs to reach the provider - and `AttachTo` only ever recorded a `TStreamIndex`, with no engine reference anywhere in `TaskProvider`. Two ways out, and the difference is visible in the tests. Making `AttachTo` resolve a stream and *refuse* otherwise is the purest form, and it breaks the three probes that attach to invented indices (the idempotence probe on streams 2 and 5, the capacity probe that fills slots with streams nobody has, the lane-mask probe on 3 and 4) - those probes are testing the provider's own rule, and rewriting them to first conjure live streams would test stream registration instead. Chosen instead: `AttachTo` keeps recording the slot exactly as now, and *additionally* registers a pointer into that stream's lane list when the engine has a task system and the index names a real stream. Cost stated plainly: two lists that `DetachFrom`/`DetachAll` must keep in step, and a provider attached to a stream that does not exist is recorded but never drained - which is the R36 refusal log's job to make loud, not a silent hole. |
| R39 | **`Produce` hands the stream a work item instead of enqueueing one, and the drain sits where the lane is empty, under the same budget gate that governs taking new work.** | A provider that enqueued into the queue itself could not be policy-checked - it would bypass the lane it attached to, the budget that stream is running down, and the re-add path an unfinished item depends on - so the signature yields an item (`bool Produce(const TaskProduceContext&, WorkItem& outWorkItem)`) and the stream decides what to do with it. Users construct items only through `Task::GenerateSubTask`, whose owning type is already a friend of the item, so the item stays something a stream can trust: `taskID`, `start`, `current`, `end` come from a real record. Consequences for the loop: an empty lane asks its provider once per pass rather than spinning on it; `false` ends that lane's drain for the pass (a provider with nothing is not a stream with nothing - the item may arrive next pass); a spent allowance must not ask at all, or the provider would manufacture work the budget already refused; and the stream counts a drain in flight so R37's detach has something to wait for. |
| R40 | **The queue lock is held across `Produce`, so a detach is a lock acquisition and no drain-in-flight counter exists. A provider must not call back into the stream draining it.** | R37 asked for detach to "wait for a drain already in flight", which implies a counter, a wait loop, and a rule for what to do with an item that arrives after the caller detached. Holding `queueLock` across the call collapses all three: `DetachProvider` takes the same lock, so a detach that lands mid-`Produce` simply waits, and once it holds the lock nothing can be on the way. The guarantee is unchanged; the mechanism is one lock instead of a lock plus a counter plus a discard rule - and a counter whose wait loop could never be entered without a race would be the fourth thing in this subsystem that no test can distinguish from its absence. Cost, stated where a user will read it: user code runs under a lock that `EnqueueFifo` and `EnqueuePriority` also take, so a provider that enqueues into its own stream deadlocks on a non-recursive mutex, and the drain asks providers once per pass rather than continuously. `TaskSystem::CreateTask` remains legal inside `Produce` because the registry has its own lock and nothing in the engine takes registry-then-stream nested. |
### The mechanism I am proposing under R26, flagged as mine rather than the owner's

The obvious shape - the base pass walks the streams and calls `Reset()` on each budget - is wrong, and wrong in
a way this document has already been burned by: `CPUBudget` documents itself as belonging to exactly one thread,
because it charges *that thread's* CPU time. `Reset()` writes `accumulatedNanos`, which is atomic and portable,
but also `isMeasuring` and `taskStart`, which are plain members of an owner-thread class. A base-thread reset
landing between the owner's `BeginTask()` and `EndTask()` is a data race on both, and the header's own note
explains what a broken pairing costs: either the charge is lost, or the thread's entire life is billed to the
budget and the stream refuses work forever. The pass would be introducing R2 by trying to close it.

So the pass **signals** and the stream **reopens**:

| Step | Where it runs | What it does |
|---|---|---|
| Window advances | Base stream, in the pass | Sets one atomic flag per stream. No write to any budget field. |
| Reopen | That stream's own thread, at the top of its loop | Swaps its own flag, and if it was set calls `budget.Reset()` and `drainPolicy.EndRound()` itself. |

The cost is that a reopen takes effect on the owner's next loop iteration rather than at the instant the pass
runs. That cost is free, and better than free: it means a window boundary can only ever fall **between** tasks,
which is exactly the overshoot rule R13's successor already states - a task in flight is never truncated, so a
reopen that cannot interrupt one is the honest implementation rather than a laggy one. The gate itself, when it
is wired, belongs in front of *acquiring* - draining a provider, dequeuing from the general queue - and never in
front of running a work item already held or delivering a result, per `TaskStream::MayTakeNewWork`'s own contract.

### Two defects the guide audit turned up, with their evidence

Neither is in the budget window's scope, and neither is fixed - documenting them is all that happened here.

| Defect | Evidence | Why it has not failed yet |
|---|---|---|
| `Task::Start` divides by the wrong counter. `const TIndex length = endIndex - startIndex + 1; TIndex interval = length / numSubTasks;` uses the task's **member** `numSubTasks`, not the `numberOfSubTasks` parameter it was handed. | `Engine/Core/Task.cpp`, in `Task::Start`. `GenerateSubTask` is the only writer of that member and it runs after the division, so a task that has generated nothing divides by zero. (`length` is also off by one for a half-open range.) | `Task::Start` has **no caller** anywhere in `Engine/` or `Applications/` - the only `.Start(` call sites are `TaskStream::Start`, `TestCollection::Start` and `TestEnv::Start`, which are different functions on different classes. Nobody has executed the arithmetic. What did execute was `docs/TaskSystemGuide.md`, which presented stack-construct + `Start` + `BusyWait` as the primary usage for five months; the guide now says do not use it and why. |
| `ac496e6`'s commit message states the five `(void)` casts "are gone with the refusal they annotated". Three went; **two remain**. | Still in the tree at HEAD: `Engine/Log/Logger.cpp:246`, `Engine/Test/UnitTestCollection.cpp:166`. `git log ac496e6..HEAD -- <both files>` prints nothing, so no later commit removed them either - the message was wrong when it was written. | Casting a `void` call to `void` compiles and executes nothing. The harm is communicative: the cast asserts "this call can fail and I have no fallback", which stopped being true the moment `Enqueue` lost its refusal. It is the same class of stale annotation this file corrected for R8 in R23d, one level down. |

Both were found by checking documentation against source line by line rather than reading the source, which is
the argument for keeping a user-facing guide at all: three of the five months of drift were invisible from
inside the code.

One measured side effect of `ac496e6` that nobody wrote down when it landed, and which this audit caught while
sizing a sentence: removing `RangedTask::declaredResults` shrank the **work item** from 128 bytes to **120**, which
is 6.25% of every queued item in every stream's queues, and `RangedTask` is still trivially copyable - measured
with `std::is_trivially_copyable_v`, not assumed from the defaulted destructor. R25 recorded the record size and
the resident-set figure; this is the third number the same commit moved.


## Correction to the call-site census above, and what the budget window found

The census concluded that an allowance "gates **nothing** today" and that "the budget primitive is inert in the
running engine". **That is wrong, and it was written in this file.** The census counted callers of
`TaskStream::MayTakeNewWork` - correctly, there are none in production - but that is not the only place the
allowance is consulted. `StreamDrainPolicy::ChooseLane` compares the round's own accumulations against the same
allowance and returns `ELane::None` when they reach it (`Engine/Core/StreamDrainPolicy.cpp:40-44`, reached from
`TaskStream::RunLoop` under the queue lock). `EndRound()` - the only thing that clears those accumulations - had
no production caller. So the true statement is the opposite of inert:

> The moment any caller configures an allowance and a stream spends it, that stream **stops serving its own lanes
> for the rest of the process's life.** Not "takes a while to resume" - forever, with the work items still sitting
> on the lane, and nothing in the engine able to unstick it.

That is R2 as originally written, and it is reachable today by one call to `ConfigureBudget`. What made it
demonstrable rather than theoretical is that I built it: a test that configured a 1 ms allowance on a worker and
queued a task to that worker hung the process for its entire life, because a previous test had already charged
that stream's round 219 ms and no `EndRound` had ever run. The R26 section's own defect table found the sibling
of this problem two rounds earlier from documentation, and missed this one because it looked for callers of the
wrong predicate.

The window that landed closes it: `RequestWindowAdvance` sets one atomic per stream, and the stream's own thread
calls `budget.Reset()` **and `drainPolicy.EndRound()`** at the top of its loop. `EndRound` goes from zero
production callers to one, and it is the half that actually frees a stalled lane - removing it alone is enough to
make a queued task never run (mutation M4 below).

### What the mechanism refuses to be measured by, with the two failures that proved it

A window advance zeroes `CPUBudget::accumulatedNanos` and zeroes the round. Both facts, and one more that is
easier to state than to believe:

| Attempt | What it looked like | What it was |
|---|---|---|
| Read the stream's charged CPU to prove it spent its allowance | A charge of **-222,014 us** | The baseline read before the task ran was itself zeroed by a window the test advanced, so the delta went negative. A quantity the mechanism erases cannot witness the mechanism. |
| Read it again, waiting for a charge rather than a completion | A charge of **0 us** on a task that had just spent 222 ms | The stream reported the task finished before it recorded the charge, and the pending window advance consumed at the top of the next iteration zeroed the figure as it landed. |
| Advance windows while looking for a refusal | **0 refusals in 5 s** | Advancing windows is what un-shuts a stream. Polling the pass every millisecond keeps every stream permanently reopened, so the shut state under test cannot exist while a test looks for it. |
| Sleep for the spent state | Hang, exit 143 after 5 min 5 s | A test runs inside a work item on the base stream's thread. Sleeping in it freezes the pass the test is waiting for, and blocks the suite. |

What survives is behavioural and does not read a erased quantity: queue a task, advance windows **only until that
task is confirmed to be running**, then leave the windows alone. The charge lands when the stream closes the task,
the stream's next round the loop is a refusal, and the refusal counter only ever counts up. Measured on a run:
**14 refusals while shut, resumed on its own thread at window 5, with 3 windows advanced by the base stream's own
loop before the test ever ran** - which is the witness that the wiring, not just the mechanism, exists.

The general rule for the next person: **count things a reopen cannot erase.** Tasks that ran, and refusals
recorded, are both monotone; charges, round usage and lane credit are all reset by design, and a test that reads
them is testing which one of them the timing happened to leave behind.


## Delivery shape, 2026-09-22 (R30: no container for completions either)

A completion list was designed, argued, and had its overflow proof written down (R23e) before the owner rejected
the shape: *"there's not task completion container. Since task itself has a data for its result."* That is the same
stroke that removed the result buffer (R23) and `ResultContainer` (`ac496e6`), and applied here it collapses the
design to one sentence: **the thread that closes the join delivers**. It copies the finished task's packet onto the
successor's record and pushes one work item onto the destination stream. Nothing is published, nothing is drained,
nothing is scanned.

| One delivery touches | Cost | Note |
|---|---|---|
| Registry lookup of the finishing task | one bounds check plus a generation compare | The task is live: the join closed a moment ago on this thread. |
| Registry lookup of the successor | same | This is also the check that it has not been released. |
| Packet copy | 128 bytes | The destination byte travels with it, so the successor can see where it was routed from. |
| Destination stream's queue lock | one push | The only cross-thread lock this shape takes. |

What that lock costs is worth stating rather than burying, because it is the price of the whole decision: a worker
delivering into a stream can wait for that stream's queue lock, and a stream holds its own lock across the lane
rotation that chooses its next work item. R23's two-container design existed to keep a worker off another stream's
lock entirely. That design is gone, and what replaces the guarantee is a bound - the wait is one push long, it is
never taken for a stream the producer did not name, and it is taken once per completed task, which is the same
frequency at which the stream already pays a CPU-time syscall per task (R4's arithmetic, reused).

**Refusal is always reported, never guessed.** Four states decline to dispatch, each naming the pair that produced
it, because the symptom of a half-filled routing field is a task that simply never runs and that symptom surfaces as
a hang somewhere far downstream: the packet names no stream; the stream it names does not exist in this engine; the
successor is no longer tracked (released while its producer ran, which is a caller abandoning work it asked to be
told about); the successor reserved no subtask, so there is no work item to make for it (R29). A task with no
successor recorded is the common case and is silent - fire-and-forget under R16 must not cost a log line per
completion.

Two things this shape gives back and one it does not close:

- A chain is not special. The successor's own work item closes its join the same way, so A can wake B and B wake C
  with the engine holding no notion of a pipeline (R9). Measured: first 1, middle 1, last 1, last link reading the
  byte the middle wrote. Dispatch queues rather than runs, so depth costs pushes, not stack.
- Delivery no longer waits on the base stream. Under R23c an outcome could not move until the base thread pumped the
  pass, and the unit suite blocks that thread for the best part of a minute - every delivery in this repository
  would have been frozen behind the test run. R8's rule against blocking the base stream keeps its force for the
  budget window, which still runs there (R23d, reason narrowed).
- The one item the engine makes for a successor carries the default priority band, because making it is the
  engine's job and R18's bands are declared by whoever queues an item. Closing that gap needs a priority on the
  record, and the record has no room that R28 did not already price. Left open on purpose.

## Superseded and narrowed by the shutdown and threading work

Rows above are the record as written, and the record is not edited into a different shape after the fact. This section says, in
one place, which of those decisions no longer describe the engine, so a reader who starts at the top and stops halfway does not
leave with a wrong model. Each entry names what replaced it, not merely that it changed.

| Superseded decision | Now | Why it moved |
|---|---|---|
| The base stream is served like any other, by a worker thread of its own. | Stream 0 has no thread. `TaskSystem::Update()` gives it one pass' worth of work inside the main thread, and `Engine::Run` calls it every frame. A designated nested pump, `TestHelper::DriveUntil` (`Engine/Test/TestHelper.h`, test-build API; `TaskSystem::DriveUntil` until 2026-10-02) is the only place that pumps it out of turn — and since `TaskStream::SetNestedPumpAllowed` is public, "the only place" means there is one caller, not that anything else is prevented. | A base thread competing with the loop was the source of the freezes; a base that runs only inside the loop cannot delay a frame, only share one. |
| `ProcessMainThreadTasks` / `MainThreadTaskQueue` as a facility beside the task system. | Absorbed: the base stream owns `postedTasks`, drained at the top of every pass, and `DispatchToMainThread` forwards to it. | Two queues for one thread was the reason a wait on "the main thread" could be satisfied by a queue the pump never read. |
| The IO stream is a worker like the others and is drained when the system closes. | IO is driven by the logger's own thread, created before the task system and stopped last, and it stays open through shutdown. | A logger that depends on what it reports cannot report that thing failing. Measured: the IO stream drained over 2.6 million passes because a log line re-posts its own drain task. |
| Waiting on a task handle is how you join work (`Task::Wait`, `Task::BusyWait`). | Both removed. Callers ask the queue (`TaskStream::CountPendingItems`); tests use `hbe::WaitUntil`, which carries a deadline and reports a stall as a failure. | The counter cannot distinguish a finished task from one that was never dispatched, so a timeout on it is not a deadline but an unnameable hang. |
| `RangedTask` is a type callers pass around. | `WorkItem`, 56 bytes, engine-internal. A provider builds one through the protected `TaskProvider::MakeWholeItem`; a caller that wants work run uses `TaskSystem::EnqueueTask`. | Items built outside the engine bypassed the registry's bookkeeping, which is the failure mode behind the released-task reports. |
| Shutdown drains until the queues are empty. | Bounded at 2000 ms of wall clock, then abandoned work is reported. Streams close in order, each with a graceful close, and the pump that waits for the threads is bounded too. | Resumable work outruns any pass-count bound, and an unbounded join turned a stuck test into an unanalysable hang. Shutdown means stop. |
| The budget gate is a behaviour, verified by what the stream does. | Asserted at the provider-ask site in Debug/Dev, and counted in every build (`GetProviderAskWhileSpentCount`, `GetLaneWorkRefusalCount`). | Asserts are compiled out in Release, so Release shipped with no observation of the rule at all. A broken gate now fails a Release run by name. |

Left open deliberately, not overlooked: the engine throttles from **two books** - `StreamDrainPolicy`'s per-lane time book gates
the lanes, while `CPUBudget` gates the provider ask and the general-queue pickup. The counters make that visible from outside for
the first time. Whether one of them should own the quantity is an owner decision, and it is the natural next question now that the
numbers can be read.

### 6.4 Abandonment now reaches the requestor that asked (implemented; supersedes the silence in 6.2)

6.2 stated the ownership invariant - an abandoned task is still destroyed - and said nothing about the party waiting on the
outcome, because nothing told it. `TaskSystem::SetAbandonedNotice` closes that for the drop sites that can reach it: the handler is a plain function pointer plus a `void*`, `nullptr` by default, and it travels on the
queue item rather than on the task because abandonment is where the task record is by definition unavailable. The per-task
optional *deadline* of section 2 remains **not implemented** and no caller in the tree asks for it; what is implemented instead is
the per-stream ceiling specified in `.Plans/PLAN_b3d_max_age.md`, which prevents the harm the deadline was for without adding a
field to every task. The shutdown close sites are still count-only, so this closes the gap for released-work drops and not yet
for teardown.


## Layout exceptions on record, 2026-10-02 (two types that cannot head their class)

`docs/CodingStandards.md` puts types at the head of a class, before the state and the API. Two members of
this module cannot obey that, and both fail for the same reason: the type's own definition names a constant
of the enclosing class, and a name is not visible before it is declared.

| Member | Declaration | Why it follows the constant |
|---|---|---|
| `TaskStream::LaneProviders` | `Engine/Core/TaskStream.h:69` | its array is `std::array<TaskProvider*, MaxProvidersPerLane>`; moved above `MaxProvidersPerLane` it fails with `error: use of undeclared identifier 'MaxProvidersPerLane'` |
| `MainThreadTaskQueue::TQueue` | `Engine/Core/MainThreadTaskQueue.h:59` | the alias is bounded by `MaxQueueSize`, so it needs that constant declared first for the same reason |

Both lines carry `// hb-standards:ignore`, which is the visible marker the standard allows, and `layout.py`
reports each as `MEMBER-WAIVED` and prints the count, so the exception is never silent. The constant stays
immediately above the type that needs it — the only position available. No behaviour changed; nothing about
locking, stream selection or teardown is affected by the order of two declarations that already had to sit
this way to compile.
