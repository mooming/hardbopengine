# PLAN: Marching Cubes core + interactive example (Phases 6–7)

Status: **approved and in execution** — decisions settled 2026-09-01 (§4), Phase 6 under way.
Parent plan: `.Plans/PLAN_real_vulkan_renderer.md`.

---

## 1. Recon findings (verified in tree, not assumed)

| # | Finding | Evidence | Consequence |
|---|---------|----------|-------------|
| F1 | **No input API exists.** | `grep KeyDown\|KeyState\|KeyCode\|mouse` over `Engine/OSAL/*.h` → empty. `Engine/OSAL/OSInputOutput.h` is a *file-I/O* test collection; the name is misleading. `OS::Window` exposes only Create/SetTitle/SetSize/SetVisible/PollEvents/Close/Get*/IsVisible/IsClosed/GetNativeHandle. | ASDW + mouse-look + space-to-dig have **no platform support to build on**. Input must be created in this phase or the demo cannot be interactive. → **D1** |
| F2 | **Unit-test failure is real, but only through Error logs.** `TestCollection` installs a `LogFlush(…, ELogLevel::Error, &errorMessages)` and sets `isSuccess = errorMessages.empty()` (`Engine/Test/TestCollection.cpp:20,64`); per-test PASS/FAIL compares an error cursor (`:120`). `Assert()` is compiled out (`__DEBUG__` undefined anywhere). | 246 existing `Assert(` sites, all inert. | MC tests must report failure with an **Error-level log**, then PASS/FAIL is truthful — no need to flip the global `__DEBUG__` flag. → **D5** |
| F3 | No noise/Perlin anywhere. | `grep -rl Perlin\|Simplex\|ValueNoise Engine/ Applications/` → empty. | Noise is ours to write, and must be deterministic to be testable. → **D4** |
| F4 | `Math::Matrix4x4` is **row-major** (`TNumber m[row][column]`); the shader/push path expects column-major. | `Engine/Math/Matrix4x4.h:41` | Keep using the app-local column-major helpers already proven in `Applications/VulkanExample/Main.cpp` (extend with a view matrix), or transpose explicitly. Do not feed `Matrix4x4::data` straight in. |
| F5 | `VulkanRenderer::SetMesh` destroys and recreates its vertex/index buffers on every call; mesh memory is host-visible and mapped directly. | `VulkanRenderer.cpp` `SetMesh` | Re-mesh on dig works today, but a full re-upload per frame is buffer churn. Coalesce to ≤1 rebuild/frame. → **D3** |
| F6 | Push-constant block is exactly 128 B (`model` + `viewProj`, measured). | `sizeof(PushConstants) == 128` | `SetView`/`SetProj` exist and are usable (combined CPU-side as today). Light direction stays a shader constant. |
| F7 | `Applications/MarchingCubes/` exists but is an empty directory (git does not track it). | `ls` | Needs `.module.config` + `Main.cpp`; the generator picks a module up from its `.module.config` (same mechanism that fixed `VulkanExample`). |
| F8 | Tests live per-module (`Engine/<Mod>/<Mod>Test.cpp`, body under `#ifdef __UNIT_TEST__`), registered in `Engine/Test/UnitTestCollection.{h,cpp}`. | `Engine/Renderer/RendererTest.cpp` | New module needs its test file + registration + dependency wiring, then `./generate_cmake_files.sh`. |

---

## 2. Phase 6 — Marching Cubes core with real tests

New static module `Engine/Voxels/` (keeps geometry generation engine-side and unit-testable; the
renderer stays free of voxel concepts, and the app converts to `Renderer::Vertex`).

| File | Responsibility |
|------|----------------|
| `.module.config` | `name=Voxels`, `buildType=StaticLibrary`, `dependency = Log, Math` |
| `PerlinNoise.h/.cpp` | Seeded, deterministic 3D gradient noise + fBm (`Octaves`, `Frequency`, `Amplitude`). Pure functions → directly testable. |
| `VoxelField.h/.cpp` | Dense node lattice `(N+1)³`, trilinear sampling, `GeneratePerlin(...)`, `AddSphere`/`SubtractSphere` (SDF-style edits used by digging), `Value(x,y,z)`, dirty tracking. |
| `MarchingCubes.h/.cpp` | Standard 256-entry edge/triangle tables, per-cell case index, edge-vertex interpolation, gradient normals (central differences, with a degenerate-normal fallback), emits `MeshResult{ positions, normals, indices }`. |
| `VoxelTest.h/.cpp` | Test collection (below). |

Table provenance: the published Marching Cubes edge/triangle tables (Lewerenz / Bourke form),
corner-bit and winding convention fixed and asserted by test.

Design choices encoded: iso-value 0 with **inside = field < 0** (SDF sign convention, so digging
subtracts); outward-facing triangles; per-vertex normals from the field gradient (facets the
surface smoothly, which is what makes the Lambert shading read as terrain).

Tests — each signals failure with an **Error-level log** so `IsSuccess()`/PASS-FAIL is honest:

| Test | Asserted property |
|------|-------------------|
| Noise determinism | Same seed → identical samples; different seed → differs somewhere |
| Noise range / continuity | \|value\| ≤ 1; adjacent-sample delta bounded (no cliffs) |
| Lattice property | Noise is 0 at integer lattice coordinates (classic Perlin signature) |
| All-outside / all-inside | 0 triangles |
| Single corner inside | Exactly 1 triangle, 3 unique vertices, positive area |
| Winding | Triangle normal points **outward** (against the field gradient) |
| Sphere is closed | Every edge shared by **exactly 2** triangles (manifold, no holes) |
| Sphere volume | Enclosed volume from the triangle soup is positive and ≈ 4/3πr³ within tolerance |
| Normals | Unit length (no NaN, no zero-length) |
| Threshold monotonicity | Raising the iso-value shrinks the enclosed volume |
| Determinism | Meshing the same field twice → byte-identical output |
| Subtract edit | Field drops inside the radius, unchanged far away |

**Gate 6:** `./build.sh Applications/EngineTest -dev -test`, run it, expect the new test names
listed with FAIL = 0 — **plus a negative control**: deliberately break one assertion, confirm the
suite reports FAIL = 1, then revert. Without the negative control a green run proves nothing
(this project has a documented history of vacuous PASS).

---

## 3. Phase 7 — interactive demo

| Piece | Detail |
|-------|--------|
| Input layer | Per **D1/D2**: a small `OS::Input` surface (key-down query by key code, mouse delta, button state) implemented for macOS in `Engine/OSAL/OSXWindow.mm` behind a cross-platform header; Win32/X11 stubs return "unsupported" rather than pretending. Polled from the app after `PollEvents()`. |
| `Applications/MarchingCubes/.module.config` | `buildType = Application`, `dependency = Config, Core, Log, Math, OSAL, Renderer, Voxels` |
| `Main.cpp` | Generate a Perlin terrain field → mesh → upload → loop: poll input, fly the camera (ASDW + Q/E vertical + mouse-look), space subtracts a sphere along a DDA ray-march from the camera into the field, mark dirty, re-mesh ≤1×/frame, `SetMesh`, then `SetModel`/`SetView`/`SetProj` and `Render(dt)`. |
| Camera | Extend the existing app-local column-major helpers with a yaw/pitch view matrix (F4: do not reuse row-major `Matrix4x4::data`). |
| Logging | One-time `[MC] field=64^3 verts=… tris=… remesh=Xms`, and a WARN if a remesh exceeds a frame budget — this is the only way to see performance without a profiler. |

**Gate 7:** build clean (`-Wall -Werror`) and exit 0; run and confirm the log milestones
(`first frame presented …`, mesh stats, remesh timings) plus key/mouse events arriving; **pixel
correctness needs your eyes** — screen capture and Accessibility scripting are both blocked in my
environment, so I can only prove the log milestones, never the picture.

---

## 4. Decisions — need your answer before I start

| ID | Question | Options | My recommendation |
|----|----------|---------|-------------------|
| **D1** | Input scope | **A — full cross-platform OSAL input** (macOS + Win32 + X11) | Win32/X11 code will be written to the documented APIs but **compile-unverified**: this machine cannot build for those platforms. Say so in the commit message. |
| **D2** | Mouse-look | **(i)** relative motion + cursor warp | — |
| **D3** | Field size + re-mesh | **(a)** 64³, full re-mesh on dig | Chunking stays a recorded follow-up |
| **D4** | Noise location | **`Engine/Math/`** | Landed as `Engine/Math/PerlinNoise.{h,cpp}` |
| **D5** | Fix `__DEBUG__` no-op | **Yes** | Site still open — see D5a. `ConfigParam` has a member that exists only under `__DEBUG__`, so the define **must** be global and config-driven: a `Debug.h`-only fix is an ODR violation. (A) MakeBuild submodule, `CMakeLists.cpp:128-130`, 2 lines — recommended. (B) `build.sh` `-test` flags — stopgap, config-blind. |
| **D6** | Module name | **`Engine/Voxel`** | — |

### Findings that arrived during execution

| # | Finding | Evidence | Consequence |
|---|---------|----------|-------------|
| F9 | **`~Application` called `exit()`.** `OS::Application::~Application()` did `[NSApp terminate:]`, which calls `exit(0)`. `Engine::Run()` ends with `application.reset()`, so the process died inside a destructor and **`main` never resumed** — every statement after `Run()` in every macOS application was dead code, and `EngineTest` returned 0 no matter how many tests failed. | lldb: `exit` ← `-[NSApplication terminate:]` ← `OS::Application::~Application()` `OSXApplication.mm:34`; breakpoint on `TestMain.cpp:21` never hit | Fixed: destructor now calls `[NSApp stop:]`. Test suite is now gateable (verified exit 1 on injected failure, 0 when green). |
| F10 | **`Window::PollEvents()` is dead for applications.** It is the only place that maps "window not visible" to `closedFlag`, but apps pump `OS::Application::PollEvents()` instead. | Hook placed in `Window::PollEvents` never fired; the one in `Application::PollEvents` fired immediately | The close-button path works only because `windowShouldClose:` writes `closedFlag` directly (as built in `55009c6`). The visibility check belongs in the app-level pump, or is misleading dead code. |
| F11 | **Test stream flush convention.** `TestCollection::operator<<` drains the whole test stream into the logger at the level of the `LogFlush` given, and does **not** clear it. Writing `ls << "text"` alone emits nothing; two flushes in one test duplicate the text. | `TestCollection.cpp` `operator<<`; `AABB.cpp:22` uses `ls << bbox << lf;` | Each test flushes exactly once, at Error level on failure so the measured numbers travel with the reason. Tests that only do `ls << "…"` (e.g. `MathUtil.cpp`'s `2^10`) have always been silent. |

## 5. Out of scope (recorded, not silently skipped)

Descriptor-set/app-controlled lighting (blocked by the full 128 B push budget) · swapchain
recreation on resize · device-local + staging uploads · `VK_EXT_debug_utils` · back-face normal
flip · sRGB swapchain · Win32/Linux surface + close-button parity · chunked/async meshing ·
global `__DEBUG__` test-integrity fix.

## 6. Checklist (fills in as each step is verified)

- [x] 6.0 **done out of order, because it gates everything else**: `EngineTest` exit status now
      reflects failures (F9). Verified: green = exit 0, injected failure = exit 1.
- [x] 6.1 *partially* — noise went to `Engine/Math` per D4; `Engine/Voxel` module still to create
- [x] 6.2 `PerlinNoise` (seeded, deterministic) — 5 tests, negative control proves they fail.
      Measured: 729/729 lattice points exactly 0, peak |value| 0.739, max step jump 0.0376 at
      step 0.02, all 256 samples differ across seeds.
- [ ] 6.3 `VoxelField` (lattice, trilinear, Perlin fill, sphere edits)
- [ ] 6.4 `MarchingCubes` (tables, case index, interpolation, gradient normals)
- [ ] 6.5 `VoxelTest` collection + registration + dependency wiring + regenerate CMake
- [ ] 6.6 **Gate 6** including the negative control
- [ ] 7.1 `OS::Input` surface + macOS implementation
- [ ] 7.2 `Applications/MarchingCubes` (module config, field, mesh, camera, dig)
- [ ] 7.3 **Gate 7**: clean build, run milestones, your visual confirmation
- [ ] 7.4 Journal + commits (no push without your word)
