# Plan — `gen_spv_header.py` → C++ tool application under `Tools/`

Confirmed goal (owner statements, in order):

1. Convert `Engine/Renderer/Vulkan/gen_spv_header.py` into a `.cpp` tool application under `Tools/`.
2. A tool must be able to **use engine modules**. A tool under `Tools/` may be either an
   external git submodule (as `Tools/MakeBuild` is) **or** an engine application.
3. Engine bring-up must be **flexible: the engine supports various initialisation levels**,
   so a tool starts only what it needs.
4. CLI: named options (`--var NAME=file.spv`, `--out header.h`), no hardcoded `MC` names.
5. Regeneration stays **manual**, but with **per-platform scripts**.

---

## 0. Findings that shape the design (all verified in source, not assumed)

| # | Fact | Consequence |
|---|------|-------------|
| 1 | Root `.module.config` has `ignoreSubdirectories = build docs Tools Scripts`, so MakeBuild never scans `Tools/` | No tool can reference engine targets today. Wiring change is mandatory. |
| 2 | Generated CMakeLists for an `Executable` module emits `link_directories(${CMAKE_SOURCE_DIR}/lib)`, `target_link_libraries(<name> <dependency>...)`, `add_dependencies`, `install(... ${CMAKE_SOURCE_DIR}/bin)` | An `Executable` module inside the project tree links engine targets exactly like `Applications/*` does. |
| 3 | `Engine/.module.config` has `include = Engine` → `include_directories(${CMAKE_SOURCE_DIR}/Engine)` | Tool includes use the app style: `"OSAL/OSInputOutput.h"`, `"Log/Logger.h"`. |
| 4 | `MemoryManager` registers itself + the system allocator **in its constructor**, which runs as an `Engine` member | `hbe::String` / `HVector` / `Array` / `Shareable` work with a *constructed* `Engine`, no `initialize()` needed. |
| 5 | `Logger::instance` is set in `Logger`'s constructor (also an `Engine` member), but the drain task starts only in `Engine::initialize()` | Logs written without the IO task are buffered and **never written out**. |
| 6 | `addLog()` sets `needFlush = true`; only `processBuffer()` (the IO task) clears it; `Logger::flush()` from another thread spins on `needFlush` | **Existing hazard:** with no logger task running, `flush()` — and the `FatalError` path in `addLog` which calls `flush()` unconditionally — **hangs forever**. |
| 7 | `Logger::SimpleLogger::out()` with `instance == nullptr` calls `fallbackLog()`, which calls `Engine::get()` → `Assert(engineInstance != nullptr)` | **Existing hazard:** the "fallback" is reachable only when no `Engine` exists → assert in Debug/Dev, null deref in Release. Logging without an engine is impossible today. |
| 8 | `Engine::run()` ends with `fatalAssert(application != nullptr)`; `signalHandler` calls `logger.stopTask(taskSystem)` (which `task.wait()`s) | Both must become level-aware or a partial-init application hangs or aborts. |
| 9 | `Engine/Renderer/.module.config` excludes `DX12 Metal Vulkan`; `.spv` + generated header are committed | Build graph stays untouched; regeneration is an author action (owner decision #5). |
| 10 | `Tools/BuildAllTools.sh` builds *every* `Tools/<dir>` that owns a `CMakeLists.txt` | Once MakeBuild generates `Tools/SpvHeaderGen/CMakeLists.txt`, that script would try to configure it standalone and fail. Needs a discriminator. |

Discriminator chosen: **a tool that owns a `.project.config` is a standalone / submodule project;
one that does not is an engine application.** This is exactly MakeBuild's own definition of a
project root, so it is self-describing rather than a new magic list.

---

## 1. Part 1 — Engine: selectable initialisation levels

**Files:** `Engine/Engine/Engine.h`, `Engine/Engine/Engine.cpp`, `Engine/Log/Logger.h`, `Engine/Log/Logger.cpp`

1.1 Add `enum class EEngineInitLevel : uint8_t` with `None`, `TaskSystem`, `Logger`, `Application`,
`Resources`, and `All`, plus the `operator|` / `operator&` combine helpers (reuse an existing
flag macro from `Core/CommonMacros.h` if one is present — check before writing a new one).
Contract (what each bit provides, what is already up on construction alone) goes in `Engine.h`,
not the `.cpp`.

1.2 `initialize(int argc, const char* argv[], EEngineInitLevel levels = EEngineInitLevel::All)`.
One method with a defaulted parameter, not an overload pair, so `Applications/*` keep compiling
unchanged and there is one code path. Keep today's start order (TaskSystem → Logger → argv banner →
Application → `postInitialize`) and guard each step by its bit.

1.3 **No new state.** The engine already tracks `isMemoryManagerReady`, `isTaskSystemReady`,
`isLoggerReady`, `isResourceManagerReady`; `application != nullptr` covers the Application bit.
Shutdown paths read those, so the level cannot disagree with what actually started.

1.4 Level-aware teardown: `run()` skips the join loop and the application assert when the bit was
not requested; `shutDown()` skips `requestShutDown()`; `signalHandler()` stops the logger only when
it was started.

1.5 Logger correctness at partial levels — this is what makes the levels real rather than cosmetic:
- `Logger::flush()` processes the buffer inline when the drain task is not running (fixes #6).
- `fallbackLog()` writes `[category][level] text` straight to `std::cout` / `std::cerr` and never
  touches `Engine::get()` (fixes #7).
- `Logger.h` states that logging is legal at every level, including none.

1.6 **Non-goals:** no behaviour change for `All`; no re-ordering of subsystems; no new subsystems;
no changes to any existing application's source.

**Verify:** 3-configuration build of every target; `EngineTest` pass/fail counts identical to the
baseline in Debug/Dev/Release; `WindowExample` still opens and polls.

---

## 2. Part 2 — make `Tools/` a legal home for engine applications

| Step | File | Change |
|---|---|---|
| 2.1 | `.module.config` (root) | drop `Tools` from `ignoreSubdirectories` |
| 2.2 | `Tools/.module.config` (new) | `name = Tools`, `buildType = None`, `ignoreSubdirectories = MakeBuild bin build` — keeps the submodule and its `TestCases/` out of the engine project |
| 2.3 | regenerate | `./generate_cmake_files.sh` → `Tools/CMakeLists.txt` + `add_subdirectory (Tools)` in the generated root `CMakeLists.txt` |
| 2.4 | `Tools/BuildAllTools.sh` | build only tools that own a `.project.config`; print the skipped engine tools and say how to build them (`./build.sh Tools/<Tool>`) |
| 2.5 | `.pi/skills/hb-standards/scripts/check.sh` | add `SpvHeaderGen` to `TARGETS` so the standards gate actually compiles the new target |
| 2.6 | `docs/BuildSystem.md` | document the two tool flavours and the `.project.config` discriminator |

**Verify:** regenerated root/`Tools` CMakeLists reviewed; `cmake --fresh` configures clean;
`Tools/MakeBuild` appears nowhere in the engine CMake graph (`grep -c MakeBuild build/CMakeFiles/*/build.ninja`
style check); `BuildAllTools.sh` still builds MakeBuild and does not attempt `SpvHeaderGen`.

---

## 3. Part 3 — the tool: `Tools/SpvHeaderGen/`

| File | Role |
|---|---|
| `.module.config` | `name = SpvHeaderGen`, `buildType = Executable`, `dependency =` the modules it truly links (start from `Core;Config;Container;HSTL;Log;Memory;OSAL;Resource;String`, then trim to the real closure) |
| `SpvHeaderGenerator.h` | contract + usage block: emits `static const unsigned char NAME[]` + `constexpr size_t NAME_size` per input, 12 bytes/line, `#pragma once`, `#include <cstddef>`, auto-generated banner. Owns the emit API: read a `.spv`, append one array to an output buffer, write once |
| `SpvHeaderGenerator.cpp` | implementation on engine modules: `OS::Open` / `FileHandle::getFileSize` / `OS::Read` / `OS::Write` / `OS::Close` (`OSAL/OSInputOutput.h`, `OSFileHandle.h`, `OSFileOpenMode.h`), bytes in `hbe::Array<uint8_t>`, text via `hbe::StringBuilder` |
| `Main.cpp` | engine at the **minimal** level (`TaskSystem | Logger` — no window application, no resource manager), option parser, diagnostics, exit status |

CLI (owner decision #4):

```
SpvHeaderGen --out <header.h> [--var <NAME>=<file.spv>]... [--quiet] [--help]
```

- `--var` is repeatable and order-preserving; NAME must be a valid C++ identifier (validated,
  rejected with a message).
- Any unknown option, missing value, unreadable input, unwritable output, or zero `--var` →
  diagnostic on stderr + usage + **non-zero exit**. Every early return logs first (standards rule).

Equivalence target: regenerating `ShadersSpv.h` from `MC.vert.spv` / `MC.frag.spv` reproduces the
committed header **byte-for-byte except the banner line** (banner names the tool, not the `.py`).
This is the acceptance test, not a smoke test.

Standards constraints honoured: line-1 copyright, no comments in `.cpp`, Allman braces, tabs,
camelCase functions (post-`4e9e373`), `[[nodiscard]]` getters, `out`/`in` parameter prefixes,
`explicit` single-arg ctors, `final` classes.

---

## 4. Part 4 — manual invocation scripts + documentation

| File | Content |
|---|---|
| `Scripts/GenerateSpvHeader.sh` | macOS/Linux: pick the config dir (`build/Tools/SpvHeaderGen/<Config>/SpvHeaderGen`, default `Dev`), default inputs = `Engine/Renderer/Vulkan/Shaders/MC.vert.spv` + `MC.frag.spv`, default output = `Engine/Renderer/Vulkan/ShadersSpv.h`; accepts argument pass-through |
| `Scripts/GenerateSpvHeader.bat` | Windows twin, `build\Tools\SpvHeaderGen\<Config>\SpvHeaderGen.exe` |
| `docs/design/LightweightRenderer_Design.md` + `.html` | line 44 still names `gen_spv_header.py` → point at the tool and the scripts |
| `Engine/Renderer/Vulkan/gen_spv_header.py` | **deleted** (conversion, and the two docs references are updated in the same commit so nothing points at a dead path) |

Scripts fail loudly when the tool has not been built yet and print the exact `./build.sh` line.

---

## 5. Part 5 — verification matrix (max 10 iterations / 15 min per protocol)

| Check | Method | Gate |
|---|---|---|
| Golden output | `sha256` of regenerated header vs. committed, ignoring line 1 | identical except banner |
| Idempotence | regenerate twice → same bytes | identical |
| Error paths | no args / unknown option / bad name / missing `.spv` / unwritable `--out` | each non-zero exit + a message |
| Partial init | tool run under a debugger-free timing check; exits, no hang, no leaked thread | exit ≤ 1s |
| Engine regression | `EngineTest` in Debug/Dev/Release with `-test` | counts equal to baseline |
| Full apps | `hb-standards` gate (9 configurations + `CodingStandards` target) | exit 0 |
| Style | `check.sh --staged --apply` then `check.sh --staged` | 0 violations |
| Submodule isolation | MakeBuild tree absent from the engine CMake graph | 0 references |

## 6. Commits

1. `feat(engine): selectable initialisation levels + level-safe logging`
2. `build(tools): allow engine-application tools under Tools/`
3. `feat(tools): SpvHeaderGen — SPIR-V to C++ header generator, replaces gen_spv_header.py`
4. `docs: tool flavours, regeneration scripts; journal`

No push. JOURNAL.md gains a timestamped entry per decision, including hazards #6 and #7 as
pre-existing defects this change fixes, and anything left open.

## 7. Open risks (stated up front, not discovered later)

- **R1** Removing `Tools` from the root ignore list lets MakeBuild walk into the submodule. Step 2.2
  is the guard; step 5's isolation check proves it rather than trusting it.
- **R2** `Engine::shutDown()` prints the static-string table, config parameters and allocator
  statistics — right for games, noise for a build tool. Resolution: the tool takes the quiet path
  (`flushLog()` + task-system shutdown) rather than the reporting path, and the plan's level API is
  what makes that legal. Decided in Part 1, verified by the tool's stdout being a single summary line.
- **R3** Link closure of engine static libraries may need extra `dependency` entries (the generated
  `target_link_libraries` has no `PUBLIC` propagation guarantees). Resolved by the first real link,
  not by guesswork.
- **R4** `Logger` uses `Config` params and `Memory` pools; a `TaskSystem | Logger` level must not
  assume anything the bit did not start. Covered by the partial-init check.
