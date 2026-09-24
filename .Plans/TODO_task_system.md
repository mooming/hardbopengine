# TODO — task system, current state and what a fresh session should do next

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

## Remaining, in the order I recommend

### N1 — Let the base stream own the work posted to the engine loop (todo #15)
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

`MainThreadTaskQueue` is self-contained (`BoundedPriorityQueue<TaskItem,256,1024>`, mutex-guarded, tasks run
with the lock released), so it moves into `TaskStream` without a circular include — `TaskStream.h` includes
`MainThreadTaskQueue.h`, not `TaskSystem.h`. Then: `TaskStream::Update()` drains posted callables first, so
`Update`, `DriveUntil` and the shutdown pump all reach them by construction; `DispatchToMainThread` becomes
one line forwarding to the base stream; `GetMainThreadTaskQueue` and `ProcessMainThreadTasks` go away and
`Engine.cpp` asks the base stream for its pending count. **Acceptance:** one intake, one pump, all three
configs at exit 0 with 59 collections, and a mutant that skips the posted drain dying to a named test.

### N2 — Test the drain's budget gate (todo #10)
Design settled and proven race-free: the provider reads `MayTakeNewWork()` inside `Produce`, which runs on
the draining stream's own thread — the only legal place to read the unsynchronised allowance. The earlier
attempt reported `Result [PASS]` and then hung at shutdown; that hang was the shutdown defect, now fixed, so
retry it as written. **Acceptance:** the mutant replacing `if (!laneIsEmpty || !budget.CanTakeWork())` with
`if (!laneIsEmpty)` must die by a named test — it survives all 59 today.

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
