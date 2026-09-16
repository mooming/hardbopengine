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
| G2 | B1 | Deadline clock: the engine epoch must be readable from any thread. `SystemStatistics::GetStartTime()` exists (`SystemStatistics.h:68`) — likely the baseline, shared with Task A's A2 |
| G3 | B2 | `TaskProvider::Produce()` signature — what it is handed per call (stream handle, frame epoch, budget remaining?) |
| G4 | B5 | Enqueue: SPSC lock-free per stream where one producer, mutex otherwise. **MPMC lock-free is not the default** |
| G5 | B7 | Is range-splitting retired or bridged? `EngineTest` and any data-parallel caller decide this — find the call sites before choosing |

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

---

## 4. Steps

Ordered so the `EngineTest` 53/53 baseline holds at **every** commit; each step is independently revertable.

| Step | Work | Gate |
|---|---|---|
| **B1** | Expose the engine epoch and a per-stream budget primitive. No behaviour change. | G1, G2 |
| **B2** | Introduce `TaskProvider` + `TaskHandle` alongside the existing `Task`/`RangedTask`. Both models coexist. | G3 |
| **B3** | Add the FIFO queue and budget enforcement to `TaskStream`; keep its thread and `MultiPoolAllocator`. | — |
| **B4** | Named streams: Engine / IO / Render / Base Application (`UserThread[0]`) / Custom (`UserThread[1..N]`). Replace `BaseStreamIndex = 0` / `IOStreamIndex = 1` constants and `baseTaskThreadID`-captured-at-construction identity (`TaskSystem.cpp:70`). Update its users, incl. the `Assert(IsBaseThread())` at `TaskSystem.cpp:232`. | — |
| **B5** | Outcome delivery: size-class payload (inline copy vs heap `unique_ptr` via a thread-safe allocator), ownership following the holder, cross-stream successor enqueue. | G4 |
| **B6a** | `Engine::Run()` pumps a frame tick to the major systems (task system, renderer, log flush) while applets still own their loops and call the tick. Early — parent-plan steps 6/7 target this. | G1, G2 |
| **B6b** | Frame ownership moves to the pump; applets become `TaskProvider`s; applet-owned loops removed. **Parent-plan §6 is re-derived here.** | G3 |
| **B7** | Retire or bridge `Task`/`RangedTask` range splitting; migrate every remaining call site. | G5 |
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
