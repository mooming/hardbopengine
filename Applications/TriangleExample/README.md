# Triangle Example

This application demonstrates a basic rendering of a triangle using the HardBop Engine.

## Build
Use the provided `build.sh` script or the standard CMake commands:
```bash
./build.sh Applications/TriangleExample -dev -debug -release
```

## Run
After building, execute the binary located in the appropriate configuration folder:
```bash
./build/Applications/TriangleExample/<Config>/TriangleExample [options]
```
Replace `<Config>` with `Debug`, `Dev`, or `Release`.

### Options
- `--max-fps=<N>` – caps the frame rate to *N* frames per second (default = 60).
- `--headless` – runs the demo without creating a window (useful for automated testing).
