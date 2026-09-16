# Plan A — Bug fixing: partial-init legality, log zero point, duplicate OS application

**Task A of two.** Companion: [`PLAN_task_system_refactor.md`](PLAN_task_system_refactor.md).
Parent record (uncorrected architecture decisions): [`PLAN_single_executable_app_registry.md`](PLAN_single_executable_app_registry.md).
**Depends on:** nothing. **Blocks:** nothing — Task B can start in parallel.

---

## 0. Scope

**In.** Four live defects (D3, D5, D6) plus `EInitLevel` as the contract that makes them unreachable, and the level-safe teardown that `EInitLevel` requires.

**Out.** Every registry/refactor step (steps 3–9 of the parent plan), and every change to how tasks or task streams are driven — that is Task B. This plan changes no scheduling behaviour.

**Why `EInitLevel` is in the *bug* task and not the refactor.** D6's root cause is "logging before the subsystems that support it exist." A level that states what is started is the contract that closes it — which is why the owner put D5/D6 on step 1 rather than in two isolated patches. Recorded in the parent plan §1 and in `JOURNAL.md` (2026-09-13).

---

## 1. Starting state (verified, do not redo)

| Fact | Evidence |
|---|---|
| Step 2 of the parent plan is **done** — inline drain and Engine-free fallback | `5677fce`, `95053e5`; formatting split into `a499508`, `bc16916` |
| `Engine/Engine/EngineInitLevel.h` exists, **untracked**, and compiles | `clang++ -std=c++2b` clean; its `static_assert`s pass. **Nothing includes it yet** — no target compiles a header in isolation, so it is not build-verified |
| The staged index is **formatting only** | Strip-whitespace hash: `TaskSystem.h` `f5980c6d3d` both sides, `Engine.h` `bc6620c34c` both sides; `Engine.cpp` differs only by an include reorder (`git diff --cached -w`) |
| Baseline | `EngineTest` **53/53** in Debug/Dev/Release |

**First action:** commit the staged formatting as its own `style(engine):` commit. It is not described by any commit message in the parent plan's sequence, and mixing it into the `feat` commit would hide the semantic change — the same split already used for steps 2 and 7.

---

## 2. Fix list

### A0 — D7: the `-test` configuration did not compile at HEAD ✅ `176e8f1` (format), `ccc05ec` (fix)

Discovered while verifying A1, not listed here when this plan was written. Six `windowFuture.get()` calls in the `WindowTest` bodies
(`Window.cpp`) discard a `[[nodiscard]]` `std::future<void *>::get()`, and `-Werror` turns that into 12 errors — but only under
`__UNIT_TEST__`, which the default gate does not define.

**Fix.** Explicit discard, following the tree's own convention (`TaskSystem.cpp:164`). Verified by building with `__UNIT_TEST__` in all
three configurations and running the suite: 53/53, with `WindowTest` executing bodies that had not been compiled in a passing build.

**Routine change this plan now requires:** the gate for any commit touching a test body is `check.sh --staged --test`, not the default.
`--test` already builds with `-test` and would have caught this; the default gate cannot see `-test`-only breaks.

### A1 — D6: logging before `Engine::Initialize` aborts ✅ `d75e7bc`

*Done as written, with one correction to the fix text above:* the guard is `TaskSystem::HasStream(index)`, not `isTaskSystemReady`. The
latter is a proxy for "the drain task is alive", and it misses the shutdown window — `streams.Clear()` runs in `JoinAndClear`, and
`Logger::StopTask` is only reached from `SignalHandler`. Guarding stream *existence* is the actual precondition of the indexing that
follows. Repro went exit 133 → exit 0.

`Logger::AddLog` ends by waking the IO stream:

```cpp
auto& ioStream = taskSystem.GetIOTaskStream();   // Logger.cpp:392
ioStream.WakeUp();                               // Logger.cpp:393
```

`GetIOTaskStream()` indexes `streams[IOStreamIndex]` (`TaskSystem.h:27`, `IOStreamIndex = 1`), and `Array::operator[]` `FatalAssert(IsValidIndex(index))` with **no message** (`Container/Array.h:84-86`). `streams` is empty until `TaskSystem::Initialize()`, so the assert fires with nothing to explain it.

**Fix.** Do not wake a stream that does not exist: guard on `isTaskSystemReady`, and on the ungated path flush synchronously. This is the same guard `EInitLevel` needs — one mechanism, not two.

### A2 — D5: every log timestamp counts from the clock epoch, not from engine start ✅ `5f7b853`, format `69267b8`

*Decided and implemented, and the recommendation above was not followed — for a reason worth keeping.* Option (2) does not fit as
written: `SystemStatistics::GetStartTime()` returns `time::TTime` over `high_resolution_clock`, while `LogLine`, `Logger` and `LogUtil`
are all uniformly `steady_clock`, and `high_resolution_clock` is `system_clock` on some standard libraries. Passing the value across is a
clock-domain conversion that is silently correct here and wrong elsewhere. What ships is option (1) as the default, plus an explicit
`LogUtil::ResetStartTime()` called from `Engine`'s constructor — the same instant `SystemStatistics` records (its ctor is the member just
before), taken from the log's own clock, with no `Log → Core` dependency.

*Zero padding:* left alone, so `37.004 s` prints `0:0:37.4`. Open if the owner wants fixed-width columns; it is cosmetic and would widen
this commit for no correctness gain.

Measured before: `[48:47:47.257]`. After: `0:0:0.0` to end of run, hour field `0` in every line, 53/53 in all three configs.

`LogUtil::GetStartTime()` returns a function-local static that nothing ever assigns (`LogUtil.cpp:13-17`); `LogUtil.h:19` returns it `const&`, so it cannot be assigned through; and `Logger.cpp` calls `LogUtil::GetTimeStampString(timeStampStr)` and discards the result. Measured: `1222 h 28 m` against `uptime` 50 d 22 h — i.e. machine uptime.

**Fix — decision required, then implement.** Two viable zero points:
1. value that static on first use, or
2. take the baseline from `SystemStatistics::GetStartTime()`, which already records one (`SystemStatistics.h:68`).

Recommend **(2)** — it reuses an existing zero point instead of minting a second one. Also decide here whether `GetTimeStampString` zero-pads; it currently prints `1:2:3.45`.

**This changes the first field of every log line.** It is a user-visible decision, not a repair — say so in the commit message.

### A3 — D3: two `OS::Application` objects per process ✅ `6913d8c` (style), `ba42dd4` (fix)

*Done as specified, and the correction of record below was carried out — the journal entry that declared D3 "already gone" is now marked
superseded with the reason.* The duplicate was live, not shadowed: `CreateApplication` returns a fresh `make_unique` per call. Both mains read
`Engine::GetApplication()` and a grep for `CreateApplication` under `Applications/` is empty. No construction counter, as planned.

`Engine::Initialize` creates one, and **both** example mains create a second one and call `Initialize()` on it:

| Site | Code |
|---|---|
| `Engine/Engine/Engine.cpp` | `application = OS::CreateApplication();` |
| `Applications/WindowExample/Main.cpp:16` | `auto app = OS::CreateApplication();` + `app->Initialize()` |
| `Applications/VulkanExample/Main.cpp:86` | `auto app = OS::CreateApplication();` + `app->Initialize()` |

`OS::CreateApplication()` returns `std::make_unique<Application>()` (`OSAL/Application.cpp:8-11`) — a fresh object each call, **not** a singleton. So the duplicate is live today.

> **Correction of record.** `JOURNAL.md` (top entry) states D3 "was already gone," on the grounds that `CreateNewApplication` exists nowhere. That reasoning is wrong: the function is `CreateApplication`, and the behaviour is present in both mains. The parent plan §7's claim was correct; the journal "correction" is the error. Part of this task is to restore the journal entry.

**Fix.** Gate creation under the `Application` level (A4) and have the apps read the host-owned instance via `Engine::GetApplication()` (`Engine/Engine/Engine.h:148`), which `OSAL/Window.cpp` already consumes. **Do not** add an `OS::Application` construction counter yet — while the mains are unconverted, that assert would fire on every direct run. It belongs with the app conversions.

### A4 — `EInitLevel`, and teardown that respects it ✅ `7e62dcf`

*Wired as specified, with one premise of this plan proven false by running it.* "No new state: `isLoggerReady` … already say[s] what started" is
wrong for that flag — `Logger`'s constructor raises it, so it reports construction and was true at every level. A `SIGINT`-at-level-`None` test
hung on it (`StopTask` → `Task::Wait` forever) and the guard now asks `Logger::IsDrainTaskRunning` instead. See `JOURNAL.md` for the stack and
for why that predicate is right here and was wrong for A1.

Spec lives here (the parent plan §1 is now a summary of this section).

```cpp
enum class EInitLevel : uint8_t { None = 0, TaskSystem = 1<<0, Logger = 1<<1, Application = 1<<2, All = 7 };
void Initialize(int argc, const char* argv[], EInitLevel levels = EInitLevel::All);
```

`EngineInitLevel.h` already declares this and compiles; the work is **wiring**, not authoring. One method with a defaulted parameter — one code path, no overload pair — so **every existing caller keeps its present behaviour** (`All`).

`Logger` implies `TaskSystem`, **enforced in `Initialize`**, not documented — A1 is the concrete reason. No new state: `isTaskSystemReady`, `isLoggerReady` and `application != nullptr` already say what started, so the level cannot drift from reality.

| Currently unconditional | Becomes |
|---|---|
| `Engine::Run()` → `FatalAssert(application != nullptr); application.reset();` | reset only when one exists |
| `Engine::ShutDown()` → `taskSystem.RequestShutDown()` | guarded by `isTaskSystemReady` |
| `SignalHandler()` → `logger.StopTask(taskSystem)` | guarded by `isLoggerReady`; else synchronous flush |
| `Engine::Initialize` → `OS::CreateApplication()` | only when `EInitLevel::Application` is set (this is what makes A3 structural) |

**Not in scope:** making `Engine::Run()` refuse to pump off the base thread. `TaskSystem::IsBaseThread()` is already public (`TaskSystem.h:51`, impl `TaskSystem.cpp:56`, already used at `:232`) — there is nothing to expose — and the *meaning* of "base thread" is being replaced by Task B. Leave it there.

---

## 3. Verification

| Check | Method | Pass |
|---|---|---|
| Baseline preserved | 3-config build, `EngineTest` ×3 | 53/53, unchanged |
| D6 reproduced before fixed | log between `Engine hengine;` and `Initialize(..., None)` | aborts pre-fix, clean post-fix |
| A4 partial init legal | `Initialize(..., TaskSystem\|Logger)` then log | no crash, no hang, exit 0 |
| D5 zero point | one-second run | timestamp reads `0:0:0x.xxx`, not `1222:12:38` |
| D3 single app object | both examples run; second `CreateApplication` gone from sources | one construction per process |
| Signal before `Initialize` | `SIGINT` at each level | no hang, no assert; CI-safe |
| Style + build | `check.sh --staged --apply` then `--staged`, 3 configs | exit 0 |

**Harness requirement, learned the hard way.** A signal test must background the *bare binary*, not a `cd dir && ./app` list — otherwise `$!` is
the subshell, the signal goes to bash, and the app looks like it hung. Confirm `$!` names the executable before believing any signal result.

**Found while doing this, deliberately not fixed here.** **D8:** `Engine::Log` formats its own timestamp inline against `statistics.GetStartTime()`
while everything else uses `LogUtil::GetTimeStampString` — one rule, two implementations that agree today only because the two instants coincide.
**D9:** `Engine::IsLoggerReady` reports construction, not readiness, and has no readers since A4.

`EngineInitLevel.h` must be **named by a translation unit** to be build-verified — a header nothing includes is unchecked. A `TaskSystem`-style unit test naming `EInitLevel`, `operator|`, `HasFlag` is the cheapest guarantee, and matches the `LinkedList` lesson in `JOURNAL.md`.

---

## 4. Commit sequence

1. `style(engine): Allman formatting and include order for Engine.h/.cpp/TaskSystem.h` — the staged index, proven whitespace-only
2. `fix(log): do not wake an IO stream that does not exist` — A1
3. `fix(log): count log timestamps from engine start, not from the clock epoch` — A2
4. `feat(engine): EInitLevel so a caller states which subsystems start` — A4
5. `fix(osal): one OS application per process; examples use the engine's` — A3
6. `docs(journal): restore D3 — my correction was wrong`

---

## 5. Deliberate non-goals

| Not doing | Because |
|---|---|
| Changing task/stream scheduling or pump semantics | Task B owns it; touching it here guarantees rework |
| `OS::Application` construction guard | would assert during the interim while examples still create their own |
| A base-thread pump guard in `Engine::Run()` | `IsBaseThread()` already exists; its meaning changes in Task B |
| Rewriting citations in completed sections | they moved in the style commits; rewriting a verified record is churn, and §2 is done |
