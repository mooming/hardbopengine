# TODO — task system, including the half-finished change in the working tree

**Read this before anything else in this directory:** group **A** landed and is gated (Debug/Dev/Release, 59 collections,
0 mechanical violations), but **nothing yet proves the drain delivers work** - that is group B, and until it lands the drain
is implemented-and-inert rather than implemented-and-proven. The only production user is `Examples/WindowExample`. Gate for every item: `./build.sh Applications/EngineTest -test -debug|dev|release`, then
`build/gate/runtest.sh <Config> [seconds]` and require the line `EngineTest: all 59 collections passed` — exit 0 alone is
not a pass.

| Group | What | Tree state |
|---|---|---|
| **A** | Provider lane drain and detach (R35, R37, R38, R39, R40) | **done and gated**; unproven (see B) |
| **B** | Verification owed on A | blocked by A |
| **C** | Deletions the new model now permits | independent of A |
| **D** | Remaining plan items (B3d, B6, B8, B9) | ordered, some blocked |
| **E** | Recorded and deliberately not done | — |

---

## A. Finish the in-flight change (11 items)

Already written and compiling-clean in isolation: `TaskStream.h/.cpp` — per-lane provider lists (`MaxProvidersPerLane = 8`),
`AttachProvider` / `DetachProvider` / `IsProviderAttached`, `DrainProvidersLocked`, and the drain wired into the lane path
(asks only an empty lane, only when `budget.CanTakeWork()`, `CommitTake` on the lane it fed). `TaskProvider.h` — constructor
takes `TaskSystem&`, `~TaskProvider` declared, `Produce` now `std::optional<WorkItem>`, `DetachFrom` / `DetachAll`, private
`RegisterOnStream` / `UnregisterFromStream`, and the `.cpp` implementations of all of those.

| # | Task | Note |
|---|---|---|
| A1 | Forward-declare `class TaskSystem;` in `TaskProvider.h` | Anchor on `class TaskProvider` — `namespace hbe` occurs **twice** (the `__UNIT_TEST__` block), which is what aborted the last edit |
| A2 | `RecordingProvider`: override `std::optional<WorkItem> Produce(...)`, add `std::optional<WorkItem> itemToHand{}`, keep `produceResult == false → nullopt` | Its `using TaskProvider::TaskProvider;` inherits the new 2-arg ctor |
| A3 | Give `TaskProviderTest::Prepare()` a `auto& taskSystem = Engine::Get().GetTaskSystem();` and pass it to all 8 `RecordingProvider` constructions (7 named `provider`, one `other`) | Regex used last time was correct but the file was never written because the script died at A1 |
| A4 | Add `#include "Engine/Engine.h"` to `TaskProvider.cpp` | Mirrors `TaskStream.cpp:11` |
| A5 | `Examples/WindowExample/Main.cpp`: construct with `(name, taskSystem)`, return `std::optional<WorkItem>` from `Produce`, drop its own `Enqueue` call | The whole point: the stream now drains it, so the interim `PumpProvider` call goes away |
| A6 | Confirm the three existing provider probes still mean what they claim | Capacity probe attaches 64 invented indices — all `HasStream` false, so all recorded and none registered: correct by R38, but the probe must not start asserting registration |
| A7 | `~TaskProvider`'s assert | **proven in a real run**: the example destroyed a still-attached provider and the engine trapped (exit 133) with the assert naming the provider; after adding `DetachAll` the app exits 0. Not provable *inside* the suite (it aborts the process), but it was observed firing outside it | ~~not proven~~ **proven in a real run**: it aborts the process, so a test that provokes it kills the run. It was observed firing during development (a probe that registered on a real stream and never detached), and that is the honest extent of the evidence. What changed instead: the assert counts **registrations**, not recorded attachments, because an attachment naming a stream the engine does not have dangles nothing |
| A8 | Verify no lock-order inversion: `Produce` runs holding `queueLock` and may call `TaskSystem::CreateTask` (registry lock) | Documented as the expected pattern in `Produce`'s contract; check nothing takes registry-then-stream nested, which would invert it |
| A9 | Three-configuration build + run, `check.sh --staged` | 59 collections, 0 mechanical violations |
| A10 | Record **R40** in `docs/TaskSystemRedesign.md`: the queue lock is held across `Produce`, so detach is a lock acquisition and no drain-in-flight counter exists | Supersedes R37's "waits for a drain already in flight" mechanism, not its guarantee; also amend R39's `false` wording to `nullopt`, and R38's "when the engine has a task system" to "the task system it was constructed with" |
| A11 | Commit as one commit | Steps 1–5 of `PLAN_queue_item_type.md` §8 are one unit: a detach before the drain would have a wait whose body can never run |

## B. Verification owed on A (6 items — each test names the gate it proves)

| # | Test | Gate | Trap that makes it pass falsely |
|---|---|---|---|
| B1 | A provider's item **runs** on the stream that drained it | the drain exists and routes by lane | asserting "the provider was called" passes while nothing is ever run |
| B2 | A stream whose allowance is spent never asks | the drain is budget-gated, not merely queue-gated | must not consume the shared general queue |
| B3 | `nullopt` is asked at most once per lane per pass | `nullopt` ends the drain instead of spinning | needs a pass boundary, not a timeout |
| B4 | Attached to both lanes → asked per lane | R36's per-(stream,lane) rule is real | — |
| B5 | `DetachAll` cannot return while a `Produce` is inside | R37/R40's guarantee against use-after-free | a `Produce` that returns instantly proves nothing — block inside it on a flag the test controls |
| B6 | An item naming a released task is dropped with the R25 warning | providers do not bypass liveness | needs the in-band sentinel on the same lane, not a sleep |

Then mutation-prove each: substitution **changed the file**, that file **owns the code**, build **relinked** (0 errors + new
binary timestamp). Two false verdicts this session came from skipping one of those three.

---

## C. Deletions the new model now permits (3 items)

| # | Task | Detail |
|---|---|---|
| C1 | Remove `Task::Wait`, `Task::BusyWait`, public `HasDone` | Last producer-side waiters are `Logger.cpp:266` (`HasDone`) and `:272` (`Wait`); give Logger the "queue this task whole on a named stream" entry point that `DispatchSuccessor` already performs inline, then rewrite ~15 `#ifdef __UNIT_TEST__` sites to count runs or use a sentinel. Do **not** keep `HasDone` public "because tests use it" |
| C2 | Demote `Task::GenerateSubTask` to an engine-internal helper | It is the range protocol; users get `ParallelFor`, providers get it via friendship until a narrower surface exists |
| C3 | Prove or retire the resume test's own assertions | `An item that returns part of its range resumes…` passes, but neither caught mutant (`progress dropped`, `strict HasFinished`) fails it — both are caught by `Bagel Problem`. Candidates for a mutant only it kills: `GenerateSubTask` setting `current` to `end`, or `Run` passing `start` instead of `current` |

## D. Remaining plan items (ordered)

| # | Item | Blocked by | Scope |
|---|---|---|---|
| D1 | **B3d** — max age and abandonment: steady-clock stamp at enqueue, evaluated in the existing sweep, per-task abandon-or-escalate | C1 (the abandoned-child rule needs no `Wait`) | Also decides G5's open abandoned-child question; deadline must read engine clock, not wall clock |
| D2 | **B8 guardrail 4** — cross-stream isolation: communicate only via outcome delivery | — | Independent, doable now |
| D3 | **B8 guardrail 3** — enqueue is SPSC lock-free or mutex, never MPMC lock-free | needs a ThreadSanitizer build this tree lacks | Decide: add the build, or record the gap explicitly |
| D4 | **B8 guardrails 1, 2** — abandonment destroys (RAII on a dropped task); deadlines are engine-clock and stream-independent | D1 | Leak/heap checks for 1; fast custom stream vs engine stream for 2 |
| D5 | Extra listed guardrails — budget sustained over many frames; stopping a provider mid-frame must not corrupt a task | A | Cheapest to add with B1–B6 |
| D6 | **B6a** — `Engine::Run()` pumps a frame tick to task system, renderer, log flush (applets keep their loops) | — | — |
| D7 | **B6b** — pump owns the frame; applets become `TaskProvider`s; applet-owned loops removed | A, D6 | Re-derive parent plan §6 here rather than assume it |
| D8 | **B9** — rewrite `docs/TaskSystemGuide.md` (still teaches range splitting); mark superseded R rows rather than deleting | A, C1 | Must cover declare-then-enqueue, delivery by the join-closing thread, `ParallelFor`, the item and its resumability, lane-attached providers, and the user prohibitions (never wait on a task, never invent a range) |
| D9 | HTML API pages for `TaskProvider` and `WorkItem` (no pages exist; header contracts are the only docs) | A | `docs/Core` is the concurrent agent's territory — **coordinate before writing** |
| D10 | Parent-plan hygiene: mark **B3b**, **B3c**, **B4** done (evidence: `BoundedPriorityQueue.h:16-17`, `TaskStream.h:62-63`, `TaskSystem.h:86,91`); fix the commit ladder that numbers two steps "8" | — | Docs only |

## E. Recorded and deliberately not done

| Item | Why |
|---|---|
| Shrinking `TaskStreamAffinity` below 64 bits (~10 streams today) | Correct at 8 bytes; the next byte is not worth touching every user |
| `docs/OSAL/Application/index.html:98` cites the pre-move `Applications/` path | That file is the concurrent agent's untracked work |
| A drain-in-flight counter per stream | Made unnecessary by A10's decision; adding it would be dead code no test can distinguish from absence |

---

## Standing rules this list was written under

Stage owned paths by name — never `git add -A` (concurrent agent owns `docs/Core`, `docs/OSAL`). Never push without
permission. No comments in `.cpp` except structural labels; contracts in the paired `.h`, rationale in `docs/`. Recompute
every anchor after mutating the same string. If something is going to be written as "not verified", verify it before the
commit rather than promising it afterwards.
