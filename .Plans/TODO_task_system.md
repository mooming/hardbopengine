# TODO — task system, current state and what a fresh session should do next

### D3 groundwork, measured 2026-09-25: ThreadSanitizer runs here, and the suite does not finish under it

No CMake change was needed for a first look. Out-of-tree, zero risk to the tracked build:

    cmake -B /tmp/hbe/tsan -S . -G "Ninja Multi-Config" \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1 -D__TEST__ -D__UNIT_TEST__" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=thread"
    cmake --build /tmp/hbe/tsan --config Debug --target EngineTest -j 8
    /usr/bin/perl -e 'alarm 420; exec @ARGV' /tmp/hbe/tsan/Applications/EngineTest/Debug/EngineTest

Result: the build succeeds, the binary lands inside the out-of-tree dir (the repo build/ is untouched), the suite starts and
runs, and ThreadSanitizer reports **66 races** before the 420s wall clock kills it - it does not complete. Captured in
`build/gate/tsan-run-first.log` (gitignored dir; copy it out before cleaning).

What the counts say, without pretending they are diagnosed:

| Frame | Mentions | Reading |
|---|---|---|
| `TaskStream::Update()` TaskStream.cpp:515 | 165 | the item-run/re-add neighbourhood, adjacent to the code this arc touched - investigate first |
| `WorkItem::Run(Task&)` WorkItem.cpp:24 | 147 | the same path one level in |
| `TestEnv::Start` / `ExecuteTest` / `TestCollection::Start` / `Test::RunTests()::$_0` | 120 each | one shared test-harness story, not four bugs |
| `TaskSystem.cpp:781 hbe::(anonymous namespace)::TrackedTask::Track` | first SUMMARY | a **test helper**, so probably a test bug rather than an engine one |

So D3 is a real project, not a checkbox: (1) the suite needs splitting or a per-collection TSan budget because it cannot finish;
(2) **corrected after reading a race block rather than its summary**: the harness frames are the call chain, not the defect. The
contested object is `MemoryManager`'s allocator registry - `AtomicStackView<AllocatorProxy>::Push` inside
`MemoryManager::DeregisterAllocator`, reached from `PoolAllocator::~PoolAllocator` <- `MultiPoolAllocator::~MultiPoolAllocator`
<- `TestCollection::ExecuteTests()` (TestCollection.cpp:118) <- `TestEnv::Start` <- `WorkItem::Run`. One whole test collection's
allocators are deregistered from a task running on a stream thread, concurrently with someone else touching the same registry -
which is why TestEnv/TestCollection frames show up 268 and 134 times. So "triage the harness first" was wrong guidance: the share
belongs to **Memory**, and the first question there is whether `AtomicStackView`'s reader walks plain fields (a real defect) or is
correctly atomic and being mis-modelled (a false positive wanting an annotation, not an engine change); (4) a
Sanitizer *configuration* in CMake is what makes this a gate rather than an event - and `__TEST__` must stay on, or the binary
prints advice and exits, which is the oldest trap in this repository.


### Verified facts for the next session, gathered 2026-09-25 (no change made, deliberately)

`TaskSystem::ReleaseTask(TaskID id)` is one line: `taskRegistry.Release(id)`. **It checks nothing** - not whether the task's
sub-tasks were reported, not whether items naming that id may still sit in a lane. The released-task case is handled later, at
drain time, by `ReportReleasedTask(WorkItem)` - which lives in `TaskStream.cpp`, not in `TaskSystem.cpp` - where a popped item
whose task cannot be found is reported and dropped.

So early release today means: **the promised work is quietly dropped, with a log line, some time after the release.** That is a
defined protocol rather than a use-after-free, and it is not a bug to "fix" by asserting in passing without deciding the
semantics. The decision to make first, and it is the owner's:

1. **Refuse** - `ReleaseTask` on a task whose reserved sub-tasks are unreported does nothing and reports. Cost: `Task::HasDone()`
   is private with four friends, and `TaskSystem` is already one of them, so the check is available at no API cost. Risk: existing
   legitimate callers, e.g. the `Empty Task` test releases a task that never ran anything, and `Logger::StopTask` gives up on a
   task after 1000ms and deliberately leaves it alive.
2. **Assert in Debug/Dev, report in all builds** - matches how the budget gate was done (assert plus a counted witness), and the
   counted witness here would be the existing released-task report.
3. **Document it as intended** - early release cancels work - and write the test that pins the cancellation, which is the cheapest
   option and may be the honest one, since the drain already reports the case.

Option 3 is likely correct and requires no engine edit; option 1 is what the hazard instinct wants and is the one that will break
callers. Do not start this without picking one.

> **Status 22:55 — #17 is CLOSED.** Three counters (`laneWorkRefusals`, `providerAsksWhileSpent`, plus the existing
> `generalQueueRefusals`) and one test that reads them. Acceptance proof met: with `!budget.CanTakeWork()` removed from the drain
> gate, the **Release** build fails and names it — "A provider was asked 24 time(s) while the allowance was spent." Debug/Dev catch
> the same mutant via the assert in `a166b67`. Everything from the earlier diagnosis (configuration anomaly, pop-and-return,
> misplaced counters) is superseded by this; the anomaly was an iteration-count burn, fixed by bounding the work in wall clock.
> **Status as of 07:10.** **#6 B3d is now specified to the line** in `.Plans/PLAN_b3d_max_age.md` - per-stream max age, per-task deadline excluded, every anchor quoted from the tree, mutant list and gate commands included. Still open: **implementing** that plan; **#7 D3** (true-positive Treiber ABA in `AtomicStackView`, the Memory module's to fix); the **`Engine::Run` guardrail**, which needs the owner's call on whether the suite should run while `Run()` pumps; and three owner decisions - the two allowance books, the shape of `MayTakeNewWork`, what releasing a task early means. Closed: #9 docs, #7 D2/D4/D5, #10-#17.
> allowance books, and `MayTakeNewWork` having only test callers.

**Read this first.** Everything below is verified against a real run unless a line says otherwise.
HEAD is `4655298`; nothing has been pushed. The suite is **59 collections**, green in **Debug, Dev and
Release** at HEAD, with `check.sh` at **0 mechanical violations / build gate PASS 12/12**.

## Gate — the only commands that count

```bash
./build.sh Applications/EngineTest -test -debug -dev -release          # must be -test, see trap 1
.pi/skills/hb-standards/scripts/hang.sh Debug 240                      # prints: FINISHED exit=0 : ... all 59 collections passed
.pi/skills/hb-standards/scripts/hang.sh Dev 240
.pi/skills/hb-standards/scripts/hang.sh Release 240
bash .pi/skills/hb-standards/scripts/check.sh                          # mechanical violations + 12-target build gate
```

| Script | What it protects |
|---|---|
| `hang.sh <Config> <patienceSeconds>` | builds, refuses to run a binary whose build failed, and **if the run stalls it samples that pid** and prints `hbe::` frames. It found two bugs this session that reasoning did not. |
| `runtest.sh <Config> [limit]` | wall clock, reports `128 + signal`, **refuses a binary not configured with `-test` (exit 99)**, and warns when a non-zero exit printed no `FAILED` line. |
| `check.sh` | clang-format/Allman/include rules, the copyright line, and a 12-target build gate including `WindowExample`. |

Tracked copies now live in `.pi/skills/hb-standards/scripts/`. The ones under `build/gate/` are wiped by
`-clean` — do not rely on them.

## Traps that produced false verdicts this session (all real, all mine)

1. **Only `-test` compiles the test bodies.** They live in the library sources, not only in the test
   executable. A binary built without it prints advice saying exactly that and exits 1; any verdict from
   it — including a fast exit — is vacuous. Several `build exit=0` results reported mid-session proved nothing.
2. **A failed link leaves the previous binary in place.** `hang.sh` refuses this; a manual `cmake --build`
   does not.
3. **Mutation backups must be taken from HEAD.** A stale backup restored over 27 committed lines, twice.
   Assert every substitution actually changed the file.
4. **macOS has no `timeout`.** Exit 127 from it means missing tool, not test failure.
5. **A log flush that blocks the base thread is a hazard.** A thread blocked on the task system must keep
   driving what it owns — `TaskSystem::DriveUntil` is the only designated nested-pump wait point.

## Landed this session (all gated, all committed)

| Commit | What | The measurement that mattered |
|---|---|---|
| `a02e21c` | Per-stream close requests, wall-clock drain, base closes last | a pass-count bound was outrun by resumable work and hung |
| `78c3946` | IO stream stays open through shutdown, not drained to empty | IO drained 2,635,177 passes / the full 2000 ms — a log line re-posts its own drain task |
| `fd8633f` | Logger driver thread; IO stream becomes ride-on-thread | flush give-up 1 → 0 |
| `16ffd7e` | Driver thread named `LogDriver`, event-driven idle | `[LogDriver]` observed in all three configs |
| `fcc07c8` | Every stream asserted closed before `streams.Clear()` | 12 close reports per run |
| `ba5f3a7` | **Base stream rides the engine loop**; `TaskSystem::Update` is the single pump; `DriveUntil` at the six `OSAL/Window.cpp` waits; message-less `Assert`/`FatalAssert` print file and line | sampling found the wait still in `std::future::get` (drive was not pumping the main-thread queue); `source_location` named `Array.h:112` — indexing `streams` after clear, i.e. defect T4 |
| `0ec7538` | Logger waits on the queue observable, not the task handle | — |
| `a25168b` | **C1**: `Task::Wait`/`BusyWait` deleted, `HasDone` private, `hbe::WaitUntil` for tests | every remaining caller was a *test body*, not production |
| `4655298` | **C2**: `GenerateSubTask` engine-internal; `TaskProvider::MakeWholeItem` (protected); new `TaskSystem::EnqueueTask(stream, task, priority)` | found `Examples/WindowExample/Main.cpp`, three internal providers, the logger and `hbe::Test` hand-building items |
| `2345f03` | **N1**: `MainThreadTaskQueue` becomes `TaskStream::postedTasks`, drained by the stream's own pump; one intake | mutation-proven, and the mutation found two more gaps |
| `50c91eb` | `DriveUntil` reports a failed wait; the six window waits fail in band instead of blocking | the mutant now kills `WindowTest` TC0/TC1/TC2 by name, where it had been a silent timeout |
| `e86dd55` | Journal + this file rewritten to reality; `hang.sh`/`runtest.sh` tracked in the skill | the tracked `runtest.sh` run from its new location is the proof |

## Remaining, in the order I recommend

### N1 — ~~Let the base stream own the work posted to the engine loop~~ **DONE: `2345f03` + `50c91eb`**

`MainThreadTaskQueue` is now `TaskStream::postedTasks`, drained at the top of every pass, so one pump reaches
it. `ProcessMainThreadTasks` and `GetMainThreadTaskQueue` are gone; `DispatchToMainThread` forwards to the base
stream and `HasPendingPostedWork` carries the engine loop's run condition. The acceptance mutant (delete the
posted drain) initially produced **no failing test at all** — exit 60 on a wall clock, nothing printed — which
uncovered two gaps fixed in `50c91eb`: `DriveUntil` now reports what it waited for via `ReportDriveTimeout`, and
the six `OSAL/Window.cpp` waits report in band instead of calling `get()` on a future nothing will satisfy.
With the mutant restored, `WindowTest` TC0/TC1/TC2 each name the missing intake. That is the standard every
future mutation is held to: **a named test, naming the gate.**

### N1b — ~~Bound the shutdown pump over posted work~~ **DONE: `a8946ea`** — it was two unbounded waits, not one

The pump in `JoinAndClear` got the 2000ms wall clock plus an abandoned-work report. The more serious half was the
**engine loop's own run condition**, found by sampling: it kept looping while posted work remained, *including after
shutdown had been requested*, when no executor can ever run it. Shutdown must mean stop, so `Engine::Run` now loops
while the task system is running and nothing else, and `HasPendingPostedWork` was deleted rather than left as dead
code. Accepted mutant evidence: six named window tests (TC0–TC5) + "Shutdown abandoned work on the base stream after
2000ms…" + the process ending 13ms after the deadline instead of never, then trapping in `MemoryManager::Allocate` on
a stale allocator ID — abandoned promised work corrupts what the promise was for, which is why the report is an error.

### N1c — Inventory used by N1, kept here so it does not have to be rediscovered
Behaviour is already right: `TaskSystem::Update()` pumps the main-thread queue and base-stream passes under
the frame budget. What is left is one object, not two. Inventory at `4655298`:

| Site | Symbol |
|---|---|
| `TaskSystem.h:11` | `#include "MainThreadTaskQueue.h"` |
| `TaskSystem.h:27` | `using TMainThreadTask = void (*)(void*)` |
| `TaskSystem.h:69` | `MainThreadTaskQueue mainThreadTaskQueue;` |
| `TaskSystem.h:350/353/385` | `DispatchToMainThread`, `ProcessMainThreadTasks`, `GetMainThreadTaskQueue` |
| `TaskSystem.cpp:178/204/207/593/598` | the pump in `Update`, the wait in `JoinAndClear`, the two forwarders |
| `Engine.cpp:139/146` | `GetMainThreadTaskQueue().HasPendingTasks()` in the run condition, `ProcessMainThreadTasks()` after the loop |

The queue turned out to be self-contained (`BoundedPriorityQueue<TaskItem,256,1024>`, mutex-guarded, callables
invoked with the lock released), so it moved into `TaskStream` with `#include "Core/MainThreadTaskQueue.h"` —
note the `Core/` prefix: this tree's includes are project-qualified, and a bare `"MainThreadTaskQueue.h"` does
not resolve from `TaskStream.h`. Nothing calls `RequestStop`/`IsRunning` from outside the queue, so no stop
forwarding was needed.

### N2 — Budget gate: assert landed `dc9a400`, **Release coverage still owed (todo #17) — do this next**

`TaskStream` now asserts at the provider ask site that the allowance permits work. Accepted mutant
evidence, in a real run: gate without `!budget.CanTakeWork()` traps with *"Stream Worker1 was asked
for provider work while its allowance was spent"* inside the existing `TaskSystemTest` case **"Stream
charges a configured budget"** — a named test that was already in the suite, exit 133. Without the
mutant: 59 collections, exit 0, all three configurations.

**Correction that outranks everything in this section:** the assert `dc9a400` claimed was **not in that commit** — my
mutant revert deleted it and I committed afterwards, writing a message about code that was gone. It is genuinely committed
in `a166b67` now, verified with `git show HEAD:` rather than in the working tree, and re-proven against the gate mutant
(exit 133, names Worker1). **Rule: commit before mutating, never after.**

**Correction to the paragraph below: the "taken and put back" hypothesis is wrong.** The pop at `TaskStream.cpp:414/419` leads
straight to `Run` at 499; the only re-adds (530/534) are for resumable items that did not finish, and the probe's items finish.
Gate map: `410` `ChooseLane` (policy's own `fifoUsed`/`priorityUsed`, called unconditionally every pass), `434` provider gate
(`CPUBudget`), `465` general-queue pickup (`CPUBudget`), `499` run.

**Side finding, wants an owner decision:** `TaskStream::MayTakeNewWork()` has exactly four callers in the tree and **all four are
unit-test bodies** — production never consults it, so the name reads like the engine's own gate while the engine actually gates on
the policy book and `CPUBudget` alone. Either the header says so explicitly or the method changes shape.

**Closing contradiction, one measurement, no inference:** pass delta 27 with 3 items frozen implies `ChooseLane` returned no lane,
yet the counter on that condition read zero. Print pending + pass delta + decline counter from ONE build of ONE run — never again
compare numbers taken from different builds.

**Measured, and it voided the model.** Probe output: *"dispatched 4, pending 4 -> 3, pass delta over 500ms = 27, accumulated
CPU 220648us"*; after lifting the allowance: *"pending 1, pass delta 2"*. So (a) a throttled stream **does** keep cycling — 27
passes in 500ms, the 10ms `WaitForWork` cadence — and every conclusion drawn from "a spent stream takes no passes" is void;
(b) `ChooseLane` is called unconditionally every pass (read to confirm), and it returned a lane on essentially all 27 passes,
since the decline branch I instrumented counted zero; (c) work is nonetheless held. Those three together fit exactly one shape:
**the item is taken and then put back** — the FIFO rotation re-adds unfinished entries and there is a second re-add path for items
whose task finished elsewhere. **Next instrumentation: count items popped-and-returned while spent**, between `ChooseLane` and
execution. Not an open question any more, a specific search.

 `StreamDrainPolicy::ChooseLane` returns no lane
once its **own** `fifoUsed + priorityUsed` has spent the allowance — a per-lane time book separate from `CPUBudget`. Two
accounts of one intent: lanes gated by the policy, provider gate and general queue gated by `CPUBudget`. **That split is itself
a finding for the owner** — one quantity, two books, so a stream can be throttled by one and not the other, and it explains why
my first counter asked the wrong book. My corrected counter (work present + no lane chosen, a condition `ChooseLane`'s own
branches make unattainable by accident) still counted **0** with **3 items queued** for 150ms. **Next step, one number before
any code: delta of `GetDrivenPassCount()` across the throttle window** — it separates "the worker is not cycling while spent"
from "the decline is elsewhere". Widen the window to ~500ms; a 50M-iteration burn is tens of ms in Debug and a 150ms window can
straddle the first task finishing. Test and counters reverted; the assert in `a166b67` stands as the only enforcement.

**Instrumentation location, measured:** a no-budget control ran **6 of 6** with nothing left queued, so dispatch is sound
and the old 1-of-6 was the budget path. A throttled run then reported *"0 pass(es) declined a lane that held work, 2 item(s)
still queued, 0 provider ask(s) while spent"* — work waited while spent, and my new decline counter never fired, so the
decline is inside `StreamDrainPolicy`'s own allowance accounting, not at the call site I instrumented. Test and speculative
counters reverted; **read `StreamDrainPolicy::ChooseLane` before instrumenting again.**

**Third attempt, and the real mechanism, measured:** a worker whose allowance is spent **stops taking passes**. Test output:
*"Worker1: 1 of 6 task run(s), registry refused 0, 0 run(s) observed after the allowance was spent, 0 provider ask(s), 0 of
those while spent."* No passes means no lane draining, no asks, and — the crash — a deferred `DetachAll` that never completes,
because a detach completes on the stream's next drain; the stream then invoked a destroyed provider. **A spent stream observes
nothing and nothing observes it**, so every sentinel built on runs/asks/refusals-inside-a-pass is unreachable, including
`generalQueueRefusals` (it counts a pass that noticed it was spent). This item is therefore a **production decision**: let a
spent stream keep taking passes and decline inside the pass — makes the throttle observable, lets detaches complete, and
removes a liveness trap where a provider on a spent stream is never asked again even after its allowance returns. Full write-up
in todo #17. **Independent hazard:** detach completion depends on the stream draining, so a provider attached to a quiet stream
is a live pointer to a dying object, and R37's guarantee is only as strong as the stream still running.

**Root cause of both failed attempts, found at last:** a stream whose allowance is spent **stops taking work**, so
the tasks written to spend the allowance never run — the first runs, spends it, and the rest stay queued. That is the
whole explanation for the earlier "1 of 12 tasks ran", and it makes *"task runs observed while spent"* an unreachable
sentinel: spending the budget and running work while spent are mutually exclusive. It also explains the SIGSEGV: releasing
a task whose item is still queued lets the registry recycle the record under that item, which then executes against a
recycled task seconds later — the crash appeared in a provider drain on a worker, nowhere near the release.

**The sentinel must be about passes happening while spent, not work happening while spent.** Either use the existing
`generalQueueRefusalCount` (incremented exactly when a stream declines the general queue because it is spent, so non-zero
proves spent passes occurred with a provider attached), or add its lane counterpart — one relaxed atomic plus an accessor,
mirroring `generalQueueRefusals`. The counter is recommended: it is honest production state, because it answers what an
operator asks about a throttled stream — was it gated, and how often. Acceptance stays: the mutant dropping
`!budget.CanTakeWork()` must produce a **named failure in Release**, where `dc9a400`'s assert is compiled out.

**Why it is not finished:** asserts compile out in Release, so the configuration where a silent
regression would ship has no observation of the invariant. The positive test design is in #17, with
the two traps my first attempt fell into recorded there — releasing a task while its item is still
queued is a genuine use-after-free (the runnable's `userData` was a dead stack frame; that was the
SIGSEGV), and waiting on a run count that assumed every dispatch would run hid the fact that only 1
of 12 executed.

### N2-superseded — original count-based design (kept: it was wrong, and why matters)
Design revised — **and the earlier version of this item was flaky by construction.** `CPUBudget.h` says an allowance of
**zero means unlimited**, so no value expresses "permanently spent"; a tiny allowance plus accumulated charge does get
spent, but the accumulation resets when the accounting window advances, so a test asserting "the provider was never
asked" races the window and can pass for the wrong reason.

**Test the invariant instead**, which needs no timing luck: at the drain gate, on the owner thread, record the pair
(was the budget the reason for refusing, did the drain ask the provider) and assert that **no ask ever happened while
the budget was spent**. Give the stream a small allowance and a provider whose `Produce` burns a few ms of CPU so the
gate is genuinely crossed — the number of passes is irrelevant because the assertion is about the pair. Keep the in-band
sentinel the earlier tests learned to require: assert the refusal counter is **non-zero**, or the test passes vacuously on
a stream that was never budget-gated.

**Acceptance, still the point:** the mutant replacing `if (!laneIsEmpty || !budget.CanTakeWork())` with `if (!laneIsEmpty)`
must die by a named test — it survives all 59 collections today. Implementation: the gate is in `TaskStream.cpp`'s provider
drain; the counters are relaxed atomics with `TaskStream` accessors (useful as diagnostics regardless). Copy the stream setup
from the `TaskSystemTest` case that enqueues a busy task on a worker and reads `GetAccumulatedCPUTime` — same stream
setup, same measurement idiom.

### N3 — Guardrails as executable tests (todo #7, plan D2–D5)
Start with D2 (cross-stream isolation: communicate only via outcome delivery) — independent, no new seams.
D3 needs ThreadSanitizer. D4 is RAII-on-drop plus engine-clock deadlines. D5 is sustained budget over frames.

### N4 — B3d: task max age and abandonment (todo #6, plan D1)
Steady-clock stamp at enqueue, evaluated in the existing sweep, per-stream policy. Was blocked by C1 (the
handle waits had to go first); unblocked now.

### N5 — Documentation and plan hygiene (todo #9, plan D8–D10)
`docs/TaskSystemGuide.md` still teaches range splitting. Mark superseded R rows rather than deleting them.
HTML API pages for `TaskProvider` and `WorkItem` do not exist — the header contracts are the only docs.
Parent-plan: mark B3b, B3c, B4 done with the evidence already cited in plan D10.

## Standing rules

Stage owned paths **by name** — never `git add -A`; `git restore --staged docs/Core docs/OSAL` before every
commit (a concurrent agent owns those). Never push without permission. No comments in `.cpp` except
structural labels; contracts in the paired `.h`; rationale in `docs/`. Talk to the owner in English.
Working style the owner asked for: do not stop to ask; find at least three solutions, compare them, take the
best, keep going, then report an executive summary of solutions and why.

### D3 answered 02:55 by reading the container instead of running another sanitizer cycle

`AtomicStackView` is a Treiber stack: `top` is a real `std::atomic<T*>` with an `is_always_lock_free` static_assert, and Push and
Pop use `compare_exchange_weak` with release ordering - so the top pointer is sound, and it is not what ThreadSanitizer flags. The
flagged access is the **plain `next` field**: `Push` writes `newItem.next` unsynchronised, and `Pop` reads `node->next` from a node
it has only loaded, not exclusively claimed. Those nodes are recycled - `MemoryManager::DeregisterAllocator` ends with
`proxyPool.Push(allocator)`, returning the node to the pool it is re-pushed from. Node recycling plus a plain `next` read by a
non-owner is the precondition for the classic Treiber-stack ABA, so the report is a **true positive on a real defect class, not a
mis-model of a correct lock-free structure**, and the correction I wrote earlier in this file - "or it is correctly atomic and being
mis-modelled" - is the second branch that turned out not to hold.

The fix is therefore a design choice rather than a tweak: per-node atomic `next`, an epoch or hazard-pointer scheme, or a lock
around the registry. It is not mine to make - Memory is another agent's module right now - and the first step is to prove or
disprove ABA with a targeted stress on whether a proxy node can be re-pushed while another thread may still hold a pointer to it,
before any data structure is touched.

### Left deliberately by the review, 04:10

`docs/Engine/Engine/run.html` still names `ProcessMainThreadTasks` and `GetMainThreadTaskQueue`. It is a per-class reference page
for `Engine::Run`, which is exactly the surface the guide just corrected, so it is wrong for the same reason. Not edited in this
pass because I had not read it, and editing an unseen page is how the guide lost 292 lines earlier today. Fix by reading the page
and applying the same substitution the guide got: the loop condition is `taskSystem.IsRunning()` alone, `taskSystem.Update()` gives
the base stream one pass, and there is no separate main-thread queue left to drain.

### Guardrail that does not exist yet, and the design that would work (written after fixing the defect, 05:00)

`Engine::Run` has no test, and that is why my own commit could delete its loop header and 59 collections stayed green. A
mechanical source-text rule was attempted and **reverted for being dishonest**: it inserted after the summary so it printed nothing,
it carried an unused variable, and it pinned a string rather than a behaviour. Do not repeat that attempt.

What would actually work, in the order I would try it:

1. **Behavioural, in-process.** A collection that runs `Engine::Run` on a helper thread, counts `TaskSystem::Update` passes through
   an existing counter, calls `RequestShutDown()` from the test, joins, and asserts the pass count exceeded one. This is the only
   guardrail that fails for the reason the defect happened. Two obstacles to solve first: `Run` begins with
   `FatalAssert(isTaskSystemReady, ...)`, and it ends with `JoinAndClear()` plus `application.reset()`, so calling it mid-suite tears
   the engine down. Both are solvable - a dedicated collection that expects to be last, or a fixture that re-initialises afterwards -
   but they need thought, not an afternoon.
2. **A smoke run of a real application.** `check.sh` builds `WindowExample` without running it; running it under a bounded wall clock
   and requiring it to still be alive when the clock fires would have caught this exactly. The obstacle is headless environments: a
   windowed app may fail for reasons unrelated to the loop, and a flaky gate is a gate nobody trusts. Needs a config flag or a
   headless target before it can be wired in.
3. **Source-shape lint.** Cheapest, and the weakest: it pins a spelling, not a behaviour, and reformulating the condition would break
   it. Only worth it if 1 and 2 are refused, and then it belongs as an explicit, documented pin, not as a heuristic.

### Why EngineTest stayed green over a dead engine loop, and the trap in the obvious check (05:30)

`Applications/EngineTest/TestMain.cpp:17` says it plainly: *"The suite runs as a task that shuts the engine down when it finishes,
so the [results] are only complete once Run() returns."* So `Engine::Run()` is supposed to pump for the whole suite - and when my
commit removed its header, `Run` fell straight into `JoinAndClear()`, whose bounded pump finished the suite's tasks anyway. The
binary still reported 59 collections passing while the engine loop never ran once. Which means **the base-stream-driven-by-the-engine
-loop design (ba5f3a7, #15) has never been proven end-to-end**, including by the one binary that looks like it exercises it.

The obvious guardrail - measure how long `Run()` took - **does not work, and here is why**: `JoinAndClear()` is called *inside*
`Run()` (`Engine.cpp:148`), and it pumps the streams under a wall-clock bound, so a broken `Run` still takes seconds. Elapsed time
inside `Run` cannot tell the two cases apart, and a test built on it would pass over the very defect it is meant to catch. Same
trap for anything that asserts "the suite completed": the bounded drain completes it either way.

What actually discriminates is **who was running when the suite finished**. Two candidate witnesses, both cheap:

1. Sample the base stream's driven-pass count from another thread *while* `Run()` is executing, and require the loop - not
   `JoinAndClear` - to have been driving it. Distinguishable because the loop body is `Update(); yield();` with no wait, so its pass
   rate is far above the 10 ms `WaitForWork` cadence that `JoinAndClear`'s pump produces. A rate threshold with both a floor and a
   ceiling separates them; a bare count does not.
2. Record the moment the suite requests shutdown and the moment `Run()`'s loop exits, and assert the request happened *inside* the
   loop rather than during teardown. That is the property the design actually promises: the engine loop drives base work until the
   engine says stop.

Either belongs in `TestMain.cpp`, where `Run()` is already called and teardown is expected - which is why this never had to be a
collection fighting the engine's `FatalAssert(isTaskSystemReady)` and its `application.reset()` at the end.

### The in-process guardrail is dead, measured - and what that measurement says about the design (06:00)

I implemented the pass-rate witness in `Applications/EngineTest/TestMain.cpp` (a thread sampling the base stream's driven-pass count
every 5 ms, keeping the largest delta observed while `taskSystem.IsRunning()` was true, and failing the run if that stayed below 50).
It failed against **correct** code, with numbers that explain why:

    a 5ms window observed at most 1 pass(es) while the task system reported itself running
    Base closed after 8 driven pass(es).

The loop header was present at the time - I had fixed it earlier - so the only reading is that **`IsRunning()` is already false when
`Run()` is entered**: the suite completes inside `Test::RunTests()`, which blocks, and requests shutdown on its own. The loop body
therefore never runs in this binary, and no witness living inside this binary can ever tell a loop from no loop. **Design 1 is dead,
and it is dead for a structural reason, not an implementation one.**

The same measurement says something uncomfortable about the design I shipped in `ba5f3a7`: **the base stream received 8 driven passes
in the whole test binary**, all of them from `JoinAndClear`'s bounded pump. So "base work is driven by the engine loop" has never been
exercised by the test binary, and the comment at `TestMain.cpp:17` - "the [results] are only complete once Run() returns" - describes
an arrangement the code does not have: `Test::RunTests()` blocks and finishes first. The comment is not wrong about intent, it is
wrong about fact, and I wrote tests under that assumption.

What follows, in the order I would do it:

1. **Decide what the test binary is for.** Either the suite runs *while* `Engine::Run()` pumps - in which case `Test::RunTests()` must
   not block, and every test that sleeps in place becomes a test that the loop must carry - or the binary keeps driving its own
   collections and the comment and the intent both get corrected to say so. This is the owner's call, and it is the same call as #15.
2. **Then the smoke run becomes the guardrail** (design 2): a real application under a wall clock, which is the only harness where
   the engine loop actually runs.

### N4 (todo #6, B3d) - three designs compared, the one to build, and the one NOT to build with it (06:30)

Requirement, from `docs/TaskSystemRedesign.md`: a task may declare an optional deadline (engine-clock relative); exceeded means
**abandoned**, and the critical invariant is that **an abandoned task is still destroyed** - RAII frees what it possesses, and the
registry record must go with it. `N4`'s own note says: steady-clock stamp at enqueue, evaluated in the existing sweep, per-stream
policy.

| # | Design | Cost | Why it wins or loses |
|---|---|---|---|
| 1 | Stamp the enqueue time on every `WorkItem`, compare against a per-stream ceiling in the sweep | `WorkItem` 56 -> **64 bytes**, +8 per queued item, paid on every enqueue copy and every re-add after an unfinished run | **Chosen.** Evaluation stays local to the item the sweep is already holding; no second lookup, no queue-internal clock to reason about |
| 2 | Put the deadline on the task record, read age from a per-lane monotonic watermark | no field growth | **Rejected.** A lane that stalls while other lanes advance gets the wrong age, which is exactly the case the feature exists for - and the age is a property of when the work was offered, not of when the lane last moved |
| 3 | Enforce at dispatch only, no sweep: an over-age item is abandoned instead of run | zero sweep work | **Viable and cheap**, but stale items keep occupying queue memory until something reaches them, and on a starved lane the head item is the only one examined - so it protects the decision, not the capacity |

**Build the per-stream ceiling first, and do NOT build the per-task optional deadline in the same change.** The per-stream max age is
what prevents the documented harm - work whose context went stale hours later being run because it was queued - and it costs one
field per stream. The per-task deadline is a task-author convenience with **no caller in the tree today**; adding a field to `Task`
and a second comparison path for a feature nobody requests is the over-engineering I have been removing all session, not adding.

Implementation shape, so the next session starts from a decision and not a question:
1. `WorkItem` gains a steady-clock enqueue stamp; `TaskSystem`'s enqueue sites stamp it; the existing `static_assert` on the item's
   size has to be updated to 64 with the reason, since that assert exists precisely to catch a silent layout change.
2. `StreamDrainPolicy` gains `SetMaxAge` / `GetMaxAge`, default **none** - unlimited, because every stream behaves as it does today
   until someone opts in.
3. The sweep already in `TaskStream::Update` drops over-age items instead of re-adding them: release the registry record, destroy the
   item, and count it through the **same abandoned-work report path #16 built for shutdown**, so abandoned work has one accounting
   vocabulary in the engine instead of two.
4. Test, with the in-band sentinel the negative assertion needs: set a ~20 ms ceiling, enqueue work, let it age without driving, then
   drive - and assert the runnable never executed (sentinel stays 0), the registry record is gone, and exactly one work item was
   reported abandoned. Named mutant to kill it: force the age comparison false; the test must fail by name, per the standing standard
   that a mutation is not accepted until a named test names the gate.

