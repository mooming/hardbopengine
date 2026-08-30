# PLAN: Renderer + Application + Window static refactor (Vulkan-only, no inheritance)

## Goal
Replace runtime polymorphism with static compile-time dispatch:
1. Renderer → **Vulkan only**, no `IRenderer`, no inheritance.
2. Application → no `IApplication`; platform selected via preprocessor macro.
3. Window → compact `class Window` + per-platform `.cpp`/`.mm` impl (OSAL-like).
4. Add Window-API + simple rendering tests to `EngineTest`.

## Approach (commit-per-task, build kept green each step)
| Step | Scope | Build gate |
|------|-------|-----------|
| A | Application (macro `HBE_APPLICATION_CLASS`) | build `WindowExample` |
| B | Window (compact class + per-platform impl) | build `WindowExample`, `TriangleExample` |
| C | Renderer (Vulkan-only) + `VulkanExample` | build `VulkanExample` |
| D | EngineTest: Window-API + rendering tests | build + run `EngineTest` |

## Files
### Application (Step A)
- `OSAL/Application.h`: drop `IApplication` + `CreateApplication`; add `HBE_APPLICATION_CLASS` macro.
- Delete `OSAL/Application.cpp`; remove from `OSAL/CMakeLists.txt`.
- `OSAL/{Linux,OSX,Win32}Application.h`: drop `: public IApplication`.
- `Engine/Engine.{h,cpp}`: `unique_ptr<HBE_APPLICATION_CLASS>`; `GetApplication()` concrete ptr.
- `Applications/{TriangleExample,WindowExample,VulkanExample}/Main.cpp`: `HBE_APPLICATION_CLASS app;`.

### Window (Step B)
- `OSAL/Window.h`: `class Window` (compact, no platform includes) + `OS::CreateWindow()` → `unique_ptr<Window>`; keep `WindowTest` decl.
- `OSAL/Window.cpp`: `CreateWindow()` factory; rewrite `WindowTest::Prepare()` to compact `Window`; drop `IWindow` test impl.
- `OSAL/{LinuxWindow,OSXWindow}.cpp|mm`, `Win32Window.cpp`: implement `Window::` methods under `#if defined(PLATFORM_*)`.
- Delete `OSAL/{LinuxWindow,OSXWindow,Win32Window}.h`; remove from `OSAL/CMakeLists.txt`.

### Renderer (Step C)
- Delete: `Renderer/IRenderer.h`, `RendererFactory.{h,cpp}`, `DX12/*`, `Metal/*`.
- `Renderer/Vulkan/VulkanRenderer.{h,cpp,mm}`: standalone concrete class (drop `: public IRenderer`); wire into `Renderer` lib (`VulkanRenderer.cpp` + `.mm` on Apple).
- `Renderer/RHICapabilities.{h,cpp}`: Vulkan-only (`IsVulkanSupported`, `GetCapabilities`); trim test.
- `Renderer/RendererCommon.h`: `APIType` → `Vulkan` only; keep `Vertex`, `RenderCapabilities`.
- `Renderer/RendererTest.cpp`: add simple rendering tests.
- `Applications/VulkanExample`: construct `VulkanRenderer` directly; add linking.

### Tests (Step D)
- `WindowTest` (in `Window.cpp`): Window-API coverage via compact `Window`.
- `RendererTest`: Lambert shading math + triangle coverage (GPU-free, deterministic).
- `UnitTestCollection.cpp`: drop `RHICapabilities.h` include if deleted (kept).

## Verification
- Grep sweep: no `IRenderer`/`IApplication`/`RendererFactory`/`MetalRenderer`/`DX12Renderer`/`APIType::Metal/DX12/Unknown`.
- Build (Dev, `-Werror`): `WindowExample`, `TriangleExample`, `VulkanExample`, `EngineTest`.
- Run `EngineTest`: no regressions vs baseline `8a33412`.

## Constraints / reality
- No Vulkan SDK in env → `VulkanRenderer` compiles as a stub (no GPU render). Verification = compile/link + math tests, not live render.
