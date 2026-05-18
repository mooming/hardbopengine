# HardBop Engine — RHI Architecture (Data-Oriented Design)

> **Architecture:** Hybrid Pipeline (Extract → Prepare → Render Graph → Submit)  
> **Core Principle:** Bindless, Stateless, and Data-Oriented.  
> **Shader Philosophy:** HLSL as Single Source of Truth (SM 6.6+), cross-compiled to DXIL/SPIR-V/MSL

---

## Hybrid Pipeline Architecture Overview

The HardBop Engine RHI implements a **4-phase Hybrid Pipeline** that separates rendering concerns into distinct, data-oriented stages:

### Phase 1: EXTRACT
- **Responsibility:** Lockless extraction of ECS world data to CPU-side SoA buffers
- **Output:** Contiguous arrays (TransformStack, MeshBuffer, MaterialIndexBuffer)
- **Pattern:** Copy-on-write from immutable ECS snapshots to lockless extraction buffers
- **Data Layout:** 128-byte aligned transform stack, 192-byte aligned mesh instances

### Phase 2: PREPARE  
- **Responsibility:** Compute shaders execute culling, LOD selection, TLAS building
- **Output:** CullMaskBuffer (visible instances), DepthPrepassBuffer, InstanceSortBuffer
- **Parallelism:** Multi-threaded worker threads with lockless command recording
- **Intermediate Buffers:** Frustum culling → BVH build → MeshID generation

### Phase 3: RENDER GRAPH
- **Responsibility:** DAG construction with dependency resolution and barrier injection
- **Nodes:** Compute, Graphics, RayTracing, Post-process passes
- **Barrier Generation:** Automatic VK Pipeline Barrier / DX12 Enhanced Barrier / MTL ResourceBarrier
- **Temporal Handling:** Livelock tracking for history-critical resources (TAA/Tensor)

### Phase 4: SUBMIT
- **Responsibility:** Radix-sorted command buckets with zero lock contention
- **Sort Key Layout:** 63-bit PassID | 59-48 PSOHash | 47-32 MatIdx | 31-0 DepthSort
- **Stateless Patterns:** Thread-local command buffers, parallel aggregation
- **Zero Lock Contention:** Direct pointer access via thread-local buckets

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ [ GAME WORLD (ECS) ]    ECS World Snapshot                                    │
│ - ActorTransformComponent          - MeshHandle                               │  
│ - MaterialInstanceHandle           - VisibilityData                           │
└─────────────────────────────┬─────────────────────────────────────────────────┘
                              │
                              ▼  (PHASE 1: EXTRACT)
              ┌────────────────── Lockless Copying / SoA Extraction ────────────┐
              │  TransformStack<MAX_TRANSFORMS>       MeshBuffer<MAX_INDICES>   │  
              │  MaterialIndexBuffer<MAX_ENTITIES>    VisibilityCullingMask      │
              └─────────────────────────────────────┬───────────────────────────┘
                                                    │
                                                    ▼ (PHASE 2: PREPARE)
            ┌─────────────────── Compute Shaders / Worker Threads ───────────────┐
            │  Frustum Culling        → CullMaskBuffer                           │  
            │  Hi-Z Occlusion         → DepthPrepassBuffer                       │
            │  LOD Selection          → InstanceSortBuffer                       │
            │  TLAS Build (DXR/VK/MTL)→ AccelerationStructDesc                   │
            └───────────────────────────────┬────────────────────────────────────┘
                                            │
                                            ▼ (PHASE 3: RENDER GRAPH)
              ┌─────────────────── Dependency Resolution / Barrier Injection ────┐
              │  DAG Construction          Alias Detection                        │  
              │  Resource Livelock Tracking                                       │  
              │  Barriers Injected (VkPipelineBarrier2/DX12/Metal)                │
              └─────────────────────┬───────────────────┬────────────────────────┘
                                    │                   │
              ┌─────────────────────▼────┐  ┌───────────▼──────────┐            │
              │ Compute Pass Node        │  │ Graphics Pass Node    │◄ History-Critical         │  
              │ (Transient GPU Buffers)  │  │ PSO + Bindless Handles│      Flags            │
              ├────────────◄─────────────┤  └───────────────────────┘                   ▲   │
              │ Ray Tracing Pass Node    │                                            │   │  
              │ (TLAS/SBSBR Desc)        │ Temporal History Buffers           ┌───────┴───┴──┐│
              └─────────────────────────┘                                    │ POST PROCESS ││
              ┌─────────────────────►◄─── Blending/Tonemap Node             └───────────────┘│
              │                                                   (Temporal Replay Flag)          │
              └──────────────────────────────────────────────────────────────────────────────┘
                                            │
                                            ▼ (PHASE 4: SUBMIT)
              ┌────────────────── SortCommandKeys / Radix Sort ──────────────────┐
              │ SortKey Layout: 63-bit PassID | 59-48 PSOHash |                     │  
              │ 47-32 MatIdx | 31-0 DepthSort                                     │  
              │ Buckets[64] = ThreadLocalList<DrawIndirectDesc> (SoA for GPU)     │
              └───────────────────┬───────────────────── Lock-Free Aggregation ───┘
                                  ▼
              ┌────────────────── Hardware Submission (Zero Contention) ─────────┐
              │ DX12: ID3D12GraphicsCommandList4 + Enhanced Barriers             │  
              │ Vulkan: VkCommandPool + VkPipelineBarrier2 + descriptor_indexing  │
              │ Metal: MTLCommandBuffer with MTLArgumentBuffer for bindless       │
              └──────────────────────────────────────────────────────────────────┘
```

---

## 1. High-Level Data Flow Diagram

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [ GAME WORLD (ECS) ]                                                              │
│   - ActorTransformComponent              - MeshHandle                          │
│   - MaterialInstanceHandle               - VisibilityData                       │
└──────────────────────────────────────────────────────────────────────────────────┘
                  │
                  │ (PHASE 1: EXTRACT - Lockless Copying / SoA)
                  │  • TransformStack<MAX_TRANSFORMS>      • MeshBuffer<MAX_INDICES>
                  │  • MaterialIndexBuffer<MAX_ENTITIES>   • VisibilityCullingMask
                  ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [ RHI TRANSIENT STORAGE ]    <─── [ RHI BINDLESS HEAPS ]                         │
│                                                                          ┌────▼────┐
│  Contiguous SoA Arrays:                                                  │ TEX/HB │
│  • TransformStack (128-byte aligned)                                     │ BUFFER │
│  • MeshInstanceArray (192-byte aligned)                                  ├─────┬───┤
│  • MaterialBindlessSet                                                   │ GPU │CPU │
│                                                                          └────────┴───┘
└──────────────────────────────────────────────────────────────────────────────────┘
                  │
                  │ (PHASE 2: PREPARE - Compute Shaders / Worker Threads)
                  │  • Frustum Culling     → CullMaskBuffer
                  │  • Hi-Z Occlusion      → DepthPrepassBuffer
                  │  • LOD Selection       → InstanceSortBuffer
                  │  • TLAS Build (DXR/VK/MTL) → AccelerationStructDesc
                  │  • MeshID Generation   → BindlessMeshHandles
                  ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [ GPU CULLING & LOD PIPELINE ]                                                   │
│                                                                          ▲────◄───┘
│  ├─ Frustum Culling (Bounding Volume Hierarchy + Frustum)                         │
│  ├─ Hi-Z Occlusion Rejection (3-stage pipeline with depth comparison)              │
│  ├─ TLAS Build (Mesh Grouping + BVH Construction - all APIs unified via HLSL)      │
│  └─ Instance Data Buffer Generation (Interleaved/SoA hybrid for GPU access)        │
└──────────────────────────────────────────────────────────────────────────────────┘
                  │
                  │ (PHASE 3: RENDER GRAPH - Dependency Resolution)
                  │  • DAG Construction          • Alias Detection
                  │  • Resource Livelock Tracking • Barrier Injection (VkPipelineBarrier2,
                  │    (Temporal History Buffers)   DX12 Enhanced Barrier, MTLResourceUsage)
                  ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [ RENDER GRAPH DAG ]    <─── [ RESOURCE ALIASING / BARRIER GENERATION ]          │
│                                                                          ┌────▼────┐
│  RenderGraph<Passes, Dependencies>                                        │ TEMP   │
│  ├─ Compute Pass Node (Transient GPU Buffers)      ├◄─────────────────────┤ HISTS │
│  ├─ Graphics Pass Node (PSO + Bindless Handles)    │ RESOURCES            │        │
│  ├─ Ray Tracing Pass Node (TLAS/SBSBR Desc)       ├──┐                     │        │
│  └─ Blending/Tonemap Node (Post-process Buffers)   │◄┘                     │ TEMP  │
│                                                   ▲                       ◄────────┤
│  Injected Barriers:                               │                        ┌──────▼┐
│  • VkPipelineBarrier2 / D3D12_PIPELINE_BARRIER    │ History-Critical     │ POST   │
│    - ResourceBarriers (Layout transitions)        │ flags                │ PROCS  │
│    - MemoryBarriers (Shared/NonShared)           └───────────────────────┘
│  • MTLResourceBarrier + Usage Tracking                                                    │
└──────────────────────────────────────────────────────────────────────────────────┘
                  │
                  │ (PHASE 4: SUBMIT - Stateless Command Buckets)
                  │  • SortCommandKeys (64-bit radix sort)
                  │  • Parallel Bucket Recording (Thread-local command lists)
                  │  • State Change Minimization via grouping
                  ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [ SORTED COMMAND BUCKETS ]                                                       │
│                                                                          ┌────▼────┐
│  Key Layout: 63-bit PassID | 59-48 PSOHash | 47-32 MatIdx | 31-0 DepthSort     │
│                                                                          │ SORT   │
│  Buckets[64] = ParallelRecordingThreadLocalList<DrawIndirectDesc>           │ CMD   │
│             (Interleaved/SoA for GPU submission)                             │ BKTS  │
└──────────────────────────────────────────────────────────────────────────────────┘
                  │
                  │ (Hardware Submission - Zero Lock Contention)
                  ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [ NATIVE API ]                                                                   │
│   ├─ DX12: ID3D12GraphicsCommandList4 + Enhanced Barriers                          │
│   ├─ Vulkan: VkCommandPool + VkPipelineBarrier2 + VK_EXT_descriptor_indexing      │
│   └─ Metal: MTLCommandBuffer (with MTLArgumentBuffer for bindless)                 │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Interface & Data Structures (C++23)

### A. HLSL-Centric Bindless Resource Management

Unified 32-bit handle architecture with cross-platform descriptor mapping:

```cpp
#pragma once
#include "Engine/Math.h"
#include "Engine/Allocator.h"
#include <array>
#include <optional>

namespace hbop {
namespace gfx {

// ============================================================================
// UNIFIED BINDLESS RESOURCE INTERFACE
// ============================================================================

using ResourceHandle = uint32_t; // 32-bit index into global descriptor arrays

constexpr uint32_t BINDLESS_ROOT_DESC_SLOT = 0u;   // Root signature slot 0 (DX12)
constexpr uint32_t BINDLESS_BUFFER_SLOT   = 4u;    // Slot 4: SRVBuffers/UBO
constexpr uint32_t BINDLESS_TEXTURE_SLOT  = 8u;    // Slot 8: Sampler/Texture arrays
constexpr uint32_t BINDLESS_RAYGEN_SLOT   = 12u;   // Slot 12: Ray tracing pipelines

// ============================================================================
// HLSL Bindless Structure (SM 6.6+ Source of Truth)
// ============================================================================

namespace hlsl {
// Single source shader interface - cross-compiled to all APIs
struct __attribute__((abi("hlsl3_0"))) BindlessResourceTable {
    StructuredBuffer<uint4>      s_rtvTextures[MAX_BINDLESS_TEXTURES];   // Read-only textures
    UnorderedAccessWriter<float4> u_rwDepthBuffer;                        // Depth/UAV for compute
    Textures2D<float>             t_diffuseTextures[MAX_BINDLESS_SAMPLERS]; // Diffuse atlas
    StructuredBuffer<uint32_t>    s_meshIDs[MAX_BINDLESS_MESHS];         // Mesh instance IDs
    ConstantBuffer<Mat4x4>        cb_views[MAX_BINDLESS_VIEWS];          // Camera/projection views
    
    // Ray tracing hit data (DXR/VK RT/Metal RayTracing unified)
    Textures2D<uint>              t_raygenPayloads[MAX_BINDLESS_RAYGENS]; 
};

struct __attribute__((abi("hlsl3_0"))) BindlessMaterialIndices {
    ConstantBuffer<vec4>          c_matIndices[MAX_MATERIAL_INDICES];     // Material ID arrays
    ConstantBuffer<Sampler>       s_materialSamplers[MAX_MATERIAL_SAMPLERS]; 
};
} // namespace hlsl

// ============================================================================
// CPU-SIDE BINDLESS DESCRIPTOR POOL
// ============================================================================

struct BindlessDescriptorPool {
private:
    using TexRTV = void*;   // API-specific RTV/SRV handles
    using TexSampler = void*; // API-specific sampler handles
    
public:
    std::array<TexRTV, MAX_BINDLESS_TEXTURES> m_rtvTextures;
    std::array<void*, MAX_BINDLESS_MESHS>     m_meshHandles;
    std::array<void*, MAX_MATERIAL_INDICES>   m_matIndices;
    uint32_t                                  m_generation[MAX_BINDLESS_TEXTURES]; // For stale detection
    std::optional<uint32_t>                   m_lastValidRenderFrame;
    
    void*                                     m_cpuDescriptorHeapDX12 = nullptr;
    VkDescriptorPool                          m_vulkanDescriptorPool = VK_NULL_HANDLE;
    id<MTLTexture>                            m_metalTextureArray     = nullptr;
    
    // API-specific descriptor validation (Tier 2 Metal limits)
    bool IsWithinMetalTier2Limit() const; 
    
    void SplitIntoPages(ResourceHandle handle, uint32_t* outPageIndex);
};

} // namespace gfx
} // namespace hbop
```

#### PSO Caching & Management (Graphics/Compute/RT Unified via HLSL Source)

Unified pipeline state caching with versioning for automatic recompilation triggers:

```cpp
struct PipelineStateObjectDescriptor {
    uint32_t            shaderProgramHash;  // High-16 bits of full hash for quick matching
    ResourceHandle      vertShaderHandle;   // Bindless vertex/input signature handle
    ResourceHandle      hullShaderHandle;   // Hull tessellation input
    ResourceHandle      domShaderHandle;    // Domain tessellation eval
    ResourceHandle      geoShaderHandle;    // Geometry output
    ResourceHandle      meshShaderHandle;   // Mesh output
    ResourceHandle      fragShaderHandle;   // Fragment shader
    ResourceHandle      computeShaderHandle;// Compute variant
    uint32_t            rtPipelineVersion;  // Version for TLAS/SBSBR updates
    
    struct RTDesc {
        ResourceHandle tlasBuildBuffer;      // Build buffer for acceleration structures
        ResourceHandle sbasrHandle;         // Sparse BVH handle (MSAA ray tracing)
    } rt = {};
    
    bool                isRTPipeline = false;  // Graphics vs Compute vs Ray Tracing flag
    bool                requiresBarrierSwap  = true;  // PSO invalidation signal
};

struct PipelineCacheEntry {
    ResourceHandle vertexShader;
    ResourceHandle fragmentShader;   // Or compute/RT shader variant
    ResourceHandle bindlessTextureSet;
    ResourceHandle bindlessBufferSet;
    
    PipelineStateObjectDescriptor psoDesc;
    
    uint32_t cacheVersion = 0;         // Increment on resource handle changes in set
    bool      compiled = false;        // PSO successfully compiled for current API
    
    void Invalidate() {
        compiled = false;
        cacheVersion++;                 // Bump version to force reload from shaders
    }
};

class PSOCache : public VirtualDestructor {
public:
    std::unordered_map<uint32_t, PipelineCacheEntry> graphicsPSOs;
    std::unordered_map<uint32_t, PipelineCacheEntry> computePSOs;
    std::unordered_map<uint32_t, PipelineCacheEntry> rayTracingPSOs;
    
    bool GetCachedPSO(
        const PipelineStateObjectDescriptor& desc, 
        ResourceHandle textureSet,
        ResourceHandle bufferSet,
        PipelineCacheEntry& outPSO
    ) {
        // Compute hashkey from shader handles + PSO descriptor
        uint32_t key = CombineHash(desc.shaderProgramHash, static_cast<uint32_t>(textureSet), static_cast<uint32_t>(bufferSet));
        
        auto it = graphicsPSOs.find(key);
        if (it != graphicsPSOs.end()) {
            outPSO = it->second;
            return outPSO.compiled && !desc.requiresBarrierSwap;
        }
        return false;
    }
    
    // Check for invalidation due to resource lifetime changes
    void CheckResourceLifetimeInvalidation(const ResourceHandle* handles, uint32_t count) {
        // Validate cached PSOs against temporal buffer lifetimes
        for (auto& [_, entry] : graphicsPSOs) {
            if (entry.cacheVersion != ComputeCacheVersionFromHandles(handles, count)) {
                entry.Invalidate();
            }
        }
    }
};

### B. Stateless Command Buckets (64-bit Sort Key Design)

Radix-sort friendly 64-bit sort key minimizing pipeline state changes:

```cpp
#pragma once
#include "Engine/Math.h"

namespace hbop {
namespace gfx {

// ============================================================================
// 64-BIT SORT KEY STRUCTURE - Radix Sort Layout
// Bit layout: [31-0] Depth/Distance | [32-47] Material Index | [48-59] PSO Hash | [63] PassID Flag
// ============================================================================

struct alignas(8) RenderSortKey {
public:
    static const uint64_t PASS_ID_OFFSET      = 63u;                                    // Bit 63 (1 bit)
    static const uint64_t PSO_HASH_OFFSET    = 48u;                                    // Bits 59-48 (12 bits)
    static const uint64_t MATERIAL_INDEX     = 32u;                                    // Bits 47-32 (16 bits)
    static const uint64_t DEPTH_BITMASK      = 0u;                                      // Bits 31-0 (32 bits), mask applies to raw_key

    union {
        struct SortComponents {
            uint8_t pass_id : 1;     // PassID: 0=Graphics, 1=Compute, 2=RayGen, 3=Trace, 4=Miss, 5=ClosestHit
            uint16_t pso_hash : 12;  // PSO Hash: Upper bits of shader program hash for quick grouping
            uint16_t mat_index : 16; // Material Index: Matches bindless texture/buffer indices
            uint32_t depth_distance : 32; // Depth (MSB) + Distance from camera (LSB) for Z-sorting
        };

        uint64_t raw_key;

        inline SortComponents& components() { return SortComponents{}; }
        inline const SortComponents& components() const { return components(); }

        // Constructors for radix sort friendliness (bitwise operations only)
        constexpr static RenderSortKey MakeKey(
            uint8_t passId, 
            uint16_t psoHash,
            uint16_t materialIndex,
            int32_t depth = INT32_MAX
        ) {
            RenderSortKey key{};
            key.components().pass_id     = passId      ? 1u : 0u;
            // Use upper bits for hash to maintain sort order with small depth changes
            key.components().pso_hash    = static_cast<uint16_t>(psoHash >> (32 - 12)); 
            // Material index in middle bits (matches bindless table)
            key.components().mat_index   = materialIndex;
            // Depth MSB + distance LSB for Z-stable sorting
            key.components().depth_distance = static_cast<uint32_t>(static_cast<int32_t>(depth) >> 16);
            return key;
        }

        inline uint32_t GetMaterialIndex() const { return static_cast<uint32_t>(raw_key & DEPTH_MASK); }
        inline uint16_t GetPSOHash()   const { return static_cast<uint16_t>((raw_key >> PSO_HASH_OFFSET) & 0xFFF); }
        inline bool     IsGraphicsPass()const { return (raw_key & PASS_FLAG) == 0; }
    };

private:
    static constexpr uint64_t PASS_FLAG      = 1ULL << PASS_ID_OFFSET;
    static constexpr uint64_t PSO_HASH_MASK  = ((1ULL << 12) - 1) << PSO_HASH_OFFSET;
    static constexpr uint64_t MAT_INDEX_MASK = ((1ULL << 16) - 1) << MATERIAL_INDEX;
    static constexpr uint64_t DEPTH_MASK     = ((1ULL << 32) - 1);

public:
    constexpr explicit RenderSortKey(uint64_t key) : raw_key(key) {}
    // No copy constructor needed POD semantics

    friend bool operator==(RenderSortKey a, RenderSortKey b) { return a.raw_key == b.raw_key; }
};

// ============================================================================
// DRAW COMMAND (SoA Layout for GPU Submission)
// ============================================================================
// Layout: [DrawCmd 256 bytes] = [SortKey @0][InstanceCount @32][IdxOffset/VertexOff @48][IndirectAddr @72]
// This layout is GPU-cache friendly when batched

struct alignas(32) DrawCommand { // 32-byte aligned for cache line efficiency
public:
    RenderSortKey sortKey;
    
    uint16_t      instanceCount;       // # instances to draw (uint16_t for SIMD packing)
    uint16_t      firstInstance;       // First instance in mesh
    
    uint32_t      firstMeshIndex;      // Offset into mesh buffer array
    uint32_t      indexBufferOffset;   // Index buffer offset in bytes
    uint32_t      primitiveType        = DrawPrimitiveTriangles;
    
    uint32_t      indirectCommandAddr;  // DX12 indirect argument buffer address / VK indirect write location
    ResourceHandle meshHandle;          // Bindless mesh resource (SRV + UAV if mutable)
    
private:
    static constexpr uint32_t MaxInstanceCount = 0xFFFFu; // Limit for SIMD compatibility

public:
    inline uint16_t GetFirstMeshIndex()  const { return firstMeshIndex; }
    inline bool     HasIndirectCommand() const { return indirectCommandAddr != 0; }
};

// ============================================================================
// COMPUTE COMMAND (Unified dispatch across all APIs)
// ============================================================================

struct alignas(32) ComputeCommand {
public:
    RenderSortKey sortKey; // Same key structure for pipeline state grouping
    
    uint32_t      threadGroupSizes[3] = {0, 0, 0}; // X/Y/Z threads per workgroup
    ResourceHandle computeShaderHandle;             // Bindless shader program handle
    ResourceHandle sharedMemoryBufferIdx;           // Shared/UAV buffer for workgroup state
    
    uint64_t      indirectDispatchAddr = 0;         // DX12/Vulkan/MTL indirect dispatch location
};

} // namespace gfx
} // namespace hbop
```

### C. Render Graph (DAG) Interface with Cross-Platform Barriers

Automatic barrier injection for resource transitions across all native APIs:

```cpp
#pragma once
#include "Engine/HAL.h"
#include <vector>
#include <unordered_map>
#include <functional>

namespace hbop {
namespace gfx {

// ============================================================================
// RENDER GRAPH NODE ABSTRACTION
// ============================================================================

struct RenderGraphNodeInput {
    ResourceHandle resourceIdx;        // Index into bindless descriptor array
    RHIResourceUsage usage;             // VK/MTL: VK_STORAGE_READ, D3D12_SHADER_RESOURCE, MTLReadOnly
    
    struct Stages {
        PipelineStageFlags src_read;     // Sources read by this node
        PipelineStageFlags dst_write;    // Sources written by this node
    } transfer_stages;
};

struct RenderGraphNodeOutput {
    ResourceHandle resourceIdx;
    RHIResourceUsage usage;
};

// ============================================================================
// CROSS-PLATFORM PIPELINE STAGE MAPPING
// ============================================================================

using PipelineStageFlag = uint32_t; // 4-bit flags per API-specific stage bit positions

namespace ps_stages {
constexpr PipelineStageFlag SHADER_COMPUTE = 1 << 0;
constexpr PipelineStageFlag SHADER_VERTEX  = 1 << 1;
constexpr PipelineStageFlag SHADER_FRAGMENT= 1 << 2;
constexpr PipelineStageFlag RAYGEN        = 1 << 3;
} // namespace ps_stages

// ============================================================================
// INJECTABLE BARRIER COMMANDS (Unified Interface)
// ============================================================================

struct BarrierCommand {
    enum class Type {
        VK_PIPELINE_BARRIER,           // Vulkan-specific structure
        D3D12_RESOURCE_BARRIER,        // DirectX 12 specific structure
        MTL_COMMAND_BUFFER,             // Metal resource usage barrier
        MEMORY_BARRIER,                // Atomic operations on memory locations
        NONE                           // No barrier needed (same read/write pattern)
    };

    Type type;
    
    union Args {
        struct VkBarrier {
            VkPipelineStageFlags src_stage;
            VkPipelineStageFlags dst_stage;
            VkAccessFlags        src_access;
            VkAccessFlags        dst_access;
            bool                 memory = false;
        } vk;
        
        struct Dx12Barrier {
            D3D12_RESOURCE_BARRIER_DESC desc;           // DX12 resource barrier layout
            ID3D12Resource* p_resource_dx12              = nullptr;
        } dx12;

        struct MtlBarrier {
            MTLResourceUsageMask prev_usage;          // Metal previous usage mask
            MTLResourceUsageMask new_usage;           // Metal new usage mask
            id<MTLBuffer> buffer                      = nullptr;  // Optional buffer for atomic ops
        } mtl;
        
        bool IsMemoryBarrierOnly() const { return type == Type::MEMORY_BARRIER; }
    };

    Args args;
};

// ============================================================================
// RENDER GRAPH DAG COMPILER BASE CLASS
// ============================================================================

class RenderGraphCompiler : public VirtualDestructor {
public:
    // Build pass execution with automatic barrier injection
    virtual void CompileExecution(CommandList& cmd, const ResourceTracker& tracker) = 0;
    
    // Dependency resolution for temporal reprojection (defer/reduce pipelines)
    virtual void ResolveTemporalHistory(RenderGraphDAG& dag) = 0;
    
protected:
    std::vector<BarrierCommand> m_injectedBarriers;      // Graph-compiled barriers from resource aliasing
    
    // Inject barrier between nodes based on usage analysis
    BarrierCommand CreateResourceBarrier(
        const ResourceHandle& src, 
        const ResourceHandle& dst,
        RHIResourceUsage srcUsage, 
        RHIResourceUsage dstUsage
    ) {
        RenderGraphNodeInput input = {};
        input.resourceIdx = static_cast<uint32_t>(src);
        
        BarrierCommand barrier;
        // Auto-select type based on platform:
        #ifdef PLATFORM_VULKAN
            barrier.type = BarrierCommand::Type::VK_PIPELINE_BARRIER;
            barrier.args.vk.src_stage   = GetVulkanStageFlagsFromRHIUsage(srcUsage);
            barrier.args.vk.dst_stage   = GetVulkanStageFlagsFromRHIUsage(dstUsage);
            barrier.args.vk.src_access  = GetVkAccessFlags(srcUsage);
            barrier.args.vk.dst_access  = GetVkAccessFlags(dstUsage);
        #elif defined(PLATFORM_D3D12)
            barrier.type = BarrierCommand::Type::D3D12_RESOURCE_BARRIER;
            // DX12 Enhanced barrier via ID3D12PipelineBarrierDesc
            barrier.args.dx12.desc.type      = D3D12_RESOURCE_BARRIER_SYSTEM_TO_GPU;
        #else // METAL
            barrier.type = BarrierCommand::Type::MTL_COMMAND_BUFFER;
            barrier.args.mtl.prev_usage      = GetMetalUsageFromRHIUsage(srcUsage);
            barrier.args.mtl.new_usage       = GetMetalUsageFromRHIUsage(dstUsage);
        #endif
        
        return barrier;
    }

private:
    VkPipelineStageFlags GetVulkanStageFlagsFromRHIUsage(RHIResourceUsage usage) const;
    MTLResourceUsageMask GetMetalUsageFromRHIUsage(RHIResourceUsage usage) const;
};

// ============================================================================
// HISTORY-Critical Resource Flag (Temporal Replay Prevention)
// ============================================================================

enum class TemporalReplayFlag : uint8_t {
    HistoryCritical,      // Must preserve for temporal algorithms (TAA/DLSS/MLAA)
    Transient,            // Can be rebuffered safely
    RenderTarget          // Standard render target lifecycle
};

struct RenderGraphNodeTraits {
    TemporalReplayFlag historyFlags = TemporalReplayFlag::Transient;
    
    // For UAVs (storage buffers with read/write): prevent aliasing issues
    bool allowAliasRead   = false;   // Allow reading from previously written data?
    bool requireFullBarrier() const {
        return !allowAliasRead || static_cast<RHIGraphicsUsage>(usage) == VK_FRAGMENT_SHADER_ATTACHMENT;
    }
};

} // namespace gfx
} // namespace hbop
```

### D. Parallel Command Recording (Multi-threaded Submit Phase)

Lock-free command list generation using thread-local storage:

```cpp
#pragma once
#include <atomic>
#include <thread>

namespace hbop {
namespace gfx {

// ============================================================================
// THREAD-LOCAL COMMAND BUCKET WITH LOCK-FREE AGGREGATION
// 64 Buckets x 1 thread per bucket on dual-core/threaded builds
// ============================================================================

struct ThreadLocalCommandBucket {
private:
    ThreadLocalStorage<DrawIndirectDesc* const> m_drawBuckets; // Flat pointer array (64 buckets)
    volatile std::atomic<uint32_t>              m_builtCount;   // Atomic counter for built commands
    
public:
    void RecordGraphicsCommand(DrawSortKey& key, DrawCommand& cmd);
    
    // Parallel radix sort across all threads at the end of frame
    void AggregateAndRadixSortParallel(
        std::vector<DrawCommand>& outCommands,
        std::vector<uint32_t>& outIndices // For stable merge with depth sorting
    );
};

// ============================================================================
// STATELESS COMMAND BUCKET INTERFACE (Platform-specific implementations)
// ============================================================================

struct ICommandBucket : public VirtualDestructor {
public:
    virtual void BeginSubmit() = 0;     // Initialize platform-specific command list
    
    virtual void EndSubmit() = 0;       // Flush command list to device queue
    
protected:
    // Platform-specific command buffer pointers (thread-local)
    std::atomic<void*> m_currentCmdListDX12 = nullptr;
    VkCommandBuffer    m_currentCmdBufVulkan= VK_NULL_HANDLE;
    id<MTLCommandQueue> m_commandQueueMetal  = nullptr;
    
    bool IsRecording() const;           // Platform-specific recording state check
};

// ============================================================================
// PLATFORM-SPECIFIC BUCKET IMPLEMENTATIONS (Inline in respective RHI headers)
// ============================================================================

// DX12: Enhanced multi-threaded command lists with zero contention
namespace dx12 {
struct CommandBucket : public ICommandBucket {
private:
    ID3D12GraphicsCommandList4* m_threadLocalLists[D3D12_COMMAND_LISTS_PER_THREAD];
    
public:
    void BeginSubmit() override {
        // Each thread owns a command list - no synchronization needed
        m_currentCmdListDX12 = GetThreadCommandList();
        m_currentCmdListDX12->ResourceBarrier(1, &m_dx12EnhancedBarrier);
    }
    
    // Zero-lock recording using direct pointer access
    void RecordSortKeyBasedDraw(DrawSortKey& key, DrawCommand& cmd);
};

// Vulkan: Multiple command buffers per pool with atomic dispatch tracking
namespace vulkan {
struct CommandBucket : public ICommandBucket {
private:
    std::array<VkCommandBuffer, 256> m_commandBuffersFlat; // Pool of ready-to-dispatch buffers
    
public:
    void BeginSubmit() override {
        m_currentCmdBufVulkan = AllocateVkCommandBufferFromPool();
        ResetVkCmdBuf(m_currentCmdBufVulkan);
    }
    
    void RecordDraw(DrawSortKey& key, DrawCommand& cmd);
};
} // namespace vulkan

// Metal: MTLCommandQueue with multiple buffers for concurrent recording
namespace metal {
struct CommandBucket : public ICommandBucket {
private:
    id<MTLCommandBuffer> m_currentBatch[16];     // Multiple command buffers per batch
    
public:
    void BeginSubmit() override {
        m_currentBatch[thread::id()] = NewMetalCmdBufferForRecording();
    }
};
} // namespace metal

} // namespace gfx
} // namespace hbop
```

---

## 3. HLSL & API Mapping Matrix

| Feature | HLSL (SM 6.6+) | DirectX 12 | Vulkan (Descriptor Indexing) | Metal (Argument Buffers) |
| :--- | :--- | :--- | :---| :---|
| **Resource Binding** | Unbounded Descriptor Tables (`register(t0)`)| Descriptor Heap Pointer `ID3D12_CPU_DESCRIPTOR_HEAP` | `VK_DESCRIPTOR_BINDING` + Array Indexing | `MTLArgumentBuffer` + Texture/Buffer Arrays |
| **Indexing Syntax** | `myTex[index]` | Index into heap: `GetDescriptorHeap().Get()->SetResource(i)` | Index into `VkDescriptorSet` array | Index into texture buffer: `texture(sampler, [index])` |
| **Ray Tracing Access** | Via Bindless Handles (`rayHitObject`) | `ShaderResourceView` from Descriptor Heap | `DescriptorSet` traversal via `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR` | `mtl::acceleration_structure` + MTL argument buffers |
| **UAV Access Pattern** | `RWTexture2D<float4> tex[] : register(t1)` | `ID3D12Resource*` + Enhanced Barriers | `VkDescriptorSet` with `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` | `MTLReadBuffer`/`MTLReadWriteBuffer` |
| **PSO Types** | Unified HLSL source → DXIL/SPIR-V/MSL | `ID3D12PipelineState` (Graphics/Compute/RayTracing unified) | `VkPipeline` + `VkPipelineShaderStageCreateInfo` | `MTLRenderPipelineState` / `MTLComputePipelineState` |
| **Barrier Injection** | Implicit via descriptor layout transitions | `ID3D12PipelineBarrierDesc` + Enhanced Barriers | `VkPipelineBarrier2` (VK_KHR_pipeline_barrier_2) | `MTLResourceBarrier` auto-generated by RHI compiler |

### HLSL Cross-Compilation Flow

```
HLSL Source (.hlsl)
    │
    ├───► DXC ─────────┐
    │                   ▼
    │               DXIL (DX12 Native)
    │                   │
    └───────────────────┘
    │
    ├───► DXC ─┬────► SPIR-V ───► vkInitializePipelineShaderModule() → VkPipeline 
    │          │                    VkCreateRenderPass2() with VK_EXT_descriptor_indexing
    │          ▼
    │       HLSL-to-SPIR-V (via HLSL-SHADER-CONVERTER)
    │          │
    └───────────┴────► SPIR-V (Vulkan Native via MoltenVk / SPIRV-Cross)
                       VkCreatePipeline() with indirect draw support


    │
    └───► Metal Shader Converter (msbc.exe / mslc.exe) ───► MSL Source (.metal)
                              │
                              ▼
                             MTLCompileLibrary → .metal_binary
```

---

## 4. Critical Edge Cases

### Bindless Descriptor Exhaustion (Metal Tier 2 Limits)

Metal's hardware limits require descriptor partitioning:

```cpp
// Split global indices into manageable pages for Metal Argument Buffers
inline void DescriptorsSplitIntoPages(BindlessDescriptorSet& set, uint32_t maxBindingsPerPass = 16) {
    // Example: Limit to 64 textures per buffer (Metal Tier 2 limit)
    const uint32_t MaxTexturesPerPage = 64u;
    
    uint32_t currentPageIdx = 0;
    for (uint32_t i = 0; i < MAX_BINDLESS_TEXTURES; ) {
        set.textures[currentPageIdx] = SetNextDescriptorBatch(set.textures, i, MaxTexturesPerPage);
        i += MaxTexturesPerPage;
        
        // Metal Tier 2 check (hardware limit validation)
        if (!IsBelowMetalTier1()) {
            // Split into additional argument buffers for MTLArgumentBuffers
            MTLBuffer newBuffer = AllocateMTLArgumentBufferFromPool(MaxTexturesPerPage);
            currentPageIdx++;
        }
    }
}

// Stale handle detection across frames
constexpr uint32_t HANDLE_GENERATION_MASK     = 0xFFu; // Low 4 bits for generation tracking
constexpr uint32_t HANDLE_INDEX_MASK          = ~HANDLE_GENERATION_MASK << 4; 
inline bool IsHandleStale(ResourceHandle handle, ResourceHandle current) {
    return (handle & HANDLE_GENERATION_MASK) != Generation();
}
```

### Temporal History Aliasing Prevention

TLAS / Motion Vectors / TAA history preservation:

```cpp
// History-Critical resources must maintain temporal consistency across frames
struct TemporalTextureMetadata {
    uint32_t generation;                      // Frame counter for alias detection
    vec2      motionVector[MAX_HISTORIES];     // For TAA/DLSS motion vectors
    bool      isHistoryCritical = false;       // Prevents premature reclamation
    
    void EnsureTemporalConsistency() const {
        if (isHistoryCritical && generation != GetCurrentFrameGen()) {
            // Must copy motion vectors and history data before swap
            CopyMotionVectorsToHistoryBuffer(motionVector);
        }
    }
};

// TLAS must be rebuilt after mesh updates, but preserve culling results for frames without changes
const bool RebuildTLASMandatoryAfterMeshUpdate = true; 

// For Ray Tracing Miss/Hit shaders: ensure bindless handles to acceleration structures are history-critical
constexpr uint32_t RAY_ACCELERATION_STRUCTURE_HANDLE_OFFSET = 10u; // Specific handle slot for RT
```

### Multi-threaded Command Recording Pitfalls

- **Lock Contention:** Use thread-local buckets (no mutex during recording)
- **Memory Ordering:** Explicit `memory_fencing` between extraction phase and command submission
- **Descriptor Cache Coherence:** Invalidate CPU caches when swapping descriptor heaps across threads


### HLSL & API Mapping Matrix

| Feature | HLSL (SM 6.6+) | DirectX 12 | Vulkan (Descriptor Indexing) | Metal (Argument Buffers) |
| :--- | :--- | :--- | :---| :---|
| **Resource Binding** | Unbounded Descriptor Tables (`register(t0)`)| Descriptor Heap Pointer `ID3D12_CPU_DESCRIPTOR_HEAP` | `VK_DESCRIPTOR_BINDING` + Array Indexing | `MTLArgumentBuffer` + Texture/Buffer Arrays |
| **Indexing Syntax** | `myTex[index]` | Index into heap: `GetDescriptorHeap().Get()->SetResource(i)` | Index into `VkDescriptorSet` array | Index into texture buffer: `texture(sampler, [index])` |
| **Ray Tracing Access** | Via Bindless Handles (`rayHitObject`) | `ShaderResourceView` from Descriptor Heap | `DescriptorSet` traversal via `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR` | `mtl::acceleration_structure` + MTL argument buffers |
| **UAV Access Pattern** | `RWTexture2D<float4> tex[] : register(t1)` | `ID3D12Resource*` + Enhanced Barriers | `VkDescriptorSet` with `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` | `MTLReadBuffer/MTLReadWriteBuffer` |
| **PSO Types** | Unified HLSL source → DXIL/SPIR-V/MSL | `ID3D12PipelineState` (Graphics/Compute/RayTracing unified) | `VkPipeline` + `VkPipelineShaderStageCreateInfo` | `MTLRenderPipelineState` / `MTLComputePipelineState` |
| **Barrier Injection** | Implicit via descriptor layout transitions | `ID3D12PipelineBarrierDesc` + Enhanced Barriers | `VkPipelineBarrier2` (VK_KHR_pipeline_barrier_2) | `MTLResourceBarrier` auto-generated by RHI compiler |

### HLSL Cross-Compilation Flow

```
HLSL Source (.hlsl)
    │
    ├───► DXC ─────────┐
    │                   ▼
    │               DXIL (DX12 Native)
    │                   │
    └───────────────────┘
    │
    ├───► DXC ─┬────► SPIR-V ───► vkInitializePipelineShaderModule() → VkPipeline 
    │          │                    VkCreateRenderPass2() with VK_EXT_descriptor_indexing
    │          ▼
    │       HLSL-to-SPIR-V (via HLSL-SHADER-CONVERTER)
    │          │
    └───────────┴────► SPIR-V (Vulkan Native via MoltenVk / SPIRV-Cross)
                       VkCreatePipeline() with indirect draw support


    │
    └───► Metal Shader Converter (msbc.exe / mslc.exe) ───► MSL Source (.metal)
                              │
                              ▼
                             MTLCompileLibrary → .metal_binary
```

---

## 4. Critical Edge Cases

### Bindless Descriptor Exhaustion (Metal Tier 2 Limits)

Metal's hardware limits require descriptor partitioning:

```cpp
// Split global indices into manageable pages for Metal Argument Buffers
inline void DescriptorsSplitIntoPages(BindlessDescriptorSet& set, uint32_t maxBindingsPerPass = 16) {
    // Example: Limit to 64 textures per buffer (Metal Tier 2 limit)
    const uint32_t MaxTexturesPerPage = 64u;
    
    uint32_t currentPageIdx = 0;
    for (uint32_t i = 0; i < MAX_BINDLESS_TEXTURES; ) {
        set.textures[currentPageIdx] = SetNextDescriptorBatch(set.textures, i, MaxTexturesPerPage);
        i += MaxTexturesPerPage;
        
        // Metal Tier 2 check (hardware limit validation)
        if (!IsBelowMetalTier1()) {
            // Split into additional argument buffers for MTLArgumentBuffers
            MTLBuffer newBuffer = AllocateMTLArgumentBufferFromPool(MaxTexturesPerPage);
            currentPageIdx++;
        }
    }
}

// Stale handle detection across frames
constexpr uint32_t HANDLE_GENERATION_MASK     = 0xFFu; // Low 4 bits for generation tracking
constexpr uint32_t HANDLE_INDEX_MASK          = ~HANDLE_GENERATION_MASK << 4; 
inline bool IsHandleStale(ResourceHandle handle, ResourceHandle current) {
    return (handle & HANDLE_GENERATION_MASK) != Generation();
}
```

### Temporal History Aliasing Prevention

TLAS / Motion Vectors / TAA history preservation:

```cpp
// History-Critical resources must maintain temporal consistency across frames
struct TemporalTextureMetadata {
    uint32_t generation;                      // Frame counter for alias detection
    vec2      motionVector[MAX_HISTORIES];     // For TAA/DLSS motion vectors
    bool      isHistoryCritical = false;       // Prevents premature reclamation
    
    void EnsureTemporalConsistency() const {
        if (isHistoryCritical && generation != GetCurrentFrameGen()) {
            // Must copy motion vectors and history data before swap
            CopyMotionVectorsToHistoryBuffer(motionVector);
        }
    }
};

// TLAS must be rebuilt after mesh updates, but preserve culling results for frames without changes
const bool RebuildTLASMandatoryAfterMeshUpdate = true; 

// For Ray Tracing Miss/Hit shaders: ensure bindless handles to acceleration structures are history-critical
constexpr uint32_t RAY_ACCELERATION_STRUCTURE_HANDLE_OFFSET = 10u; // Specific handle slot for RT
```

### Multi-threaded Command Recording Pitfalls

- **Lock Contention:** Use thread-local buckets (no mutex during recording)
- **Memory Ordering:** Explicit `memory_fencing` between extraction phase and command submission
- **Descriptor Cache Coherence:** Invalidate CPU caches when swapping descriptor heaps across threads
