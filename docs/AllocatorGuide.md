# HardBop Engine - Custom Allocators Guide

## Overview

The HardBop Engine provides a comprehensive custom memory allocation system designed for different use cases. This guide covers all available allocators and how to use them effectively.

## Table of Contents

1. [Allocator Types](#allocator-types)
2. [Quick Reference](#quick-reference)
3. [Detailed Usage](#detailed-usage)
4. [Helper Functions](#helper-functions)
5. [Best Practices](#best-practices)

---

## Allocator Types

### 1. PoolAllocator

**Purpose:** Fixed-size block allocator for objects of the same size.

**Use case:** When you need to allocate many objects of the same size (e.g., game entities, particles).

```cpp
#include "Memory/PoolAllocator.h"

// Create a pool with 64-byte blocks, 1000 blocks
PoolAllocator allocator("MyPool", 64, 1000);

// Allocate
void* ptr = allocator.Allocate(64);

// Deallocate
allocator.Deallocate(ptr, 64);
```

**Key features:**
- O(1) allocation and deallocation
- No memory fragmentation
- Fixed block size

---

### 2. StackAllocator

**Purpose:** Stack-based (LIFO) allocation for temporary data.

**Use case:** Frame-by-frame temporary allocations, scratch memory.

```cpp
#include "Memory/StackAllocator.h"

// 1MB stack allocator
StackAllocator allocator("FrameStack", 1024 * 1024);

// Allocate (returns pointer)
void* ptr = allocator.Allocate(256);

// Deallocate (optional - can just reset)
allocator.Deallocate(ptr, 256);

// Or reset entire stack
// allocator resets to beginning automatically via cursor
```

**Key features:**
- Very fast allocation (just move cursor)
- No individual deallocation needed - reset clears all
- Great for frame-local temporary data

---

### 3. MonotonicAllocator

**Purpose:** Arena allocator that only allocates, never frees individual blocks.

**Use case:** Batch allocations that are all released together.

```cpp
#include "Memory/MonotonicAllocator.h"

// 256KB arena
MonotonicAllocator allocator("BatchArena", 256 * 1024);

// Allocate
void* ptr = allocator.Allocate(128);

// Deallocate - still in arena but marked as free
// (actually no-op, memory stays allocated until arena destruction)
allocator.Deallocate(ptr, 128);
```

**Key features:**
- Fast like stack allocator
- Memory released all at once when allocator is destroyed
- Good for level loading or batch processing

---

### 4. MultiPoolAllocator

**Purpose:** Multiple pool sizes for different allocation sizes.

**Use case:** When you need to handle varied allocation sizes efficiently.

```cpp
#include "Memory/MultiPoolAllocator.h"

// Default: automatic pool generation based on allocation patterns
MultiPoolAllocator allocator("MultiPool");

// Or with custom configuration
MultiPoolAllocator allocator("CustomMulti", {
    {16, 1000},   // 16-byte blocks, 1000 blocks
    {32, 500},    // 32-byte blocks, 500 blocks
    {64, 250},    // 64-byte blocks, 250 blocks
    {128, 100},   // 128-byte blocks, 100 blocks
});

// Allocate any size - allocator picks best pool
void* ptr = allocator.Allocate(48);  // Uses 64-byte pool
allocator.Deallocate(ptr, 48);
```

**Key features:**
- Automatic size class selection
- Falls back to system allocator for large allocations
- Configurable per-application

---

### 5. InlinePoolAllocator

**Purpose:** Stack-embedded pool for short-lived objects.

**Use case:** Per-object local allocators, small game entities.

```cpp
#include "Memory/InlinePoolAllocator.h"

// Stack-allocated: 10 objects of MyClass, 2 buffers
struct MyAllocator : InlinePoolAllocator<MyClass, 10, 2>
{
    using Base = InlinePoolAllocator<MyClass, 10, 2>;
    MyAllocator() : Base() {}
};

// Use like STL allocator
std::vector<MyClass, MyAllocator> objects;
objects.reserve(10);
```

**Key features:**
- No heap allocation - entirely on stack
- Great for per-frame or per-scene objects
- Compile-time buffer size

---

### 6. InlineMonotonicAllocator

**Purpose:** Stack-embedded arena allocator.

**Use case:** Small temporary buffers.

```cpp
#include "Memory/InlineMonotonicAllocator.h"

// 4KB stack arena
InlineMonotonicAllocator<4096> frameArena;

void* ptr = frameArena.Allocate(256);
frameArena.Deallocate(ptr, 256);  // Marked free but not cleared

// Reset entire arena
frameArena.Reset();
```

---

### 7. ThreadSafeMultiPoolAllocator

**Purpose:** Thread-safe version of MultiPoolAllocator.

**Use case:** Multi-threaded allocation with varied sizes.

```cpp
#include "Memory/ThreadSafeMultiPoolAllocator.h"

ThreadSafeMultiPoolAllocator allocator("ThreadSafeMulti");
void* ptr = allocator.Allocate(32);  // Thread-safe
```

---

### 8. SystemAllocator

**Purpose:** Direct wrapper around system malloc/free.

**Use case:** Default fallback, or when custom allocation is not needed.

```cpp
#include "Memory/SystemAllocator.h"

SystemAllocator allocator;
void* ptr = allocator.Allocate(1024);
allocator.Deallocate(ptr, 1024);
```

---

### 9. DefaultAllocator

**Purpose:** STL-compatible allocator adapter.

**Use case:** Using with STL containers.

```cpp
#include "Memory/DefaultAllocator.h"

std::vector<int, DefaultAllocator<int>> vec;
vec.push_back(42);
```

---

## Quick Reference

| Allocator | Allocation Type | Use Case |
|-----------|----------------|----------|
| PoolAllocator | Fixed-size blocks | Many same-size objects |
| StackAllocator | LIFO stack | Frame-temp data |
| MonotonicAllocator | Arena (bulk free) | Batch processing |
| MultiPoolAllocator | Multiple pools | Varied sizes |
| InlinePoolAllocator | Stack-embedded | Per-object local |
| InlineMonotonicAllocator | Stack-embedded arena | Small temp buffers |
| ThreadSafeMultiPoolAllocator | Thread-safe multi-pool | Multi-threaded |

---

## Helper Functions

### New<T> / Delete<T>

```cpp
#include "Memory/Memory.h"

// With custom allocator
PoolAllocator allocator("Test", 64, 100);
MyClass* obj = New<MyClass>(allocator, constructor_args...);
Delete(allocator, obj);

// With global allocator
MyClass* obj2 = New<MyClass>(constructor_args...);
Delete(obj2);
```

### AllocatorScope

```cpp
#include "Memory/AllocatorScope.h"

// Set default allocator for scope
{
    MultiPoolAllocator allocator("ScopeAlloc");
    AllocatorScope scope(allocator);
    
    // All allocations in this scope use allocator
    auto* ptr = MemoryManager::GetInstance().Allocate(64);
}
```

---

## Best Practices

### 1. Use Appropriate Allocator for Use Case

```cpp
// Bad: Using general allocator for fixed-size objects
for (int i = 0; i < 10000; ++i) {
    auto* entity = new Entity();  // System allocator
}

// Good: Pool allocator for fixed-size entities
PoolAllocator entityPool("Entities", sizeof(Entity), 10000);
for (int i = 0; i < 10000; ++i) {
    auto* entity = (Entity*)entityPool.Allocate(sizeof(Entity));
}
```

### 2. Reset Stack/Monotonic Allocators Regularly

```cpp
void GameFrame()
{
    // Frame-temp allocations
    StackAllocator frameTemp("Frame", 1024 * 1024);
    
    // ... do work ...
    
    // Don't deallocate individually - just let it go out of scope
    // Or manually reset if reusing
    frameTemp = StackAllocator("Frame", 1024 * 1024);
}
```

### 3. Profile Before Optimizing

Enable profiling to see allocation patterns:

```cpp
// In BuildConfig.h
#define PROFILE_ENABLED 1

// Then check allocator stats
allocator.GetUsage();
allocator.GetAvailableBlocks();
```

### 4. Handle Out-of-Memory

Allocators can return nullptr when exhausted. Check return values:

```cpp
void* ptr = allocator.Allocate(size);
if (ptr == nullptr) {
    // Handle out of memory - fall back to system allocator
}
```

---

## Configuration Flags

These can be modified in `Engine/Config/BuildConfig.h`:

| Flag | Default | Description |
|------|---------|-------------|
| `__MEMORY_VERIFICATION__` | 0 | Verify memory integrity |
| `__MEMORY_LOGGING__` | 0 | Log all allocations |
| `__MEMORY_DANGLING_POINTER_CHECK__` | 0 | Detect dangling pointers |
| `__MEMORY_BUFFER_UNDERRUN_CHECK__` | 0 | Detect buffer underruns |
| `__FORCE_USE_SYSTEM_MALLOC__` | 0 | Disable custom allocators |

---

## Profile-Guided MultiPoolAllocator Optimization

The `MultiPoolAllocator` can optimize itself based on actual allocation patterns. The engine records allocation sizes and frequencies, then generates an optimized pool configuration.

### How It Works

1. **First Run (Learning Phase)**
   - MultiPoolAllocator starts with default/generic configuration
   - All allocations are tracked in `MultiPoolConfigCache`
   - A log file `.multiPoolConfig.dat` is created

2. **Subsequent Runs (Optimized)**
   - On next launch, the allocator loads the cached configuration
   - Pools are generated to match actual usage patterns
   - Fewer memory wasted on over-sized pools

### Enabling Profile Logging

In `Engine/Config/BuildConfig.h`:

```cpp
#define __MEMORY_LOGGING__ 1
```

This enables detailed allocation logging to `.multiPoolConfig.dat`.

### How to Use

**Step 1: Run your application normally**
```bash
# Run with memory logging enabled
./build/Applications/EngineTest/EngineTest
```

This creates `.multiPoolConfig.dat` in the working directory.

**Step 2: Review the configuration**
```cpp
// The next run will automatically use the cached configuration
MultiPoolAllocator allocator("OptimizedMultiPool");

// Print current usage
allocator.PrintUsage();
```

**Step 3: Fine-tune manually (optional)**

You can also specify custom pool configurations:

```cpp
MultiPoolAllocator allocator("CustomMulti", {
    {16, 1000},   // 16-byte blocks, 1000 blocks - for small objects
    {32, 500},    // 32-byte blocks, 500 blocks
    {64, 250},    // 64-byte blocks, 500 blocks - for medium objects
    {128, 100},  // 128-byte blocks, 100 blocks
    {256, 50},   // 256-byte blocks, 50 blocks - for large objects
}, 1024 * 1024,  // 1MB bank size
16);             // minimum 16-byte blocks
```

### Configuration File

The cache file format (`.multiPoolConfig.dat`) contains:
- Allocation size histogram
- Frequency of each size class
- Peak usage per size class
- Bank size recommendations

### Resetting the Profile

To clear cached data and start fresh:

```bash
# Delete the cache file
rm .multiPoolConfig.dat
```

Or programmatically:

```cpp
auto& mmgr = MemoryManager::GetInstance();
mmgr.ResetMultiPoolConfig();
```

### Best Practices

1. **Run representative workload**: Profile with typical game scenarios, not minimal tests
2. **Multiple runs**: Let it accumulate data over several sessions
3. **Platform-specific**: Profile on each target platform separately
4. **Version reset**: Re-profile when allocation patterns change significantly

---

## See Also

- `Engine/Memory/MemoryManager.h` - Global memory manager
- `Engine/Memory/AllocatorID.h` - Allocator identification
- `Engine/Memory/AllocatorScope.h` - Scoped allocator context
- Unit tests in `Engine/Test/` for each allocator