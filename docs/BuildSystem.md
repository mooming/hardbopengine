## Build System Overview

- **Build Tools**: Compile all tools with `./Tools/BuildAllTools.sh`.
- **CMake Configuration**: Use the custom `MakeBuild` system to generate `CMakeLists.txt`. **Never edit generated CMakeLists.txt directly** – modify `.module.config`, `.project.config`, or specifier files (`dependency.txt`, `library.txt`, `include.txt`, `customCMake.txt`) and run `makebuild` to regenerate.
- **MakeBuild Details**: See `Tools/MakeBuild/README.md` for usage and extending the tool.
- **Regeneration is authoritative**: `./generate_cmake_files.sh` rewrites every generated
  `CMakeLists.txt` from scratch — it does not patch them. Anything typed into a generated file is
  deleted on the next run, quietly. Two such hand-edits existed and cost real behaviour: the
  `-D__DEBUG__` define in the root `CMakeLists.txt` (which gates `Assert`/`FatalAssert`) and the
  `CodingStandards` target in `Engine/CMakeLists.txt`. Both now come from supported inputs: the
  define from MakeBuild's own template, the target from `Engine/customCMake.txt`.
  Verify any change here by regenerating **twice** and diffing — the second pass must change nothing.
- **`customCMake.txt` caveats**: its lines are appended to the end of the generated file, after
  `add_subdirectory`, so it can define targets and set target properties but **cannot** influence
  directory variables (e.g. `CMAKE_CXX_FLAGS_*`) that children already inherited. `ParseList()`
  de-duplicates identical lines and drops empty ones, so two identical separator lines collapse
  into one — see the header note in `Engine/customCMake.txt`.
- **`__DEBUG__`**: emitted by MakeBuild's template onto the Debug flag line; `Dev` is derived from
  those Debug flags so it inherits the define, `Release` is a separate variable and does not get it.
  This is per-translation-unit state, so it is checked by counting the flag in the generated ninja
  files rather than by reading a CMakeLists: `grep -c 'D__DEBUG__' build/CMakeFiles/impl-Dev.ninja`
  reads 146 in Debug and Dev, 0 in Release.
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
