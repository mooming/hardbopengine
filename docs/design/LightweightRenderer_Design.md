# Lightweight Data-Oriented Renderer — Design Document

**Author:** HardBop Engine Team
**Date:** 2026-06-21
**Status:** Draft — Awaiting Review

---

## 1. Vision

A lightweight, data-oriented renderer targeting **Vulkan** and **Metal**. Built on cache-friendly data layouts, explicit dependency management, and a minimal cross-platform abstraction layer.

### Goals

| # | Goal | Metric |
|---|------|--------|
| G1 | Data-oriented scene representation | SoA layouts for all hot-path data |
| G2 | Cross-platform: Vulkan + Metal | Single render graph, two RHI backends |
| G3 | Lightweight scope | Forward rendering, no deferred/skinning/particles |
| G4 | Cache-friendly performance | Batching, parallel recording, minimal state changes |

### Out of Scope

- Deferred rendering
- Skinned mesh animation
- Particle systems
- UI rendering
- Physics integration

---

## 2. Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                        Application                            │
├──────────────────────────────────────────────────────────────┤
│                       Render Graph                            │
│  (Explicit pass dependency graph → topological execution)     │
├──────────────────────────────────────────────────────────────┤
│                   Scene Processing (CPU)                      │
│  ┌────────────┐  ┌────────────┐  ┌───────────────────────┐   │
│  │ Frustum    │  │ Transform  │  │ LOD Selection         │   │
│  │ Culling    │  │ Update     │  │                       │   │
│  └────────────┘  └────────────┘  └───────────────────────┘   │
├──────────────────────────────────────────────────────────────┤
│              Command Recording (CPU, Parallel)                │
│  ┌─────────────────┐  ┌────────────────┐  ┌──────────────┐   │
│  │ Batch Assembly  │  │ Uniform Upload │  │ Draw Call    │   │
│  │ (group by pipe) │  │ (per-frame)    │  │ Recording    │   │
│  └─────────────────┘  └────────────────┘  └──────────────┘   │
├──────────────────────────────────────────────────────────────┤
│                   Cross-Platform RHI                          │
│              Vulkan Backend  │  Metal Backend                 │
├──────────────────────────────────────────────────────────────┤
│                      OSAL (Platform)                          │
└──────────────────────────────────────────────────────────────┘
```

**Three-layer separation:**

1. **Scene Processing (CPU, parallel):** Read SoA arrays → filter, transform, cull. No GPU interaction.
2. **Command Recording (CPU, parallel):** Assemble batches → record commands. No GPU interaction.
3. **RHI (GPU):** Submit pre-recorded command buffers to Vulkan/Metal. Single threaded, deterministic.

This separation lets CPU-side work (steps 1 & 2) be parallelized freely without synchronization with GPU submission.

---

## 3. Data-Oriented Design

### 3.1 Entity Registry — Sparse Set

Entities are lightweight IDs backed by a sparse-set index for O(1) existence checks and cache-friendly iteration.

```
EntityRegistry
├── packedToSparse : uint32_t[]   // packed index → sparse index (-1 = dead)
├── sparseToPacked : uint32_t[]   // sparse index → packed index
├── entities     : Entity[]       // sparse set of alive entities
├── capacity     : uint32_t
├── count        : uint32_t
└── nextID       : uint32_t
```

- `packedToSparse[i]` = -1 means entity at packed index `i` is dead.
- Iteration over alive entities = iterate `entities[0..count)`.
- `Contains(entity)` = `packedToSparse[entity.ID] != -1` → single array lookup.

### 3.2 Component Arrays — Struct of Arrays (SoA)

Every component type lives in its own contiguous array. Hot-path iteration touches only the components it needs.

```
// Transform components (iterated during culling + uniform upload)
TransformComponent
├── positions    : float3[]    // all positions contiguous
├── rotations    : float4[]    // all rotations contiguous
└── scales       : float3[]    // all scales contiguous

// Mesh components (read once, batched)
MeshComponent
├── vertexPositions : float3[]
├── vertexNormals   : float3[]
├── vertexUVs       : float2[]
├── vertexCounts    : uint32[]
├── indexBuffers    : uint32[]  // GPU handle per mesh
├── indexCounts     : uint32[]
└── bounds          : AABBox[]

// Material components (grouped by shader ID for batching)
MaterialComponent
├── shaderID        : uint32[]
├── albedo          : float4[]
├── roughness       : float[]
├── metalness       : float[]
└── emissive        : float4[]

// Light components
LightComponent
├── positions       : float4[]  // w = type: 0=point, 1=spot, 2=directional
├── intensities     : float[]
├── colors          : float4[]
└── directions      : float3[]  // for spot/directional
```

**Key property:** If you only need positions and rotations for culling, you iterate two float arrays — not the whole entity struct. Memory bandwidth scales with data actually used.

### 3.3 Instance Pool

Bridges entity IDs to their GPU representation. Stores the per-instance data needed for draw call assembly.

```
InstancePool
├── entityRef    : uint32[]    // backing entity ID (for reverse lookup)
├── transformIndex : uint32[]  // index into TransformComponent arrays
├── meshIndex      : uint32[]  // index into MeshComponent arrays
├── materialIndex  : uint32[]  // index into MaterialComponent arrays
├── visible        : bool[]    // per-frame culling result
└── capacity     : uint32
```

During culling: iterate `visible` array, mark instances as visible or not.
During batching: iterate visible instances, group by `materialIndex → shaderID`.

---

## 4. Render Graph

Explicit dependency graph replaces implicit render loop. Each pass declares its inputs/outputs and dependencies.

### 4.1 Pass Types

| Pass | Purpose | Parallelism |
|------|---------|-------------|
| **Init** | Create swapchain, RTVs, command pool | Once |
| **Cull** | Frustum culling, visibility flags | CPU, parallel |
| **Record** | Pipeline binding, uniform upload, draw batching | CPU, parallel |
| **Shadow** | Render shadow map | GPU |
| **Forward** | Main scene rendering | GPU |
| **PostProcess** | Tone mapping, VRS | GPU |
| **Present** | Submit and present | GPU |

### 4.2 Dependency Graph

```
Init ──────────────────────────────────────────────────┐
  │                                                     │
  ▼                                                     │
Cull ──────────────────────────────────────────────────►│
  │                                                     │
  ▼                                                     │
Record ────────────────────────────────────────────────►│
  │                                                     │
  ├─────────────┐                                       │
  ▼             ▼                                       │
Shadow    Forward ──────────────────────────────────────►│
  │                                                         │
  ▼                                                         │
PostProcess ──────────────────────────────────────────────►│
  │                                                         │
  ▼                                                         │
Present ──────────────────────────────────────────────────►│
```

Each pass declares which other passes it depends on. The graph executor topologically sorts and executes in correct order.

### 4.3 Render Pass Descriptor

```
RenderPassDescriptor
├── name        : const char*
├── dependencies : const char*[N]  // pass names this pass depends on
├── colorTargets : RenderTarget[N]
├── depthTarget  : RenderTarget?
├── execute      : function(RenderContext&, FrameData&)
```

---

## 5. Cross-Platform RHI

Thin abstraction between render graph and platform APIs.

### 5.1 Abstraction Layer

```
IRenderer (abstract)
├── CreateBuffer()     → IBuffer*
├── CreateTexture()    → ITexture*
├── CreatePipeline()   → IPipeline*
├── CreateCommandBuf() → ICommandBuffer*
├── CreateRenderPass() → IRenderPass*

IBuffer (abstract)
├── BindToVertex()
├── BindToIndex()
├── BindToUniform()
└── UpdateData()

ITexture (abstract)
├── BindToColor()
├── BindToDepth()
├── TransitionLayout()
└── CopyFrom()

IPipeline (abstract)
├── Bind()
├── SetPushConstants()
└── SetDescriptorSets()

ICommandBuffer (abstract)
├── Begin()
├── End()
├── SetRenderPass()
├── SetPipeline()
├── BindVertexBuffers()
├── DrawIndexed()
└── Dispatch()
```

### 5.2 Backend Separation

```
RHI/
├── Vulkan/
│   ├── VulkanRenderer.h/cpp       — IRenderer implementation
│   ├── VulkanBuffer.h/cpp         — IBuffer implementation
│   ├── VulkanTexture.h/cpp        — ITexture implementation
│   ├── VulkanPipeline.h/cpp       — IPipeline implementation
│   └── VulkanCommandBuffer.h/cpp  — ICommandBuffer implementation
│
└── Metal/
    ├── MetalRenderer.h/cpp        — IRenderer implementation
    ├── MetalBuffer.h/cpp          — IBuffer implementation
    ├── MetalTexture.h/cpp         — ITexture implementation
    ├── MetalPipeline.h/cpp        — IPipeline implementation
    └── MetalCommandBuffer.h/cpp   — ICommandBuffer implementation
```

Each backend wraps only platform-specific API calls. The render graph and scene processing code never touches Vulkan/Metal directly.

### 5.3 Vulkan vs Metal Mapping

| Concept | Vulkan | Metal |
|---------|--------|-------|
| Command recording | `vkCmdBegin/EndRendering` | `MTLRenderCommandEncoder` |
| Render pass | `VkRenderPass` | `MTLRenderPassDescriptor` |
| Pipeline | `VkPipeline` | `MTLRenderPipelineState` |
| Descriptor sets | `VkDescriptorSet` | `MTLCommandEncoder setBuffer:at:` |
| Push constants | `vkCmdPushConstants` | `MTLCommandEncoder setBytes:length:atIndex:` |
| Shader module | SPIR-V | MSL (Metal Shading Language) |
| Synchronization | Semaphores + Fences | `MTLBuffer` lifetime + command buffer completion handler |

**Key difference to handle:**
- Vulkan uses explicit descriptor sets with layouts; Metal uses direct buffer binding in command encoders. The `IPipeline` abstraction must handle this transparently.
- Vulkan has push constants; Metal has `setBytes:length:atIndex:`. Same concept, different API.

---

## 6. Shader Management

### 6.1 Shader Library

Shaders are compiled offline and baked as platform-native bytecode:
- **Vulkan:** SPIR-V (`.spv` files)
- **Metal:** Compiled MSL (`.metallib` or function name lookup)

A runtime shader library loads the correct format and creates pipeline handles.

### 6.2 Core Shaders (In Scope)

| Shader | Type | Purpose |
|--------|------|---------|
| `core.vert` | Vertex | Position + normal + UV transform |
| `core.frag` | Fragment | PBR lighting (direct + shadow) |
| `shadow.vert` | Vertex | Shadow space transform |
| `shadow.frag` | Fragment | Depth only |
| `post.vert` | Vertex | Fullscreen quad |
| `post.frag` | Fragment | Tone mapping + bloom |

### 6.3 Shader Type Definition

```
ShaderType
├── name         : const char*
├── vertexSPV    : uint8[]   // Vulkan vertex shader bytecode
├── fragmentSPV  : uint8[]   // Vulkan fragment shader bytecode
├── vertexMSL    : const char*  // Metal vertex shader source/function
├── fragmentMSL  : const char*  // Metal fragment shader source/function
└── pushConstant : PushConstantBlock
```

Push constants carry per-material / per-frame data (matrices, albedo, roughness, etc.) to avoid descriptor set churn.

---

## 7. File Structure

```
Engine/Renderer/
├── Core/                          # Data-oriented scene data
│   ├── EntityRegistry.h/cpp       # Sparse-set entity management
│   ├── ComponentArray.h           # SoA template for component arrays
│   └── InstancePool.h/cpp         # Per-instance data bridging
│
├── Graph/                         # Render graph
│   ├── RenderGraph.h/cpp          # Graph executor
│   ├── RenderPass.h/cpp           # Pass definition + execution
│   └── RenderNode.h               # Node in dependency graph
│
├── Resources/                     # RHI abstraction
│   ├── IBuffer.h/cpp              # Vertex/index/uniform buffers
│   ├── ITexture.h/cpp             # Color/depth textures
│   ├── IPipeline.h/cpp            # Graphics pipelines
│   └── IResourceFactory.h/cpp     # Factory interface
│
├── Commands/                      # Command recording
│   ├── ICommandBuffer.h/cpp       # Command buffer abstraction
│   ├── CommandRecorder.h/cpp      # High-level command assembly
│   └── DrawBatch.h                # Batched draw call descriptor
│
├── Shaders/                       # Shader assets
│   ├── ShaderLibrary.h/cpp        # Runtime shader loading
│   └── Shaders/                   # Source shaders
│       ├── core.vert
│       ├── core.frag
│       ├── shadow.vert
│       ├── shadow.frag
│       ├── post.vert
│       └── post.frag
│
├── Backends/                      # Platform implementations
│   ├── Vulkan/
│   │   ├── VulkanRenderer.cpp/h
│   │   ├── VulkanBuffer.cpp/h
│   │   ├── VulkanTexture.cpp/h
│   │   ├── VulkanPipeline.cpp/h
│   │   ├── VulkanCommandBuffer.cpp/h
│   │   └── Shaders/spv/           # Compiled SPIR-V
│   └── Metal/
│       ├── MetalRenderer.cpp/h
│       ├── MetalBuffer.cpp/h
│       ├── MetalTexture.cpp/h
│       ├── MetalPipeline.cpp/h
│       ├── MetalCommandBuffer.cpp/h
│       └── Shaders.metal            # MSL shaders
│
└── CMakeLists.txt
```

**Key principle:** `Core/`, `Graph/`, `Resources/`, `Commands/`, and `Shaders/` are **platform-agnostic**. Only `Backends/Vulkan/` and `Backends/Metal/` contain platform-specific code.

---

## 8. Performance Strategy

### 8.1 Cache Efficiency

| Technique | Mechanism |
|-----------|-----------|
| SoA data layout | Hot components iterate contiguous arrays |
| Batch assembly | Group by pipeline/material → sequential memory access |
| Uniform buffer batching | Single buffer upload per frame, not per draw call |

### 8.2 Parallelism

| Stage | Parallelism |
|-------|-------------|
| Transform update | Thread pool (write to SoA arrays) |
| Frustum culling | Thread pool (read SoA, write `visible[]`) |
| Command recording | Thread pool (each thread records a batch's commands) |
| GPU submission | Single thread (deterministic) |

### 8.3 Minimized State Changes

- Draw calls are grouped by `shaderID` first, then `materialIndex`.
- Pipeline binding changes only when shader changes.
- Uniform buffer changes only when material values change.
- Result: ~10-20 state changes for thousands of draw calls.

---

## 9. Implementation Phases

### Phase 1 — Core Data Structures
**Scope:** Entity registry, SoA component arrays, instance pool.
**Effort:** ~3 days
**Deliverables:**
- `EntityRegistry` with sparse-set add/remove/contains/iterate
- `ComponentArray<T>` template (SoA storage)
- `InstancePool` for per-instance data
**Verification:** Unit tests for entity CRUD, array bounds, sparse-set correctness

### Phase 2 — Render Graph
**Scope:** Graph executor, pass definitions, basic forward pass.
**Effort:** ~3 days
**Deliverables:**
- `RenderGraph` with dependency resolution (topological sort)
- `RenderPass` base class + execution framework
- Basic forward rendering pass
**Verification:** Graph executes passes in correct dependency order; basic forward pass renders to a color target

### Phase 3 — RHI Abstraction
**Scope:** IBuffer, ITexture, IPipeline interfaces + Vulkan backend.
**Effort:** ~5 days
**Deliverables:**
- `IRenderer`, `IBuffer`, `ITexture`, `IPipeline` interfaces
- Vulkan backend for all four
- Resource lifecycle (create, use, destroy)
**Verification:** Vulkan builds and runs; creates/binds/destroys resources correctly

### Phase 4 — Metal Backend
**Scope:** Metal implementation of the RHI abstraction.
**Effort:** ~5 days
**Deliverables:**
- `MetalRenderer`, `MetalBuffer`, `MetalTexture`, `MetalPipeline`
- Cross-compile verification (Vulkan + Metal both build)
**Verification:** Metal builds on macOS; renders same scene as Vulkan

### Phase 5 — Command Recording
**Scope:** Command buffer abstraction, draw batching, uniform management.
**Effort:** ~4 days
**Deliverables:**
- `ICommandBuffer` interface + Vulkan/Metal implementations
- `CommandRecorder` for batch assembly
- `DrawBatch` descriptor
**Verification:** Command buffers record and execute correctly; batching reduces state changes

### Phase 6 — Shaders & Integration
**Scope:** Core shaders, shader library, end-to-end rendering.
**Effort:** ~4 days
**Deliverables:**
- 6 core shaders (vertex/fragment for core, shadow, post)
- `ShaderLibrary` runtime loader
- End-to-end: scene → render graph → GPU → present
**Verification:** Render triangle + cube with PBR material on both backends

### Phase 7 — Post-Processing
**Scope:** Tone mapping, bloom, VRS.
**Effort:** ~3 days
**Deliverables:**
- Post-process render pass
- Fullscreen quad rendering
- Tone mapping + bloom filters
**Verification:** Post-process pass renders to swapchain; VRS reduces GPU load

---

## 10. Verification Plan

### 10.1 Unit Tests

| Test | Target | Pass Criteria |
|------|--------|---------------|
| EntitySparseSet | `EntityRegistry` | Add/Remove/Contains correct for 1000 entities |
| ComponentArraySoA | `ComponentArray<T>` | Write/read roundtrip for float3/float4 arrays |
| InstancePool | `InstancePool` | Add/remove/iterate visible instances |
| RenderGraphDAG | `RenderGraph` | Topological sort produces valid execution order |
| BatchAssembly | `CommandRecorder` | Draw calls grouped correctly by shader/material |

### 10.2 Integration Tests

| Test | Target | Pass Criteria |
|------|--------|---------------|
| VulkanRenderTriangle | End-to-end | Triangle rendered, screenshot matches |
| MetalRenderTriangle | End-to-end | Triangle rendered, screenshot matches |
| VulkanRenderCube | End-to-end | Colored cube rendered with PBR material |
| MetalRenderCube | End-to-end | Colored cube rendered with PBR material |
| Performance10KInstances | Both backends | 10,000 instanced cubes < 1ms GPU time |

### 10.3 Build Verification

- [ ] `build.sh Renderer/Vulkan -dev` succeeds
- [ ] `build.sh Renderer/Metal -dev` succeeds
- [ ] `build.sh Applications/TriangleExample -dev` succeeds
- [ ] Unit tests in `Engine/Test/UnitTestCollection.cpp` pass

---

## 11. Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Data layout | SoA | Cache efficiency on hot paths (position iteration for culling) |
| Entity system | Sparse set | O(1) existence check, contiguous iteration |
| Render approach | Forward | Simpler than deferred; sufficient for lightweight scope |
| Shader format | Platform-native (SPIR-V / MSL) | No runtime translation layer needed |
| RHI depth | Thin abstraction | Vulkan and Metal are similar enough; deep abstraction adds cost |
| Uniforms | Push constants + UBO | Per-material data in push constants, per-frame data in UBO |
| Parallelism scope | CPU-side only | GPU handles parallelism internally; CPU recording parallelized |

---

## 12. Open Questions

| # | Question | Impact |
|---|----------|--------|
| Q1 | Should shadow maps use PCF or VSM? | Affects shadow shader complexity |
| Q2 | Should materials support transparency (alpha test)? | Adds blend state handling |
| Q3 | How many lights max? Fixed per-material or dynamic? | Affects push constant size |
| Q4 | Should we use Vulkan dynamic rendering or render passes? | Affects Vulkan command recording API |
| Q5 | Metal backend: compile MSL at runtime or pre-compile to `.metallib`? | Affects shader loading pipeline |

---

*End of Design Document*
