# Plan — everything still open in the task system, with order and proof

**Design authority:** [`docs/TaskSystemRedesign.md`](../docs/TaskSystemRedesign.md) — higher R-number wins.
**Parent plan:** [`PLAN_task_system_refactor.md`](PLAN_task_system_refactor.md) (B/G codes).
**Immediate predecessor:** [`PLAN_queue_item_type.md`](PLAN_queue_item_type.md) §8 (drain steps, already written).
**Gate for every step:** `./build.sh Applications/EngineTest -test -debug|dev|release`, then the wall-clock runner
`build/gate/runtest.sh <Config> [seconds]` — exit 0 is *not* a pass, require the line
`EngineTest: all 59 collections passed`. Then `.pi/skills/hb-standards/scripts/check.sh --staged --no-build`.
Stage owned paths by name; never `git add -A` (a concurrent agent owns `docs/Core`, `docs/OSAL`).

---

## 0. Status as measured today

| Done and gated | Evidence |
|---|---|
| Results live in the task, one 128-byte packet | `1f7e777`, price test pins 128/176/256 |
| Join is declared up front, record carries the successor | `2ea4d72` (R29) |
| The thread that closes a join delivers the outcome, no completion container | `8b540ac` (R30) |
| `ParallelFor` splits and its join fires a collator | `74bd776` (R32, R33) |
| `RangedTask` deleted; the queue holds a 56-byte `WorkItem` | `f39fe2e`, `d920d3c` (R25) |
| Provider attaches to a lane, per-stream slot with lane mask, cap 64, refusal logs | `7806ca2` (R36) |
| `WindowExample` lives in `Examples/`, driven by a provider skeleton | `96945c6` |
| Drain and detach design settled, unresolved fork resolved | R38, R39 (`bd77ec6`) |

**Plan-file hygiene owed:** the parent plan still draws B3b, B3c and B4 as open though they are in the code
(`BoundedPriorityQueue.h:16-17` priority inversion, `TaskStream.h:62-63` two lanes, `TaskSystem.h:86,91` named streams),
and its commit ladder numbers two steps "8". Correct both.

---

## 1. Provider lane drain and detach — the current step (R35, R37, R38, R39)

Five steps, in this order, one commit. Full text in `PLAN_queue_item_type.md` §8.

| # | Change | Why this order |
|---|---|---|
| 1 | `TaskStream` holds one `TaskProvider*` list per lane plus a drain-in-flight count, both under `queueLock` | The drain reads the stream, so the stream must own the list (R38) |
| 2 | `AttachTo(stream, lane)` keeps slot bookkeeping **and** registers into a live stream | Refusing invented indices would convert the idempotence, capacity and lane-mask probes into registration tests |
| 3 | `Produce(const TaskProduceContext&, WorkItem& outWorkItem)`; update `RecordingProvider` and the example | An item from `Task::GenerateSubTask` carries a real `taskID` and real range (R39) |
| 4 | Lane empty **and** `MayTakeNewWork()` **and** provider attached → ask once per pass; `false` ends that lane's drain | A spent allowance must not manufacture work the budget refused |
| 5 | `DetachFrom`/`DetachAll` under the stream lock waiting out the in-flight count; `~TaskProvider` asserts clean | Without step 4's counter this is a bare assert that cannot see the one window that matters |

| Test | Gate it proves | Trap |
|---|---|---|
| A provider item runs on the stream that drained it | the drain exists and routes by lane | asserting "the provider was called" passes while nothing runs |
| A stream with a spent allowance never asks | the drain is budget-gated | tests must not consume the shared general queue |
| `false` is asked at most once per pass | `false` ends the drain instead of spinning | needs a pass boundary, not a timeout |
| Attached to both lanes → asked per lane | R36's per-(stream,lane) rule is real | — |
| `DetachAll` returns only after an in-flight `Produce` | R37's wait prevents use-after-free | a `Produce` that finishes instantly proves nothing — block inside it |
| An item naming a released task is dropped with the R25 warning | providers do not bypass liveness | needs the in-band sentinel, not a sleep |

---

## 2. Prove or retire the resume test's own assertions

`An item that returns part of its range resumes at the index it stopped at` passes, but neither mutant that the suite
catches (`progress dropped`, `strict HasFinished`) fails it — both are caught by `Bagel Problem`. Either find a mutation
only it kills (candidates: `GenerateSubTask` setting `current` to `end`, or `Run` passing `start` instead of `current` to
the runnable) or state it as a direct `WorkItem::Run` contract test that deliberately does not depend on stream timing.
No engine change expected; a test-only commit.

## 3. Remove `Wait`, `BusyWait` and public `HasDone`

Producer-side waiters left: `Logger.cpp:266` (`HasDone`) and `Logger.cpp:272` (`Wait`). Everything else is inside
`#ifdef __UNIT_TEST__` (`TaskSystem.cpp` 12 sites, `TaskRegistry.cpp` 3). Give `Logger` the "queue this task whole on a
named stream" entry point that `DispatchSuccessor` already performs inline, so it stops waiting, then delete the three
entry points and rewrite the ~15 test sites to count runs or use an in-band sentinel. **Do not keep `HasDone` public
"because tests use it"** — that keeps the synchronous model alive by its tail. This unblocks item 4.

## 4. B3d — max age and abandonment

Steady-clock stamp taken at enqueue, evaluated in the existing sweep, per-task choice of abandon (report the subtask
finished) or escalate. Guardrails 1 and 2 are untestable before this exists. Decides the abandoned-child rule that G5
left open, and needs the deadline read in engine-clock, not wall-clock (guardrail 2).

## 5. B6a then B6b — the pump owns the frame

B6a: `Engine::Run()` pumps a frame tick to task system, renderer and log flush while applets keep their loops.
B6b: applets become `TaskProvider`s and applet-owned loops go away — the `Examples/WindowExample` skeleton is the pilot,
and its interim `PumpProvider` call disappears exactly when step 4 of item 1 lands. Parent plan §6 must be re-derived
here rather than assumed.

## 6. B8 — four guardrails as tests, not prose

| # | Guardrail | Status |
|---|---|---|
| 1 | Abandonment always destroys — a dropped task still runs RAII | blocked by item 4 |
| 2 | Deadlines are engine-clock relative and stream-independent | blocked by item 4 |
| 3 | Enqueue is SPSC lock-free or mutex, never MPMC lock-free | independent; needs a ThreadSanitizer build, which this tree does not have — decide whether to add one or record the gap |
| 4 | Cross-stream isolation — communicate only via outcome delivery | independent, doable now |

Plus two listed extras: budget enforcement sustained over many frames, and stopping a provider mid-frame must not corrupt
a task (that one belongs with item 1's tests, where it is cheapest to arrange).

## 7. B9 — documentation catch-up

`docs/TaskSystemGuide.md` still teaches range splitting. It must describe: declare-then-enqueue (`ReserveSubTasks`),
delivery by the join-closing thread, `ParallelFor`, the work item and its resumability, providers attached to lanes, and
what a user may not do (never wait on a task, never invent a range). Mark superseded rows in
`docs/TaskSystemRedesign.md` rather than deleting them. Per `AGENTS.md`, the HTML tree mirrors `Engine/`: `TaskProvider`
and `WorkItem` have **no** page yet, and their header contracts are currently the only documentation — those two pages
belong to the concurrent agent's `docs/Core` territory, so coordinate before writing them.

## 8. Recorded, deliberately not done

| Item | Why left |
|---|---|
| `TaskStreamAffinity` width 64 bits for ~10 streams | Correct at 8 bytes now; shrinking it again buys 6 bytes and touches every affinity user |
| `docs/OSAL/Application/index.html:98` cites the pre-move path | That file is the concurrent agent's untracked work; editing it collides |
| Drain-in-flight count when nothing can detach yet | Would be dead code no test can distinguish from absence — resolved by item 1, which is why item 1 is first |

---

## Verification discipline, learned the expensive way this session

1. Exit 0 is not a pass — require `all 59 collections passed`; the runner reports `128 + signal` for signal death.
2. A mutation verdict needs three proofs in order: the substitution **changed the file**, that file **owns the code**, and
   the build **relinked** (0 errors and a new binary timestamp). Two false verdicts came from skipping one each — a
   rejected build re-ran the previous binary, and a header mutation written into a `.cpp` was a silent no-op.
3. After a mutation run, rebuild before believing a "clean" result.
4. Recompute every anchor after mutating the same string; a stale byte offset spliced decision rows into a prose
   paragraph.
5. Do not consume the shared general queue from a test; a negative assertion needs an in-band sentinel queued behind the
   subject on the same lane, not a timeout.
6. If something is going to be written as "not verified", verify it before the commit.
