# TODO — task system, current state and what a fresh session should do next
> **Status 22:55 — #17 is CLOSED.** Three counters (`laneWorkRefusals`, `providerAsksWhileSpent`, plus the existing
> `generalQueueRefusals`) and one test that reads them. Acceptance proof met: with `!budget.CanTakeWork()` removed from the drain
> gate, the **Release** build fails and names it — "A provider was asked 24 time(s) while the allowance was spent." Debug/Dev catch
> the same mutant via the assert in `a166b67`. Everything from the earlier diagnosis (configuration anomaly, pop-and-return,
> misplaced counters) is superseded by this; the anomaly was an iteration-count burn, fixed by bounding the work in wall clock.
> Everything still open in this file: #9 docs, #7 guardrail tests (D2/D3/D4/D5), #6 B3d, and the two owner decisions — the two
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
