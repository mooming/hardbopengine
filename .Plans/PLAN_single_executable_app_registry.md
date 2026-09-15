# Plan — single `hbengine` executable: application registry, lifecycle, per-application memory

## 0. Confirmed goal

Owner request: *"instead of having multiple executables, it has a base executable class. Each
application has an executable class that inherits the base executable class. They register themselves
to the engine. And then the engine can select them with a simple TUI or from a commandline argument.
For instance, `hbengine run EngineTest`."*

Follow-on owner decisions, each recorded where it was made:

| # | Decision | Basis |
|---|---|---|
| 1 | Registration by **explicit call chain**, not C++ static initialisers | [basic.start.dynamic]: running a dynamic initialiser before `main` is *implementation-defined or deferred*, and deferral is triggered by a non-initialisation odr-use **in the same TU** — a registrar TU exports none. Whether a linker pulls an unreferenced archive member is outside the standard. Measured: app inside a static archive → registry count `0`, no diagnostic. |
| 2 | **Static** applications, not `dlopen` plugins | Measured at 100 apps: static `1.83 ms` launch vs eager-plugin `50.4 ms`; 1 artifact vs 101 per config. Plugin catalogue requires loading every image. Plugin seam preserved via `ApplicationDescriptor`. |
| 3 | Descriptor referenced as **data**, not called as a function | Measured at 100 apps: 1424 KB vs 1408 KB RSS; startup then never *executes* application code. |
| 4 | Catalogue **generated** from one `application =` key | A hand-maintained manifest is the one thing that does not scale past a handful of apps. |
| 5 | Lifecycle **gains** `Initialize`/`Shutdown`, states, `Pause`/`Resume`, and per-application threading as a descriptor property | owner request |
| 6 | Per-application **private arena** with memory status; **containment contract, not owner-tagged allocations** | owner chose M2-only. Coverage in §5. |
| 7 | `hbengine` is the directory name too | `build.sh`/`run.sh` derive the CMake target from the directory basename |

Naming follows the tree as it stands after `a783a45` (PascalCase functions).

**Split into two executable tasks (2026-09-14).** §1 is executed by
[`PLAN_defect_board_partial_init.md`](PLAN_defect_board_partial_init.md) (Task A — bug fixing, owns the
authoritative `EInitLevel`/D5/D6 spec). Task scheduling and `Engine::Run()` semantics are owned by
[`PLAN_task_system_refactor.md`](PLAN_task_system_refactor.md) (Task B). What remains here — steps 3–9 —
does not depend on either, and the seam between them is documented in Task B §1.

## 1. Step 1 — `EInitLevel`, and teardown that respects it

> **Spec moved.** Task A owns this section now; treat the text below as a summary and implement from
> there. Two citations here were stale after the `a499508` style commit and are corrected below.

`EInitLevel` goes in its own header, `Engine/Engine/EngineInitLevel.h`, included by `Engine.h`
(`Engine.h` pulls `MemoryManager.h`, `TaskSystem.h`, `Logger.h`, `ResourceManager.h`; a descriptor
must not drag those into the registry module and every application).

```cpp
enum class EInitLevel : uint8_t { None = 0, TaskSystem = 1<<0, Logger = 1<<1, Application = 1<<2, All = 7 };
constexpr EInitLevel operator|(EInitLevel, EInitLevel) noexcept;   // All pinned by static_assert
constexpr bool HasFlag(EInitLevel value, EInitLevel flag) noexcept;

void Initialize(int argc, const char* argv[], EInitLevel levels = EInitLevel::All);
```

One method, defaulted parameter — one code path, no overload pair. `Logger` implies `TaskSystem`,
enforced in `Initialize` rather than documented. No new state: `isTaskSystemReady`, `isLoggerReady`
and `application != nullptr` already say what started, so the level cannot drift from reality.

| Currently unconditional | Becomes |
|---|---|
| `Engine::Run()` → `FatalAssert(application != nullptr); application.reset();` | reset only when one exists; creation failure already asserted in `Initialize` |
| `Engine::ShutDown()` → `taskSystem.RequestShutDown()` | guarded by `isTaskSystemReady` |
| `SignalHandler()` → `logger.StopTask(taskSystem)` | guarded by `isLoggerReady`; otherwise synchronous flush |

`TaskSystem::IsBaseThread()` **already exists** and is public (`TaskSystem.h:51`, impl
`TaskSystem.cpp:56`, already called at `:232`) — there is nothing to expose, and `baseTaskThreadID` is
captured at construction (`TaskSystem.cpp:70`). The intended guard on `Engine::Run()` is **dropped**:
`Engine::Run()` blocks until shutdown rather than pumping per frame (§6), and the meaning of "base
thread" is replaced by named streams in Task B. Adding a guard on an identity about to be removed
would be work done twice.

**Step 1 also carries D5 and D6** (owner decision, 2026-09-13: both ride with `EInitLevel`, so the
defect board is closed by the step that needs the contract rather than by two isolated patches).

| Defect | What step 1 must do |
|---|---|
| **D6** — `Logger::AddLog` ends with `taskSystem.GetIOTaskStream().WakeUp()` (`Logger.cpp:392-393`), `GetIOTaskStream()` is `streams[1]` (`TaskSystem.h:27`, `IOStreamIndex = 1`) and `Array::operator[]` FatalAsserts with no message (`Container/Array.h:84-86`), so logging before `Engine::Initialize` aborts | Do not wake a stream that does not exist. This is the same guard `EInitLevel` needs, which is why they belong together - and it is the concrete reason "Logger implies TaskSystem" is **enforced in `Initialize`**, not documented |
| **D5** - every log timestamp is machine uptime, not engine uptime: `steady_clock::now().time_since_epoch()` measured 1222 h 28 m against `uptime` 50 days 22:28. `LogUtil.cpp:13` returns a function-local static nobody assigns, `LogUtil.h:19` returns it `const&` so it cannot be assigned through, and `Logger.cpp:103` calls it and drops the result | Give the timestamp a real zero point - value that static on first use, or take the baseline from `SystemStatistics::GetStartTime()`, which already records one - and decide there whether the formatter should zero-pad, since it currently prints `1:2:3.45`. Changes the first field of every log line, so it is a decision and not a repair |

## 2. Step 2 — make logging legal at every level (two live defects)

| Defect | Evidence | Fix |
|---|---|---|
| `Logger::Flush()` spins on `needFlush`; `addLog()` sets it and only the IO task clears it; `addLog()` calls `Flush()` unconditionally on `FatalError` | `Logger.cpp` ~335 / ~430 | drain inline when the drain task is not running. Today a `FatalError` with no logger task **hangs forever** |
| `FallbackLog()` calls `Engine::Get()`, which asserts a non-null instance | reachable exactly when no `Engine` exists | print `[category][level] text` to `cout`/`cerr` without touching `Engine`. Today: assert in Debug/Dev, null deref in Release |

Step 2 lands **before** step 5 because `MultiPoolAllocator::Deallocate` raises `OutFatalError` on a
foreign pointer, and that path flushes — an arena misroute under a worker-thread application would
otherwise hang instead of reporting.

**Done: `95053e5` and `5677fce`, with `a499508` and `bc16916` carrying the formatting.** Both defects
reproduced against HEAD before being fixed (D1 killed at 25 s; D2 died of SIGSEGV with no diagnostic,
not the assert previously assumed), and `WaitForFlush` is now one bounded loop with a single-drainer
guard, because two waiting threads with no drain task both reach the inline path. The 1000 ms give-up
branch is **not covered by execution** — it needs a live drain task that stops making progress.

Two more defects surfaced while reproducing these — **D5**, log timestamps counting from the clock
epoch rather than engine start, and **D6**, `AddLog` FatalAsserting when it wakes an IO stream that
`Engine::Initialize` has not created yet. Neither is fixed here: by owner decision on 2026-09-13 both
ride with **step 1**, whose contract D6 is the reason for. They are specified once, in §1, rather than
copied here as well.

## 3. Step 3 — new engine module `Engine/Application/`

`hbe::Component` is deliberately **not** the base: it mandates `Update(float deltaTime)` (no frame
clock in an application), stores `name` as an engine-allocated `String`, and exposes a **public**
`SetState` with a plain non-atomic field. The vocabulary is shared and the mapping is documented:
Created↔BORN, Running↔ALIVE, Paused↔SLEEP, Stopped/ShutDown↔DEAD.

| Header | Contract |
|---|---|
| `ApplicationState.h` | `EApplicationState { Created, Initializing, Ready, Running, Paused, Stopping, Stopped, ShutDown, Failed }`; `EApplicationRequest { None, Pause, Resume, Shutdown }` + `HasRequest` |
| `ApplicationControl.h` | **Two atomics, deliberately**: the host owns *requests*, the application owns *state*; collapsing them is how "who set my state" bugs are born. `Transition(from,to)` is compare-and-set; `WaitForChange()` parks on a condition variable so no application ever polls. Pause therefore reads truthfully as `state=Running, request=Pause` until the application parks |
| `EngineApplication.h` | `virtual bool Initialize(Engine&, const CommandLineArguments&)` (default true) · `virtual int Run() = 0` · `virtual void Shutdown()` — called exactly once, on the host thread, after the instance's thread is joined, **including after failure**. `GetState()`, `GetStatus()`, `IsFailed()`; host→app `RequestPause/RequestResume/RequestShutdown`; app-side protected `IsPauseRequested/IsShutdownRequested/WaitWhilePaused/TryTransition`, `GetEngine()`, `GetArguments()`. Not `noexcept`. Non-copyable, non-movable. The name lives only in the descriptor |
| `ApplicationDescriptor.h` | `{ name, description, requiredLevels, runMode, memoryCapacity, factory }`, `TFactory = EngineApplication* (*)()` — `const char*` and a plain function pointer so a descriptor needs no dynamic initialisation and no engine allocator. `ERunMode { HostThread, WorkerThread }` |
| `ApplicationRegistry.h` | `Get()` function-local static; `Register()` false on null **or duplicate name**; `Find`, `GetItems`, `IsEmpty`, `PrintList`. `std::vector` storage: `Register` runs before any `Engine` exists. `hbe::HVector` would *not* assert there — `DefaultAllocator` captures `GetScopedAllocatorID()`, which starts at `0` == `SystemAllocatorID` (`MemoryManager.cpp:22`, `MemoryManager.h:49`), so `allocate()` fast-paths straight to `malloc` (`DefaultAllocator.h:44-47`). The decision is unchanged, the reason is allocator **invisibility**, not a crash: registry memory allocated that way is untracked and would not appear in `hbengine memory` |

Tests (the module is a static library precisely so `EngineTest` can link it): duplicate rejection,
find, registration order, legal/illegal transitions, request acknowledge, wait-then-wake, and logging
before any `Engine` exists.

## 4. Step 4 — per-application arena and memory status (containment contract)

```cpp
struct ApplicationMemoryStats final
{ size_t capacity, used, peak, allocCount, deallocCount, fallthroughCount; };

class EngineApplication
{
protected:
	[[nodiscard]] TAllocatorID GetAllocatorID() const noexcept;   // 0 = system allocator
	[[nodiscard]] ApplicationMemoryStats GetMemoryStats() const noexcept;
	[[nodiscard]] ScratchScope CreateScratch(const char* phaseName, size_t capacity);
private:
	ApplicationMemory memory;   // owns a MultiPoolAllocator + its RegisterAllocator/Deregister pair
};
```

- `descriptor.memoryCapacity == 0` is the **default**: the application allocates from the system
  allocator and nothing about today's behaviour changes. An application opts in to a private arena.
- The host enters `AllocatorScope(app->GetAllocatorID())` around **`Initialize`, `Run`, `Shutdown` and
  the deletion of the instance itself** — one scope spanning the instance's whole life, which is what
  makes the containment contract enforceable rather than aspirational. On a worker thread the
  trampoline opens it (needed regardless: `MemoryManager::scopedAllocatorID` is `thread_local`).
- `CreateScratch` is where a **stack** allocator belongs: LIFO discipline is the *scope's* contract,
  not an application's whole life. `StackAllocator` asserts on non-LIFO release, and `hbe::Vector`
  growth frees the old buffer after allocating the new one — a stack allocator as an application
  default would assert on first contact with an engine container.
- `hbengine memory` prints one row per application; `Engine::ShutDown` keeps `PrintAllocatorProfiles`.
- Budget: `MaxNumAllocators = 256`, two slots per application (arena + scratch) → ~125 applications.

**Accepted residual risk** (stated because it was the reason M1 was proposed):

| Misrouted free | Covered? |
|---|---|
| foreign pointer → `PoolAllocator` | yes: forwarded to its parent (`PoolAllocator.cpp:193`) |
| foreign pointer → `MultiPoolAllocator` | yes: `OutFatalError` + bank dump, **no free performed** |
| arena pointer freed under a **wider** scope (`free()` on a pooled block) | **no** — corruption. Closed *by construction* (the host-held scope covers destruction) and made visible by a Debug tripwire: non-zero live blocks at arena teardown logs and asserts and **names the application** |

Escape freed into another application's arena *before* teardown is not detected (the live count has
already dropped). That is the remaining hole, it is a direct consequence of declining owner-tagged
allocations, and `docs/MemoryManagement_Guide.md` will state it next to the `AllocatorScope` rules.

## 5. Step 5 — MakeBuild learns `application =` (submodule)

`Applications/hbengine/.module.config` owns the one list:

```
name = hbengine
buildType = Executable
dependency = Application;Config;Container;Core;External;HEngine;HSTL;Log;Math;Memory;OSAL;Renderer;Resource;String;Test
application = EngineTest;SpvHeaderGen;VulkanExample;WindowExample
```

Per host module carrying the key, MakeBuild (a) links each application name as a dependency too, so
one key drives both, and (b) writes `<host>/ApplicationCatalogue.def` — banner `GENERATED BY
MAKEBUILD - DO NOT EDIT` — containing **bare names only**:

```
HBE_APPLICATION(EngineTest)
HBE_APPLICATION(SpvHeaderGen)
…
```

Bare names keep the build tool out of C++ business: the `hbe::` namespace, `ApplicationDescriptor` and
the `##ApplicationDescriptor` suffix live in three hand-written lines in the host manifest, which the
compiler checks. A wrong name is a **compile error**, never a silent gap in `hbengine list`.
Re-run MakeBuild's TestCases and revert the `.txt`→`.module.config` fixture churn out of the commit;
submodule commit first, gitlink bump second, no push.

## 6. Step 6 — host `Applications/hbengine/`

`Main.cpp` (dispatch, no application knowledge) · `ApplicationRegistration.cpp` (X-macro expanded
twice over the generated `.def`) · `ApplicationCatalogue.def` (generated, committed) · `.module.config`.

```cpp
void hbe::RegisterBundledApplications()
{
	auto& registry = ApplicationRegistry::Get();
#define HBE_APPLICATION(Name) registry.Register(&hbe::Name##ApplicationDescriptor);
#include "ApplicationCatalogue.def"
#undef HBE_APPLICATION
}
```

CLI: `hbengine list [--memory]` · `hbengine run <App> [args…]` · `hbengine memory` · `hbengine help` ·
no args → TUI picker with pause/resume/stop on running applications. Interactivity is detected by
attempting the read: a non-tty gives EOF → usage, exit `2`, so CI can never block on a prompt.

**`Engine::Run()` is not a pump.** It blocks until shutdown is requested — it spins on
`taskSystem.IsRunning()` (set at `TaskSystem.cpp:271`, cleared by `RequestShutDown()` at `:101`), then
joins and resets the OS application. Only `EngineTest/TestMain.cpp:15` calls it today; the two windowed
examples never do, running their own event loops before `ShutDown()`. So "pump `Engine::Run()`, *then*
`RequestShutdown()`" on one thread **deadlocks**, and a `HostThread` applet that owns its loop cannot
share that thread with the pump. The host must pick one of the two **before** `Initialize`:

| Selected apps | Host behaviour |
|---|---|
| any `HostThread` app | the applet owns the host base thread and runs its own loop; the host does **not** call `Engine::Run()` for it |
| all `WorkerThread` | host starts each on its dedicated thread, then calls `Engine::Run()`, with `RequestShutdown()` arriving from the TUI or signal path |

Launch order: `RegisterBundledApplications()` → `levels = OR of selected requiredLevels` →
`Initialize()` × N **sequentially on the host thread** → start (`HostThread` inline on the host base
thread, `WorkerThread` on a dedicated thread inside its allocator scope) → *either* the applet's own
loop *or* `Engine::Run()` per the table → `RequestShutdown()` → join → `Shutdown()` × N sequentially →
instance deletion **under the application's scope** → arena teardown with the live-block tripwire →
`Engine::ShutDown()`.

**Transitional — not the target design.** Both rows describe `Engine::Run()`'s *current* blocking semantics. When Task B lands **B6b** the pump owns the frame and applets are ticked `TaskProvider`s rather than loop-owners, which makes this table obsolete. The interim target for steps 6/7 is **B6a**, where an applet's loop calls the frame tick. See [`PLAN_task_system_refactor.md`](PLAN_task_system_refactor.md) §2.1–2.2.

Refused before anything is created, with a message: `WorkerThread` + `EInitLevel::Application` (Cocoa
requires the main thread for `NSApplication`/windows); two `HostThread` applications at once; a
duplicate descriptor name. Exit status: `0` iff every application stopped with `0`, else the first
non-zero.

## 7. Step 7 — convert the three applications

`Applications/<Name>/.module.config` `Executable` → `StaticLibrary`; entry point →
`<Name>Application.h/.cpp` (`Main.cpp` for the two examples, **`TestMain.cpp` for `EngineTest`**);
`main()` body → `Initialize`/`Run`; descriptor with `memoryCapacity = 0` initially.

| Application | levels | runMode |
|---|---|---|
| `EngineTest` | `TaskSystem \| Logger` | `HostThread` |
| `WindowExample`, `VulkanExample` | `All` | `HostThread` |

**Two commits per application** — reformat in place, then convert — because the post-revert app mains
are back in pre-Allman style and the formatter would otherwise bury the semantic change. Verified
during conversion, not assumed: whether `EngineTest` truly runs without the `Application` level;
**whether `EngineTest` still needs `Engine::Run()`** — its `TestMain.cpp:15` calls it today, but as a
`HostThread` applet it owns the host base thread and the host does not pump it (§6), so check whether
`ProcessMainThreadTasks` still gets serviced or the queued work is stranded;
`Engine::Initialize` **already** creates an `OS::Application` that both app mains duplicate, so apps
switch to `engine.GetApplication()`; `VulkanExample`'s `SIGINT`/`SIGTERM` handlers vs the ones
`Engine`'s constructor installs, with one owner and the ordering recorded in the app header;
`VulkanExample/Main.cpp:106` flushes logs by hand every frame (`Logger::Get().Flush()`) — carry it over
only if it is still needed after B6a makes flush an engine-ticked system, and record the reason either
way;
`(const char**)argv` becomes one `const_cast` in the host with the contract on `Initialize`.

## 8. Step 8 — tooling, scripts, docs

`check.sh` `TARGETS=(EngineTest VulkanExample WindowExample)` → `(hbengine)` and its `--test` path
(`hbengine run EngineTest`); `SKILL.md` gate wording; `run.sh`/`run.bat` pass arguments through
(they currently drop everything after the target); `AGENTS.md`, `docs/RunningTests.md`,
`docs/HelperScript.md`, `docs/BuildSystem.md`, `docs/MemoryManagement_Guide.md`, `README.md`.
New gate: `./generate_cmake_files.sh` then `git diff --exit-code` — one check for the CMake graph
**and** the catalogue.

## 9. Step 9 — the SPIR-V tool (original request, on the new structure)

`Applications/SpvHeaderGen/` — `levels = TaskSystem | Logger`, `runMode = HostThread`,
`memoryCapacity = 0`; CLI `hbengine run SpvHeaderGen --out <hdr> --var NAME=<file.spv>`;
`Scripts/GenerateSpvHeader.sh` / `.bat`; `gen_spv_header.py` deleted with both doc references moved in
the same commit. Acceptance gate: regenerating from `MC.vert.spv`/`MC.frag.spv` reproduces the
committed `ShadersSpv.h` **byte-for-byte except the banner line**, and idempotently.

## 10. Verification matrix

| Check | Method | Pass |
|---|---|---|
| Baseline preserved | 3-config build; `hbengine run EngineTest` ×3 | 53/53 each, matching today |
| Partial init safe | test logs before any `Engine`; `EngineTest` at `TaskSystem\|Logger` | no crash, no hang, exit 0 |
| State machine | unit tests: transitions, acknowledge, wait-then-wake, worker-thread fake app | all pass |
| Launch validation | `run` a windowed worker app; two host-thread apps | refused, exit `2` |
| Catalogue complete | `hbengine list` vs the `application =` key | one row each |
| Registration real | `nm` shows each descriptor symbol in the image | present for all |
| Generated in sync | regenerate → `git diff --exit-code` | empty |
| Non-interactive | `hbengine < /dev/null` | usage, exit `2`, never blocks |
| Arena containment | app with `memoryCapacity > 0` exercising containers; tripwire armed | zero live blocks at teardown |
| Tool output | `sha256` of regenerated header, twice | identical, banner excepted |
| Style + build | `check.sh --staged --apply` then `--staged`, 3 configs | exit `0` |

## 11. Accepted costs

App modules become static libraries (`lib/<Config>/lib<Name>.a` artifacts); adding an application
relinks the host (~0.19 s measured at 100 apps); `hbengine` always links Vulkan/Cocoa (already true —
`EngineTest` links `Renderer`, whose `customCMake.txt` makes a missing Vulkan loader `FATAL_ERROR`);
**no application uses `WorkerThread` yet**, so that path ships with tests as its only user and
`HostThread` stays the default; interactive TUI control works only for `WorkerThread` apps; the arena
corruption direction in §4 is closed by construction and by a Debug tripwire, not by the allocator.

## 12. Commit sequence

1. `feat(engine): EInitLevel with level-safe teardown; expose TaskSystem base thread`
2. `fix(log): inline drain without a drain task; fallback log independent of Engine` — **done** (`5677fce`, `95053e5`, plus two style commits split and hash-proved whitespace-only); `build(core,resource): declare the OSAL dependency` done as `18f05e0`
3. `feat(application): lifecycle, control, registry and tests`
4. `feat(memory): per-application arenas under a containment contract`
5. `build(makebuild): application = key emits the host catalogue` *(submodule + gitlink)*
6. `feat(applications): single hbengine host with generated catalogue`
7. ×3 `style(applications): …` then ×3 `refactor(applications): … becomes a registered application`
8. `docs,scripts,gate: one executable per tree`
9. `feat(tools): SpvHeaderGen replaces gen_spv_header.py` + `docs` / `JOURNAL.md`

Already committed and independent: submodule `39ccfae` (template emits `__DEBUG__`) and superproject
`486649a` (regeneration made lossless) — both verified: `regen twice → no change`, `-D__DEBUG__` on
146 compile lines in Debug/Dev and 0 in Release, `EngineTest` 53/53 ×3.
