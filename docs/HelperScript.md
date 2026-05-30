## Helper Build Script (`build.sh`)

`build.sh` simplifies building individual targets with optional flags.

### Usage
```bash
./build.sh <target> [-dev] [-debug] [-release] [-clean] [-notest]
```
- `<target>` – path to the target (e.g., `Applications/TriangleExample`).
- `-dev` – build with Dev configuration (default if no config flag is given).
- `-debug` – also build Debug configuration.
- `-release` – also build Release configuration.
- `-clean` – clean the target before building.
- `-notest` – skip running Engine tests after build.

### Example
```bash
./build.sh Applications/TriangleExample -dev -debug -release -clean -notest
```
This builds the TriangleExample target for Dev, Debug, and Release, cleans first, and skips tests.
