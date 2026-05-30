## Build System Overview

- **Build Tools**: Compile all tools with `./Tools/BuildAllTools.sh`.
- **CMake Configuration**: Use the custom `MakeBuild` system to generate `CMakeLists.txt`. **Never edit generated CMakeLists.txt directly** – modify `.module.config`, `.project.config`, or specifier files (`dependency.txt`, `library.txt`, `include.txt`, `customCMake.txt`) and run `makebuild` to regenerate.
- **MakeBuild Details**: See `Tools/MakeBuild/README.md` for usage and extending the tool.
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
