# PLAN: Device-queried, API-neutral render capabilities

## Confirmed goal (from user, in order asked)
1. `RenderCapabilities` values must be **queried from the device**, not hardcoded.
2. Use **API-neutral property names** that can also be satisfied by DX12 and Metal later.
3. **Remove dead `APIType`**.
4. **Remove `RendererCommon.h`**.
5. Include **device name and API version** — do not skip useful information.

## Evidence gathered (measured, not assumed)
Real values on this machine (Apple M4 Pro / MoltenVK 1.4.1, Vulkan 1.1.334) vs the old
hardcoded defaults — proves the defaults were wrong, not merely approximate:

| Neutral field | old hardcode | real | old wrong? |
|---|---|---|---|
| maxTextureDimension2D | 4096 | 16384 | yes, 4x |
| maxVertexAttributes | 16 | 31 | yes |
| maxUniformBufferBindings | 16 | 155 | yes, ~10x |
| supportsTessellation | false | true | **yes, inverted** |
| supportsGeometryShader | false | false | coincidentally right |
| supportsComputeShader | true / false | core-mandatory | self-contradictory |
| supportsLogicOperations | (absent) | false | — |
| supportsDepthBounds | (absent) | false | — |
| supportsWideLines | (absent) | false | — |

Probe: `/tmp/vkprobe2.c` (needs `VK_KHR_portability_enumeration` +
`VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, because the ICD json sets
`is_portability_driver: true`).

Also measured: `maxVertexAttribs` and `shaderCompute` **are not Vulkan members** — the old
field names came from OpenGL. Vulkan's are `maxVertexInputAttributes`; compute is core-
mandatory so it has no feature bit at all.

## Design
- `RenderCapabilities` = API-neutral **DTO** (the contract). Zero-initialised, and
  `isDeviceQueried=false` means "unknown" — so an unqueried snapshot can never masquerade
  as real data again (that was the root cause of the bug).
- `Vulkan/VulkanCapabilities.{h,cpp}` = the **backend adapter**: the only place that knows
  Vulkan's field names. Translates a `VkPhysicalDevice` into the neutral DTO. A future
  `DX12/` or `Metal/` adapter fills the same DTO — that is the multi-API seam, and it
  needs no `APIType` enum to exist.
- `VulkanRenderer` owns the device, so it queries once at device-creation time and caches.
- `RHICapabilities::GetCapabilities()` = pre-device probe (creates a throwaway instance,
  queries, destroys) — reuses the same adapter, so Vulkan knowledge lives in one place.

## Steps
1. `RenderCapabilities.h/.cpp` — neutral names, identity + features + limits, zero-init,
   documented per-API mapping table (Vulkan implemented; DX12/Metal named).
2. `Vulkan/VulkanCapabilities.h/.cpp` — `FillRenderCapabilities(VkPhysicalDevice, out)`.
3. `VulkanRenderer` — add private `QueryCapabilities()`, call from `CreateDevice()`;
   delete `apiType` member + `GetAPIType()`.
4. `RHICapabilities` — delete `GetPreferredAPI()` (needed APIType); reimplement
   `GetCapabilities()` as a real probe; keep `IsVulkanSupported()`.
5. Delete `RendererCommon.h`; consumers include `RenderCapabilities.h` directly.
6. Rewrite `RendererTest` + `RHICapabilitiesTest` to assert **queried** values.
7. `CMakeLists.txt` + `customCMake.txt` for the new Vulkan adapter source.

## Verification
- Standalone compile of every touched header (self-sufficiency).
- `EngineTest` in **Debug with `-D__DEBUG__`** so `Assert()` is live — see caveat below.
- Cross-check the values the engine reports against the probe's numbers.

## Caveat found during planning (pre-existing, reported not silently fixed)
`Core/Debug.h` guards assertions on `#ifdef __DEBUG__`, but `__DEBUG__` is defined **nowhere**
in the build system (not in `Config/BuildConfig.h`, no CMakeLists, no ninja flags) — while
`BuildConfig.h` line 41 carries a stale comment claiming it tracks `NDEBUG`/`_DEBUG`/`DEBUG`.
Consequence: **every `Assert()` in the engine compiles to a no-op in every configuration**, so
"285 PASS" only proves "did not crash". This is exactly why the existing test
`Assert(caps.supportsComputeShader, ...)` — asserting `true` against a `false` default — never
failed. I will verify my work with `-D__DEBUG__` injected, and leave the build-system change
to the owner's decision.
