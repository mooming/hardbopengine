# Plan D3b — the queue item type: delete `RangedTask`, keep range progress

**Companion:** [`PLAN_task_system_refactor.md`](PLAN_task_system_refactor.md) (G5), [`PLAN_embedded_result_delivery.md`](PLAN_embedded_result_delivery.md).
Design authority: [`docs/TaskSystemRedesign.md`](../docs/TaskSystemRedesign.md) — higher R-number wins. R25 is this commit's rule.

---

## 0. Deviation approved by the owner, 2026-09-22

R25 says `RangedTask` is deleted *in the commit that lands `ParallelFor`*. `ParallelFor` landed in `74bd776`
without the deletion. The owner was asked and chose: **delete it in its own commit.** R25's stated reason was
"do not rewrite the same call sites twice", and that reason is satisfied now — `ParallelFor` exists and `Bagel
Problem` was migrated off `GenerateSubTask` onto it, so the sites below are rewritten once, not twice.

Record this in the design doc as part of the D3b commit: a sentence in R25's row saying the deletion followed the
splitter rather than riding with it, and why. Do not leave the row reading as a rule that was ignored.

---

## 1. What the type actually is today

Not a caller convenience — it is what every queue stores:

| Container | Where |
|---|---|
| `Deque<RangedTask>` (FIFO lane) | `TaskStream.h` |
| `BoundedPriorityQueue<RangedTask>` (priority lane) | `TaskStream.h` |
| `BoundedPriorityQueue<RangedTask>` (general queue) | `TaskSystem` |
| `TaskQueueItem` embeds one | main-thread dispatch |

Measured sizes: `sizeof(RangedTask)` = **128** (a `StaticString` name copy, a `TaskID`, an affinity mask, two
atomics, four index fields) against roughly **48** for the fields a queue genuinely needs:
`{ TaskID, start, current, end, priority, affinity }`.

`current` must survive. `Engine/Core/TaskSystem.cpp`'s "Bagel Problem (Incremental Task)" test exists to prove a
work item that returns less than its range is resumed by the same stream, so the replacement item carries progress.
Do not "simplify" it to `{TaskID, start, end, priority}` — that silently deletes resumability, and the test that
catches it is the one named above.

---

## 2. Sites to change (census at `74bd776`)

| Symbol | Files |
|---|---|
| `RangedTask` | `TaskStream.h` 10, `TaskStream.cpp` 9, `Task.h` 5, `TaskSystem.h` 4, `TaskSystem.cpp` 4, `RangedTask.h` 7, `RangedTask.cpp` 3, `Runnable.h` 3, `Task.cpp` 1 |
| `GenerateSubTask` | `Task.h`, `Task.cpp`, `TaskSystem.cpp`, `TaskRegistry.cpp`, `Logger.cpp`, `UnitTestCollection.cpp` |
| `Wait` / `BusyWait` / public `HasDone` | 23 sites: `TaskSystem.cpp` 12 (mostly tests), `TaskRegistry.cpp` 3, `Task.cpp` 3, `Logger.cpp` 2, `Task.h` 2, `TaskSystem.h` 1 |

Plan: introduce the item type, repoint every container and both enqueue entry points, move `RangedTask::Run`'s
join-reporting onto the new item, move `Logger.cpp`'s carrier use (it enqueues a one-unit range holding the drain
runnable) onto an engine-side "queue this task whole" entry point, then delete `RangedTask.h/.cpp` and demote
`Task::GenerateSubTask` to an engine-internal helper.

`Task.h`'s caller-side protocol after this: `ReserveSubTasks` + `ReportFinishedSubTask` + `GetResult` +
`TaskSystem::Enqueue`/`ParallelFor`/`SetSuccessor`. Nothing above it may keep the old range protocol — that is R29's
consequence and the reason `Task::Start` was already deleted.

---

## 3. Verification, in the order that has been catching things

1. Build Debug, then run with a wall-clock limit — **`/tmp/hbe/runtest.sh <Config> [seconds]`**, which reports
   `128 + signal` for a child that dies by a signal. Exit 0 alone is not a pass; require the
   `EngineTest: all 59 collections passed` line (the incremental-resume test and the general-queue gate test are the
   two most likely to break, and both fail loudly if the item type is wrong).
2. Gate all three configurations: `./build.sh Applications/EngineTest -test -debug|dev|release`.
3. Mutation-prove every check that is new, and re-run the tests that prove *resumability* by breaking resumability
   (drop `current` propagation) and *affinity* (let an item ignore its affinity mask).
4. `check.sh --staged --no-build`, then commit. Expect the one standing advisory: the `no m_ prefix` rule firing on
   a `using TIndex` type alias. It is a false positive and has been accepted every commit since it first appeared.
5. Do not `git add -A`: a concurrent agent owns `docs/Core`, `docs/OSAL` and friends. Stage owned paths by name.

---

## 4. Traps already paid for, so they are not paid twice

- `TaskSystem::TIndex` is `Array<TaskStream>::TIndex`, which is **`int`** (`Engine/Container/Array.h:20`), while
  `hbe::TIndex` is `size_t`. Same spelling, different types, and the compiler prints both as `TIndex`. Any index
  arithmetic in the item type has to say which one it means, and any count taken from a caller has to be refused at
  zero **or below** (R33).
- Exact-string file edits break whenever `clang-format` re-wraps. Use whitespace-flexible matching
  (`/tmp/hbe/flex.py`).
- After a mutation run, rebuild before believing a "clean" result: restoring sources leaves the mutant binary.
- A test runs on the base stream's thread: it may not sleep waiting for the base stream, and delivery now happens on
  the thread that closed a join, not on the base thread, so a delivery test needs an in-band barrier
  (a sentinel queued behind the subject on the same lane) rather than a timeout.
- Do not enqueue to the shared general queue from a test: `TaskSystem::Dequeue` can steal another collection's item.

---

## 5. Two things settled after this plan was written (2026-09-22)

- **R34 withdraws B3e**: providers are user-side and `TaskProvider` has zero engine callers. So this commit does **not**
  need to leave a provider-shaped hole or hook anything to a provider registry. It needs the one primitive both this
  commit and delivery want: *queue a task whole on a named stream*. `DispatchSuccessor` already does that inline for a
  successor (`ReserveSubTasks` >= 1, one item covering the range); `Logger.cpp`'s carrier use becomes the public form
  of the same thing. Give it one name and one definition, not two.
- **D3c's only producer-side user is `Logger.cpp:266` (`HasDone`) and `:272` (`Wait`)**. Everything else waiting on a
  task is inside `#ifdef __UNIT_TEST__` (`TaskSystem.cpp` 12 sites, `TaskRegistry.cpp` 3). Once this commit gives
  Logger a task-whole enqueue that it does not have to wait on, D3c is: delete `Task::Wait`, `Task::BusyWait` and the
  public `HasDone`, and rewrite ~15 test sites to observe runs through a counter or an in-band sentinel instead of
  asking a task whether it is finished. Do not leave `HasDone` public "because tests use it" - that keeps the
  synchronous model alive by its tail.
