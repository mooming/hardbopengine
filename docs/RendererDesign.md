# HardBop Engine — RHI Architecture (Data-Oriented Design)

> **Architecture:** Hybrid Pipeline (Extract $\rightarrow$ Prepare $\rightarrow$ Render Graph $\rightarrow$ Submit)  
> **Core Principle:** Bindless, Stateless, and Data-Oriented.

---

## 1. High-Level Data Flow

The architecture moves from high-level ECS objects to low-level hardware-specific command buffers through four distinct phases.

```text
[ GAME WORLD (ECS) ]
       |
       | (Phase 1: EXTRACT - Lockless Copying/SoA)
       v
[ RHI TRANSIENT STORAGE ] <--- [ RHI BINDLESS HEAPS ]
(Contiguous Arrays of Transforms, MeshIDs, MaterialIndices)
       |
       | (Phase 2: PREPARE - Compute Shaders / Worker Threads)
       v
[ GPU CULLING & LOD PIPELINE ] 
(Frustum Culling $\rightarrow$ Hi-Z Occlusion $\rightarrow$ TLAS Build $\to$ Indirect Gen)
       |
       | (Phase 3: RENDER GRAPH - Dependency Resolution)
       v
[ RENDER GRAPH DAG ] <--- [ RESOURCE ALIASING / BARRIER GENERATION ]
(Compute Passes, Graphics Passes, Temporal History Tracking)
       |
       | (Phase 4: SUBMIT - Stateless Command Buckets)
       v
[ SORTED COMMAND BUCKETS ] 
(64-bit Key Sorting $\rightarrow$ State Change Minimization $\to$ Parallel Recording)
       |
       | (Hardware Submission)
       v
[ NATIVE API (DX12 / VULKAN / METAL) ]
```

---

 $\text{---}$

## 2. Core Interface & Data Structures (C++23)

### A. Bindless Resource Management
The RHI uses a unified 32-bit handle approach. Shaders access textures and buffers via indices into global arrays.

```cpp
using ResourceHandle = uint32_t; // Index into Global Descriptor Array

struct BindlessDescriptorSet {
    // Tracks metadata for the CPU-side management of descriptor heaps
    struct Metadata {
        ResourceHandle textureIdx;
        ResourceHandle bufferIdx;
        uint32_t      generation; // For stale handle detection
    };
};
```

### B. Stateless Command Buckets (64-bit Sort Key)
Sorting by a 64-bit key allows the renderer to minimize all major pipeline state changes in a single radix sort pass.

```cpp
struct alignas(8) RenderSortKey {
    // [Bit 63: PassID] [Bits 59-48: PSO Hash] 
    // [Bits 47-32: Material Index] [Bits 31-0: Depth/Distance]
    uint64_t key;

    struct Layout {
        static constexpr uint64_t PassMask   = 0xF000'0000'0000'0000;
        static constexpr uint64_t PSOMask    = 0x0FFF'0000'0000'0000;
        static constexpr uint64_t BindMask   = 0x0000'FFFF'0000'0000;
        static constexpr uint64_t DepthMask  = 0x0000'0000'FFFF'FFFF;
    };
};

struct DrawCommand {
    RenderSortKey sortKey;
    uint32_t      instanceCount;
    uint32_t      firstIndex;
    uint32_t      vertexOffset;
    uint32_t      indexCount;
    ResourceHandle meshHandle; 
};
```

### C. Render Graph (DAG) Interface
The graph manages the lifetime and synchronization of transient resources.

```cpp
class RenderGraphNode {
public:
    struct Input  { ResourceHandle resource; PipelineStage stage; };
    struct Output { ResourceHandle resource; ResourceUsage usage; };

    virtual void Execute(CommandBuffer& cmd) = 0;

private:
    std::vector<Input>  m_inputs;
    std::vector<Output> m_outputs;
    // Injected by Graph Compiler: VkPipelineBarrier2, MTLResourceBarrier, etc.
    std::vector<BarrierCommand> m_injectedBarriers; 
};
```

---

## 3. HLSL & API Mapping Matrix

| Feature | HLSL (SM 6.6+) | DirectX 12 | Vulkan (Descriptor Indexing) | Metal (Argument Buffers) |
| :--- | :--- | :--- | :--- | :---			|
| **Resource Binding** | Unbounded Descriptor Tables | Descriptor Heap Pointer | `VK_DESCRIPTOR_BINDING` | `MTLArgumentBuffer` |
| **Indexing Syntax** | `myTex[index]` | Index into Heap | Index into Array | Index into Buffer/Texture Array |
| **Ray Tracing Access**| Via Bindless Handles | `ShaderResourceView` | `DescriptorSet` Traversal | `mtl::acceleration_structure` |

---

## 4. Critical Edge Cases

*   **Bindless Descriptor Exhaustion:** Metal (Tier 2) has hardware-specific limits on the number of resources per argument buffer. The RHI must implement **Descriptor Partitioning** to split global indices into manageable pages during the `Prepare` phase.
*   **Temporal History Aliasing:** If the Render Graph aliases a transient texture too early, it destroys temporal data (e.ring Motion Vectors) required for TAA/DLSS. The graph requires **History-Critical** resource flags to prevent premature reclamation.
</div>
