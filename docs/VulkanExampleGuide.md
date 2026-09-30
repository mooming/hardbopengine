## Vulkan Example (`Applications/VulkanExample`)

A rotating quad. It is the smallest program that shows the Vulkan renderer being driven from application
code: `VulkanRenderer` is constructed directly — no factory, no inheritance, no platform branch — because
this target exists to exercise the Vulkan path specifically.

```bash
./build.sh Applications/VulkanExample -dev
./build/Applications/VulkanExample/Dev/VulkanExample
```

### Program shape

| Step | Call | Note |
|---|---|---|
| 1 | `Engine::Initialize(argc, (const char**) argv)` | See [the `argv` cast](#the-argv-cast-is-not-redundant) |
| 2 | `OS::CreateWindow(title, 800, 600)` | A request, not a grant — see [extent](#the-aspect-ratio-comes-from-the-renderer) |
| 3 | `VulkanRenderer renderer; renderer.Initialize(window.get())` | Failure here must flush the logger before returning |
| 4 | `SetMesh` / `SetView` / `SetProj` | Matrices are handed over as `const float*`, 16 elements, column-major |
| 5 | loop: `PollEvents`, `SetModel`, `BeginFrame` / `Render` / `EndFrame` | `Render` takes `deltaTime` in seconds |
| 6 | `renderer.Shutdown()` → `window->Close()` → `hengine.ShutDown()` | Renderer first: it holds a device bound to the window's surface |

`SIGINT` and `SIGTERM` clear one `std::atomic<bool>`; the loop condition is that flag plus
`window->IsClosed()`, so either the window manager or a terminal can end the run.

### Matrix convention

Every `float[16]` in this example is **column-major**, element `[col * 4 + row]`, which is the layout the
GLSL `mat4` in `VulkanRenderer` expects. The helpers in the file's anonymous namespace write that layout
by index rather than through a math type, so the example carries no dependency on `Engine/Math` beyond
`DegreeToRadian`:

| Helper | Writes |
|---|---|
| `SetIdentity` | Diagonal ones at `[0] [5] [10] [15]` |
| `SetTranslation` | Translation column at `[12] [13] [14]` |
| `SetRotationY` | Rotation pair at `[0] [2] [8] [10]` |
| `SetPerspective` | Full projection, see below |

Each helper takes its matrix as a write-only parameter (`outMatrix`); the caller owns the storage.

### Projection

`SetPerspective` builds a right-handed perspective mapped into **Vulkan's `[0, 1]` depth range and Y-down
framebuffer**, which is why it is not the OpenGL form:

| Element | Value | Why |
|---|---|---|
| `[0]` | `f / aspect` | `f = 1 / tan(fov / 2)` |
| `[5]` | `-f` | **The Y flip** — world +Y is screen up, framebuffer Y grows downward |
| `[10]` | `farZ / (nearZ - farZ)` | Vulkan depth form, not the GL `-(f+n)/(f-n)` |
| `[11]` | `-1.0f` | The perspective divide |
| `[14]` | `farZ * nearZ / (nearZ - farZ)` | Depth bias, same Vulkan form |

Everything else is zero — the matrix is `memset` first, so no element is left uninitialised by accident.

### Geometry

`MakeQuad` returns a unit-ish quad in the XY plane facing +Z: four vertices at ±1 in X and Y, `z = 0`,
normal `(0, 0, 1)`, UVs at the corners, and six indices forming two triangles (`0,1,2` then `0,2,3`).
Facing +Z with a normal of +Z matters because the shader's light direction is fixed in the compiled
SPIR-V — `VulkanRenderer`'s own header notes that moving it to a uniform buffer is still future work — so
the only way the example can show shading changing is by **rotating the quad about Y**, which sweeps the
fixed directional light across the surface.

### The aspect ratio comes from the renderer

The drawable follows the window's **content rect**, not the size that was requested of the window, so the
aspect ratio must be read back:

```cpp
const VkExtent2D extent = renderer.GetExtent();
SetPerspective(proj, DegreeToRadian(FieldOfViewDegrees),
               static_cast<float>(extent.width) / static_cast<float>(extent.height), NearZ, FarZ);
```

Using the requested 800×600 here silently skews the image by whatever the title bar and content insets
amount to, which reads as a projection bug rather than a windowing one.

### The logger is asynchronous

`Logger` writes on its own thread, so any exit path that must show diagnostics has to flush first. The
renderer-initialisation failure path calls `Logger::Get().Flush()` before printing to `std::cerr` and
returning, otherwise the renderer's own reason for failing can be dropped by process teardown and the
operator is left with only the generic message.

### The `argv` cast is not redundant

`Engine::Initialize` takes `const char* argv[]`, and `main` is portably declared with `char**`. C++
provides no implicit `char** → const char**` — the conversion is rejected outright, because it would let a
`const char*` be written into a slot the caller still treats as `char*`. So the explicit cast stays. It is
one of the few places in the tree where a C-style cast is the correct spelling, and the check is a
one-file experiment: drop the cast and the compiler refuses the call.

### Constants the loop uses

The literals the example once carried inline are named in the file's anonymous namespace:
`RequestedWindowWidth`, `RequestedWindowHeight`, `FieldOfViewDegrees`, `NearZ`, `FarZ`, `CameraZ`,
`RadiansPerSecond`, `FrameSleep`, and `MatrixElementCount` for the 16 elements every helper writes. The
rotation wraps at `TwoPi` — `Engine/Core/Constants.h`, not a local copy — so `angle` cannot drift into
float precision loss over a long run.
