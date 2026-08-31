# PLAN — Real Vulkan Renderer (Path B) + Marching Cubes Example

## Goal
Replace the stub `VulkanRenderer` with a **real Vulkan pipeline** (instance→device→swapchain→render pass→pipeline→buffers→command recording→present), then a Marching Cubes example that meshes a procedural world and draws it with one directional light + Lambert shading, ASDW+mouse controls, and space-to-dig.

## Verified environment (all green)
- vulkan-headers 1.4.350, loader 1.4.350, MoltenVK 1.4.1 (device: Apple M4 Pro).
- Instance (portability flag) + device + `VK_EXT_metal_surface` + **800x600 drawable** + 2 present modes — works headless.
- glslangValidator 16.5.0 (GLSL→SPIR-V).
- Native API: surface via `vkCreateMetalSurfaceEXT` with a `CAMetalLayer`; no `vkCreateSurfaceKHR` in this header. Portability enumeration flag + `VK_EXT_METAL_SURFACE_EXTENSION_NAME` required.

## Architecture (OSAL-style: one header + per-platform impl)
- `Renderer/Vulkan/VulkanRenderer.h` — compact public class + all Vulkan handles + `Vertex` input struct.
- `Renderer/Vulkan/VulkanRenderer.cpp` — core Vulkan (cross-platform) + `CreateSurface()` dispatcher + Win32/Xlib surfaces.
- `Renderer/Vulkan/VulkanRenderer.mm` — macOS `CreateMetalSurface()` (ObjC + CAMetalLayer).
- Shaders: GLSL `+` build-time glslangValidator → SPIR-V → C++ header (embedded byte array).
- Mesh: example feeds `std::vector<Vertex>` + `uint32_t` indices → renderer uploads to vertex/index buffers (push constants for view/proj/light).

## Phases (verify at each)
| # | Phase | Deliverable | Verify |
|---|-------|-------------|--------|
| 1 | Renderer foundation | instance, device, queue, surface | compiles; instance+device OK |
| 2 | Swapchain + render target | swapchain, image views, depth, render pass, framebuffers | swapchain 800x600 |
| 3 | Pipeline + buffers + shaders | push-const layout, vertex/index buffers, graphics pipeline | pipeline created |
| 4 | Render loop | BeginFrame/Render/EndFrame + command recording | run w/ crash on a mesh |
| 5 | Wire VulkanExample | build+run existing example against new renderer | window renders |
| 6 | Marching Cubes core | Perlin→SDF→cap→mesh module | EngineTest unit tests |
| 7 | MC example + controls | feed MC mesh, ASDW/mouse, space-dig, Lambert | renders terrain |

## Verification
- Compile each phase (Dev). Run VulkanExample (window + clear + quad). EngineTest for MC math.
- Headless caveat: pipeline runs; visible output depends on display.

## Open items
- MC math placement for EngineTest coverage (new engine module vs app-local).
- Linux/Windows surface code (untested here; compile-checked).

---

# Checklist A — Make the Vulkan renderer actually build (B1–B3)

Approved 2026-08-30. Scope is *build wiring + compile/link green* only; no MC work.

### Root causes found
| ID | Cause | Fix site |
|----|-------|----------|
| B1 | `Engine/Renderer/.module.config: ignoreSubdirectories = DX12 Metal Vulkan` → MakeBuild never emits `Vulkan/*` into the Renderer target (`Module.cpp:CollectFiles` reads only the module's own directory; `ProjectBuilder.cpp:126` skips ignored subdirs entirely) | `Engine/Renderer/customCMake.txt` → `target_sources` |
| B1b | Nothing links the Vulkan loader (`libvulkan`) → undefined `vk*` symbols | `Engine/Renderer/customCMake.txt` → `find_library` + `target_link_libraries PUBLIC` |
| B1c | `.mm` needs `VK_USE_PLATFORM_METAL_EXT` or `vkCreateMetalSurfaceEXT` is `#ifdef`'d out of `vulkan_metal.h` | `Engine/Renderer/customCMake.txt` → `target_compile_definitions` (APPLE) |
| B2 | `Applications/VulkanExample/` has **no `.module.config`** → MakeBuild resolves `buildType = None` → `switch` default → no `add_executable` at all | add `Applications/VulkanExample/.module.config` |
| B3 | Regenerated CMake noise is the *symptom*, not the cause — generated files stay machine-owned | no hand-edits to any generated `CMakeLists.txt` |

### Steps
| # | Step | Verify | Done |
|---|------|--------|------|
| A1 | `target_sources` for `Vulkan/VulkanRenderer.cpp` + (APPLE) `.mm` via `customCMake.txt` | both objects in `libRenderer.a` | ☑ |
| A2 | `find_library(vulkan)` + `FATAL_ERROR` guards for missing headers/loader | configure OK here; fails loudly when absent | ☑ |
| A3 | APPLE: `VK_USE_PLATFORM_METAL_EXT` + Cocoa/Metal/QuartzCore | configure OK; `.mm` compiles | ☑ |
| A4 | `Applications/VulkanExample/.module.config` (Executable, deps incl. Renderer) | `add_executable (VulkanExample ...)` generated | ☑ |
| A5 | Regenerate CMake (`./generate_cmake_files.sh`) — no hand edits | done; keyword/plain link-signature clash resolved | ☑ |
| A6 | Compile `Renderer` (Dev) and iterate on real compiler errors | 65 (.cpp) + 11 (.mm) errors fixed, `-Wall -Werror` clean | ☑ |
| A7 | Build + run `VulkanExample` (Dev) | runs; `first frame presented (800x568, swapchain images=3)` | ☑ |
| A8 | `EngineTest` still green (Renderer is a test dependency) | builds + exits 0, but see **A10**: assertions are compiled out | ⚠️ |
| A9 | Journal entry + commit | `JOURNAL.md` updated | ☑ |
| A10 | **Found:** `__DEBUG__` is defined nowhere → `Assert()` is a no-op → the whole suite reports PASS vacuously | report to owner, do not fix here | ☑ |
| A11 | **Found:** `RendererTest` expects stub semantics (`Initialize(nullptr)==true`, `supportsComputeShader`) | rework with MC phase | ☑ |

# Checklist B — Rotating quad + close button (2026-08-31)

| # | Item | Verify | Done |
|---|------|--------|------|
| B1 | Push block -> `{ mat4 model; mat4 viewProj; }` (still 128 B) + regenerate SPIR-V/`ShadersSpv.h` | `index count=6` at runtime | ☑ |
| B2 | CPU `proj*view` multiply, `SetModel()` restored, `GetExtent()` for true aspect | warning-free build | ☑ |
| B3 | `Main.cpp` feeds quad + perspective + `rotationY(t)` | pipeline reports 6 indices | ☑ |
| B4 | Window close button must terminate the app | control test: `IsClosed()` 0 without delegate, 1 with | ☑ |
| B5 | Pixel-level confirmation | blocked here (no Screen Recording / Accessibility) | ⬜ user |

**Same close defect still open elsewhere (unverifiable on macOS):** Win32 records the close in
`shouldCloseFlag` but `IsClosed()` returns `closedFlag` (one-line fix); Linux has an empty
`ClientMessage` placeholder, so `WM_DELETE_WINDOW`/`WM_PROTOCOLS` is unimplemented (~8 lines).

# Checklist C — Lighting audit (2026-08-31, decision: keep as-is)

Lambert only: `0.25 + max(dot(n,L),0) * 0.95`, `n = mat3(model)*aNormal`. For the demo quad
this solves to `0.337 sin t + 0.421 cos t` (amplitude 0.539, peak at 38.7 deg, range
0.250..0.762, ambient-only for 50% of the turn) - which fully explains the flat look.

| Cause | Fix if ever wanted | Decision |
|-------|--------------------|----------|
| Single flat normal over the quad | nothing to fix; terrain has real normals | keep |
| Light is 84% +Y, normal sweeps XZ | tilt `model`, or aim the light | keep |
| `CULL_NONE` without a `gl_FrontFacing` flip | flip normal on back faces (~3 lines) | deferred |
| No sRGB path (`B8G8R8A8_UNORM`, raw values) | `_SRGB` format **and** `MTLPixelFormatBGRA8Unorm_sRGB` together | deferred |
| `kLightDir` hard-coded (push budget full) | descriptor set + UBO - also frees the 128 B ceiling | fold into MC phase |

### Out of scope for A (recorded, not fixed)
`Applications/MarchingCubes/` (Phases 6–7), swapchain-recreate on resize, staging-buffer upload, fence `SIGNALED_BIT` first-frame deadlock, dynamic viewport/scissor not set, push-constant layout mismatch (`model`/`lightDir` never pushed), orphaned `Engine/Renderer/Vulkan/CMakeLists.txt`, generated `ShadersSpv.h` + `*.spv` not gitignored.
