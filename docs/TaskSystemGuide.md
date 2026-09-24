# TaskSystem Guide

How to run work on the HardBop Engine task system, and what the engine will not do for you.

Every statement here was checked against the source at commit `28a0cbc` on 2026-09-20. Where the guide and
`Engine/Core/*.h` disagree, the header wins and this file is wrong - report it.

Design decisions and the reasons behind them live in [TaskSystemRedesign.md](TaskSystemRedesign.md), keyed by
`R`-number. This guide is the user-facing half: what to call, in what order, and what breaks if you do not.

## 1. The three objects

| Object | What it is | Who owns it |
|---|---|---|
| `Task` | One unit of work: a runnable, its user data, a subtask counter, and one result packet. | The `TaskRegistry`. It lives inside a registry record and never moves. |
| `WorkItem` | One **work item**: a half-open index range `[start, end)` of a task, plus priority and the task it belongs to. 56 bytes, so queues take it by value. | Whichever queue holds it. |
| `TaskID` | `index` + `generation`. The only thing safe to hold across threads. | You, by value. |

A task is split into work items, work items are queued to streams, and streams are threads. Nothing else in
this subsystem has a lifetime worth worrying about. Building an item is the engine's job, not yours: an item's
range, priority and the task's reserved count have to agree, and `Task::GenerateSubTask` is engine-internal, so a provider builds items with the protected `TaskProvider::MakeWholeItem` and a caller that only wants work run uses `TaskSystem::EnqueueTask`.
outside the engine for exactly that reason. A customer creates a task, declares how many items will fill it, and
dispatches it — `TaskSystem::EnqueueTask(stream, task)` for single-shot work, or a provider's
`MakeWholeItem(task)` when it is producing items for a stream.

The reason `TaskID` exists at all: a `Task&` is a pointer into a record another thread may hand back to the
free list and refill with a different task. `TaskRegistry::Find` refuses such a reference by comparing the
generation, and `TaskStream` drops the work item with a warning naming the record index and generation. That
behaviour is `3be27c3` and rule R7.

## 2. Create a task through the registry

```cpp
auto& taskSystem = Engine::Get().GetTaskSystem();

const hbe::TaskID taskID = taskSystem.CreateTask("Compute", computeFunc, &accumulator);
if (taskID.IsNull())
{
    // The registry had no free record. Nothing will run; decide what to do rather than proceed.
}

hbe::Task* task = taskSystem.FindTask(taskID);   // nullptr if the identity names nothing
```

`CreateTask` hands back a null `TaskID` when the registry is full and cannot grow - it never grows implicitly,
because growth is a memory decision and a silent one is a surprise at the worst moment. See `TaskRegistry::Create`.

**Do not construct a `Task` as a local variable.** The constructors are public, so it compiles, and it does
nothing: a task that was not issued by a registry keeps a null `TaskID`, every work item derived from it
carries that null identity, and every stream that picks one up calls `FindTask`, gets `nullptr`, and drops it.
You get one warning line per work item in the log and no executed work. If you need a task, create one.

## 3. Run it

```cpp
constexpr std::size_t count = 1000000;
constexpr std::size_t numSubTasks = 10;
constexpr std::size_t increment = count / numSubTasks;

for (std::size_t i = 0; i < count; i += increment)
{
    TaskSystem::EnqueueTask(workerIndex, *task);                              // one stream's own queue
}
TaskSystem::EnqueueTask(TaskSystem::GetIOTaskStreamIndex(), *task);       // the IO stream's own queue
```

| Call | Meaning |
|---|---|
| `TaskProvider::MakeWholeItem(task, priority)` | Protected static on the provider base: builds one `WorkItem` covering a whole task. A customer-authored provider inherits it. `Task::GenerateSubTask` is private and engine-internal.