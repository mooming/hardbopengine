# Plan — Pipeline: Definition/Executor task graph

Companion reading: [`docs/TaskSystemRedesign.md`](../docs/TaskSystemRedesign.md) (provider/stream
graph this builds on), [`PLAN_task_system_refactor.md`](PLAN_task_system_refactor.md) (the runtime
this reuses).

> **Ownership (owner decision, 2026-10-26).** The owner implements. The assistant's role is
> review, verification runs, and reference-page authoring on request — not implementation.
> Stages below describe the intended sequence; the owner may reorder or reshape them.

---

## 0. Scope

**In.** New pipeline layer in `Engine/Core`: `Batch`, `StageChannel`, `JobContext`,
`PipelineStageConfig`, `PipelineDefinition`, `Pipeline`. Stage chaining reuses the existing
sub-task counters and streams; no scheduler rewrite.

**Out.** Dynamic runtime graph mutation (future separate class, owner decision 2026-10-26),
coroutine adapters, a `std::execution`/stdexec bridge, changes to `TaskStream` scheduling
semantics, changes to any existing consumer (`Logger`, `OSAL::Window`, examples).

---

## 1. Confirmed design decisions (owner, 2026-10-26)

| # | Decision | Rationale |
|---|---|---|
| D1 | `PipelineDefinition` (base, subclassed) / `PipelineInstance` (final, executor) | The definition is a **const blueprint** (owner decision, second round): stages, jobs, connections only. Progress state and intermediate results (channels, arena slots, join counters) belong to the instance, which is reusable across frames |
| D2 | Stages are member function pointers on the definition subclass | `this` captures state — the `void* userData` pattern disappears |
| D3 | Zero-copy edges: only `Batch{void* data, TIndex count, uint64_t requestID}` (24 B) crosses a stage | Batch is a **handle, not the payload** — data lives once in arena memory; spans reference it. `requestID` = the originating input's ID (see D15) |
| D4 | Per-**job** parallelism: `AddJob(stage, job, numConcurrency = 1)` — every job carries its own concurrency; a stage's sub-task count = Σ(job concurrencies) → existing `Task::numSubTasks` | Unifies data-parallel (one job × N) and task-parallel (N jobs × 1) into one mechanism; no separate `AddParallelJob` (owner rejected it); reuses `ReportFinishedSubTask`; no new scheduler |
| D5 | Dual mode, one mechanism: streaming = ring + `Close()` EOF + back-pressure; frame = flush at frame boundary + `Recycle()` swap | Streaming vs batch differ only in close/recycle timing |
| D6 | Streaming workers never block a stream thread: empty `TryPop` → self re-add | `TaskStream` already supports re-adding (`readdingFifo`/`readdingPriority`, the Logger flush pattern); blocking a stream thread would break budget windows and max-age |
| D7 | Results flow through channels; `ResultPacket` is not used by pipelines | Channels are typed and sized by the stage, not capped at 128 B |
| D8 | Both job-set stages **and** fan-out/join nodes are supported (owner decision, second round) | `AddJob` covers homogeneous jobs sharing a stage's channel types; separate stages + a join node cover heterogeneous types with per-stage parallelism. An intra-stage "join group" was rejected: the stage barrier already joins — parallelism of one job is just its `numConcurrency` |
| D9 | A join node moves no data: it passes a `std::span<const Batch>` — one 16 B handle per predecessor | Copying handles is not copying data; the zero-copy rule survives the join. Downstream stages read predecessor outputs through the handles |
| D10 | A stage may be created empty and filled with `AddJob`; workers pop jobs from the stage's job queue; the stage completes when the queue is drained and no job is in flight | This is the task-parallel counterpart of D4's data-parallel `numWorkers` duplication |
| D11 | **One allocator per `PipelineInstance`** (owner decision, third round): all intermediate results and job scratch come from it, returned in bulk at recycle/frame boundaries | Lifetime of every intermediate result equals the instance run — an arena/bulk-return allocator fits; per-pipeline memory budget becomes a contract; mirrors `TaskStream`'s own `MultiPoolAllocator` |
| D12 | **A job produces exactly one outcome per stage, consumed as the next stage's input** (owner decision, third round) — the structural invariant that keeps the graph tight | See guardrails I1–I5. Without it, ownership, lifetime and the memory bound all diverge |
| D13 | **No separate "one big task" mode** (owner decision): a bounded run = finite source + `Close()` + `Wait()`; sink outputs are held by the instance until the consumer reads them, recycled at the next run | Streaming subsumes the batch application; completion = all channels closed and drained (the join condition). `Wait()`'s contract is exactly that sentence |
| D14 | `JobInstance` = fixed-size AoS record with banking: `{input Batch, output Batch, EJobState, jobIndex, invocationIndex}`; identity is **derived, not stored** (Build-time prefix sums of job concurrences map sub-task index ↔ stage/job/invocation); slot freed when the invocation completes | Rejected SoA: N is tens–hundreds, one worker touches one record as a unit, no field scans (dispatch is channel-push-driven, completion is counted). Mirrors `TaskRegistry`'s banked fixed-size records |
| D15 | `Batch` field semantics (owner decision): `data` = start of the item array, `count` = item count, `requestID` = the **originating input's ID** — source-assigned, invariant across the whole pipeline | "JobID" was rejected: jobs change per stage, and the producer is already known from the edge — the surviving identity must be the request's. Enables correlation despite out-of-order completion by parallel invocations |
| D16 | Type binding is two-layered (owner decision): typed **alias templates** are the access vocabulary — `template<typename T> using BatchSlot = std::span<T>` (pre-publication write lease) and `template<typename T> using BatchView = std::span<const T>` (consumer read view); the erased boundaries (join → downstream, sink) get **one runtime assert** in the `TypedView<T>(const Batch&)` factory, debug-only. A constexpr check is **not required**. (Owner round 12: with B3 making `view.data()` the whole slot identity, the wrapper structs dissolved — a one-member struct loses to the span itself) | The type's home is the edge (`StageChannel<T>`); the handle stays erased. Trade-offs accepted: any `span<T>` reaches `Push`, so Push validates slot-pointer ownership in debug; the single-write rule (B1) is contract. The formal guarantee is still the View's template parameter |
| D17 | `typeID` = the **signature string literal's address stored as `std::uintptr_t`** — `reinterpret_cast<std::uintptr_t>(TypeSignature<T>().data())` from `__PRETTY_FUNCTION__`/`__FUNCSIG__` (owner decisions, rounds 9–11: compile-time integer conversion removed from the requirements — the pointer beats the hash on formal uniqueness; the owner's portability point killed fixed-width integers — **an integer narrower than the pointer width breaks on wider platforms**; and `std::uintptr_t` was chosen over `void*` for integer semantics: opaque ID, cannot flow into pointer parameters without a cast. Owner's fallback rationale: a platform lacking `std::uintptr_t` can define it manually — it is trivially the platform's pointer width) | Distinct content per instantiation → distinct literal → distinct address: uniqueness holds **formally** within a program on **any pointer width**; type aliases collapse to the same instantiation correctly. No list, no registration, **no RTTI**. Batch size follows pointer width (24 B on 32-bit, 32 B on 64-bit) — documented, not a defect. `requestID` stays `uint64_t`: it is our own request counter, not an address. Rejected along the way: central `IndexOf` list (owner — the list grows with every class), `typeid(T).hash_code()` (RTTI-dependent; not constexpr in C++26; collisions standard-allowed), `__COUNTER__` (per-TU inconsistent), friend-injection counter (UB-adjacent), FNV signature hash (superseded once constexpr was dropped), `uint32_t`/`uint64_t` fixed widths (truncation or pointer-width dependence), `void*` (zero casts but can flow into pointer parameters, reads as "points at something") |
| D18 | **StageLink internals = a bounded house ring** (owner decision, 2026-10-27): `AtomicFixedRingMPMC<Batch>` as the base candidate — placement-new Push already proven Batch-compatible; lock-free MPMC matches the worker-level reality. Direct hand-off is the capacity-1 special case, frame double-buffering is capacity 2 — the ring is the general form, not an added structure | Absorbs the owner's "produce, then hand straight to the next stage" and "one stage does it all" models: they are capacity-1 rings and Build-time edge **fusion** respectively, both expressible later without changing the core. Stage-parallel tuning (per-stage `numConcurrency`), stream mixing and back-pressure all require the ring; the fusion optimization is deferred (§6) |
| D19 | **Per-edge capacity ownership** (owner decision, 2026-10-28/11-01): the hybrid — `PipelineStageConfig.capacity` (0 = auto); auto resolves to **four batches in flight** (double buffer + slack; unit correction — capacity counts batches, not items, so it is a small constant, not batchSize-derived) | The channel maps capacity to facade blocks (`RingBuffer(maxBlocks)` — the facade's `maxBlocks` is its runtime bound; block capacity is the compile-time `TBlockCapacity = 8` template) and reports the resolved `Capacity()` for I5's arena sizing |

### Vocabulary — the units (settled 2026-10-26)

| Word | Unit | Meaning |
|---|---|---|
| Job | logic | one function registered on a stage (`AddJob`); no data, no state of its own |
| JobInstance | execution | one invocation of a job over one batch — the runtime record; input/output are handles |
| Batch | data | a **handle** `{data, count, requestID}` to an arena slot — the payload lives in the arena, the handle travels the edge |
| Stage | barrier | a set of jobs; completes when every job invocation is done; then outputs flow downstream |
| Edge (StageChannel) | flow | Bounded ring of Batch handles between two stages (stage-level SPSC; at worker level MPMC — `RingBuffer<Batch, AtomicFixedRingMPSC<Batch>>` is the base) — the channel carries its `fromStageID`/`toStageID` endpoints (StageChannel **is** the edge; owner reverted StageLink → StageChannel, 2026-10-28) |
| Join | fan-in | a node collecting predecessors' handles (`std::span<const Batch>`); moves no data |

Analogy worth keeping: Batch = the *waybill*, the arena slot = the *box*, the payload = the *goods*.

**Open decision (pending owner).** The context object handed to a job invocation: `JobContext`
(narrow — invocation identity + stop only, assistant's recommendation, matches
`TaskProduceContext`) vs `PipelineContext` (wide — read-only view of the instance). Filed in §6.

## 2. New files

| File | Contents |
|---|---|
| `Engine/Core/Batch.h` | Handle: `void* data; uint32_t count; std::uintptr_t typeID; uint64_t requestID;` — 32 B on 64-bit, 24 B on 32-bit (pointer-width dependent, D17) |
| `Engine/Core/StageChannel.h/.cpp` | The edge: const endpoints (`fromStageID`/`toStageID`/`typeID`/`allocatorID`) + bounded batch ring, `Close/IsClosed/AllocateSlot/Push`. **Final Push contract** (rounds 13–15): `BatchSlot<T> Push(EPushResult& outResult, RequestID requestID, BatchSlot<T>&& slot, TSize itemCount)` — returns the lease **by value** (registers; an earlier `&&` return dangled — compiler-proven), empty span = Pushed, the filled slot back on Full/Closed; the out-param carries the failure reason **atomically with the failure** — querying `IsClosed()` afterwards races with shutdown's `Close()` (TOCTOU, owner caught it). Out-param is ABI-optimal (1-byte store, assembly-verified vs `std::optional`) and race-free. `AllocateSlot` = the arena allocates (owner rename — was ObtainSlot), `BatchSlot` = the pre-publication writable lease (owner rename — was WritableSlot); family: BatchSlot → Batch → BatchView<T> |
| `Engine/Core/JobContext.h` | Invocation view (name pending, see open decision): invocation identity, stream, `IsStopRequested()` |
| `Engine/Core/JobInstance.h` | Fixed-size AoS record (D14): input/output Batch, state, job/invocation indices |
| `Engine/Core/PipelineStageConfig.h` | `name`, `stream`, `priority`, `batchSize` |
| `Engine/Core/PipelineDefinition.h/.cpp` | Base: `Define()`, `AddStage<TIn,TOut>(cfg, member fn)`, `Connect`, mode/close policy, `OnPrepare()/OnTeardown()` hooks |
| `Engine/Core/PipelineInstance.h/.cpp` | Final executor: `Run/Wait/RequestStop`; owns channels, arena slots, join state; builds stage Tasks, worker loops, job queues, join nodes |
| `Engine/Core/JoinNode.h` | Join record: predecessor edges, `std::span<const Batch>` assembly, completion counter |

## 3. Implementation stages (each ends in a commit)

| Stage | Content | Verification |
|---|---|---|
| S1 | `Batch` + `StageChannel` (ring, close, recycle) | Unit tests in `Engine/Test` (push/pop/close/recycle, back-pressure, multi-producer) |
| S2 | `JobContext`, `PipelineStageConfig`, `PipelineDefinition` (const blueprint: `AddStage` (single-fn or empty), `AddJob(stage, job, numConcurrency = 1)`, `AddJoin`, `Connect`, `Build(arena) const` — no execution) | Unit tests: stage/job/join registration, `Connect` validation, cycle check |
| S3 | `PipelineInstance` executor, streaming mode: worker loop = `TryPop` (batch or job) → run fn → `Push`, empty → re-add; stage join via sub-task counters; join node fires on all predecessors | Unit test: 3-stage pipeline (parallelism 1/4/2), a job-set stage, a fan-out with join; order and zero-copy checks |
| S4 | Frame mode: `Flush()` at frame boundary, `Recycle()` swap | Unit test: two consecutive frames, buffer reuse without stale reads |
| S5 | API reference pages (`docs/Core/…` per class, per method) + pipeline section in `docs/TaskSystemGuide.md` | `docs_coverage.py`, `check.sh`, three-config build (`-dev -debug -release`) |

**S3 worker loop sketch (the load-bearing mechanism):**

```
WorkItem runs on a TaskStream thread:
  while (!stop && !frameDone):
    if TryPop(batch): fn(ctx, span(batch), outSpan); Push(result batch)
    else: return re-add signal      // never blocks, never spins hot
last finishing sub-task of the stage → successor dispatch (join)
```

**Execution decision (owner, 2026-10-29): the pipeline rides the streams — confirmed over replacing
them.** Steady-state per-batch cost is channel-ring ops only, because the worker processes until
the channel is empty and only then re-adds (process-until-empty). The two replacement options lost:
per-stage threads would triple-reimplement waiting, budgets and shutdown; a private worker pool is
a second scheduler. The only regime where riding loses — sub-microsecond batches — is edge fusion's
territory (§6), not a scheduler change.

**The pipeline is not a `TaskProvider` (owner decision, 2026-10-29).** The streams drain worker
tasks; the workers drain the channels — two drains, each doing one thing. The provider variant was
rejected: it would pull per-batch `WorkItem`s through the stream's lane queues (data crossing the
scheduler, an 80 B copy and a Produce call under the stream lock per batch), and `WorkItem`'s
range-slice model carries no batch handle. Vocabulary: a `TaskProvider` is an engine-owned work
*producer* (WindowTickProvider, Logger flush); a worker task is a data-*consuming* loop whose data
lives in channels — data never enters a lane queue.

## 4. Guardrails — the structural invariants (D12)

```
I1  Single producer, single consumer per edge — the producing stage writes, the consuming
    stage reads (stage-level). At worker level the ring is MPMC (both stages may run several
    concurrent invocations), so the base is the lock-free `AtomicFixedRingMPMC`.
I2  Batches are immutable — one write by the producer, read-only everywhere after.
I3  A slot is recycled only after consumption; fan-out recycles by consumer count.
I4  Single ownership — edge payloads and job scratch all come from the instance's allocator.
I5  Live-set bound: peak concurrent batches = Σ(edge capacity × batchSize);
    Build() derives the allocator size from the definition.
B4  Slot stability — a Batch only points, so the arena must never relocate a slot between
    allocation and release; fixed-block pools satisfy this for free (why D11 works).
```

I5 is what makes D11 viable: the one-outcome rule bounds the live set, so one allocator per
instance suffices. The pipeline layer sees a *stream of batches along edges* (data view);
TaskStreams see ordinary WorkItems/sub-tasks (execution view) — the invariants are the seam
between the two.

## 5. Risks

| Risk | Mitigation |
|---|---|
| Blocking a stream thread on `TryPop` (D6 violation) | Worker uses the existing re-add mechanism; hot-spin guarded by stream budget/max-age — asserted in tests |
| `numWorkers` capped at 255 (`Task::MaxNumSubTasks`) | Documented; 255 workers per stage is beyond any stream count in the engine |
| Batch lifetime across stages (use-after-recycle) | Arena slots owned by the channel until sink consumption; `Recycle` only after drain — debug assert on double-handoff |
| Back-pressure deadlock (ring full + worker re-added behind other work) | Priority-lane re-adds; test with small capacities |
| Fan-out recycle: one `Batch` handle delivered on several edges completes at different times | Per-batch consumer count (or consume-stage-completion basis) tracked by the channels — guardrail I3; covered by an S3 test |
| Job-set ordering is nondeterministic (workers pop in any order) | Documented contract; jobs that need order belong in separate stages — asserted in the guide |
| Join fed by a branch that never closes (streaming mode) | Join completion requires every predecessor channel closed AND drained — covered by the S3 close-propagation test |
| Scope creep into `TaskStream` | Stages 1–4 touch only new files; S5 docs only |

## 6. Out-of-scope follow-ups (recorded, not planned)

- **Open decision — context object naming/scope**: `JobContext` (narrow, recommended) vs
  `PipelineContext` (wide read-only instance view). Blocks the S2 signature only.
- **Edge fusion**: `Build()` may fuse consecutive stages that share a stream into one inline chain
  (the producing thread walks the whole chain — owner's "one stage does it all" model), removing the
  rings on fused edges. Deferred until the ring-based core is stable; the lazy definition (D1) is
  what makes it possible.
- `DynamicPipeline`: runtime mutation class (owner: later, separate class).
- `std::execution` scheduler adapter for `TaskStream` / stdexec bridge.
- Coroutine `hbe::Task<T>` awaiting a pipeline sink.
