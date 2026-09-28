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
> **Status as of 2026-09-27 22:45** (read from `date`). **Todos #6 and #7 are both DONE.** #6 (B3d max age) at `feb1c73`/`215db9b`/`6d9f2f5`; #7 closed by `e74e05c`, which put the last of the four guardrails - D4 RAII-on-drop - into executable form, and whose own mutant run exposed and then fixed a FIFO/priority blind spot in that very test. Standing decisions 4, 5 and 6 stay closed (6 by owner-proxy, reopen conditions written). **What is genuinely left, none of it in the numbered list:** the `AtomicStackView` Treiber ABA **fix**, which is the Memory module owner's together with the fact that ThreadSanitizer cannot finish this suite; nothing is blocked. The **`Engine::Run` guardrail is CLOSED** (`429a9c1`/`4a64905`): a testlet is a task, each posts its successor, the total is registered before `Run()` and any registered-vs-executed gap fails the run - measured to kill the original one-pass defect; **todo #9's** missing `docs/WorkItem` and `docs/TaskProvider` pages, verified absent by listing `docs/`; and standing decisions 2 and 3. HEAD `e74e05c`, 59 collections green in Debug/Dev/Release, `check.sh` 0 violations, nothing pushed.
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

### N3 — ~~Guardrails as executable tests~~ **DONE (todo #7, plan D2–D5): all four have executable tests; `e74e05c` closes D4**

Measured per guardrail against the suite and the commit history rather than against this paragraph's memory:

| Guardrail | State | Evidence |
|---|---|---|
| **D2** cross-stream isolation: lanes talk only through outcome delivery | **landed, with a control that proves its recorder works** | `34de7e6`, control `eb21617` |
| **D3** recycled-node / allocator-stack hazard | **guardrail landed; the fix is not mine** | groundwork `c80a319`, stress guardrail `9ef0f4f` (3200 pops, 0 double hand-offs). The Treiber ABA in `AtomicStackView<AllocatorProxy>` is a confirmed true positive in the **Memory module**, and TSan still cannot finish this suite here - both belong to that owner |
| **D4** RAII-on-drop + engine-clock deadlines | **landed** | deadlines: B3d's `SetMaxAge` at `215db9b`, `WaitUntil` at `798813e`; RAII-on-drop: `e74e05c` walks all three drop sites and asserts the registry live count and the ID's findability end where a release leaves them |
| **D5** sustained budget over frames | **landed, and made provable in Release** | `fe9ade1` (a spent stream parks on its condition variable, 950x margin), `a166b67`, `2a74ba5` |

**The D4 witness landed**, and the lesson from it is not the test - it is how the blind spot was found. Its first version offered the
closing-site work on the FIFO lane; the mutant placed in `AbandonHeldWork`'s **priority** drain loop walked straight past it and the
suite passed. Site 3 now uses the priority lane, which the other two sites do not cover, and the mutation is killed by name. That is the
same lane blind spot for the third time, and the only time it was caught by *running* a mutant instead of reading a test - which is the
standing standard earning its keep.

One sentence in that commit's first draft was false and was withdrawn in the amended message: I claimed a record-freeing close site "is
not expressible because `TaskStream` holds no `TaskSystem` handle". `TaskStream` has held `TaskSystem*` at line 86 all along; my mutant
failed because I wrote `taskSys` where the member is `taskSystem`. A mechanism argument invented from a compile error I had not read
closely is the most durable kind of wrong documentation, because it reads like a design fact.

**And the guardrail that still does not exist is `Engine::Run`.** Design 1 (in-process, count passes while `Run()` pumps) is measured
dead, not difficult: `JoinAndClear()` is called *inside* `Run()` and pumps under a wall-clock bound, so both candidate witnesses -
elapsed time, and "the suite finished" - are blind, and a sampled base-stream pass rate peaked at 1 pass inside the loop. Design 2 (a
real application under a bounded wall clock) is the only one that can see the defect, and its obstacle is a headless-safe target or a
config flag, which is a decision rather than an afternoon. Design 3 (source-shape lint) was attempted and **reverted as dishonest** -
it pinned a spelling, printed nothing, and carried an unused variable; do not repeat it. See 21:40 and 17:07 above.

### N4 — ~~B3d: task max age and abandonment~~ **DONE (todo #6): `feb1c73` + `215db9b` + `6d9f2f5`**

Shipped, and two of the three design words above were wrong, which is why they are kept:

| Planned | Built | Why it moved |
|---|---|---|
| stamp at **enqueue** | `Task::offerTime`, stamped in **`LoadIntoRecord`** | the item is a slice of the task; a sub-slice has no parent item reachable from `GenerateSubTask`, so item-level stamping made the inheritance the plan demanded unwritable, and would have let a task postpone the ceiling by splitting late or not finishing in one call |
| **steady clock** | `time::ElapsedSinceEngineEpoch()` | `TaskProduceContext` already documents its `now` in the engine epoch base; an age book in a different base from the drain book is a bug that reports nothing |
| evaluated in the **existing sweep** | evaluated on the **take path**, after the `FindTask` lookup | there is no periodic sweep of queue ages; the take path is where both lanes converge, so one check covers both, and it sits after the lookup because a task that is gone has no owner to explain a refusal to |

Mechanism: `TaskStream::SetMaxAge`/`GetMaxAge` (per stream, **off by default**), `StreamDrainPolicy::IsOverAge` with three guards,
over-age work **dropped, counted (`GetAgedOutWorkCount`), reported in the released-task vocabulary, and notified** - never run, never
requeued. Tests are control-first and duplicated per lane. Mutants: remove-check, remove-notice, remove-counter and
report-then-run-anyway were **killed by name**; inverting the predicate **stopped the suite instead of failing it** (`exit=60`, no named
test) because the default ceiling is unlimited - a detection with nothing alive to attribute it to, recorded as such.
Docs: guide sections 16 and 17, and the redesign's per-task deadline is now marked designed-but-not-built.

### N5 — Documentation and plan hygiene (todo #9, plan D8–D10) — partially closed, re-verified 22:15
Closed: guide sections 14 (`SetAbandonedNotice`), 15 (lane choice), 16 (max age) and 17 (what the rate does and does not promise);
the redesign's per-task deadline is marked designed-but-not-built with reopen conditions; `WorkItem::priority` carries its
insertion-label contract (`073e746`).
**Still true when checked, so still owed:** `docs/WorkItem` and `docs/TaskProvider` do not exist - verified by listing `docs/`, and the
header contracts are the only documentation those types have. `Engine::Run`'s reference page was rewritten from source (`5758812`) but
the `WorkItem`/`Task` size changes of `feb1c73` are not reflected in `docs/Core`, which is **another agent's directory** - recorded here
so they can apply it.
Superseded R rows are marked rather than deleted, and the parent plan's B3b/B3c/B4 claims still need checking against plan D10.

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

### Standing decision 4 (NEW, 07:40) — who is told when work is abandoned, and today the answer is nobody

The owner asked whether abandonment reaches the requestor. It does not, and grepping the public surface proves the
scope of it: there is **no abandon/notify/callback anywhere in `TaskSystem.h`, `Task.h`, `TaskProvider.h` or
`TaskStream.h`** - every occurrence of the word is an internal report or a doc note. The single delivery primitive is
`TaskSystem::DispatchSuccessor(TaskID finishedTask)`, and it is called from exactly one place, `TaskStream.cpp:515`,
inside `if (workItem->Run(*task))` - so it fires only when a join **completed**. Every drop path - the released task at
`TaskStream.cpp:500`, the shutdown deadline reports at `:572` and `:627`, and the max-age check that
`PLAN_b3d_max_age.md` Step 3 would add - destroys the item, writes a log line, and never calls it.

Three consequences, in order of how much they hurt:

1. The requestor's **successor is never enqueued**, so its chain stops with no signal at all. `SetSuccessor`/
   `GetSuccessor` exist, and nothing on the abandonment side consults them.
2. `FindTask(id)` returning `nullptr` is the only thing a requestor can observe, and **it is ambiguous by construction**:
   it cannot distinguish abandoned from completed-and-released from never-existed. That is not a signal, it is the
   absence of one wearing a signal's clothes - which is the failure mode this session has been chasing in every other
   form. (C1 made it worse in a useful direction: `Task::HasDone` is private and the handle waits are gone, so nobody
   can even poll completion; requestors poll their own sentinels.)
3. The only in-band, honest signal is the **wait side**: `DriveUntil("waiting for ...", predicate, timeout)` reporting
   `ReportDriveTimeout`. A requestor that waits finds out it never got its outcome; a requestor that does not wait never
   finds out anything.

Three solutions compared:

| Option | Shape | Cost | Verdict |
|---|---|---|---|
| **A. Failure successor dispatch** | on abandonment route a packet to the successor carrying an abandoned status, one path for success and failure | needs a status in `ResultPacket`'s 128 bytes, and **every** task author must then handle "my input is missing" | Right for chains that must continue; too broad to impose by default - the owner's call |
| **B. Outcome state on the record, polled** | `EOutcome { pending, done, abandoned }` on the registry record + public `GetOutcome(TaskID)` | one field - it fits the record's existing 48 bytes of cache-line padding - and one accessor | **Recommended first.** It removes the `nullptr` ambiguity at no dispatch cost and burdens nobody |
| **C. Per-task abandonment callback** | `SetAbandonedHandler(id, fn)` | a callable per task: size, ownership, and *which thread runs it* - re-importing the exact cross-thread callback surface this arc spent itself deleting in C1 | **Rejected**, for the same reason the handle waits went |

**RESOLVED 08:10 — the owner chose the optional callback (option C with a function pointer, unset by default), and it is
specified to the line in `.Plans/PLAN_abandonment_notice.md`.** The read that changed my recommendation: at two of the three
abandonment sites the task is **not resolvable** - `:500` has already had `FindTask` return `nullptr`, and the two shutdown
close sites only call `CountPendingItems()` and never look at an item - so a handler stored on the task or beside
`successor` in the record is unreachable precisely when it is needed. The notice therefore travels **on the `WorkItem`**:
function pointer plus `void* userData`, 16 bytes, copied for free by the item's existing `= default` copy semantics, so
re-adds and split slices keep it. That makes the queue item **72 bytes, or 80 with max age**, which is the one number the
owner still has to accept; the fallback if it is refused is named in the plan and does not fake the shutdown case. Option
B (`EOutcome` on the record + `GetOutcome`) is still worth doing on its own merits - it removes the `FindTask`-returns-
`nullptr` ambiguity that no callback fixes.

**Ordering constraint: settle this before implementing `PLAN_b3d_max_age.md`.** A max-age drop is the third site that
abandons work; building it while the answer is "log only" bakes the ambiguity into one more place, and unwinding that is
the kind of change that has already cost this project a session.

### Standing decision 5 (NEW, 11:15) - the priority lane is unreachable from the public API, so my drain test was vacuous

Investigated because a mutant survived twice: `AbandonHeldWork`'s priority-lane loop pops, counts and fires no notice, and all 59
collections passed. The cause is three lines of production code, not the test.

```cpp
void TaskSystem::EnqueueTask(TIndex streamIndex, Task& task, uint8_t priority)
{
    task.ReserveSubTasks(1);
    Enqueue(streamIndex, task.GenerateSubTask(0, 1, priority));   // priority goes into WorkItem::priority
}

void TaskSystem::Enqueue(TIndex streamIndex, const WorkItem& task)
{
    streams[streamIndex].EnqueueFifo(task);                       // always FIFO - no branch, no lane choice
}
```

The lane a work item lands in is decided by **which function is called** - `TaskStream::EnqueueFifo` or `TaskStream::EnqueuePriority`
(`TaskStream.h:170/174`) - and the engine's public surface has exactly one route, which is `EnqueueFifo`. `priorityQueue` is fed only
by `priorityQueue.PushRange(readdingPriority)` at `TaskStream.cpp:394`, which re-adds items that were **already** in the priority lane:
circular, so nothing can ever get there. Consequences, all verified by grep rather than inferred:

* the `priority` byte is read only by `WorkItem::operator<`, which orders entries **inside** `BoundedPriorityQueue` - and the FIFO lane
  is a deque (`PushBack`/`PopFront`), so for the only work the engine can actually enqueue the byte influences nothing at all;
* `StreamDrainPolicy`'s FIFO:priority rate machinery is exercised solely by unit tests that call the policy directly
  (`StreamDrainPolicy.cpp:197` counts takes), never by real work reaching either lane;
* the priority re-add path and the priority half of the shutdown drain are unreachable, which is why a mutant there cannot be killed
  by any test that enters through the public API - my four-item test was not weak, it was **input-starved**.

Two different models of "priority" shipped under one name: **lane selection** (by function) and **in-queue ordering** (by byte). The
documents describe a two-lane stream with a rate between the lanes; the code has one reachable lane. That mismatch is the root cause,
and it is a design question, not something this arc should close by inference.

| # | Solution | Cost | Verdict |
|---|---|---|---|
| **S1** | Make the lane reachable: route `TaskSystem::Enqueue` to `EnqueuePriority` when the item asks for it, or add an explicit lane parameter to the public API. The `priority` byte and the drain-rate arithmetic become live for the first time | Small code, **real behaviour change**: priority work would preempt, lane budget shares stop being theoretical, and #16's bounded drain needs re-proving under two live lanes | Fixes the design. Owner's call, because it changes dispatch behaviour that every existing caller depends on |
| **S2** | Delete the unreachable lane: `EnqueuePriority`, `priorityQueue`, the priority re-add, the lane-share arithmetic in `StreamDrainPolicy`, `WorkItem::operator<` - and the rate row in the design document | Largest diff, removes a documented goal | Honest minimalism if the lane is not wanted. Also the owner's call, and it is the option my own minimalism instinct prefers |
| **S3** | Keep behaviour, un-vacuum the test: `EnqueuePriority` is **public**, so the drain test can fill the lane directly and then call `AbandonHeldWork` | A few lines | Do now, with a comment naming the reason the lane is filled by hand. Tests the drain, **not** reachability - and if the reason is not written next to it, it hides root cause B behind a green suite |
| **S4** | Collapse `AbandonHeldWork`'s two loops into one "pop one from this lane" helper | Small | Do now. The duplicated loop shape is what makes a per-loop mutant expressible at all; one path cannot have a fire in one lane and not the other |

**Recommendation: S3 + S4 immediately** (they make the drain genuinely covered and remove the shape that allowed the survivor), and
**S1 vs S2 as one owner decision** - "should the priority lane be reachable?" Code says no, documents say yes, and both cannot stay.

### Drain rate through the public API: `ChooseLane` read at last, and why the test is still not writable today (12:30)

The reachability half is done. The ratio half was blocked on one unknown - what `ChooseLane` does with no allowance - and reading
it answers the question and introduces a worse one.

`ChooseLane` is pure (it neither charges the budget nor mutates rotation state) and has two regimes:

* **Unlimited allowance** (`allowance.count() <= 0.0`): if only one lane has work it serves that lane; if **both** have work it
  returns `fifoCredit >= priorityCredit ? Fifo : Priority` - credit-driven alternation, where credit comes from
  `ConfigureRate(fifoWeight, priorityWeight)` and is spent by `CommitTake`. A weight of zero is treated as one with both falling
  back to `1:1`, and the header is explicit that a zero weight is **not** "never serve this lane", because silently configuring one
  would strand that lane's queued tasks.
* **Finite allowance**: per-lane shares derived from the rate; returns `ELane::None` when the round is exhausted, and picks the lane
  with more remaining share when both have room.

Two obstacles, and they are why this stayed a spec rather than becoming a commit:

1. **The test must configure a rate on a live stream.** The only stream a test may legally drive is the base stream, because a test
   body runs on its owner thread - so measuring the ratio end-to-end means calling `ConfigureRate` on the base stream *during the
   suite*, changing dispatch behaviour for every collection that runs afterwards. That is the same class of mistake as the
   iteration-bound test that produced the phantom "configuration anomaly": a test that alters shared engine state does not measure
   the engine, it measures the order the collections happened to run in. A correct version restores the prior configuration, and
   `StreamDrainPolicy` exposes `GetFifoShare`/`GetFifoUsed` but **no getter for the configured weights**, so there is nothing to
   restore from. Adding that getter is the first step, not an afterthought.
2. **The claim has to be the one the policy makes, not the one that is convenient to assert.** With both lanes loaded and a 3:1
   rate, the exact take split depends on credit already spent, and the number of takes a `DriveUntil` window produces is not
   controlled by the test - each pass takes at most one item and may fall back to the general queue. So the honest assertions are
   *both lanes served* (no starvation, which is the property the zero-weight fallback exists to guarantee) and *FIFO strictly more
   served than priority under 6+6 items with a 3:1 rate* (which detects a rate that is ignored entirely). Asserting an exact 6/2
   split end-to-end would be a test of `CommitTake`'s bookkeeping wearing an integration test's clothes - and that bookkeeping is
   already tested exactly where it lives, at `StreamDrainPolicy.cpp:231`.

So the sequence is: add the weights getter → save/restore the base stream's rate around the test → assert served-both and
strictly-more-FIFO. Anything shorter trades a real guarantee for a flake.

### Standing decision 5 - CLOSED, resolved as S1 by the owner (12:00)

"Both lanes should be reachable and customers can choose one with public APIs." Implemented exactly that: `TaskSystem::Enqueue`
takes a `StreamDrainPolicy::ELane`, `EnqueueTask` grew a defaulted lane as its last parameter, and `ELane::None` asserts instead of
being absorbed as FIFO. `S2` (deleting the priority lane and its rate machinery) is therefore off the table, and `S3`/`S4` were
superseded by something better than either: with the lane reachable, the drain test fills it through the public API instead of by
hand, and the previously surviving mutant is killed by name. `c07eecc` `56b70a3` `4295562` `79df8b5`.

### Standing decision 6 (NEW, 13:20) - derived keys or maintained keys, and the aging step

Blocking all remaining dynamic-priority work. Full analysis, cost model and the four-step build order are in
`.Plans/PLAN_dynamic_priority.md`; the decision is two things:

1. **Derived or maintained.** Derived keys cannot go stale, cost nothing when nothing changed, and make the per-frame refresh O(256)
   or free because the global argmax is always one of at most 256 bucket fronts. Maintained keys place the cost in a sweep you
   schedule, bill and observe, and keep the queue's physical order equal to its priority order - a real debugging property. Recommended
   hybrid: **explicit intent maintained, time derived.**
2. **The aging step** - how many frames of waiting promote an item one level. That is a starvation-policy number, not an implementation
   detail: with 256 levels it sets the worst-case wait before an item saturates the top, and it must **saturate at 255 rather than wrap**
   or the most urgent queued work becomes the least urgent, silently, and only after a long stall.



### Standing decision 6 - CLOSED by the session as owner-proxy (18:20)

Derived keys versus maintained keys, plus the aging step, blocked dynamic priority since 13:20. The owner's standing instruction for
this run was to resolve such questions myself and record the reasoning, so: **aging is not implemented, by decision rather than
omission.** Three reasons. (1) The problem it solves - low-priority work never running - is already excluded by a promise the drain
policy makes and which is now measured end to end at 8:1: a lane with work is never starved by the other lane's weight, and a zero
weight means one, not never. (2) The problem on the other side is now bounded too: a stream that cannot keep up declines stale work
instead of running it late. (3) At the depths this engine reaches - a base stream that closed holding 8 items - a per-frame aging
term changes no observable behaviour, while the container's actual hazard is that the bucket index *is* the priority, so a maintained
key is a live invariant to be kept true by every writer rather than a field being read.

What was built is the one piece that is unambiguously a defect fix: `Pop` reports the level it served from, so an item filed under one
level and reporting another cannot be handed to a caller (`9a3bf64`, mutant killed by name). The derived-key tournament, the clamp at
255 and the pending-reprioritise list remain specified in `PLAN_dynamic_priority.md` for the first caller that needs them. **Reopen
conditions:** a stream that is permanently backlogged with mixed priorities, or any measured starvation, or a caller that asks for
age-based promotion. Recorded here rather than silently, because a decision made without the owner is the owner's to overturn.

### Test lifecycle method pages - CLOSED by the 22:40 session

Twenty-two reference pages carried a banner saying the replacements for the removed one-call lifecycle were
"recorded as owed work in `.Plans/TODO_task_system.md`". That claim was only ever half true: the work was real and
owed, but no entry here tracked it - the banners were pointing at a file that did not name the item. Both halves are
now closed.

Authored, from the source rather than from memory:

| Page | Documents |
|---|---|
| `docs/Test/TestCollection/prepare-tests.html` | `TestCollection::PrepareTests` - clears buffers, calls the virtual `Prepare`, registers nothing running |
| `docs/Test/TestCollection/run-test-at.html` | `TestCollection::RunTestAt` - one testlet, its scope, its measurements, its ceiling, its verdict contribution |
| `docs/Test/TestCollection/complete.html` | `TestCollection::Complete` - verdict from accumulated errors, and what each of the three outcomes does at the environment |
| `docs/Test/TestEnv/prepare-testlets.html` | `TestEnv::PrepareTestlets` - flattening, labels, offsets, run counts, and what state it leaves |
| `docs/Test/TestEnv/run-testlet.html` | `TestEnv::RunTestlet` - index space, the past-the-end guard, the duplicate guard that still runs the testlet, and the shortfall message |
| `docs/Test/TestEnv/finalize.html` | `TestEnv::Finalize` - the report, who posts it, and where the verdict the gate reads actually lives |

Every banner's final sentence now names those pages instead of pointing here.

One falsehood of mine was removed on the way: the class nav I authored yesterday labelled `TestCollection/prepare.html`
as "Prepare (removed API)". `Prepare` is live - it is the virtual `PrepareTests` calls, which is exactly why
`PrepareTests` exists. The label now reads `Prepare`, and the page states plainly that its own function is live API
whose job changed from running tests to registering them.
