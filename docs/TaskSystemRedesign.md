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
| Range-splitting tasks | The current `Task`/`RangedTask` model is data-parallel and is being retired. | Decide: retire, or bridge range tasks into the job graph as a provider. |

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
