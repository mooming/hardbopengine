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
    TaskSystem::EnqueueTask(workerIndex, *task);                        // one stream's own queue
}
TaskSystem::EnqueueTask(TaskSystem::GetIOTaskStreamIndex(), *task); // the IO stream's own queue
```

| Call | Meaning |
|---|---|
| `TaskProvider::MakeWholeItem(task, priority)` | Protected static on the provider base: builds one `WorkItem` covering a whole task, inherited by a customer-authored provider. `Task::GenerateSubTask` is private and engine-internal.
| `TaskSystem::Enqueue(const WorkItem&)` | Engine-internal. General queue: shared, lowest priority, any eligible stream may take it.
| `TaskSystem::Enqueue(TIndex streamIndex, const WorkItem&)` | Engine-internal. One stream's own queue. |
| `TaskSystem::EnqueueTask(TIndex streamIndex, Task& task, uint8_t priority = 0)` | **The customer API.** Put a task's work on one stream without building a queue item. |
| `taskSys.Update()` | One pump pass of the base stream, driven by the main thread. |
| `TestHelper::DriveUntil(taskSys, waitingFor, predicate, patience)` | The designated nested-pump wait; reports a failed wait instead of blocking forever. Test-build API in `namespace hbe::TestHelper` — until 2026-10-02 it was `TaskSystem::DriveUntil`, and it moved because all fourteen of its callers were test bodies. See `docs/Test/TestHelper/drive-until.html`. |

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

### Splitting one task across several items

A `Task` is a promise about a range and a result; a `WorkItem` is one piece of that range sitting in a queue. They are not the
same object and they do not have the same lifetime. The task's **reserved count** is how many items you intend to hand out, and
the task is finished when that many items have reported - so the count and the items must agree exactly, which is the single most
common way to make a task that never finishes.

Three facts that are not visible from the declarations:

| Fact | Consequence |
|---|---|
| A `WorkItem` is 56 bytes and names its task by id, not by pointer. | A queue holding a hundred items costs a few kilobytes, and an item stays valid even if the caller loses track of the `Task` object - but the registry record must still be alive, so do not `ReleaseTask` while items may still be queued. |
| The item carries `[start, end)` and a priority. | Every worker of a split task writes a disjoint slice of the same result packet, so ranges must not overlap and must together cover what `Start` declared. |
| A runnable may return short of its range. | The stream re-queues the remainder, which is how one long item yields to a frame budget. That is resumable work; it is also why a shutdown that refuses to re-queue has to say so rather than loop forever. |

As a customer you do not build items. `TaskSystem::EnqueueTask(streamIndex, task, priority)` hands one stream the whole task, and
that is the right call almost always - splitting is the engine's decision when a provider is involved, and it is made by the
provider through the protected `TaskProvider::MakeWholeItem`. If you find yourself wanting two items for one task, the question to
answer first is which stream must run which slice; if the answer is "not specifically", the general queue and the drain policy will
spread it better than a hand-written split will.

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
| `TaskSystem::MainThreadName` (`"Main"`) | The OS thread that drives `Engine::Run` and shuts the engine down. |

A thread the application created, and the main thread itself, report
`TaskSystem::GetCurrentStreamIndex() == TaskSystem::NonStreamIndex`. That is deliberate and it is not zero:
before it had its own value, **every** thread in the process answered "stream 0", which made `IsBaseThread`
useless as a check and charged a foreign thread's contact with a task against stream 0's affinity. The
consequence you must know: **a thread that is not a stream takes nothing from the general queue.** It is not
an error, it is a silent no-op - section 7 says why.

## 5. Waiting, and why you should stop

`Task::Wait` and `Task::BusyWait` have been **removed**. Each spun or slept on a counter that cannot distinguish a finished task from one that was never dispatched, so its timeout was not a deadline but an unnameable hang. Ask the queue instead (`TaskStream::CountPendingItems`); unit tests use `hbe::WaitUntil`, which carries a deadline and reports a stall as a failure.
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
- A fresh item is taken immediately by a worker; a `WorkItem` is built with the **base and IO bits cleared**,
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

## 9. Work that must run on the main thread

```cpp
taskSystem.DispatchToMainThread([](void* userData) { /* runs on the Main thread */ }, data, 128);
// No manual drain exists any more: the base stream takes posted callables at the top of every
// pass, and the main thread gives it that pass.
```

Priority is `0` = least urgent, `255` = most urgent, and the parameter **defaults to 128**. It used to default
to 0 under an inverted "0 = highest" convention; with the direction fixed, passing an explicit `0` now means
"as late as possible", so do not copy old snippets that do.

`Engine::Run()` is not a frame loop. It is:

Reduced to the two facts that matter today: the loop condition is `taskSystem.IsRunning()` alone, and inside it
`taskSystem.Update()` gives the base stream one pass' worth of work. There is no separate main-thread queue to
drain, so nothing in the loop has to remember to do that - which is the whole point of absorbing it.

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
| `task.Wait()` / `task.BusyWait()` | **Removed.** Ask the queue (`CountPendingItems`); tests use `hbe::WaitUntil`. |
| Relying on `HasDone()` as the completion mechanism | R8 removes it. Continuation jobs replace it. |
| `(void) taskSystem.Enqueue(streamIndex, item)` | The cast is now a lie. `Enqueue` cannot refuse since `ac496e6` - a result is no longer something a stream can run short of, because it is allocated when the task is created. Two such casts remain in the tree (`Engine/Log/Logger.cpp:246`, `Engine/Test/UnitTestCollection.cpp:166`) and are stale annotations, not evidence of a failure path. |
| Writing a test that drains the general queue | Steals another collection's work item and produces an allocator fatal in a subsystem your test never touched. Section 7. |

## 13. What is tested

`TaskSystemTest` lives in `Engine/Core/TaskSystem.cpp` and holds 36 tests, sorted by area.

| Area | Test | What stops being true if it fails |
|---|---|---|
| Identity | `The base stream is named Base, and the thread driving the engine is not called that` | Stream 0 is `Base`, stream 1 is `IO`, the main thread's thread is `Main`, and the thread running a test reports itself as the base stream — so the name in a log line identifies one executor and not two. |
| Identity | `A thread that was never given a stream does not claim to have one` | `NonStreamIndex` is not 0, so a thread nobody made a stream is not treated as the base stream, and it lies outside the affinity mask, so the general queue does not charge its sightings to whatever stream shares the value. |
| Task basics | `Empty Task` | A record with no work item is not marked done before it runs. |
| Task basics | `Task of size 0` | A zero-length range still reports itself finished rather than hanging whatever waits on the join. |
| Splitting | `Bagel Problem` | A million terms split over two worker streams sum to the right answer, and the collator posted as the successor actually runs — an asynchronous join that never fires is silent. |
| Splitting | `Bagel Problem (Incremental Task)` | The same sum built by re-enqueuing ranges of one task closes its join. |
| Splitting | `An item that returns part of its range resumes at the index it stopped at` | A runnable that returns short of its range is called again from where it stopped, once per remaining index, and is not restarted — otherwise every index before that point is done twice. |
| Splitting | `A split queues no empty work item and counts only what it queued` | Three items asked for in ten sub-jobs produce three items, the join is declared as three, no item covers an empty range, and the collator runs. |
| Splitting | `A split larger than the join counter can count is cut down, not truncated` | A count a `uint8_t` cannot hold is clamped to `Task::MaxNumSubTasks` rather than silently wrapped to its low eight bits, which would close the join while most of the work is still queued. |
| Splitting | `A split naming a stream the engine does not have is refused and creates nothing` | Stream indices are checked before use, so an out-of-range index cannot index past the stream array, and a refused split neither creates a record nor runs. |
| Splitting | `Asking for streams picks workers and never the base stream or the IO stream` | The count form of `ParallelFor` places work only on worker streams; blocking the base stream stalls the engine, and the IO stream carries log work. |
| Splitting | `A split with nothing to do is refused rather than queued as a task that can never close` | Zero and negative item, sub-job and stream counts are refusals — a negative becomes a fourteen-digit range the moment it is used as an index — and a successor with no stream to run on is refused for the same reason every other unroutable result is. |
| Delivery | `A finished task hands its outcome to the successor on the stream it named` | The whole of section 6.1: the producer's packet is copied to the successor and queued on the stream the producer named, once. |
| Delivery | `Outcomes chain: a successor that produces in turn dispatches its own successor` | A successor dispatched by a task the engine itself dispatched — chains are supported, they do not merely look supported until a second link is needed. |
| Delivery | `A task that recorded no successor wakes nobody, and does not run itself again` | The fire-and-forget case stays fire-and-forget: finishing wakes only what was recorded, and a task that wakes itself with no successor recorded is a loop no caller can tell from slow work. |
| Delivery | `A successor with no stream named in the packet is refused out loud, not guessed` | Half-filled routing produces an error naming the pair, never a stream chosen on the caller's behalf. |
| Delivery | `A successor addressed to a stream this engine does not have is refused` | The destination byte is checked against the stream count before it is used as an index. |
| Delivery | `A successor released while its producer runs is reported and never dispatched` | A released record is not followed, so its slot cannot be handed to a new tenant that then runs someone else's work under the abandoned identity. |
| Delivery | `A successor that reserved no subtask cannot be dispatched and says why` | An outcome is delivered as one item over the successor's whole range, which is one reserved subtask; without it the join counter never reaches a count it was never given. |
| Lanes | `Both lanes serve their tasks` | A stream drains both of its queues, and an unfinished task is returned to its lane rather than dropped. |
| Lanes | `Work offered on each lane through the public API is reached and run` | The lane a caller names is a lane something acquires from — the state the drain-rate machinery was written to describe is not the normal state of the priority lane. |
| Lanes | `A lopsided rate reaches both lanes through the public API and starves neither` | A lane with work is never starved by the other lane's weight. The ratio itself is not asserted here; see section 17. |
| Budget | `Stream charges a configured budget` | `RequestBudget` makes the stream measure the CPU its tasks use, `MayTakeNewWork` turns false once the allowance is spent, and restoring the unlimited allowance turns it true again. An unconfigured stream measures nothing and pays no syscall for a number nobody reads. |
| Budget | `A throttled stream is throttled, not broken, and never asks a provider while spent` | Work waits rather than being stranded, passes decline a lane that holds work, and no provider is asked while the allowance is spent — counted across every stream, because the assert that also catches this is compiled out in Release. |
| Budget | `A stream with a spent allowance declines the general queue and resumes after a window` | A spent allowance changes a decision, and the budget-window pass reopens the stream instead of leaving it latched spent for the life of the process. The pass counter is read before the test touches anything, which is the witness that the stream's own loop calls the pass. |
| Budget | `A stream with no allowance never declines general work` | The gate reads an unspent allowance and nothing else, and an unlimited stream is not measured and charged anyway. |
| Budget | `A stream whose allowance is spent sleeps instead of spinning` | A spent stream parks on its condition variable; the pass count over a fixed window separates parking from polling the budget, and the lower bound on that count separates a throttled stream from a stalled one. |
| Isolation | `Work queued on one stream is never run by another` | Lane work is confined to the stream that holds it, and each stream's work runs on one thread — its own, never the one that dispatched it. |
| Abandonment | `Work still held when a stream is cleared is abandoned with its notice fired, not silently` | Clearing a stream takes every item it holds, fires the notice for each, runs none of the work, and counts what it dropped. |
| Abandonment | `Work held on the priority lane is abandoned with its notice fired, not silently` | The same, for the other queue: the priority drain loop cannot pop items, report a number, and tell nobody. |
| Abandonment | `Work dropped because its task was released notifies the requestor through the stream` | The notice arrives through the stream that dropped the work, once, carrying the task's own identity and the requestor's `userData`. |
| Abandonment | `A dropped work item notifies the requestor that asked and a task nobody asked about notifies nobody` | The notice travels from the task onto its work item, the default is silence, and silence is asserted as firmly as the firing — if the default ever started notifying, every task in the engine would. |
| Abandonment | `Work dropped at each of the three sites leaves its task record exactly where a release leaves it` | A dropped task still ends up where `ReleaseTask` would have left it: no double free, no resurrection, no leak, and no work that both ran and was reported abandoned. |
| Age | `Every slice carries the age the registry stamped, and a recycled record is dated afresh` | The age lives on the task, so a slice cannot be born with a fresh timestamp to dodge a ceiling, and a record reused by a new task is stamped strictly newer rather than dropped as stale the moment it is offered. |
| Age | `Work older than the stream's max age is dropped, reported and notified, while the same work runs when no ceiling is set` | Over-age work is refused on both lanes, counted, and notified in the same words as work dropped for a released task — with a ceiling-free control pass in the same test proving the fixture can run at all. |
| Harness | `WaitUntil reports both outcomes and never decides by itself` | The wait helper every polling test in the suite relies on reports true only for a condition that became true and keeps the timeout its caller asked for. |

### How these tests are written, and why

The suite runs as engine work on the base stream, so most of its hazards are about observing the engine
without becoming a second engine loop. Each rule below replaced something that passed for the wrong
reason, and the measurement that killed it is named.

| Rule | Why it is that way |
|---|---|
| **Test a worker stream, never the base stream.** | The suite's own testlet is a work item on the base stream, so that stream is occupied until the suite ends and can never run what a test queues to it. The first worker stream is the nearest one genuinely idle. |
| **A witness must be a quantity the mechanism cannot erase.** | Advancing a budget window zeroes the stream's accumulated CPU, so a test that reads the charge to prove the budget watches the mechanism delete its own evidence — measured as a charge of 0 us on a task that had just spent 222 ms. Predicates count tasks that ran and refusals recorded, which a reopened window cannot undo. |
| **A negative assertion needs its positive control in the same test.** | "the runnable never ran" is indistinguishable from "the test never queued anything", so each guardrail first runs the identical fixture with nothing configured and has to see the work run. For the same reason a baseline is only meaningful once the counter is proven to have moved while the subjects were alive: a comparison of a baseline with itself is also satisfied by a counter that never changes. |
| **Bound a wait by wall clock, not by iterations or passes.** | An iteration-count burn is milliseconds in Release and tens of them in Debug, so a fixed number of passes sees a queue in one configuration and an empty stream in another. The same reasoning sets the bounds on a spent stream's pass count: `WaitForWork` parks on a condition variable with a 10 ms timeout, so a parked stream wakes about fifty times a second and one polling the budget instead would report thousands of passes in the same window. |
| **An in-band sentinel beats a sleep.** | Queued behind the subject on the same lane, a counting sentinel can only start once the subject's work item has left the stream — and `DispatchSuccessor` runs inside that item — so the sentinel's run is proof the delivery attempt is over. Measured before this existed: a test that slept 200 ms and then looked found what the machine happened to have done in 200 ms, which passed on a loaded machine for the wrong reason and failed on an idle one for the right one. |
| **Write the two lanes out separately.** | The lanes are two queues drained by two loops, and one body parameterised over both is how the priority lane went a whole session unwitnessed. The drop-site guardrail test therefore uses the lane its neighbours do not: an earlier version used FIFO everywhere, and a mutation placed in the priority drain loop walked straight past it — found by running a mutant, not by reading the test. |
| **Read a shared setting before changing it, and restore it on every exit path.** | Max age and lane weights belong to the base stream, which every later collection shares. Restoring them only at the end of a body would leave the whole suite throttled by the first early return. |
| **A test owns its stream, and does not run in front of a timing-sensitive neighbour.** | Throttling a stream costs about two seconds of wall clock, and the test that measures charges over a bounded window reported the throttle as a fault until the throttling test was moved after it. Order is part of what a test asserts. |
| **An isolation result needs a thread it can distinguish.** | Recording which thread ran each item proves nothing unless the recorder is seen to tell threads apart, so the isolation test also records the dispatching thread as its control. Work that ran on the thread which dispatched it makes the isolation figure silence rather than evidence. |
| **A notice is tested in both directions.** | An item that should report itself and silently does not looks exactly like a stream with nothing to report, which is the same class of blind spot that once let a deleted loop header pass 59 collections. So the firing is asserted, and so is the silence of a task nobody asked about. |
| **Assert the promise, not the arithmetic.** | The lane rate is a per-take decision and is unit-tested where it is decided, in `StreamDrainPolicyTest`; with no CPU allowance configured borrowing is free and the long-run ratio is not preserved, which section 17 records as correct behaviour. An end-to-end test of the ratio would have been a test that fails on correct code — worse than no test, because it teaches everyone to ignore tests. |

Adjacent collections that cover the same subsystem: `TaskRegistryTest` (identity, generation, growth,
ceiling), `ResultPacketTest` (packet layout and the clearing rule), `CPUBudgetTest`,
`StreamDrainPolicyTest`, `TaskStreamAffinityTest`.

Build and run them:

```bash
./build.sh Applications/EngineTest -test -debug      # or -dev, -release
./build/Applications/EngineTest/Debug/EngineTest     # prints "EngineTest: all <N> collections passed"
```

`-test` is what compiles the unit-test sources at all; there is no `-notest`. See
[RunningTests.md](RunningTests.md).

## 14. Being told when your work is dropped: `SetAbandonedNotice`

Every task asks nothing by default, and that default is the point: `FAbandonedNotice` is `nullptr` unless a requestor asks, so a
task nobody asked about notifies nobody and costs nothing.

```cpp
void OnAbandoned(hbe::TaskID dropped, void* context) noexcept
{
    auto* tracker = static_cast<MyJobTracker*>(context);
    tracker->MarkUnfinished(dropped);
}

const hbe::TaskID id = taskSys.CreateTask("Load", &LoadRunnable, ctx);
taskSys.SetAbandonedNotice(id, &OnAbandoned, &myTracker);   // before the task is offered
taskSys.EnqueueTask(streamIndex, *taskSys.FindTask(id));
```

**Why the notice rides on the queue item, not on the task.** At the moments it must fire, the task is exactly the thing that is not
there: the most common drop is work whose task was already released, so `FindTask` has returned null, and at the shutdown close
sites the stream reports how many items it is discarding without resolving any of them. A handler stored on the task, or beside
`successor` in the registry record, would be unreachable precisely when it was needed. So `WorkItem` carries the pair, which also
means the existing copy semantics do the rest for free: an item re-added to a lane after a partial run, and every slice of a split
job, keep the notice their task was offered with. The price is 8 + 8 bytes per queued item, and queue-item width is paid per lane
change rather than per task, which is why the size is guarded by `decidedWorkItemBytes`.

**Three rules the callback must respect.** It runs on the thread that dropped the work, which may be a worker mid-shift, so it must
not block. It must not take a task-stream lock. It must not look the task record up - that record is being dropped around the call,
and touching it is the single hardest thing to diagnose in this subsystem. It receives the dropped `TaskID` and the `void*` you
supplied, which the engine passes through untouched and never clears: the lifetime of that pointer is yours to guarantee, and it
must outlive every drop this work can suffer.

**Call it before you offer the task.** Once work is queued, the engine may drop an item before the notice lands - that loses the
notice rather than racing it, and there is deliberately no lock closing that window: a mutex on the offer path to protect an opt-in
courtesy is the trade this engine has refused twice already.

**Every drop site now notifies.** Work dropped because its task was released, and work still held when a stream closes, both fire the
notice: a closing stream calls `AbandonHeldWork`, which pops each lane, fires each item's notice and counts what it dropped.
`GetAbandonedWorkNoticeCount()` is the readable witness for that, in the style of `GetLaneWorkRefusalCount`, because a shutdown report
that can only be verified by scraping its own output is not verified.

## 15. Choosing a lane: `StreamDrainPolicy::ELane`

A stream holds two queues, and **which one your work lands in is now your choice** rather than a consequence of which internal
function a caller happened to reach:

```cpp
using ELane = hbe::StreamDrainPolicy::ELane;   // Fifo | Priority | None

taskSys.EnqueueTask(streamIndex, *task, 0, ELane::Fifo);       // arrival order
taskSys.EnqueueTask(streamIndex, *task, 7, ELane::Priority);   // highest priority first, oldest within a tie
```

**Lane and priority are different things and must not be confused.** The *lane* is the queue that serves the work.
`WorkItem::priority` is the ordering **inside the priority queue** - it selects nothing, and on the FIFO lane it changes nothing at
all. The engine previously exposed exactly one route, `EnqueueFifo`, so the priority lane could not be filled from the public API:
its share of the FIFO:priority drain rate, its re-add path and its half of the shutdown drain were unreachable, and a mutant that
drained that lane without notifying anyone survived a full suite twice because there was no input a test could construct. The lane
parameter exists to remove that class of blind spot, not to add a knob.

`ELane::None` is rejected with an assert rather than absorbed as FIFO: work with nowhere to go and work nobody attached to a lane are
different problems, and merging them turns a provider bug into a mysteriously missing task.

## 16. Refusing work you can no longer run in time: `TaskStream::SetMaxAge`

A stream that falls behind does not lose work; it defers it. Deferral is harmless for a frame or two and harmful
for longer, because the world the work was queued against keeps moving: the entity it was told to update was
destroyed, the settings it read were changed, or the frame it was annotating is four frames gone. Running that
work late is not neutral - it looks completely legitimate to whoever queued it, and it is stale to whoever owns
the state it touches.

```cpp
	// A frame-bound stream declines anything it could not start within two frames.
	taskSystem.GetStream(TaskSystem::GetBaseTaskStreamIndex())
		.SetMaxAge(std::chrono::duration_cast<std::chrono::nanoseconds>(2 * frameTime));
```

Three properties worth knowing before you reach for it:

* **It is per stream and off by default.** Staleness is a property of what a stream is for. The same task is
  still worth running on a background stream and worthless on a frame-bound one, so the ceiling belongs to the
  stream and a stream that never opts in behaves exactly as it always has.
* **The age is the task's, not the item's.** `Task::offerTime` is stamped when the registry loads the record and
  is carried by every work item the task hands out, including sub-slices. Stamping the item instead would let a
  task postpone its deadline by splitting late or by not finishing in one call - both of which this engine
  advertises as normal behaviour.
* **Over-age work is dropped, counted, reported and notified, never run and never requeued.** It uses the same
  vocabulary as work dropped because its task was released, and it fires `SetAbandonedNotice` for requestors that
  asked, so nobody waits for a result that was declined. `GetAgedOutWorkCount()` is the engine-side signal; the
  log line names the task, its range and its age.

The check sits after the task lookup on the shared take path, so one rule covers both lanes. `GetMaxAge` exists
so a caller that tightens a shared stream temporarily can put back exactly what it found; the test that does so
restores it on every exit path, because the base stream is shared by every later collection.

## 17. What the lane rate promises, and what it does not

`ConfigureRate(fifoWeight, priorityWeight)` divides a stream's CPU allowance between its two lanes, and it is a
per-take decision, not a quota enforced over time. With no allowance configured, borrowing is free, and the
documented consequence is that **the long-run ratio is not preserved when both lanes are permanently backlogged**:
whoever has work gets the CPU. Measured on the base stream at 8:1, eight offered items came back as four FIFO and
four priority in a single pass. That is correct behaviour, not a bug - which is why no test asserts a ratio end to
end. The ratio is tested where it is decided, in `StreamDrainPolicyTest`.

The promise both drain modes do make, and the one the end-to-end test asserts, is narrower and more useful: **a
lane with work is never starved by the other lane's weight.** A weight of 1 means "later", never "never"; a zero
weight falls back to one rather than switching a lane off, because a lane with a queue and no service would strand
those tasks forever.

---

## 18. How the suite is driven, and what that proves

The suite is engine work, not a thing that happens around the engine. `Test::RegisterSuite()` registers all 59
collections and flattens them into a sequence of 372 **testlets** - one test lambda each - and then
`Test::ScheduleSuiteOnBaseStream()` posts the first one to the base stream. Each testlet runs as its own task, and
each one posts the next; the last posts a task that prints the report and requests shutdown, so `Engine::Run` is the
only thing that can carry the suite to its end and it returns with the tallies already settled.

That shape is load-bearing in a way a witness never was. Three facts make it so:

| Fact | Consequence |
|---|---|
| Registration happens before `Run()` is entered | The expected total belongs to the harness, so a suite that never started cannot report a total matching the nothing it executed |
| A testlet posts its successor only after running | The queue is empty the moment an item is taken, so one pass cannot reach far: measured with `Engine::Run`'s loop reduced to a single pass, 13 of 372 testlets ran |
| `Engine::Run` returning without finishing is a gap between two numbers the harness owns | `TestMain` compares registered with executed and exits 1, naming the shortfall. No hand-written observer of the loop is involved |

Two support pieces, both of which exist because something was measured rather than assumed:

* **`TaskStream::IsDrivenByShutdownPump`** - `TaskSystem::JoinAndClear` pumps the base stream under a wall-clock bound
  to close the system down. That rescue is correct for real shutdowns, and it is what once let a dead engine loop pass
  the whole suite: the drain ran the tests and reported them. A testlet that runs while that pump is driving records a
  failure, because results produced by the rescue path are not evidence about the loop.
* **A bounded per-pass allowance on the base stream** (`ConfigureBudget`, set by the harness). Today the chain alone
  forces many passes, so the allowance is insurance rather than the load: `CPUBudget` treats zero as unlimited and reads
  the allowance once per pass, so a future driver that re-scanned the queue after every completed item could drain a
  whole chain in one pass and make the guard vacuous again - silently. A small allowance makes that impossible, and too
  small an allowance costs only extra passes.

The engine-loop contract this enforces is the one `Engine::Run` has always documented: pump while the task system is
running, and shut down only through the shutdown path. Breaking the loop is now a test failure with the loop in its
name, which is the property the suite previously lacked while claiming 59 collections of coverage.
