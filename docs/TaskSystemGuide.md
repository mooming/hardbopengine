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
| `RangedTask` | One **work item**: a half-open index range `[start, end)` of a task, plus priority and affinity. 120 bytes, trivially copyable, so queues take it by value. | Whichever queue holds it. |
| `TaskID` | `index` + `generation`. The only thing safe to hold across threads. | You, by value. |

A task is split into work items, work items are queued to streams, and streams are threads. Nothing else in
this subsystem has a lifetime worth worrying about.

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
    taskSystem.Enqueue(task->GenerateSubTask(i, i + increment));            // general queue
}
taskSystem.Enqueue(TaskSystem::GetIOTaskStreamIndex(), task->GenerateSubTask(0, increment));  // one stream
```

| Call | Meaning |
|---|---|
| `Task::GenerateSubTask(start, end, priority = 0)` | Returns one work item naming `[start, end)` and **increments the task's subtask count**. One call, one work item, one count. |
| `TaskSystem::Enqueue(const RangedTask&)` | General queue: shared, lowest priority, any eligible stream may take it. |
| `TaskSystem::Enqueue(TIndex streamIndex, const RangedTask&)` | One stream's own queue. Cannot refuse - see section 12. |

The runnable:

```cpp
std::size_t computeFunc(void* userData, std::size_t startIndex, std::size_t endIndex)
{
    auto* accumulator = static_cast<double*>(userData);
    for (std::size_t i = startIndex; i < endIndex; ++i)
    {
        *accumulator += 1.0 / (i + 1);
    }
    return endIndex - startIndex;    // processed count
}
```

Return fewer indices than you were given and the work item is **re-added to the same lane later**, resuming at
`startIndex + returnedCount`. That is the time-slicing hook, and it is how a long task yields without being
preempted - a task already running is never stopped, only re-queued.

`Task::HasDone()` is true when at least one subtask was generated and all of them reported finished. It is
public today and it is **scheduled for removal by R8**; read section 5 before relying on it.

Release the record when you are done with it:

```cpp
taskSystem.ReleaseTask(taskID);   // the creator releases. Releasing with work items still queued is
                                  // detected and logged, not silently wrong - R7.
```

## 4. Streams, and which thread you are on

| Index | Stream name | Purpose |
|---|---|---|
| 0 | `Base` | The base stream. Has its own thread; is **not** the thread that drives `Engine::Run`. |
| 1 | `IO` | I/O work. |
| 2 and above | `Worker1`, `Worker2`, ... | Scaled to hardware concurrency. |

Three different things used to be called "base". They have three names now (`2b30818`):

| Name | Question it answers |
|---|---|
| `TaskSystem::GetBaseTaskStreamIndex()` / `BaseStreamIndex` | Which stream is the base stream: `0`. |
| `TaskSystem::IsBaseThread()` | Is the caller **running as stream 0** right now - i.e. inside a task on that thread. |
| `TaskSystem::EngineLoopThreadName` (`"EngineLoop"`) | The OS thread that drives `Engine::Run` and shuts the engine down. |

A thread the application created, and the engine-loop thread itself, report
`TaskSystem::GetCurrentStreamIndex() == TaskSystem::NonStreamIndex`. That is deliberate and it is not zero:
before it had its own value, **every** thread in the process answered "stream 0", which made `IsBaseThread`
useless as a check and charged a foreign thread's contact with a task against stream 0's affinity. The
consequence you must know: **a thread that is not a stream takes nothing from the general queue.** It is not
an error, it is a silent no-op - section 7 says why.

## 5. Waiting, and why you should stop

`Task::Wait(intervalMilliSecs)` and `Task::BusyWait()` still exist and both are spin-or-sleep loops over
`HasDone()`. R8 removes them, and the reason is not tidiness:

- **Never block on the base stream.** That thread runs the pass that reopens every stream's budget and
  performs every result delivery (R23c, corrected rationale in R23d). Block it and the whole engine's
  bookkeeping stops - including the delivery your wait is hoping to see. The unit-test suite runs *inside* a
  task on the base stream, so a wait written there is standing on that thread.
- On any other thread a wait still costs you a thread that could be running work, for no ordering guarantee
  the successor model would not give you more cheaply.

The replacement is a continuation job - a successor task enqueued onto the destination stream with the outcome
embedded - and that is the next round of work (R9, section 6.1 of the design document). Until it lands,
polling your own `TaskID` with `FindTask` is the only non-blocking observation available:

```cpp
if (auto* task = taskSystem.FindTask(taskID); task != nullptr && task->HasDone())
{
    // read task->GetResult()
}
```

## 6. Results: one packet per task

A task's result is a `ResultPacket` **embedded in the task object** - not parked in a buffer owned by a stream
(that design was deleted; see section 12). The sizes are fixed and asserted at compile time:

| Field | Size | Notes |
|---|---|---|
| Header | 8 bytes | Byte 0: kind. Byte 1: destination stream index. The rest has no meaning yet. |
| Payload | 120 bytes | Yours. The engine never interprets it. |
| Total | 128 bytes | `static_assert`ed. Prices the registry record at 192 bytes. |

| Constant | Value | Meaning |
|---|---|---|
| `ResultPacket::KindNoResult` | `0` | No packet was written. `HasResult()` is false. A record's packet is cleared when the registry issues it, so a reused record cannot report the previous occupant's answer. |
| `ResultPacket::FirstApplicationKind` | `64` | Kinds below this are the engine's. Yours start here. |
| `ResultPacket::NoDestinationStream` | `0xFF` | Nobody is to be told. This is fire-and-forget, and the caller that polls its own `TaskID` instead (R23b). |

Writing it, from inside the runnable:

```cpp
ResultPacket& result = task->GetResult();
result.SetKind(MyKind::HistogramDone);
result.SetDestinationStreamIndex(TaskSystem::GetBaseTaskStreamIndex());
*reinterpret_cast<std::uint64_t*>(result.GetPayload()) = total;
```

Three rules follow from R23 and are not negotiable at the API level:

1. **One packet.** A task with more output than 120 bytes keeps that output in memory it owns - a buffer, a
   file, a handle - and puts a reference to it in the payload. The engine's knowledge of results stops at this
   packet.
2. **The producer fills the destination.** Routing lives on the result, not on the task, because one task's
   output may go somewhere the caller did not expect.
3. **Nothing copies or frees a packet.** It is valid until its declaring task is released. A delivered packet
   is no longer "valid for one frame" - that was R4, superseded.

## 7. The general queue, and what affinity really does

`TaskSystem::Dequeue` looks at the **head** of the shared priority queue and asks one question: has this
stream already been refused this item once?

- First contact from a stream that is not eligible: mark the item and return **without popping**. The head
  stays, so arrival order is preserved and the stream moves on to its own lanes.
- Second contact from the same stream: take it.
- A fresh item is taken immediately by a worker; a `RangedTask` is built with the **base and IO bits cleared**,
  so those two streams each have to pass once before they may take a shared item.
- A thread whose stream index is `NonStreamIndex` is out of range for the 64-bit mask: `Get` answers false,
  `Set` changes nothing, and the head is left in place forever as far as that thread is concerned. So a
  foreign thread silently takes nothing.
- `Set`/`Unset`/`Get` store the mask inverted in raw bits (raw `0` means set), and one stream index is spread
  across 8 bits per 64-bit word rather than `index % 64`. The three operations are consistent with each
  other; do not read the raw buffer as a normal bitmask.

**Trap, learned the hard way:** the general queue is shared with every other test collection in the suite. A
test that calls `TaskSystem::Dequeue` to "look at" the queue can steal another collection's work item and make
an unrelated subsystem abort on an allocator error one log line later. There is no test seam for this queue -
do not write that test.

## 8. Budget and lane rates

A stream can be given a CPU allowance and a FIFO-to-priority ratio:

```cpp
taskSystem.GetStream(3).ConfigureBudget(std::chrono::duration<double>(0.004));  // 4 ms per window; 0 = unlimited
taskSystem.GetStream(3).ConfigureRate(3, 1);                                     // 3:1 FIFO to priority
```

The contract, from `CPUBudget.h` and `StreamDrainPolicy.h`:

| Promise | Detail |
|---|---|
| The gate sits in front of **acquiring** work | Never in front of finishing work already held. Refusing to dequeue is the mechanism; refusing to deliver would strand results. |
| Overshoot is bounded by the longest task | A task cannot be stopped once taken, so a lane can pass its share by the length of one task. `"share spent"` never means `"nothing in flight"`. |
| Zero allowance means unlimited | And an unlimited stream does not measure its tasks at all - `GetAccumulatedCPUTime()` stays zero until an allowance is configured. That is absent accounting, not free work. |
| Weights of zero are treated as one | A lane that can never be served should not exist. Free borrowing means an idle lane does not hold its share hostage, at the cost of the long-run ratio when both lanes are permanently backlogged. |
| Configure before the streams start, or from that stream's own thread | `CPUBudget` is single-owner by design: it charges the calling thread's CPU time, so pairing `BeginTask`/`EndTask` on two threads measures the span between unrelated threads. |

**Enforcement status as of `28a0cbc`, measured rather than assumed:** `CPUBudget::Reset()` has no production
caller, `TaskStream::MayTakeNewWork()` is called only from unit tests, and
`StreamDrainPolicy::EndRound()`/`IsRoundExhausted()` have no caller anywhere. So an allowance configured today
is not actually gating its stream, and there is no window boundary that would repay it. That gap is defect R2
and the budget-window round exists to close it; once that lands, expect a window to open on each stream's own
thread and the acquire gate to bite.

## 9. Work that must run on the engine-loop thread

```cpp
taskSystem.DispatchToMainThread([](void* userData) { /* runs on the EngineLoop thread */ }, data, 128);
taskSystem.ProcessMainThreadTasks();   // drains the queue
```

Priority is `0` = least urgent, `255` = most urgent, and the parameter **defaults to 128**. It used to default
to 0 under an inverted "0 = highest" convention; with the direction fixed, passing an explicit `0` now means
"as late as possible", so do not copy old snippets that do.

`Engine::Run()` is not a frame loop. It is:

```cpp
while (taskSystem.GetMainThreadTaskQueue().HasPendingTasks() || taskSystem.IsRunning())
{
    taskSystem.ProcessMainThreadTasks();
    std::this_thread::yield();
}
```

There is **no per-frame barrier** anywhere in this design. Correctness depends only on explicit result
delivery, and a heavy result may legitimately arrive several frames after it was produced (design section 2).
If you want frames, count them yourself.

### Starting and stopping

You do not call `TaskSystem::Initialize` yourself. `Engine::Initialize(argc, argv, levels)` takes flags from
`EInitLevel` and starts the task system for you:

| Level | Effect |
|---|---|
| `EInitLevel::TaskSystem` | Calls `TaskSystem::Initialize`, which reads the registry parameters and builds the streams. `Engine::Run` asserts this happened. |
| `EInitLevel::Logger` | Implies `TaskSystem` - the logger writes through the task system's IO stream - and `Engine::Initialize` adds the dependency rather than trusting you to spell it out. |
| `EInitLevel::Application` | Creates the OS application object and connects to the window server. A headless or CI context will refuse that, which is exactly when to pass `EInitLevel::TaskSystem | EInitLevel::Logger` instead of the `All` default. |

Shutdown is also not yours: `Engine::ShutDown` calls `RequestShutDown`, and `Engine::Run` calls `JoinAndClear`
when the loop ends, then drains the main-thread queue once more. A caller that calls `RequestShutDown` by hand
has asked every stream to stop while something else may still be about to enqueue to them - use the engine's
lifecycle, or know that you are owning that ordering.

## 10. Registry sizing

Three configuration parameters, all read at `TaskSystem::Initialize`:

| Parameter | Default | Meaning |
|---|---|---|
| `Task.RegistryInitialRecords` | `4096` | Records the table starts with, allocated in banks of the grow-by figure. |
| `Task.RegistryGrowByRecords` | `4096` | Records per growth. `TaskRegistry::Grow` is explicit - nothing grows behind your back. |
| `Task.RegistryMaxRecords` | `0` | Largest the table may reach. `0` means no ceiling. |

A record is **192 bytes**, three cache lines, and `TaskRegistry.h` asserts both `RecordSizeBytes == 192` and
`RecordSizeBytes % 64 == 0`, so a change to `Task`'s layout fails the build rather than silently landing a
record that straddles lines. The default table is therefore 768 KiB. If you re-measure those numbers, you are
changing a decision (R22, R23) and the assertion is the mechanism that forces you to say so.

Records never move - `Task` embeds an atomic counter, so it has no move assignment, and a `Grow` allocates
another bank rather than relocating anything. That is what makes a `TaskID` index safe to hold.

## 11. Memory

- Work items are 120-byte values that the queues own. Enqueuing copies one.
- Each stream runs inside an `AllocatorScope` over its own `MultiPoolAllocator`, named after the stream
  (`Base`, `IO`, `Worker1`, ...). That is the stream's allocation scope, not a task allocator - work items live
  in the stream's containers.
- Results are **not allocated**. The packet is part of the record, and the engine neither copies nor frees it.
- `ResultContainer` and `NamedPoolAllocator` were deleted in `ac496e6`. Their absence is a decision (R23, R25),
  not an oversight: under the embedded-packet design there is no per-stream result buffer left to size,
  ceiling, or refuse work for. Do not reintroduce them without retiring R23 first.

## 12. Do not do these things

| Pattern | What actually happens |
|---|---|
| `Task task("Name", func, data);` on the stack | Compiles. Every work item is dropped by every stream with a warning, because the identity is null. Section 2. |
| `task.Start(numSubTasks, start, end)` | **Do not use.** It has no caller anywhere in the engine or applications, and its multi-subtask path divides by the task's *member* `numSubTasks` instead of the `numberOfSubTasks` argument - zero on a fresh task, so a hardware divide-by-zero. Split ranges with `GenerateSubTask` and `Enqueue`. |
| `task.Wait()` / `task.BusyWait()` | Works, and is on its way out. Never on the base stream: you would be blocking the thread that performs your delivery (R8, R23d). |
| Relying on `HasDone()` as the completion mechanism | R8 removes it. Continuation jobs replace it. |
| `(void) taskSystem.Enqueue(streamIndex, item)` | The cast is now a lie. `Enqueue` cannot refuse since `ac496e6` - a result is no longer something a stream can run short of, because it is allocated when the task is created. Two such casts remain in the tree (`Engine/Log/Logger.cpp:246`, `Engine/Test/UnitTestCollection.cpp:166`) and are stale annotations, not evidence of a failure path. |
| Writing a test that drains the general queue | Steals another collection's work item and produces an allocator fatal in a subsystem your test never touched. Section 7. |

## 13. What is tested

`TaskSystemTest` (in `Engine/Core/TaskSystem.cpp`) currently holds:

| Test | What it pins down |
|---|---|
| `Empty Task` | A task with no work item is released cleanly. |
| `Task of size 0` | A zero-length range still reports itself finished rather than hanging a waiter. |
| `Bagel Problem` | Range-split work over the general queue produces the right sum. |
| `Bagel Problem (Incremental Task)` | A runnable that returns short of its range resumes and finishes. |
| `Both lanes serve their tasks` | Neither lane starves the other. |
| `Stream charges a configured budget` | `MayTakeNewWork` and the accumulated charge agree. |
| `The base stream is named Base, and the thread driving the engine is not called that` | Section 4's naming. |
| `A thread that was never given a stream does not claim to have one` | `NonStreamIndex` is not zero and is out of affinity range. |

Adjacent collections that cover the same subsystem: `TaskRegistryTest` (identity, generation, growth, ceiling),
`ResultPacketTest` (packet layout and the clearing rule), `CPUBudgetTest`, `StreamDrainPolicyTest`,
`TaskStreamAffinityTest`.

Build and run them:

```bash
./build.sh Applications/EngineTest -test -debug      # or -dev, -release
./build/Applications/EngineTest/Debug/EngineTest     # prints "EngineTest: all <N> collections passed"
```

`-test` is what compiles the unit-test sources at all; there is no `-notest`. See
[RunningTests.md](RunningTests.md).
