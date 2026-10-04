# PLAN — F5: remove the task system's dead code

## Goal

Delete the six dead declarations and definitions found while reviewing the task system, and delete or
amend the `docs/` pages that own them. No behaviour change: every symbol removed has zero callers in
the tree. The base-stream redesign (`DispatchToMainThread` folded into the regular intake) is a separate
owner decision and is deliberately **not** part of this plan — removing this code shrinks that later job.

## Scope — exactly what is deleted

| # | Symbol | Declared | Defined | Reference pages |
|---|---|---|---|---|
| 1 | `TaskStream::TaskQueueItem` (nested type) | `Engine/Core/TaskStream.h:45-55` | `Engine/Core/TaskStream.cpp:20-29` (constructor and `operator<`) | row `docs/Core/TaskStream/index.html:111`, Coverage row `:221` |
| 2 | `MainThreadTaskQueue::TaskItem::isDone` + `HasFinished()` | `Engine/Core/MainThreadTaskQueue.h:24,41-44` | constructor initializer `:30` | row `docs/Core/MainThreadTaskQueue/index.html:122`, Coverage row `:165` |
| 3 | `MainThreadTaskQueue::TaskItem::operator<` | `Engine/Core/MainThreadTaskQueue.h:35-38` | — | same rows as #2 |
| 4 | `MainThreadTaskQueue::RequestStop()` | `Engine/Core/MainThreadTaskQueue.h:61` | `Engine/Core/MainThreadTaskQueue.cpp:58-61` | delete `docs/Core/MainThreadTaskQueue/requeststop.html`, drop its index row |
| 5 | `MainThreadTaskQueue::IsRunning()` + `std::atomic<bool> isRunning` | `Engine/Core/MainThreadTaskQueue.h:52,62` | `.cpp:11-14,63-66` | delete `docs/Core/MainThreadTaskQueue/isrunning.html`, drop its index row |
| 6 | `TaskStream::ProcessPostedTasks()` | `Engine/Core/TaskStream.h:225` | `Engine/Core/TaskStream.cpp:598-601` | delete `docs/Core/TaskStream/processpostedtasks.html`, drop its index row |

## Evidence that each one is dead

- `BoundedPriorityQueue` never calls `HasFinished()` and never compares elements — the only mention in
  `Engine/Container/BoundedPriorityQueue.h` is the `@tparam` sentence on line 23. The queue buckets by
  `priority`, so `operator<` has no user either. `Deque` sorts nothing.
- `isDone` is written once by the constructor and never again anywhere in the tree.
- `RequestStop`/`IsRunning`: no caller outside the class. `.Plans/TODO_task_system.md:158` already
  records this. The `RequestStop` hits in `TaskProvider.h`/`TaskHandle` are a different class.
- `TaskQueueItem`: never constructed; `duration` and `operator<` exist only inside it.
- `ProcessPostedTasks`: `TaskStream::Update` calls `postedTasks.ProcessTasks()` directly at
  `Engine/Core/TaskStream.cpp:347`; the forwarder has no caller.

## Deliberate exclusions (say so rather than drift)

- `WorkItem::operator<` — dead by the same argument, not in F5, not touched. Reported as a candidate.
- `Engine/Container/BoundedPriorityQueue.h:23` — its `@tparam` sentence still claims an element type must
  provide `HasFinished()`. Stale before this change and stale after; `WorkItem` still satisfies it, so no
  contradiction in the tree. Container is a separate unswept module: reported, not edited.
- `DispatchToMainThread`, `DispatchPostedTasks`, `HasPostedTasks`, `MainThreadTaskQueue::Enqueue`,
  `ProcessTasks`, `HasPendingTasks` — all live. Untouched.
- `TaskSystem::isRunning`, `TaskHandle::RequestStop` — different symbols with the same spelling. Untouched.

## Steps

| Step | Action | Proof |
|---|---|---|
| 1 | Delete the C++ declarations and definitions (files above) | `git diff` shows deletions only, plus one initializer |
| 2 | Delete the two method pages, drop the deleted rows, amend the Coverage sections and any prose that described the removed members | `docs_coverage.py check-file` on both headers |
| 3 | Remove every `docs/` link that pointed at a deleted page | `htmlcheck.py`, plus a tree grep for the three deleted hrefs |
| 4 | Re-run the lint layers on the touched pairs | `comments.py`, `blank_lines.py`, `includes.py`, `layout.py` |
| 5 | Build and run the suite in all three configurations | `runtest.sh Debug`, `runtest.sh Dev`, `runtest.sh Release` |
| 6 | Commit | `git show --stat` |

## Known risk

`MainThreadTaskQueue::TaskItem` keeps its byte size (24 before, 24 after: `priority` + 7 pad + two
pointers either way), so no queue layout or allocation changes. Everything else removed is unreferenced,
so the three-configuration build is the whole proof.
