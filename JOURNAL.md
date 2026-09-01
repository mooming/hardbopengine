# Journal

## Summary of Macro Application Task

Applied the new macros from `Engine/Core/CommonMacros.h` across the codebase:
- `returnIf(condition)` for `if (condition) return;`
- `returnValueIf(value, condition)` for `if (condition) return value;`
- `breakIf(condition)` for `if (condition) break;`
- `continueIf(condition)` for `if (condition) continue;`
- `ONCE()` macro kept in CommonMacros.h (removed duplicate from CommonUtil.h)

### Changes Made

1. **Engine/Core/CommonMacros.h**: Added `returnValueIf` macro.
2. **Engine/Core/CommonUtil.h**: Removed duplicate `ONCE` macro definition.
3. Applied macros to 34 files across Container, Math, OSAL, Resource, String, Logger, ComponentSystem, TaskSystem, etc.
4. Added `#include "Core/CommonMacros.h"` to each modified file.
5. Verified that all 51 unit tests pass.

### Lines Saved Estimate

- Approximately 103 macro applications replaced 2-line patterns with 1-line equivalents, saving ~103 lines.
- Added 34 include lines (one per modified file).
- Removed 1 duplicate macro line from CommonUtil.h.
- **Net lines saved**: 103 - 34 + 1 = **70 lines**.

### Verification

- Built and ran EngineTest in Debug configuration: all tests pass.
- No regressions introduced.

## RingQueue Performance Optimization & Quaternion Test Fix

### Changes Made

1. **Engine/Container/RingQueue.h**:
   - Added `#include <bit>` for `std::bit_ceil()`
   - Modified constructor to round capacity to next power of 2 using `std::bit_ceil()`
   - Changed `WrapIndex` from modulo (`% cap`) to bitmask (`& (cap - 1)`)
3. **Engine/Math/Quaternion.cpp**:
   - Previous expectation was mathematically invalid (rotating vector parallel to axis of rotation)

## EngineTest Performance Analysis and Improvement Solutions

### Summary
Built and ran Applications/EngineTest in Debug configuration with tests enabled. Identified performance warnings where custom implementations underperform STL equivalents.

### Key Findings
- InlinePoolAllocator: 1.27x slower than system malloc for variable-sized allocations (due to fallback overhead)
- MultiPoolAllocator: 7.7x slower than std::malloc
- ThreadSafeMultiPoolAllocator: 8.0x slower than std::malloc
- LinkedList: 2.1x slower than std::list
- String: 14x slower than std::string (due to lack of Small String Optimization)

### Recommended Solutions
1. **InlinePoolAllocator**: Document as fixed-size only; use segregated allocators for variable sizes.
2. **MultiPoolAllocator**: Optimize block management (power-of-two sizes, per-CPU caching); reduce lock contention.
3. **LinkedList**: Integrate node pooling; cache size; use sentinel node.
4. **String**: Implement Small String Optimization (SSO); exponential growth; optimize operations.

### Next Steps
- Start with String SSO for highest impact.
- Proceed to LinkedList node pooling.
- Address allocators last due to complexity.
- Create microbenchmarks to validate improvements.

Full analysis available in PerformanceAnalysis.md.

---

## Refactor: Static multi-platform (Application + Window) — Steps A & B

### Summary
Replaced runtime polymorphism with static compile-time dispatch, per user directive.

- **Step A (`ea01c87`)** — Application: dropped `IApplication` interface + factory `#ifdef` dispatch. Single concrete `OS::Application` class; per-platform impl in `LinuxApplication.cpp` / `OSXApplication.mm` / `Win32Application.cpp` (selected via `#if defined(PLATFORM_*)`). `CreateApplication()` returns `unique_ptr<Application>`. Engine owns `unique_ptr<OS::Application>`.
- **Step B (`5e9eaed`)** — Window: dropped `IWindow` interface. Single concrete `OS::Window` class with compact header (no platform includes); per-platform impl in `LinuxWindow.cpp` / `OSXWindow.mm` / `Win32Window.cpp`. `CreateWindow()` returns `unique_ptr<Window>`. OSX member `nsWindow` generalized to `osHandle`. Removed obsolete per-platform Window/Application headers.

### Verification
- `EngineTest`: **52/52 PASS, 0 FAIL** (test-config build).
- `WindowTest`: all runtime cases PASS (Create/Title/Size/Visibility/Poll Events) — concrete `Window` works end-to-end.

### Known / pre-existing (NOT a regression)
- Building with the global `-D__TEST__ -D__UNIT_TEST__` flag (the `./build.sh -test` path) pulls the test framework into *every* app via `Window.cpp`'s `WindowTest`, creating a pre-existing `OSAL ↔ Test` static-lib circular dependency. With `Test` linked last, `EngineTest` links fine but thin apps like `WindowExample` fail to link (missing `Logger`/`Memory` symbols). This is a build-ordering property orthogonal to the refactor — `WindowExample` links cleanly in a normal non-test config. No build files were changed for it (per AGENTS.md, generated CMakeLists are not hand-edited).

---

### Questions Addressed

1. **What is String SSO?**
   Small String Optimization (SSO) stores small strings (typically ≤15-22 chars) directly in the string object instead of allocating heap memory. This avoids allocation overhead for common cases and improves cache locality. Standard `std::string` implement SSO.

2. **LinkedList Node Pooling Check**
   Yes, verified: LinkedList uses a template allocator parameter (`TAllocator = DefaultAllocator<LinkedListNode<TType>>`). Unit tests specifically configure it with PoolAllocator:
   ```cpp
   PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
   AllocatorScope allocScope(alloc);
   ```
   Despite node pooling, performance issues may stem from:
   - Pointer chasing causing poor cache locality
   - Algorithm inefficiencies beyond allocation
   - Test configuration or measurement overhead

3. **MultiPoolAllocator Locking**
   Correct: 
   - `MultiPoolAllocator` (single-threaded): No mutex locks
   - `ThreadSafeMultiPoolAllocator`: Contains `std::mutex lock` for thread safety

4. **HString and SSO**
   Verified: `HString = std::basic_string<char, ..., hbe::DefaultAllocator<char>>` 
   As a typedef of `std::basic_string`, it inherits the standard library's SSO implementation (when available on the platform).
   The poor-performing string in tests was the custom `String` class (Engine/String/String.h), not HString.

### Key Clarification
The StringTest warning referred to the custom `String` class (always heap-allocated via `Shareable<Vector<TChar>>`), not HString which does benefit from std::string's SSO.

## Follow-up: Clarifications on Performance Analysis

### Questions Addressed

1. **What is String SSO?**
   Small String Optimization (SSO) stores small strings (typically ≤15-22 chars) directly in the string object instead of allocating heap memory. This avoids allocation overhead for common cases and improves cache locality. Standard `std::string` implement SSO.

2. **LinkedList Node Pooling Check**
   Yes, verified: LinkedList uses a template allocator parameter (`TAllocator = DefaultAllocator<LinkedListNode<TType>>`). Unit tests specifically configure it with PoolAllocator:
   ```cpp
   PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
   AllocatorScope allocScope(alloc);
   ```
   Despite node pooling, performance issues may stem from:
   - Pointer chasing causing poor cache locality
   - Algorithm inefficiencies beyond allocation
   - Test configuration or measurement overhead

3. **MultiPoolAllocator Locking**
   Correct: 
   - `MultiPoolAllocator` (single-threaded): No mutex locks
   - `ThreadSafeMultiPoolAllocator`: Contains `std::mutex lock` for thread safety

4. **HString and SSO**
   Verified: `HString = std::basic_string<char, ..., hbe::DefaultAllocator<char>>` 
   As a typedef of `std::basic_string`, it inherits the standard library's SSO implementation (when available on the platform).
   The poor-performing string in tests was the custom `String` class (Engine/String/String.h), not HString.

### Key Clarification
The StringTest warning referred to the custom `String` class (always heap-allocated via `Shareable<Vector<TChar>>`), not HString which does benefit from std::string's SSO.

## Vulkan Renderer: Build Wiring + First Running Frame (2026-08-31)

### Problem
The Vulkan renderer written on 2026-08-30 had **never been compiled**. `libRenderer.a`
held only `RHICapabilities.cpp.o` + `RendererTest.cpp.o`, and `VulkanExample` had no
build target at all, so the plan's per-phase "it compiles / it runs" claims were
unverified rather than true.

### Root causes (build system)
| ID | Cause | Fix |
|----|-------|-----|
| B1 | `Engine/Renderer/.module.config: ignoreSubdirectories = DX12 Metal Vulkan`. MakeBuild collects sources only from a module's own directory (`Module::CollectFiles`) and skips ignored subdirs entirely (`ProjectBuilder::TraverseDirectoryTree`), so `Vulkan/*` could never reach the Renderer target | `customCMake.txt`: `target_sources (Renderer PRIVATE Vulkan/VulkanRenderer.cpp)` (+ `.mm` under `if(APPLE)`) |
| B2 | `Applications/VulkanExample/` had **no `.module.config`** → `buildType = None` → the generator's `switch` emitted no `add_executable` | added `Applications/VulkanExample/.module.config` (Executable, deps incl. Renderer) |
| B3 | No one linked the Vulkan loader; `.mm` needs `VK_USE_PLATFORM_METAL_EXT` or `vkCreateMetalSurfaceEXT` is `#ifdef`'d out of `vulkan_metal.h` | `find_path`/`find_library` with `FATAL_ERROR` guards + `target_compile_definitions` |

**Decision:** fix through `.module.config` / `customCMake.txt` instead of hand-editing
generated `CMakeLists.txt` files, so the generated files stay machine-owned.

### Decisions and why
- **Plain `target_link_libraries`** in `customCMake.txt`: MakeBuild emits the plain form
  for module dependencies (`target_link_libraries (Renderer Log)`), and CMake forbids
  mixing plain and keyword signatures on one target. Plain items still propagate, so
  consumers get `libvulkan` + the Apple frameworks transitively.
- **`Renderer` now declares `dependency = Log`** and reports every Vulkan failure through
  `Logger` with the `VkResult`. Per AGENTS.md, graphics code must log; a silent `return
  false` cost hours of guessing. (`VulkanRenderer` logs a single "first frame presented"
  milestone and errors otherwise - no per-frame chatter.)
- **Dead setters removed** (`SetModel`, `SetLightDir`): `MC.vert`'s push block is
  `{ mat4 view; mat4 proj; }` (exactly the 128-byte portable maximum), so model/light
  direction had no path to the GPU. Re-introduce them in the MC phase together with the
  shader + uniform layout, not as no-ops.
- **Linux/Windows surfaces not faked.** Linux cannot create a surface from the current
  OSAL API (`Window::GetNativeHandle()` returns only the X11 `Window`, no `Display*`);
  Win32 is wired behind `VK_USE_PLATFORM_WIN32_KHR` but is **compile-unverified** here.
- **Mesh memory is host-visible/host-coherent** and mapped directly. Device-local +
  staging upload is the right long-term shape; recorded as a follow-up rather than
  half-built now.

### Runtime defects found only after it finally compiled
| # | Defect | Symptom |
|---|--------|---------|
| 1 | `CreateMetalSurface()` was defined in **both** `.cpp` (unguarded stub) and `.mm` | `VulkanRenderer.cpp.o` satisfied the symbol first, so `VulkanRenderer.mm.o` was **never pulled from the archive** - the real Metal surface never ran. Stub now guarded `#if !defined(PLATFORM_OSX)` |
| 2 | `VK_KHR_swapchain` never enabled on the device | loader: "Driver's function pointer was NULL" |
| 3 | `attachmentCount = 2` with `pAttachments = &colorAttach` (two separate locals) | Vulkan read attachment #1 out of adjacent stack memory → MoltenVK SIGSEGV in `getMTLPixelFormat`. Now one contiguous `VkAttachmentDescription[2]` |
| 4 | AppKit ignores `+layerClass` and installs `NSViewBackingLayer` | `setDevice:` → `doesNotRecognizeSelector` NSException. The `CAMetalLayer` is now created explicitly and adopted by the layer-backed view (measured: `view.layer class = CAMetalLayer`) |
| 5 | Fences created unsigned, 1 shared command buffer, no dynamic viewport/scissor, `vkCmdBindIndexBuffer` missing | would have deadlocked frame 0 and drawn nothing; fixed with the rewrite |

### Verification
- `Renderer` + `VulkanExample` build clean under `-Wall -Werror`, Dev config, no warnings.
- `libRenderer.a` = `RHICapabilities`, `RendererTest`, `VulkanRenderer.cpp.o`, `VulkanRenderer.mm.o`.
- Run: `first frame presented (800x568, swapchain images=3, index count=0)`; loop stable, zero loader/validation errors. Extent is 800x568 (content rect of an 800x600 titled window) and is taken from `currentExtent`, not assumed.
- `EngineTest` builds and exits 0.

### Important caveat found (pre-existing, NOT fixed)
`Assert()` in `Engine/Core/Debug.h` is live only under `__DEBUG__`, and **`__DEBUG__` is
defined nowhere in the project**. In every configuration `Assert` compiles to a no-op, so
the suite's "280 tests PASS / Fail = 0" is vacuous. Consequently `RendererTest`'s stale
stub-era expectations (`Initialize(nullptr) == true`, `supportsComputeShader == true`)
pass while actually being false. Needs a decision: define `__DEBUG__` for Debug builds
(likely lights up pre-existing failures) and rework `RendererTest`.

### Follow-ups recorded
Swapchain recreation on resize · device-local + staging mesh upload · VK_EXT_debug_utils
messenger · `ShadersSpv.h`/`*.spv` are generated but not gitignored and `gen_spv_header.py`
is run by hand · orphaned `Engine/Renderer/Vulkan/CMakeLists.txt` (no `add_subdirectory`)
still claims to build `VulkanRenderer.mm` · Linux surface needs `Display*` from OSAL.

## VulkanExample: rotating quad made real (2026-08-31)

### Problem
The window was titled "Rotating Quad" but showed a flat navy field. `Main.cpp` never
called `SetMesh`, `SetView` or `SetProj`, so `indexCount == 0` and `RecordFrame()`
correctly drew nothing. The stub-era example expected the renderer to supply geometry.

### Decision: where to put the rotation (user chose option B)
`MC.vert`'s push block was `{ mat4 view; mat4 proj; }` = 128 bytes, already the
portability-guaranteed maximum, so a model matrix had nowhere to go.

| Option | Assessment |
|--------|------------|
| A. Rotate inside the view matrix | cheapest, but the world-space normal never moves, so Lambert shading cannot respond - the demo would look static in lighting |
| **B. `mat4 model` + CPU-combined `mat4 viewProj` (chosen)** | still exactly 128 bytes, restores a meaningful `SetModel`, and the quad's shading visibly pulses as its normal sweeps the light |
| C. Descriptor set + uniform buffer | the real fix for the 128-byte ceiling and for app-controlled lighting; deferred to the Marching Cubes phase |

### Changes
- `MC.vert`: push block is now `{ mat4 model; mat4 viewProj; }`; normal uses
  `mat3(model) * aNormal`, with a comment that this is valid only while `model` carries
  rotation/translation (switch to `transpose(inverse(model))` once scaling appears).
- `MC.frag`: removed dead code (`vWorldPos` varying and a `depth` value that was
  computed and never read); light/ambient/albedo named as constants. The light direction
  stays compile-time fixed because model+viewProj consume the whole push range - stated
  in the file rather than implied by a dead setter.
- `PushConstants { model, viewProj }`; `MultiplyColumnMajor()` in the renderer does the
  one `proj * view` multiply per frame; `SetModel()` restored.
- `GetExtent()` added: the drawable follows the window *content* rect (800x568 for an
  800x600 titled window), so examples must not assume the requested size for aspect ratio.
- SPIR-V regenerated with glslangValidator; `MC.spv` renamed to `MC.vert.spv` so vertex
  and fragment outputs are named consistently; regeneration commands now documented in
  both shader files.
- `Main.cpp`: quad mesh (XY plane, normal +Z) uploaded once, RH perspective with the
  Vulkan depth/Y convention, camera at z=-3, `model = rotationY(t)` each frame.

### Verification
`first frame presented (800x568, swapchain images=3, index count=6)` - the index count
proves geometry reached the pipeline - with zero loader or validation output, and a
warning-free `-Wall -Werror` build. Pixel-level confirmation is the user's, because
`screencapture` is blocked in this environment.

## OSAL: the window close button was never observed (2026-08-31)

### Symptom
Closing the window left `VulkanExample` running forever.

### Root cause (macOS)
`Window::PollEvents()` is the only code that ever sets `closedFlag` (via
`![window isVisible]`), and **nothing calls it** - applications pump
`OS::Application::PollEvents()`, which drains and dispatches the event queue but has no
path back to a `Window`. So `IsClosed()` stayed false and every example loop ran forever.
`applicationShouldTerminateAfterLastWindowClosed` also returns NO, and there is no
`NSApp run` loop to consult it.

The same gap exists on the other platforms (not fixed here, see below).

### Fix
An `HBWindowDelegate` implementing `windowShouldClose:` marks the owning `Window` closed
the moment the button is pressed, and returns YES so AppKit proceeds. This is event-driven
rather than "poll until the window looks hidden", so it no longer depends on somebody
remembering to call `Window::PollEvents()`. `Window` owns the delegate reference (new
per-platform `osDelegate` member) because `NSWindow.delegate` is unretained; it is
released in `Close()` next to the window's own release.

### Verification (with control)
An ad-hoc ObjC++ harness created a real `OS::Window` and invoked `performClose:` - the
same path the red button takes:

| Build | `IsClosed()` before | after | |
|-------|--------------------:|------:|---|
| Without `[window setDelegate:]` (control) | 0 | **0** | reproduces the reported bug |
| With the delegate | 0 | **1** | fixed |

A synthetic UI click was not possible here: `osascript`/System Events is denied
Accessibility permission (-1719), and `screencapture` is likewise blocked.

### Same defect, not addressed (unverifiable on this machine)
- **Win32**: `WM_DESTROY` sets `shouldCloseFlag`, but `IsClosed()` returns `closedFlag`, so
  the information is recorded and then dropped. One-line fix: return
  `closedFlag || shouldCloseFlag`.
- **Linux**: `Window::PollEvents()`'s `ClientMessage` branch is an empty placeholder, so
  `WM_DELETE_WINDOW` is never handled and `WM_PROTOCOLS` is never registered. Needs a real
  atom handler (~8 lines).

## Lighting reviewed and deliberately left as-is (2026-08-31)

The rotating quad looked under-lit, so the shader was audited before changing anything.
The model is a single Lambert term - `ambient 0.25 + max(dot(n, L), 0) * albedo 0.95` -
with `n = mat3(model) * aNormal`, i.e. rotation is correctly applied to the normal.

Solving it for this geometry (all four vertices share normal (0,0,1), so the world normal
is `(sin t, 0, cos t)` and `L = (0.337, 0.842, 0.421)`):

`dot(t) = 0.337 sin t + 0.421 cos t` -> amplitude **0.539**, peak at **t = 38.7°**, output
range **0.250 .. 0.762**, and pure ambient for **50%** of the turn.

That explains the flat look completely, and the four causes are:

1. One flat normal across the whole quad -> identical colour in every fragment; lighting
   reads through spatial variation, which a lone quad cannot express.
2. The light points 84% along +Y while the normal sweeps only the XZ plane, so at most
   0.539 of it is ever usable.
3. `cullMode = VK_CULL_MODE_NONE` with no `gl_FrontFacing` normal flip -> the back half is
   drawn with a normal facing away and clamps to ambient.
4. No colour management: `VK_FORMAT_B8G8R8A8_UNORM` is chosen and values are written raw,
   so darks read lifted relative to a gamma-managed render. (Switching to the `_SRGB`
   format requires `CAMetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB` to match,
   or MoltenVK rejects the mismatch.)

**Decision: leave it unchanged.** Every item above is either a property of the demo
geometry (1, 2) or a deliberate improvement better made together with the work that needs
it (3, 4). Marching Cubes terrain has per-vertex normals in every direction, so Lambert
will finally have something to express, and the same phase wants app-controlled lighting -
which is the descriptor-set/uniform-buffer change that also frees the 128-byte push budget
holding `kLightDir` in the shader. Deferred options are recorded in
.PlanS/PLAN_real_vulkan_renderer.md rather than silently dropped.

**Known-deferred backlog:** back-face normal flip · sRGB swapchain + matching CAMetalLayer
pixel format · descriptor set + UBO for light direction · swapchain recreation on resize ·
device-local + staging mesh upload · VK_EXT_debug_utils messenger · Win32 `IsClosed()`
ignores `shouldCloseFlag` · Linux `WM_DELETE_WINDOW` unhandled · generated `ShadersSpv.h`
and `*.spv` not gitignored · orphaned `Engine/Renderer/Vulkan/CMakeLists.txt` ·
`__DEBUG__` undefined so `Assert()` never fires.

## TriangleExample removed (2026-08-31)

### Why
It was superseded by `VulkanExample` + the real `VulkanRenderer`, and it was the one target
that could not link: its `customCMake.txt` carried

```
include_directories(.../External/VulkanSDK/include)          # path does not exist
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_LIBRARIES Test)   # wipes the link line
```

The second line replaces the whole link line with `libTest.a`, so every `MemoryManager` and
`TestCollection` symbol goes missing. This was not repairable inside the rules that keep
generated `CMakeLists.txt` files machine-owned - the hack exists precisely to fight the
generator - and both the Vulkan linking and the sample geometry it existed to demonstrate
now live in `VulkanExample`.

### Changes
- Deleted `Applications/TriangleExample/` (8 files, incl. `MinimalVulkanRenderer.*`,
  `TriangleExampleTest.cpp`, and the offending `customCMake.txt`).
- Regenerated all `CMakeLists.txt` via `generate_cmake_files.sh`; the
  `add_subdirectory (TriangleExample)` entry and the global
  `${CMAKE_SOURCE_DIR}/Applications/TriangleExample` include path disappeared with it, so
  no generated file was hand-edited.
- Repointed instructional references at a target that exists: `AGENTS.md`, `README.md`,
  `build.sh`, `build.bat`, `run.sh`, `run.bat`, `docs/HelperScript.md`,
  `docs/EngineAPIGuide.md`, `docs/EngineAPIGuide.html`.

### Deliberately left alone
- `.Plans/PLAN_ApplyCommonMacros.md`, `.Plans/PLAN_renderer_application_window_refactor.md`
  - dated records of past work; rewriting them would falsify history.
- `docs/design/LightweightRenderer_Design.{md,html}` - a superseded design document whose
  acceptance checklist still names the old target. Retiring that document is a separate
  call, so it is flagged rather than silently edited.

### Verification
`cmake --build build --config Dev` - the build-everything command the generator itself
prints - now exits **0** with no errors; it failed on this link error before. `EngineTest`
exits 0, and `VulkanExample` still logs
`first frame presented (800x568, swapchain images=3, index count=6)`.

## LightweightRenderer design document reconciled (2026-09-01)

`docs/design/LightweightRenderer_Design.{md,html}` (dated 2026-06-21, "Draft - Awaiting
Review") still described a renderer that was never built, and parts of it had been actively
reversed. Rather than delete it, the document was updated so it can no longer mislead:

- **Header** keeps the original author/date and adds an explicit *partially superseded*
  status plus an update line.
- **New §0 "Implementation Status (read first)"**: section-by-section verdict (not built /
  superseded / not adopted / partial), the file tree that actually exists, and a table of the
  shipped renderer's real properties (backend, uniform path, mesh memory, framing, culling,
  resize behaviour, example).
- **§5 RHI** banner: the abstraction was not merely thinned but removed (`e1b5efb`), and no
  `RHI/Vulkan` + `RHI/Metal` files exist; §5.3 kept only as notes for a hypothetical second
  backend, since macOS is served by MoltenVK.
- **§7 File structure** banner: `Core/ Graph/ Resources/ Commands/ Backends/` was not adopted;
  the platform split is by file extension (`.cpp` / `.mm`), not directory.
- **§9** phase-outcome table: 1,2,5,7 not built · 3 superseded · 4 replaced by MoltenVK ·
  6 partial.
- **§10** opens with the blocking caveat that `Assert()` is a no-op because `__DEBUG__` is
  defined nowhere, so a green suite proves nothing yet.
- **§10.3** replaced with commands that exist (`Applications/VulkanExample`, full-tree build)
  and a note that `Renderer/Vulkan`, `Renderer/Metal` and `Applications/TriangleExample` are
  not targets at all.
- **§11** marks the "thin abstraction" decision as reversed in practice; **§12** answers Q4
  (render passes, not dynamic rendering) and records Q3 as already constrained by the 128-byte
  push budget.

Both mirrors were edited (527-line Markdown, 996-line styled HTML) and the HTML re-checked for
tag balance; the one unbalanced `</span>` found predates this change (old line 587, inside the
architecture diagram) and was left alone.

**Every factual claim was verified against the tree**, including `sizeof(PushConstants) == 128`,
`MAX_FRAMES_IN_FLIGHT == 2` with one command buffer per frame, `VK_CULL_MODE_NONE`, host-visible
mesh memory, and the absence of `Core/ Graph/ Resources/ Commands/ Backends/`.
