## Helper Build Script (`build.sh`)

`build.sh` simplifies building individual targets with optional flags.

### Usage
```bash
./build.sh <target> [-dev] [-debug] [-release] [-clean] [-test]
```
- `<target>` – path to the target; the basename becomes the CMake target name
  (e.g. `Applications/VulkanExample` → `VulkanExample`).
- `-dev` – build with Dev configuration (default if no config flag is given).
- `-debug` – also build Debug configuration.
- `-release` – also build Release configuration.
- `-clean` – clean the target before building.
- `-test` – reconfigure with `-D__TEST__ -D__UNIT_TEST__` so the unit-test sources
  actually compile. Without it `TestMain.cpp` compiles to an empty `main`, so
  `EngineTest` builds green while testing nothing.

### Notes
- `build.sh` only **builds**. It never runs tests. To execute the suite, use
  `run.sh <target>` or invoke the binary directly:
  `./build/Applications/EngineTest/<Config>/EngineTest`.
- It re-runs `cmake --fresh` on every invocation, so a configure happens each time.
- Because `<target>` is resolved by basename, a target that is not an application
  directory cannot be built through this script. `CodingStandards` is one such
  target; build it directly:
  `cmake --build build --config Dev --target CodingStandards`.

### Example
```bash
./build.sh Applications/VulkanExample -dev -debug -release -clean
./build.sh Applications/EngineTest -dev -test
```
The first builds VulkanExample for Dev, Debug and Release after cleaning. The
second builds EngineTest with the unit-test macros enabled.
