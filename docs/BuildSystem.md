## Build System Overview

- **Build Tools**: Compile all tools with `./Tools/BuildAllTools.sh`.
- **CMake Configuration**: Use the custom `MakeBuild` system to generate `CMakeLists.txt`. **Never edit generated CMakeLists.txt directly** – modify `.module.config`, `.project.config`, or specifier files (`dependency.txt`, `library.txt`, `include.txt`, `customCMake.txt`) and run `makebuild` to regenerate.
- **MakeBuild Details**: See `Tools/MakeBuild/README.md` for usage and extending the tool.
- **Regeneration is authoritative**: `./generate_cmake_files.sh` rewrites every generated
  `CMakeLists.txt` from scratch — it does not patch them. Anything typed into a generated file is
  deleted on the next run, quietly. Two such hand-edits existed and cost real behaviour: the
  `-D__DEBUG__` define in the root `CMakeLists.txt` (a macro since replaced by the
  `DEBUG_BUILD`/`DEV_BUILD`/`RELEASE_BUILD` trio) and the `CodingStandards` target in
  `Engine/CMakeLists.txt`. Both now come from supported inputs: the macros from MakeBuild's own
  template, the target from `Engine/customCMake.txt`.
  Verify any change here by regenerating **twice** and diffing — the second pass must change nothing.
- **`customCMake.txt` caveats**: its lines are appended to the end of the generated file, after
  `add_subdirectory`, so it can define targets and set target properties but **cannot** reach a
  subdirectory: measured on CMake 4.3, a postlude `add_compile_definitions` lands on the directory's
  own targets and on no child, and a postlude `set (CMAKE_CXX_FLAGS_DEBUG …)` lands on that file's
  Debug flags only — a child takes its variable snapshot at `add_subdirectory`, and a file's Dev
  flags are taken at its own top. The block cannot move earlier either: `Engine/Renderer` needs the
  `Renderer` target to exist first, `Engine` reads `${PLATFORM_SOURCES}` which the template sets
  above. `ParseList()` de-duplicates identical lines and drops empty ones, so two identical
  separator lines collapse into one — see the header note in `Engine/customCMake.txt`.
- **Per-configuration definitions**: `precompileDefinitionsDebug`, `precompileDefinitionsDev` and
  `precompileDefinitionsRelease` in `.project.config` are emitted by MakeBuild's template as `-D`
  tokens onto `CMAKE_CXX_FLAGS_DEBUG`, `CMAKE_CXX_FLAGS_DEV` and `CMAKE_CXX_FLAGS_RELEASE` of
  **every** generated file, which is what makes them reach every module. Plain
  `precompileDefinitions` is an `add_compile_definitions` and so belongs to all three
  configurations. Dev reads its own variable, not Debug's: the two lists are independent.
- **Build macros**: MakeBuild's template writes one macro per configuration onto that configuration's
  own flag line — `DEBUG_BUILD=1` on `CMAKE_CXX_FLAGS_DEBUG` (`-g -O0`), `DEV_BUILD=1` on
  `CMAKE_CXX_FLAGS_DEV` (`-g -O1`), `RELEASE_BUILD=1` on `CMAKE_CXX_FLAGS_RELEASE` (`-O3`). Each line
  reads only its own variable, so nothing is inherited between configurations. Source asks for
  "not the shipping build" with `#if !RELEASE_BUILD`, which is Debug **and** Dev — that is the guard
  `Assert` lives under, and `FatalAssert` sits outside it. Why each line is independent and what the
  old `__DEBUG__` macro cost is `docs/design/BuildMacros_Design.html`. These are
  per-translation-unit state, so they are checked by counting the flag in the generated ninja files
  rather than by reading a CMakeLists: `grep -c 'DDEV_BUILD'
  build/CMakeFiles/impl-Dev.ninja` reads 151 lines, and `grep -c 'DRELEASE_BUILD'
  build/CMakeFiles/impl-Debug.ninja` reads 0.
- **`TEST_ENABLED`**: given to Debug and Dev by `precompileDefinitionsDebug` and
  `precompileDefinitionsDev`; `precompileDefinitionsRelease` is deliberately left empty. It gates
  each module's own test bodies, so a definition reaching only `Applications/EngineTest` compiles
  `TestMain.cpp`'s test half and then fails to link — that is why this is a tree-wide
  `.project.config` input and not a `customCMake.txt`. The visible consequence is that
  `./build.sh Applications/EngineTest -dev -debug` runs the suite with no `-test`, while Release
  still builds a binary that reports it contains none; `build.sh -test` stays the only route that
  puts the macro into Release. `docs/design/UnitTestSelection_Design.html` owns why the bodies are
  compiled per module. Counted the same way:
  `grep -o 'DTEST_ENABLED' build/CMakeFiles/impl-Debug.ninja` reads 455, Dev reads 455, and Release
  reads 4 — and those four are `Engine/Test`'s own sources, because its `.module.config` carries a
  plain `precompileDefinitions = TEST_ENABLED` that no configuration key limits. So in Release `libTest.a`
  is compiled with the macro while the engine modules are not — which links only because
  `TestMain.cpp`'s `#else` branch never names `Test::RegisterSuite`, so the linker never pulls that
  object in. Do not make Release's `EngineTest` reference the suite by hand. There used to be two names
  before this one: `__UNIT_TEST__` was defined alongside `__TEST__` everywhere and read by no source, so it was
  removed, and `__TEST__` itself was then renamed to `TEST_ENABLED` because a leading double underscore is reserved
  to the C++ implementation.
- **Common Configurations**: Debug, Dev (default), Release.
  ```bash
  cmake --fresh -B build -G "Ninja Multi-Config" -S .
  ```
- **Build Targets**:
  ```bash
  cmake --build build --config Debug   # Debug build
  cmake --build build --config Dev     # Development (default)
  cmake --build build --config Release# Release build
  ```
