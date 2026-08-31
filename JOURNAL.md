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
