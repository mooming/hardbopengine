# PLAN: per-configuration build macros and per-configuration precompile definitions

## Goal

Give every module a definition that belongs to one build configuration, so `__TEST__` and
`__UNIT_TEST__` compile the test bodies in Debug and Dev and not in Release — without
`build.sh -test`, without a hand-edit that `./generate_cmake_files.sh` erases, and without one
configuration silently receiving another's definitions.

Origin: "add `customCMake.txt` to `Applications/EngineTest` to define `__TEST__` and `__UNIT_TEST__`
for Debug and Dev". That placement cannot do it, measured below.

## Why `customCMake.txt` cannot carry this (measured, CMake 4.3, Ninja Multi-Config)

Probe at `/tmp/cmscope`: a root file that creates one target, calls `add_subdirectory`, and only then
runs the postlude commands a `customCMake.txt` becomes.

| Probe | Debug | Dev | Release | Reaches child module |
|---|---|---|---|---|
| `add_compile_definitions (__LATE_DIR__)` after `add_subdirectory` | yes | yes | yes | **no** |
| `set (CMAKE_CXX_FLAGS_DEBUG "… -D__LATE_FLAGS__")` after `add_subdirectory` | yes | **no** | no | **no** |
| `target_compile_definitions (Self PRIVATE $<$<CONFIG:Debug,Dev>:…>)` | yes | yes | no | no (target only) |

- Row 3 shows the conditionality was never the problem; scope is.
- Row 2 is the trap: Dev's flags are taken at the top of each generated file, so a postlude append to
  the Debug line gives Debug and silently not Dev.
- Tree-wide scope is mandatory: `nm -u lib/Dev/libTest.a` lists undefined `hbe::BufferTest`,
  `hbe::StringTest`, `hbe::VectorTest`, and `nm lib/Dev/libCore.a | grep -c TaskSystemTest` is `0`.
  Each module compiles its own test bodies behind the macro, so an executable-only definition
  compiles `TestMain.cpp`'s test half and fails to link.
- The block cannot move earlier: `Engine/Renderer/customCMake.txt` needs the `Renderer` target to
  exist for its `target_*` calls, and `Engine/customCMake.txt` reads `${PLATFORM_SOURCES}` which the
  template sets above. Serving both needs two hooks, which is a larger feature than the one chosen.

## Approach (each step an owner decision)

| Step | Decision | Effect |
|---|---|---|
| 1 | Option A: a `.project.config` key emitted onto the configuration's flag line, the channel the hardcoded `-D__DEBUG__` already used | reaches every module, one configuration |
| 2 | *"Dev shouldn't inherit Debug's definitions"* → a key per configuration, and Dev's line stops reading `${CMAKE_CXX_FLAGS_DEBUG}` | Dev's effective flags unchanged (`-g -O1`), its definitions its own |
| 3 | `precompileDefinitionsRelease` added for symmetry | trio of keys, Release's left empty here |
| 4 | `DEBUG_BUILD=1` / `DEV_BUILD=1` / `RELEASE_BUILD=1`, no `HB_` prefix (reserved for engine-specific names) | every configuration names itself; `__DEBUG__` retired |
| 5 | `#if !RELEASE_BUILD` over `#if DEBUG_BUILD || DEV_BUILD` at the 53 renamed sites | shorter, and exact, because the trio is exhaustive and mutually exclusive |
| 6 | Tool sources comment-free; reasoning in `Tools/MakeBuild/README.md` | self-documented code, prose in documents |

| File | Change |
|---|---|
| `Tools/MakeBuild/Application/MakeBuild/CMakeLists.cpp` | three flag lines, one macro each; `AddConfigDefinitions` beside `AddOptimizeLevel` |
| `Tools/MakeBuild/Application/MakeBuild/BuildConfig.{h,cpp}` | `GetValues` passthrough to `ConfigParser::GetValues` |
| `Tools/MakeBuild/Application/MakeBuild/Main.cpp`, `README.md` | help lines, plus "Configuration macros" and "Per-configuration definitions" sections |
| `.project.config` | `precompileDefinitionsDebug`/`Dev` = `__TEST__ __UNIT_TEST__`, `precompileDefinitionsRelease` empty |
| 11 engine sources | `#ifdef __DEBUG__` → `#if !RELEASE_BUILD` (53 sites), 19 comments removed, 2 wrong `#endif` labels corrected, 1 empty guarded block deleted |
| 50 docs pages + 3 new design docs | new vocabulary; `ConfigParam`/`ScopedLock`/`TaskSystem`/`VulkanRenderer` quotations re-taken from source |

## Verification

| Check | Result |
|---|---|
| Macro counts in `build/CMakeFiles/impl-<Config>.ninja` | each of the three macros on 151 translation units and only in its own configuration |
| `grep -c 'D__TEST__'` Debug / Dev / Release | 151 / 151 / **0** |
| Debug + Dev + Release build of `EngineTest` | all three link; Debug and Dev execute 375 testlets |
| Failing collections | 1 — `WindowTest : TC0.Create Window`, unchanged before and after the rename; the owner declared it a separate issue |
| Release `EngineTest` | prints "built WITHOUT `__UNIT_TEST__`", exit 1 — behaviour preserved |
| Regeneration idempotence | `./generate_cmake_files.sh` twice, second pass changed nothing (19 files) |
| `comments.py` / `blank_lines.py` / `includes.py` on the 11 sources | 0 / 0 / 0 (was 19 comment findings) |
| `htmlcheck.py` over the 67 changed and new pages | 0 pages with problems |
| `makebuild --test-run` | 3/4 — `03_external_library` fails on `test_sdk.h`, reproduced with this change stashed → pre-existing |

## Deliberate limits and open items

| Item | State |
|---|---|
| `.module.config` per-configuration keys | Not implemented. The need is tree-wide; a module wanting per-config defines writes them itself |
| `build.sh -test` | Untouched: still the only route putting both defines into Release |
| `Engine/Test/.module.config` `precompileDefinitions = __UNIT_TEST__` | Kept, so Release's `libTest.a` still compiles the suite; it links because Release's `TestMain.cpp` never names `Test::RegisterSuite` |
| `add_subdirectory (Examples)` | Dropped by regeneration: `Examples/` has no `.module.config`, so MakeBuild resolves it to `Ignored`. Pre-existing drift, owner's call |
| `Engine/Renderer/Vulkan/CMakeLists.txt` | Dead file: nothing `add_subdirectory`s it (Vulkan is in `ignoreSubdirectories`); its sources reach `Renderer` via `customCMake.txt` |
| Missing reference pages | `docs/Memory/MemoryManager`, `docs/String/StringUtil`, `docs/Container/LinkedList` — `docs_coverage.py` still blocks those headers; the three new design docs record that they do not pay this debt |
| Parent-repo commit | Not made: the tree carries another session's staged work |

## Where the reasoning lives now

`docs/design/BuildMacros_Design.html` (the trio, why each flag line is independent, what a project
may not do with `customCMake.txt`), `docs/design/UnitTestSelection_Design.html` (why the suite is
compiled per module, what a test may vary per configuration),
`docs/design/MemoryManagerInternals_Design.html` (the manager's three answer-instead-of-failing
paths), `docs/BuildSystem.md` (the commands that prove a define landed), and
`Tools/MakeBuild/README.md` (the keys the tool reads).

Branch `per-config-precompile-definitions`, one commit `34dfc7d`,
[pull request mooming/makebuilder#11](https://github.com/mooming/makebuilder/pull/11).
