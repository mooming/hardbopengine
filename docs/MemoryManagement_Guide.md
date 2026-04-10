# Memory Management Guide for HardBop Engine

Welcome to the HardBop Engine memory management documentation. This guide provides an overview of the sophisticated memory management system designed for high-performance, low-latency engine operations.

## Overview

The HardBop Engine employs a tiered, multi-layered memory architecture. Instead of relying solely on the general-purpose system heap, the engine utilizes specialized allocators to minimize fragmentation, reduce allocation overhead, and provide deterministic performance.

The central authority for all memory operations is the `MemoryManager` singleton.

## Core Components

### 1. The `MemoryManager` (The Orchestrator)
The `MemoryManager` acts as the central hub for all memory-related activities.
- **Centralized Registry**: Tracks all registered allocators via `TAllocatorID`.
- **Lifecycle Management**: Provides type-safe creation (`New<T>`) and destruction (`Delete<T>`) of objects.
- **Tracking & Statistics**: Real-time tracking of allocation/deallocation counts, usage, and capacity across all registered allocators.
- **Fallthrough Mechanism**: A hierarchical fallback system for handling exhausted allocators.
- **Configuration Persistence**: Loading and saving `MultiPool` configurations via `MultiProfileConfigCache`.
- **Profiling Support**: Built-in hooks for memory investigation, logging, and detailed usage reporting when `PROFILE_ENABLED` is active.

### 2. Allocation Strategies

| Allocator Type | Best Use Case | Characteristics |
| :--- | :--- | :--- |
| **`SystemAllocator`** | Rare, large, or long-lived allocations. | Wraps OS-level `malloc`/`free` or `VirtualAlloc`. |
| **`PoolAllocator`** | High-frequency allocations of the same size. | Fixed-size blocks, extremely fast $O(1)$ complexity. |
| **`MultiPoolAllocator`**| Varying allocation sizes within a range. | Manages multiple pools of different block sizes. Features optimized configuration caching and bank expansion. |
| **`StackAllocator`** | Temporary, short-lived scope-based data. | Linear allocation from a buffer; no individual deallocation. |
| **`MonotonicAllocator`**| Arena-style, lifetime-bound allocations. | Fast, append-only allocation; cleared all at once. |
| **`InlinePoolAllocator`**| Extremely small, stack-resident pools. | No heap overhead; uses stack memory for the pool itself. |

### 3. Hierarchical Scoped Allocation (The RAII Approach)
The engine utilizes `ScopedAllocator` and `AllocatorScope` to manage allocation contexts automatically. This allows for a highly efficient, hierarchical allocation pattern:

**The Pattern**: You can nest allocators within managed scopes. For example, a `StackAllocator` can be assigned to a scope, and within that scope, the engine can use `InlinePoolAllocator` or `MultiPoolAllocator` to manage sub-allocations. When the scope exits, the context automatically reverts to the previous allocator.

```cpp
{
    // 1. Create a high-performance stack allocator for a frame/task
    hbe::StackAllocator taskAllocator("TaskBuffer", 1024 * 64);

    // 2. Enter a scope that redirects all 'New' calls to this stack allocator
    hbe::ScopedAllocator scope(taskAllocator);

    // 3. Within this scope, use other specialized allocators that might 
    //    fallback to the current scope's allocator if exhausted.
    auto* data = hbe::MemoryManager::GetInstance().New<MyType>();
    
} // 4. Context automatically reverts to the previous allocator (e.g., System)
```

## Debugging and Performance Monitoring

The `MemoryManager` provides powerful compile-time configuration options via `Engine/Config/BuildConfig.h` to aid in debugging memory corruption and optimizing performance.

### Preprocessor Macros for Memory Control

| Macro | Purpose | Recommended Setting |
| :--- | :--- | :--- |
| `__MEMORY_VERIFICATION__` | Verifies memory integrity (e.g., checking for guard bands or checksums). | `1` (Debug) / `0` (Release) |
| `__MEMORY_LOGGING__` | Enables logging of every allocation and deallocation event. Useful for tracking leaks but adds significant overhead. | `1` (Debugging) / `0` (Performance) |
| `__MEMORY_INVESTIGATION__` | Enables detailed memory investigation, including tracking which thread or location performed an allocation. | `1` (Deep Debugging) / `0` (Normal) |
| `__MEMORY_DANGLING_POINTER_CHECK__`| Attempts to detect use-after-free errors by scrubbing deallocated memory. | `1` (Debug) / `0` (Release) |
| `__MEMORY_BUFFER_UNDERRUN_CHECK__` | Detects buffer overflows/underflows by placing guard pages or canary values around allocations. | `1` (Debug) / `0` (Release) |
| `PROFILE_ENABLED` | Enables performance profiling, including usage statistics and allocator-specific metrics like `AllocStats`. | `1` (Profiling) / `0` (Release) |

### Advanced Debugging Features

#### Memory Integrity Verification
When `__MEMORY_VERIFICATION__` is enabled, the engine can perform checks during allocation and deallocation to ensure that memory hasn't been corrupted by adjacent writes. This is critical for catching "silent" bugs in custom allocators.

#### Stomp Allocator Pattern (Dangling Pointer Detection)
By combining `__MEMORY_DANGLING_POINTER_CHECK__` with specialized allocator configurations, the engine can implement a "stomp" or "poisoning" pattern. When memory is deallocated, its contents are overwritten with a specific pattern (e.g., `0xDEADBEEF`). If a pointer to this memory is subsequently used, the application will likely crash or exhibit predictable behavior, making the bug much easier to trace.

#### Memory Logging and Investigation
With `__MEMORY_LOGGING__` enabled, you can output a complete history of all memory operations to your log file. This is indispensable for:
- Identifying exactly where a leak originated.
- Tracing the lifecycle of a specific block of memory.
- Analyzing fragmentation patterns over time.

When `__MEMORY_INVESTIGATION__` is active, additional metadata (such as `threadId`) is stored within the `AllocatorProxy` to allow you to see which thread was responsible for an allocation, even if the error manifests much later on a different thread.

#### Profiling with `AllocStats`
Enabling `PROFILE_ENABLED` allows you to access detailed statistics via the `MemoryManager`. This includes:
- **Peak Usage**: The maximum amount of memory used by an allocator at any one time.
- **Allocation Counts**: How many times `Allocate` and `Deallocate` were called.
- **Fallback Count**: How many times an allocator reached its capacity and had to fall back to a parent allocator (e.g., the System Allocator).

This information is vital for tuning your memory budget and ensuring that high-frequency allocators like `PoolAllocator` are sized correctly to avoid costly fallback operations.

---
*Last updated: April 10, 2026*
