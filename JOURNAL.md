# Journal

## Top-Level Summary

- **Project**: HardBop Engine — high-performance C++23 engine
- **Current focus**: Build system, test output compaction, AGENTS.md revision
- **Build status**: Debug — 51/51 PASS, 4 expected performance warnings
- **Recent**: Removed redundant per-thread `Num Pop` logging from AtomicStackViewTest (2500 lines → 0), reducing test log from 5300→2796 lines (47% drop)

---

## 2026-05-17

### AGENTS.md revision
- Revised "CMake Configuration" section: never edit generated CMakeLists.txt directly; modify `.module.config`/specifier files and regenerate with `makebuild`
- Added "Improving MakeBuild" instruction: extend MakeBuild itself when new build features cannot be expressed through existing config files
- Committed as 6e19f9c

### Build & fixes
- Fixed incomplete `cap`→`capacity` rename in Vector.h (line 249: `std::swap(capacity, rhs.cap)`)
- Debug build passes 51/51 tests (0 fail, 4 known performance warnings)

### Log compaction
- Made `MultiPoolAllocator::PrintUsage()` safe to call regardless of `PROFILE_ENABLED` by gating its body; removed guard from call site — compiler elides the empty function
- Replaced per-iteration affinity logs in TaskStreamAffinity.cpp with first-3/last-3 summary (saved ~1034 lines)
- Removed per-worker range logs from Bagel Problem tests
- Reduced `numGrowth` 10→3 in ImportanceResampling TC2/TC3 (growth tests)
- Reduced `numRepeat` 10→5 in StratifiedSampling test
- Removed per-iteration result logs in MonteCarlo/Stratified/ImportanceResampling tests
- **Removed per-thread `Num Pop` logging from AtomicStackViewTest** — 2500 redundant lines eliminated (47% of log); aggregate summary already present
- Final test log: **2796 lines** (down from original ~7570, **63% total reduction**)

### Coding standards violation fixes (Engine/Core/)
- **Type aliases** (Types.h, Time.h, Runnable.h, TaskStream.h, TaskSystem.h): Renamed to `T`-prefix convention (`Byte→TByte`, `Runnable→TRunnable`, `ThreadID→TThreadID`, etc.), backward-compat aliases preserved in Types.h
- **Exception.h**: Added `final` to `class Exception`
- **CommonUtil.h**: Added `final` to `True_t`/`False_t`; added `[[nodiscard]]`/`noexcept` to `GetAs`, `ToAddress`, `CountOf`
- **Component.h/ComponentSystem.h**: Added `[[nodiscard]]`/`noexcept` to all getters/setters; removed `inline`; `template<typename Component>`→`template<typename TComponent>`
- **Task.h/TaskStream.h/TaskSystem.h**: Added `noexcept` to all member functions; added `[[nodiscard]]` to all getters; wrapped long comments
- **TaskStreamAffinity.h**: Added `noexcept` to all members; `[[nodiscard]]` to getters
- **RangedTask.h**: Added `noexcept` to operators/`Run`/`HasFinished`; `[[nodiscard]]` to `HasFinished`
- **MainThreadTaskQueue.h**: Added `noexcept`/`[[nodiscard]]` to all public methods
- **SystemStatistics.h**: Added `noexcept`/`[[nodiscard]]`; removed `inline`
- **CommandLineArguments.h**: Added `[[nodiscard]]`/`noexcept` to `GetArguments`
- **Debug.h**: Removed all explicit `inline` from function definitions
- **ScopedTime.h**: Added `noexcept` to constructor/destructor
- **Namespace indentation (R3)**: Removed 1 tab indent from all lines inside all `namespace hbe` blocks across every `.h`/`.cpp` file
- **Two blank lines after includes (R12)**: Fixed all `.cpp` files to have 2 blank lines between last include and code body
- **TaskSystem.cpp**: Replaced try/catch exception handling with simple join loop; updated `ThreadID`→`TThreadID`
- **ComponentSystem.cpp**: Removed `virtual` from `override` member functions; fixed `~Test() override`
- **Time.cpp**: Wrapped `using namespace` inside `namespace hbe` block; added `noexcept`; used `TMilliSec`
- **Debug.cpp**: Added copyright header; fixed blank line count

### Coding Standards — Engine/Math (28 files)
- Renamed template params: `Number`→`TNumber`, `Vec`→`TVec`
- Renamed type aliases with `T` prefix: `Float2`→`TFloat2`, `Int2`→`TInt2`, `Float3`→`TFloat3`, `Int3`→`TInt3`, `Float4`→`TFloat4`, `Float2x2`→`TFloat2x2`, `Float3x3`→`TFloat3x3`, `Float4x4`→`TFloat4x4`, `Quat`→`TQuat`, `FTransform`→`TFTransform`, `DTransform`→`TDTransform`, `UniformTRS`→`TUniformTRS`, `RigidTR`→`TRigidTR`
- Added `final` to all class declarations (Vector*, Matrix*, Quaternion, Transform, UniformTransform, RigidTransform, OBB, AABB, StratifiedSampling, ImportanceResampling, MonteCarloIntegrator, MathUtilTest + all Test classes)
- Added `[[nodiscard]]` to all value-returning functions
- Added `noexcept` to every function
- Added `explicit` to single-arg constructors (nullptr, conversion, array constructors)
- Removed explicit `inline` keyword from all function definitions
- Added blank lines before `return` statements
- Moved default constructor bodies to member initializer lists (Matrix2x2, Matrix3x3, Matrix4x4)
- Added assertion messages where missing (Matrix3x3::LookAt, Quaternion::Invert, UniformTransform/Mat4x4 ctor, RigidTransform/Mat4x4 ctor)
- Converted 4-space indentation to tab indentation in .inl files
- Removed braces from single-line if bodies (StratifiedSampling, ImportanceResampling, MonteCarloIntegrator)
- Added 2 blank lines after last include in all .cpp files
- All 16 .cpp files compile cleanly (verified via direct clang++ compilation)

### Coding Standards — Engine/OSAL (65 files)
- **Un-indented namespace bodies**: Removed tab indent from all lines inside `namespace OS { }` blocks across all .h/.cpp/.mm files
- **`noexcept`**: Added to all non-throwing function declarations and definitions
- **`[[nodiscard]]`**: Added to all getter/value-returning functions (`Get*()`, `Is*()`, `Has*()`, operators)
- **`final`**: Added to `OSDebugTest`, `OSMemoryTest`, `OSThreadTest`, `OSInputOutputTest`, `Win32Window`, `LinuxWindow`, `OSXWindow`, `Directory`, `OSXApplication`, `Win32Application`
- **`struct`→`class`**: `FileHandle`, `ProtectionMode`, `FileOpenMode`, `MapSyncMode` changed from `struct` to `class`
- **`SourceLocation.h`**: Renamed `struct source_location` → `class SourceLocation`, reordered members (public first), removed in-class private initializers to constructor
- **Member ordering**: Fixed `File.h`, `OSXWindow.h`, `OSXApplication.h`, `OSFileHandle.h` — moved `private:` after `public:`
- **In-class initialization → constructor**: `OSProtectionMode.h`, `OSFileOpenMode.h`, `OSMapSyncMode.h`, `Win32Window.h`, `LinuxWindow.h` — removed non-constexpr in-class inits, added constructor init lists
- **Braces removed** from single-line return/continue: `LinuxWindow.cpp`, `LinuxFileHandle.cpp`, `Directory.cpp`, `OSXAbstractLayer.cpp`, `LinuxAbstractLayer.cpp`, `WindowsAbstractLayer.cpp`
- **`inline` keyword removed** from: `OSDebugTest()`, `OSMemoryTest()`, `OSThreadTest()`, `Directory::FileList()`, `Directory::DirList()`, `File::operator<`, `File::GetPath()`
- **`virtual` removed** from `override` functions: `OSDebug.h`, `OSMemory.h`, `OSThread.h`, `OSInputOutput.h`
- **Win32Window.cpp**: Moved `WindowProc()` inside `namespace OS { }` block
- **OSXWindow.mm**: Converted mixed spaces→tabs for indentation
- **OSXApplication.mm**: Converted spaces→tabs for indentation
- **Two blank lines after includes**: Fixed in all .cpp/.mm files
- Include ordering improved: blank lines between standard/project headers

---

### Renderer RHI Architecture Revision (2026-05-17 23:30)

**Objective:** Complete redesign of renderer following Data-Oriented Design (DOD) hybrid pipeline with HLSL as single source.

#### Documentation Phase (COMPLETED ✓)
- **RendererDesign.md**: Revised with complete DOD architecture covering all 4 phases, bindless resources, and PSO management  
- **RendererDesign.html**: Updated HTML version matching new architecture structure
- **Architecture includes:**
  - Hybrid Pipeline: Extract → Prepare → Render Graph → Submit (strictly decoupled)
  - HLSL SM 6.6+ as single source cross-compiling to DXIL/SPIR-V/MSL  
  - Bindless Resources: Unified 32-bit handle system with Metal Tier 2 partitioning
  - Stateless Command Buckets: 64-bit sort keys for zero-contention parallel submission
  - PSO Management: Data-driven caching for Graphics/Compute/RayTracing pipelines

#### Implementation Plan Phased Approach (Simple → Complex)

**Phase 1: Core Data Structures & Bindless Systems (Week 1)**
- [ ] **Task 1.1:** Implement `BindlessDescriptorPool` class with Metal Tier 2 partitioning  
  - Split texture/buffer handles into manageable pages  
  - Add staleness tracking for temporal buffer preservation  
  - File: `Engine/Renderer/BindlessDescriptorPool.h/.cpp`
- [ ] **Task 1.2:** Create SoA data structure layouts for Extract phase  
  - `TransformStack<MAX_TRANSFORMS>` (128-byte aligned)  
  - `MeshInstanceArray<MAX_INDICES>` (192-byte aligned)  
  - `MaterialIndexBuffer<MAX_BINDLESS_TEXTURES>`
  - File: `Engine/Renderer/SoABuffers.h`
- [ ] **Task 1.3:** Implement thread-local command bucket storage  
  - Zero-contention atomic counter for built commands  
  - Flat pointer array to reduce cache misses
  - File: `Engine/Renderer/ThreadLocalCommandBucket.h`

**Phase 2: Command Recording & Submission (Week 2)** 
- [ ] **Task 2.1:** Implement platform-specific command bucket classes  
  - DX12: Thread-local `ID3D12GraphicsCommandList4` arrays
  - Vulkan: Pool of preallocated `VkCommandBuffer`s  
  - Metal: Multiple `MTLCommandBuffer` batches per recording
  - File: `Engine/Renderer/CommandBucket.h/.cpp`
- [ ] **Task 2.2:** Create 64-bit Sort Key structure and radix sort implementation  
  - Use existing SIMD sorting utilities from TaskSystem
  - Group by PassID → PSO Hash → Material Index → Depth
  - File: `Engine/Renderer/RenderSort.h/.cpp`
- [ ] **Task 2.3:** Implement thread-aggregated parallel radix sort  
  - Gather commands from all thread-local buckets into flat array
  - Sort with zero contention (pre-sorted per-thread)
  - Merge for depth-stable ordering
  - File: `Engine/Renderer/SortAggregator.h/.cpp`

**Phase 3: Ray Tracing Bindless & Acceleration Structures (Week 3)**  
- [ ] **Task 3.1:** Implement acceleration structure handles for HLSL bindless  
  - DXR: `ShaderResourceView` from descriptor heap
  - Vulkan: `VkAccelerationStructureKHR` in descriptor set
  - Metal: `MTLAccelerationStructure` in argument buffer  
  - File: `Engine/Renderer/AccelerationStructures.h/.cpp`
- [ ] **Task 3.2:** Create hit shader data pipeline (raygen/miss/closest-hit)  
  - HLSL RT Hit Program interfaces
  - Cross-compilation flow matching graphics pipelines
  - File: `Engine/Renderer/RTHitData.h/.cpp`

**Phase 4: Render Graph DAG & Barrier Injection (Week 4)**  
- [ ] **Task 4.1:** Implement `RenderGraphDAG` class with node types  
  - Compute Pass, Graphics Pass, Ray Tracing Pass, Blending/Tonemap
  - Input/Output resource dependencies for each node type
  - File: `Engine/Renderer/RenderGraphDAG.h/.cpp`
- [ ] **Task 4.2:** Implement automatic barrier injection system  
  - Create wrapper types around VkPipelineBarrier2/D3D12_RESOURCE_BARRIER/MTLResourceBarrier union arguments  
  - Auto-generate barriers for UAV/Storage Buffer transitions (read/write)
  - File: `Engine/Renderer/BarrierInjection.h/.cpp`
- [ ] **Task 4.3:** Implement RenderGraph Compiler interface  
  - Compile execution graph with barrier tracking
  - Temporal history preservation flags  
  - File: `Engine/Renderer/RenderGraphCompiler.h/.cpp`

**Phase 5: PSO Management & Pipeline Caching (Week 5)**  
- [ ] **Task 5.1:** Create PSOCacheManager data structures  
  - RenderPsoRecord, ComputePsoRecord, RayTracingPipelineRecord
  - Hash-based lookups for quick access  
  - File: `Engine/Renderer/Psocache.h/.cpp`
- [ ] **Task 5.2:** Implement PSO caching with generation tracking  
  - Invalidated on program/material changes
  - Thread-safe updates across worker threads
  - File: same as above
- [ ] **Task 5.3:** HLSL shader loader integration  
  - Load precompiled DXIL/SPIR-V/MSL binaries
  - Replace inline strings in stub implementations
  - File: `Engine/Renderer/ShadersLoader.h/.cpp`

**Phase 6: Backends Completion (Week 6)**  
- [ ] **Task 6.1:** Complete Vulkan Renderer backend  
  - Full device instance, queue family, swapchain implementation  
  - PSO creation with VK_EXT_descriptor_indexing enabled
  - Cross-platform shader loading
  - File: `Engine/Renderer/VulkanRenderer.h/.mm`  
- [ ] **Task 6.2:** Complete DX12 Renderer backend  
  - Device, command list queue, root signatures  
  - Enhanced barriers for UAV transitions
  - PSOs with DXIL ray tracing pipelines
  - File: `Engine/Renderer/DX12Renderer.h/.cpp`

**Phase 7: Temporal & Advanced Features (Week 7-8)**  
- [ ] **Task 7.1:** Implement temporal reprojection buffer system  
  - Motion vector history buffers per frame
  - TAA-compatible clear and sample stages
  - File: `Engine/Renderer/TemporalHistory.h/.cpp`
- [ ] **Task 7.2:** Add Hi-Z occlusion pipeline for Prepare phase  
  - Multi-stage depth testing
  - Depth prepass buffer management

#### Progress Tracking
**Current Status:** Documentation Complete — Implementation Planning Phase  
**Next Immediate Task:** Tasks 1.1 → 1.3 (Core Data Structures)  
**Estimated Timeline:** 6-8 weeks based on phased approach

#### Design Principles to Follow
1. **Composition over inheritance**: Future subsystems compose `IRenderer` rather than deriving
2. **Opaque handles via intptr_t**: Avoid Opaque platform types in C++ headers
3. **noexcept correctness**: All renderer API methods are noexcept  
4. **RAII with std::unique_ptr**: Lifecycle management without manual cleanup
5. **Platform abstraction**: Factory + platform-conditional compilation
6. **Data-Oriented Design**: SoA layouts, lock-free storage, cache-friendly access patterns

