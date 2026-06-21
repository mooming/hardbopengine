# HardBop Engine API Guide

**Version:** 2026.06  
**Project:** HardBop Engine — high-performance C++23 engine with custom memory, task system, OSAL  
**Namespace:** `hbe` (engine core), `OS` (OS abstraction layer), `hbe::Renderer` (renderer)

---

## Table of Contents

1. [Core Types & Foundation](#1-core-types--foundation)
2. [Task System](#2-task-system)
3. [Component System](#3-component-system)
4. [Memory Management](#4-memory-management)
5. [Containers](#5-containers)
6. [Math Library](#6-math-library)
7. [String Utilities](#7-string-utilities)
8. [Logging System](#8-logging-system)
9. [Configuration System](#9-configuration-system)
10. [OS Abstraction Layer (OSAL)](#10-os-abstraction-layer-osal)
11. [Resource Management](#11-resource-management)
12. [HSTL — HardBop STL](#12-hstl--hardbop-stl)
13. [Engine Core](#13-engine-core)
14. [Renderer (RHI)](#14-renderer-rhi)
15. [Test Framework](#15-test-framework)
16. [Build Configuration](#16-build-configuration)

---

## 1. Core Types & Foundation

### Types (`Engine/Core/Types.h`)

| Alias | Underlying Type | Purpose |
|---|---|---|
| `TByte` | `uint8_t` | Byte type |
| `TPointer` | `void*` | Generic pointer |
| `TIndex` | `size_t` | Index type |
| `TInt` | `int32_t` | Signed integer |
| `TUInt` | `uint32_t` | Unsigned integer |
| `TReal` | `float` | Real number |

Backward-compatible aliases: `Byte`, `Pointer`, `Index`, `Int`, `UInt`, `Real`.

### Constants (`Engine/Core/Constants.h`)

```cpp
constexpr size_t KB = 1024;
constexpr size_t MB = 1024 * 1024;
constexpr size_t GB = 1024 * 1024 * 1024;

constexpr float Pi     = 3.141592653589793f;
constexpr float HalfPi = Pi * 0.5f;
constexpr float TwoPi  = Pi * 2.0f;
constexpr float EulerNum = 2.718281828459f;
constexpr float Epsilon = 0.001f;
constexpr float SqrEpsilon = Epsilon * Epsilon;
```

### SourceLocation (`Engine/OSAL/SourceLocation.h`)

Captures source code location for debugging:

```cpp
class SourceLocation final {
    static constexpr SourceLocation current(
        const char* fileName = CallerFile(),
        const char* functionName = CallerFunc(),
        uint_least32_t lineNumber = CallerLine(),
        uint_least32_t columnOffset = CallerColumn()
    ) noexcept;
    // Accessors: file_name(), function_name(), line(), column()
};
```

### Intrinsic Macros (`Engine/OSAL/Intrinsic.h`)

| Macro | Purpose |
|---|---|
| `likely(x)` | Branch prediction hint (true likely) |
| `unlikely(x)` | Branch prediction hint (false likely) |
| `debugBreak()` | Halt execution for debugging |
| `CallerFile()` | Source file path |
| `CallerFunc()` | Function name |
| `CallerLine()` | Line number |
| `CallerColumn()` | Column number |

---

## 2. Task System

The task system is the backbone for parallel execution, built on `Task`, `RangedTask`, `TaskStream`, and `TaskSystem`.

### Runnable (`Engine/Core/Runnable.h`)

```cpp
using TRunnable = std::size_t(*)(void* userData,
                                  std::size_t startIndex,
                                  std::size_t endIndex);
```

### RangedTask (`Engine/Core/RangedTask.h`)

A task with a specific index range `[start, end)` for parallel processing.

```cpp
class RangedTask final {
    uint8_t priority;
    TaskStreamAffinity affinity;
    StaticString taskName;
    std::reference_wrapper<Task> taskRef;
    TIndex start, end;
    mutable TIndex currentIndex;

    bool HasFinished() const noexcept;
    void Run() noexcept;  // Calls task's runnable, reports finish
};
```

### Task (`Engine/Core/Task.h`)

A unit of work composed of multiple `RangedTask` sub-tasks that can be split across threads.

```cpp
class Task final {
    // Constructors
    Task() noexcept;
    Task(StaticString taskName, TRunnable func, void* userData) noexcept;

    // Execution
    void Start(TIndex numberOfSubTasks, TIndex startIndex,
               TIndex endIndex, uint8_t priority = 0) noexcept;

    // Synchronization
    void BusyWait() const noexcept;
    void Wait(uint32_t intervalMilliSecs = 10) const noexcept;

    // Status
    bool HasDone() const noexcept;
    void ReportFinishedSubTask() noexcept;

    // Accessors
    TRunnable GetRunnable() const noexcept;
    void* GetUserData() const noexcept;

    // Sub-task generation
    RangedTask GenerateSubTask(TIndex start, TIndex end,
                                uint8_t priority = 0) noexcept;
};
```

### TaskStream (`Engine/Core/TaskStream.h`)

A dedicated thread that processes tasks from a priority queue.

```cpp
class TaskStream final {
    // Construction
    TaskStream();
    explicit TaskStream(StaticString name, TStreamIndex streamIndex);

    // Operations
    void Enqueue(const RangedTask& task) noexcept;
    void WakeUp() noexcept;
    void Join() noexcept;
    void Start(TaskSystem& taskSys) noexcept;
    void RunLoop() noexcept;

    // Accessors
    StaticString GetName() const noexcept;
    std::thread::id GetThreadID() const noexcept;
    TStreamIndex GetStreamIndex() const noexcept;
    uint64_t GetLoopCount() const noexcept;
};
```

### TaskStreamAffinity (`Engine/Core/TaskStreamAffinity.h`)

Bitmask for specifying which task streams a task can execute on.

```cpp
template<unsigned int NumBits>
class TaskStreamAffinityBase final {
    void Set(unsigned int bitIndex) noexcept;   // Exclude stream
    void Unset(unsigned int bitIndex) noexcept; // Include stream
    bool Get(unsigned int bitIndex) const noexcept; // Is included?
};

using TaskStreamAffinity = TaskStreamAffinityBase<64>;
```

### MainThreadTaskQueue (`Engine/Core/MainThreadTaskQueue.h`)

Thread-safe queue for tasks dispatched to the main thread.

```cpp
class MainThreadTaskQueue final {
    void Enqueue(TTaskFunc taskFunc, void* userData,
                 uint8_t priority = 128) noexcept;
    size_t ProcessTasks() noexcept;
    bool HasPendingTasks() const noexcept;
    void RequestStop() noexcept;
    bool IsRunning() const noexcept;
};
```

### TaskSystem (`Engine/Core/TaskSystem.h`)

Central manager for all task streams and parallel execution.

```cpp
class TaskSystem final {
    // Static helpers
    static TIndex GetNumHardwareThreads() noexcept;
    static void SetThreadName(StaticString name) noexcept;
    static void SetStreamIndex(TIndex index) noexcept;
    static StaticString GetCurrentStreamName() noexcept;
    static StaticString GetCurrentThreadName() noexcept;
    static TIndex GetCurrentStreamIndex() noexcept;
    static bool IsBaseThread() noexcept;
    static bool IsIOThread() noexcept;

    // Lifecycle
    TaskSystem() noexcept;
    ~TaskSystem() noexcept;
    void Initialize() noexcept;
    void RequestShutDown() noexcept;
    void JoinAndClear() noexcept;

    // Task queue operations
    void Enqueue(const RangedTask& task) noexcept;
    void Dequeue(std::optional<RangedTask>& outTask) noexcept;
    void Enqueue(TIndex streamIndex, const RangedTask& task) noexcept;

    // Main thread dispatch
    void DispatchToMainThread(TMainThreadTask task,
                              void* userData, uint8_t priority = 0) noexcept;
    size_t ProcessMainThreadTasks() noexcept;

    // Stream access
    TaskStream& GetStream(int index) noexcept;
    TaskStream& GetBaseTaskStream() noexcept;
    TaskStream& GetIOTaskStream() noexcept;
};
```

---

## 3. Component System

### ComponentState (`Engine/Core/ComponentState.h`)

```cpp
enum class ComponentState : int {
    NONE, BORN, ALIVE, SLEEP, DEAD
};
```

### Component (`Engine/Core/Component.h`)

Base class for game engine components with lifecycle management.

```cpp
class Component {
    // Lifecycle hooks (must override)
    virtual void Init() = 0;
    virtual void Update(const float deltaTime) = 0;
    virtual void Release() = 0;
    virtual void OnEnable() = 0;
    virtual void OnDisable() = 0;

    // State management
    ComponentState GetState() const noexcept;
    void SetState(ComponentState inState) noexcept;
    bool IsEnabled() const noexcept;
    void SetEnable(bool isEnabled) noexcept;
    void Destroy() noexcept;
};
```

### ComponentSystem (`Engine/Core/ComponentSystem.h`)

Template-based system managing components through lifecycle states.

```cpp
template<typename TComponent>
class ComponentSystem {
    // Create a new component
    template<typename... Types>
    TComponent& Create(Types&&... args);

    // Main update loop — processes init, update, and transitions
    void Update(const float deltaTime);

    // Status
    const char* GetName() const noexcept;
    explicit operator bool() const noexcept;
};
```

---

## 4. Memory Management

### MemoryManager (`Engine/Memory/MemoryManager.h`)

Centralized singleton managing all allocators.

```cpp
class MemoryManager final {
    static MemoryManager& GetInstance();
    static TAllocatorID GetCurrentAllocatorID();

    // Allocator registration
    TAllocatorID RegisterAllocator(void* allocator, const char* name,
                                   bool isInline, size_t capacity, ...);
    void DeregisterAllocator(TAllocatorID id);

    // Allocation
    void* Allocate(TAllocatorID id, size_t nBytes);
    void Deallocate(TAllocatorID id, void* ptr, size_t nBytes);
    void* SysAllocate(size_t nBytes);
    void SysDeallocate(void* ptr, size_t nBytes);
    void* FallbackAllocate(TAllocatorID id, TAllocatorID parentId,
                           size_t requested);

    // Type-safe allocation
    template<typename Type, typename... Types>
    Type* New(Types&&... args);
    template<typename Type>
    void Delete(Type* ptr);
    template<typename Type, typename... Types>
    Type* NewArray(Index size, Types&&... args);
    template<typename Type>
    void DeleteArray(Type* ptr, size_t n);
};
```

### Allocator Types

| Allocator | Description | Key Files |
|---|---|---|
| `SystemAllocator<T>` | Wraps OS malloc/free | `SystemAllocator.h` |
| `DefaultAllocator<T>` | Proxy to current MemoryManager allocator | `DefaultAllocator.h` |
| `MonotonicAllocator` | Linear bump-pointer, no per-block dealloc | `MonotonicAllocator.h` |
| `StackAllocator` | Stack-based linear allocator | `StackAllocator.h` |
| `PoolAllocator` | Fixed-size block pool with free list | `PoolAllocator.h` |
| `MultiPoolAllocator` | Multiple pools for varied block sizes | `MultiPoolAllocator.h` |
| `ThreadSafeMultiPoolAllocator` | Thread-safe version of MultiPool | `ThreadSafeMultiPoolAllocator.h` |
| `InlineMonotonicAllocator<Capacity>` | Inline buffer bump allocator | `InlineMonotonicAllocator.h` |
| `InlinePoolAllocator<T, BlockSize, NumBlocks>` | Inline fixed-size pool | `InlinePoolAllocator.h` |

### AllocatorScope (`Engine/Memory/AllocatorScope.h`)

RAII wrapper for temporarily changing the scoped allocator:

```cpp
class AllocatorScope final {
    AllocatorScope() noexcept;  // Saves and restores current allocator
    AllocatorScope(TAllocatorID id) noexcept;
    template<typename T>
    AllocatorScope(const T& allocator) noexcept;
};
```

### Shareable (`Engine/Memory/Shareable.h`)

Reference-counted smart pointer for shared ownership:

```cpp
template<typename TType, typename RefCount = uint8_t>
class Shareable {
    // Construction with forwarded args
    template<typename... Types>
    Shareable(Types&&... args);

    // Reference counting
    RefCount GetReferenceCount() const noexcept;

    // Access
    TType& Get() noexcept;
    TType& operator*() noexcept;
    TType* operator->() noexcept;

    // Lifetime
    void Release() noexcept;
    void Swap(Shareable& rhs) noexcept;
};
```

### Memory Helpers (`Engine/Memory/Memory.h`)

Free functions for type-safe allocation using MemoryManager:

```cpp
template<typename TType, typename... TTypes, typename TAllocator>
TType* New(TAllocator& allocator, TTypes&&... args) noexcept;

template<typename TType, typename TAllocator>
void Delete(TAllocator& allocator, TType* ptr) noexcept;

template<typename TType, typename... TTypes>
TType* New(TTypes&&... args) noexcept;  // Uses MemoryManager

template<typename TType>
void Delete(TType* ptr) noexcept;
```

### AllocatorProxy (`Engine/Memory/AllocatorProxy.h`)

Proxy wrapper around any allocator for registration in MemoryManager.

```cpp
class AllocatorProxy final {
    TAllocatorID id;
    void* allocator;
    TAllocBytes allocate;   // void* (*)(void*, size_t)
    TDeallocBytes deallocate; // void (*)(void*, void*, size_t)
};
```

### PoolConfig (`Engine/Memory/PoolConfig.h`)

```cpp
struct PoolConfig {
    size_t blockSize;
    size_t numBlocks;
};
```

---

## 5. Containers

All containers use custom allocators via `DefaultAllocator<T>` by default.

### Vector (`Engine/Container/Vector.h`)

Dynamic array with exponential growth (2x). Default capacity = 4.

```cpp
template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
class Vector final {
    // Construction
    Vector() noexcept;
    explicit Vector(TIndex initialCapacity);
    Vector(std::initializer_list<TElement> list);
    Vector(Vector&& rhs) noexcept;

    // Element access
    TElement& operator[](TIndex index);
    TElement& Front() noexcept;
    TElement& Back() noexcept;

    // Modification
    void PushBack(const TElement& value) noexcept;
    void PushBack(TElement&& value) noexcept;
    template<typename... Types>
    TElement& EmplaceBack(Types&&... args) noexcept;
    void PopBack() noexcept;
    void Resize(TIndex newSize) noexcept;
    void Reserve(TIndex newCapacity) noexcept;
    void Clear() noexcept;
    void Swap(Vector& rhs) noexcept;

    // Query
    TIndex Size() const noexcept;
    TIndex Capacity() const noexcept;
    bool IsEmpty() const noexcept;
    TIndex FindIndex(const TElement& element) const noexcept;

    // Iterators
    Iterator begin() noexcept;
    Iterator end() noexcept;
};
```

### Array (`Engine/Container/Array.h`)

Fixed-size dynamic array (size set at construction, no resizing).

```cpp
template<typename Element, class TAllocator = DefaultAllocator<Element>>
class Array final {
    Array() noexcept;
    explicit Array(TIndex size);  // Allocates and default-constructs
    Array(std::initializer_list<Element> list);
    Array(Array&& rhs) noexcept;

    Element& operator[](TIndex index) noexcept;
    template<typename... Types>
    Element& Emplace(TIndex index, Types&&... args) noexcept;

    TIndex Size() const noexcept;
    Element* ToRawArray() noexcept;
    TIndex GetIndex(const Element& element) const noexcept;
    void Swap(Array&& rhs) noexcept;
};
```

### HashMap (`Engine/Container/HashMap.h`)

Open-addressing hash map with tombstone support. Load factor 0.7.

```cpp
template<typename TKey, typename TValue, class THash = std::hash<TKey>,
         class TKeyEqual = std::equal_to<TKey>,
         class TAllocator = DefaultAllocator<uint8_t>>
class HashMap final {
    // Construction
    HashMap() noexcept;
    explicit HashMap(TIndex initialCapacity);
    HashMap(HashMap&& rhs) noexcept;

    // Element access
    TValue& operator[](const TKey& key);
    TValue& operator[](TKey&& key);

    // Lookup
    Iterator Find(const TKey& key) noexcept;
    bool Contains(const TKey& key) const noexcept;

    // Modification
    bool Insert(const TKey& key, const TValue& value);
    bool Insert(TKey&& key, TValue&& value);
    bool Remove(const TKey& key);
    void Clear() noexcept;
    void Reserve(TIndex newCapacity);

    // Query
    TIndex Size() const noexcept;
    TIndex Capacity() const noexcept;
    bool IsEmpty() const noexcept;

    // Iteration
    Iterator begin() noexcept;
    Iterator end() noexcept;
};
```

### Map (`Engine/Container/Map.h`)

Sorted map using binary search (array-based, not tree).

```cpp
template<typename TKey, typename TValue, class TCompare = std::less<TKey>,
         class TAllocator = DefaultAllocator<uint8_t>>
class Map final {
    // Same interface pattern as HashMap
    // Uses binary search for O(log n) lookups
};
```

### LinkedList (`Engine/Container/LinkedList.h`)

Doubly-linked list with custom allocator.

```cpp
template<typename TType, class TAllocator = DefaultAllocator<LinkedListNode<TType>>>
class LinkedList final {
    // Head/tail insertion
    TType& AddFirst(const TType& value) noexcept;
    TType& AddLast(const TType& value) noexcept;
    TType& AddPrevious(TType& current, const TType& value) noexcept;
    TType& AddNext(TType& current, const TType& value) noexcept;

    // Removal
    Iterator Remove(const TType& element) noexcept;

    // Query
    bool Contains(const TType& value) const noexcept;
    bool FindAndRemove(const TType& value) noexcept;
    bool IsEmpty() const noexcept;
    void Clear() noexcept;
};
```

### Deque (`Engine/Container/Deque.h`)

Ring-buffer based deque with wrap-around indexing.

```cpp
template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
class Deque final {
    // Front/back insertion
    void PushFront(const TElement& value) noexcept;
    void PushBack(const TElement& value) noexcept;
    template<typename... Types>
    TElement& EmplaceFront(Types&&... args) noexcept;
    template<typename... Types>
    TElement& EmplaceBack(Types&&... args) noexcept;
    void PopFront() noexcept;
    void PopBack() noexcept;
    void Reserve(TIndex newCapacity) noexcept;
};
```

### Queue (`Engine/Container/Queue.h`)

FIFO queue wrapping Deque.

```cpp
template<typename TElement, class TContainer = Deque<TElement>>
class Queue final {
    void Push(const TElement& value) noexcept;
    void Pop() noexcept;
    TElement& Front() noexcept;
    TElement& Back() noexcept;
};
```

### RingQueue (`Engine/Container/RingQueue.h`)

Fixed-capacity ring buffer. No resizing — capacity set at construction.

```cpp
template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
class RingQueue final {
    explicit RingQueue(TIndex fixedCapacity);
    void Push(const TElement& value) noexcept;
    TElement Pop() noexcept;
    bool IsFull() const noexcept;
    bool IsEmpty() const noexcept;
};
```

### BoundedPriorityQueue (`Engine/Container/BoundedPriorityQueue.h`)

Bucket-based priority queue for 0-255 priority values. O(1) push/pop.

```cpp
template<typename T, std::size_t MaxPriority = 256,
         std::size_t BucketSizeHint = 0>
class BoundedPriorityQueue final {
    void Push(const T& item) noexcept;
    std::optional<T> Pop() noexcept;
    std::optional<T> Top() const noexcept;
    std::size_t Remove(TPredicate predicate) noexcept;
    void Clear() noexcept;
};
```

### AtomicStackView (`Engine/Container/AtomicStackView.h`)

Lock-free atomic stack using CAS operations.

```cpp
template<CNext T>
class AtomicStackView final {
    void Push(T& newItem) noexcept;  // CAS-based push
    T* Pop() noexcept;               // CAS-based pop
    bool IsEmpty() const noexcept;
};
```

---

## 6. Math Library

### Vector2/3/4 (`Engine/Math/Vector*.h`)

N-dimensional vector templates with common arithmetic, dot product, normalization, and cross product (Vector3).

```cpp
template<typename TNumber>
class Vector3 final {
    // Static constants: Zero, Unity, X, Y, Z, Right, Up, Forward
    TNumber x, y, z;

    // Arithmetic
    Vector3 operator+(const Vector3& rhs) const noexcept;
    Vector3 operator-(const Vector3& rhs) const noexcept;
    Vector3 operator*(TNumber s) const noexcept;
    Vector3 operator/(TNumber s) const noexcept;

    // Operations
    TNumber Dot(const Vector3& rhs) const noexcept;
    TNumber SqrLength() const noexcept;
    TNumber Length() const noexcept;
    Vector3 Normalize() noexcept;
    Vector3 Cross(const Vector3& rhs) const noexcept;
    float AngleTo(const Vector3& to) const noexcept;
};
```

Type aliases: `TFloat2`, `TInt2`, `TFloat3`, `TInt3`, `TFloat4`, `TInt4`.

### Matrix2x2/3x3/4x4 (`Engine/Math/Matrix*.h`)

Matrix templates with determinant, inverse, transpose, and rotation operations.

```cpp
template<typename TNumber>
class Matrix4x4 final {
    static const Matrix4x4 Zero, Identity;
    // Named members: m11-m44
    // Rotation setters: SetRotationX/Y/Z, EulerAngles
    // Transform operations: Inverse, Transpose, Determinant, IsOrthogonal
    // Factory: CreateTranslation
};
```

Type aliases: `TFloat2x2`, `TFloat3x3`, `TFloat4x4`.

### Quaternion (`Engine/Math/Quaternion.h`)

Unit quaternion for 3D rotations.

```cpp
template<typename TNumber>
class Quaternion final {
    static const Quaternion Zero, Identity;
    TNumber x, y, z, w;

    // Factory methods
    static Quaternion CreateRotationX/Y/Z(float angle);
    static Quaternion LookRotation(const TVec3& forward, const TVec3& up);
    static Quaternion RotationAround(const TVec3& unitAxis, float radian);
    static Quaternion RotationFromTo(const TVec3& from, const TVec3& to);

    // Conversions
    TMat3x3 ToMat3x3() const noexcept;
    TMat4x4 ToMat4x4() const noexcept;
    TVec3 EulerAngles() const noexcept;

    // Interpolation
    static Quaternion Lerp(const Quaternion& from, const Quaternion& to, float t);
    static Quaternion Slerp(const Quaternion& from, const Quaternion& to, float t);

    // Operations
    Quaternion Inverse() const noexcept;
    Quaternion Normalized() const noexcept;
    TVec3 operator*(const TVec3& rhs) const noexcept;
};
```

Type alias: `TQuat`.

### Transforms

#### UniformTransform (`Engine/Math/UniformTransform.h`)

Uniform scale + rotation + translation.

```cpp
template<typename TNumber>
class UniformTransform final {
    TQuat rotation;
    TNumber scale;
    TVec3 translation;

    TVec3 Transform(const TVec3& x) const noexcept;
    TVec3 InverseTransform(const TVec3& x) const noexcept;
    UniformTransform Transform(const UniformTransform& rhs) const noexcept;
    UniformTransform Inverse() const noexcept;
    TMat4x4 ToMatrix() const noexcept;
};
```

Type alias: `TUniformTRS`.

#### RigidTransform (`Engine/Math/RigidTransform.h`)

Rotation + translation (no scaling).

```cpp
template<typename TNumber = float, int PaddingSize = 8>
class RigidTransform final {
    TQuat rotation;
    TVec3 translation;

    TVec3 Transform(const TVec3& x) const noexcept;
    TQuat Transform(const TQuat& r) const noexcept;
    RigidTransform Transform(const RigidTransform& rhs) const noexcept;
    RigidTransform Inverse() const noexcept;
    TMat4x4 ToMatrix() const noexcept;
};
```

Type alias: `TRigidTR`.

#### Transform (`Engine/Math/Transform.h`)

Hierarchical transform with parent-child relationships and world transform caching.

```cpp
template<typename TNumber>
class Transform final {
    TQuat rotation;
    TNumber scale;
    TVec3 translation;

    void Attach(Transform& transform) noexcept;
    void Detach(Transform& transform) noexcept;
    void DetachAll() noexcept;
    void Invalidate() noexcept;  // Invalidates cached world transform

    const TUniformTRS& GetWorldTransform() noexcept;  // Cached
    TUniformTRS GetWorldTransform() const noexcept;    // Computed
    const TUniformTRS& GetLocalTransform() const noexcept;
};
```

Type aliases: `TFTransform` (float), `TDTransform` (double).

### AABB (`Engine/Math/AABB.h`)

Axis-Aligned Bounding Box.

```cpp
template<typename TVec>
class AABB final {
    TVec min, max;
    void Add(const TVec& point) noexcept;
    void Add(const AABB& aabb) noexcept;
    void Translate(const TVec& t) noexcept;
    bool IsContaining(const TVec& point) const noexcept;
    AABB Intersection(const AABB& aabb) const noexcept;
    bool HasIntersectionWith(const AABB& aabb) const noexcept;
    TVec Center() const noexcept;
    TVec Diagonal() const noexcept;
    TVec Closest(const TVec& point) const noexcept;
};
```

Type aliases: `AABB2`, `AABB3`.

### OBB (`Engine/Math/OBB.h`)

Oriented Bounding Box for collision detection.

```cpp
template<typename TNumber>
class OBB final {
    TVec3 center, half;
    TQuat rotation;

    bool IsContaining(const TVec3& objSpacePoint) const noexcept;
    TVec3 Closest(const TVec3& objSpacePoint) const noexcept;
};
```

### MathUtil (`Engine/Math/MathUtil.h`)

Utility math functions.

```cpp
template<typename T> T Abs(T value) noexcept;
template<typename T> T Pow(T value, T n) noexcept;
template<typename T> T MinFast(T a, T b) noexcept;
template<typename T> T MaxFast(T a, T b) noexcept;
template<typename T> T ClampFast(T value, T min, T max) noexcept;
template<typename T> T Clamp(T value, T min, T max) noexcept;

float DegreeToRadian(float deg) noexcept;
float RadianToDegree(float rad) noexcept;
bool IsZero(float value) noexcept;
bool IsUnity(float value) noexcept;
bool IsEqual(float a, float b) noexcept;
```

### CoordinateOrientation (`Engine/Math/CoordinateOrientation.h`)

Rotation trig functions configurable by handedness (`RIGHT_HANDED_COORDINATE` / `LEFT_HANDED_COORDINATE`).

```cpp
float RotationSin(float radian);
float RotationCos(float radian);
float RotationTan(float radian);
float RotationAsin(float radian);
float RotationAcos(float radian);
float RotationAtan2(float y, float x);
```

### Sampling & Integration

#### StratifiedSampling (`Engine/Math/StratifiedSampling.h`)

Generates samples within intervals for stratified sampling.

```cpp
template<typename TReal = double, typename TInteger = uint32_t>
class StratifiedSampling final {
    template<typename TRangeSampler>
    TReal Sample(const TRangeSampler& sampler) noexcept;
    void ChangeSubGroup(TInteger delta = 1) noexcept;
};
```

#### ImportanceResampling (`Engine/Math/ImportanceResampling.h`)

Importance resampling for Monte Carlo integration.

```cpp
template<typename TInput, typename TOutput, typename TReal, typename TInteger>
class ImportanceResampling final {
    bool Resample(const TFunction& f, const TPDF& p,
                  const TRandomSampler& sampler,
                  const TOutputNorm& norm, TInteger numSourceSamples) noexcept;
    bool Integrate(TOutput& result, const TFunction& f,
                   const TDiscreteSampler& discreteSampler,
                   const TUniformSampler& uniformSampler,
                   TInteger numIterations) noexcept;
};
```

#### MonteCarloIntegrator (`Engine/Math/MonteCarloIntegrator.h`)

Simple Monte Carlo integration with importance sampling.

```cpp
template<typename TInput, typename TOutput, typename TReal, typename TInteger>
class MonteCarloIntegrator final {
    template<typename TFunction, typename TPDF, typename TRandomGen>
    bool operator()(TOutput& result, const TFunction& f, const TPDF& p,
                    const TRandomGen& sample, TInteger numIterations) noexcept;
};
```

---

## 7. String Utilities

### String (`Engine/String/String.h`)

Dynamic string class with shareable buffer and comprehensive utility methods.

```cpp
class String final {
    // Construction from various types
    String() noexcept;
    explicit String(const char* text) noexcept;
    explicit String(bool, int, float, double, ...) noexcept;
    String(const char* text) noexcept;

    // Comparison
    bool operator==(const String& rhs) const noexcept;
    bool operator<(const String& rhs) const noexcept;

    // Query
    Index Length() const noexcept;
    bool IsEmpty() const noexcept;
    Index HashCode() const noexcept;

    // Search
    Index Find(const TChar ch) const noexcept;
    Index Find(const String& keyword) const noexcept;
    bool Contains(const String& keyword) const noexcept;
    bool StartsWith(const String& header) const noexcept;
    bool EndsWith(const String& tail) const noexcept;

    // Modification
    String Append(const String& string) const noexcept;
    void AppendSelf(const String& string) noexcept;
    String Replace(const String& from, const String& to, ...) const noexcept;
    String Trim() const noexcept;
    String GetLowerCase() const noexcept;
    String GetUpperCase() const noexcept;

    // Conversion
    int ToInt() const noexcept;
    float ToFloat() const noexcept;
    const char* ToCharArray() const noexcept;
};
```

### StaticString (`Engine/String/StaticString.h`)

Interned string stored in a global table. Pointer-equality for O(1) comparison.

```cpp
class StaticString final {
    StaticString() noexcept;
    StaticString(StaticStringID id) noexcept;
    StaticString(const char* string) noexcept;
    StaticString(const std::string_view& str) noexcept;

    const char* c_str() const noexcept;
    StaticStringID GetID() const noexcept;
    bool IsNull() const noexcept;

    // Comparison uses pointer equality
    bool operator==(const StaticString& rhs) const noexcept;
};
```

### StaticStringID (`Engine/String/StaticStringID.h`)

Lightweight identifier wrapping a pointer into the global string table.

```cpp
struct StaticStringID final {
    const uint8_t* ptr;
};
```

### StringBuilder (`Engine/String/StringBuilder.h`)

Template-based string builder with stream-style `<<` operators.

```cpp
template<class TCh = char, class TAlloc = DefaultAllocator<TCh>>
class StringBuilder final {
    StringBuilder& operator<<(nullptr_t, bool, char, int, float,
                               double, const char*, StaticString, ...) noexcept;
    StringBuilder& operator<<(EndLine) noexcept;
};
```

### InlineStringBuilder (`Engine/String/InlineStringBuilder.h`)

Fixed-size inline string builder (no heap allocation).

```cpp
template<size_t BufferSize = 1024, class TChar = char>
class InlineStringBuilder final {
    // Same stream-style interface as StringBuilder
    // Buffer is statically allocated
    void Clear() noexcept;
};
```

### StringUtil (`Engine/String/StringUtil.h`)

Utility functions for string processing.

```cpp
namespace StringUtil {
    TString Trim(const TString& str);
    TString ToLowerCase(const TString& src);
    bool EqualsIgnoreCase(const TString& a, const TString& b);
    bool StartsWith(const TString& src, const TString& startTerm);
    bool EndsWith(const TString& src, const TString& endTerm);
    TString PathToName(const TString& path);

    // Name extraction from __PRETTY_FUNCTION__
    StaticString ToFunctionName(const char* PrettyFunction);
    StaticString ToClassName(const char* PrettyFunction);
    StaticString ToCompactClassName(const char* PrettyFunction);
    StaticString ToCompactMethodName(const char* PrettyFunction);

    // C string utilities
    size_t StrLen(const char* text);
    const char* StrCopy(char* dst, const char* src, size_t n);
    size_t CalculateHash(const char* text);
}
```

### EndLine (`Engine/String/EndLine.h`)

```cpp
class EndLine final {};
static constexpr EndLine hendl;  // Use in string builders
```

### Letter (`Engine/String/Letter.h`)

Character classification utilities:

```cpp
class Letter final {
    static bool IsGenuineLetter(char ch);
    static bool IsUpperCase(char ch);
    static bool IsLowerCase(char ch);
};
```

---

## 8. Logging System

### LogLevel (`Engine/Log/LogLevel.h`)

```cpp
enum class ELogLevel : unsigned char {
    Verbose, Info, Significant, Warning, Error, FatalError, MAX
};
```

### Logger (`Engine/Log/Logger.h`)

Asynchronous, thread-safe logging system running on a dedicated IO thread.

```cpp
class Logger final {
    static Logger& Get() noexcept;
    static SimpleLogger Get(StaticString category,
                            ELogLevel level = ELogLevel::Info) noexcept;

    void AddLog(StaticString category, ELogLevel level,
                const TLogFunction& logFunc) noexcept;
    void Flush() noexcept;
    void SetFilter(StaticString category, TLogFilter&& filter) noexcept;

    // SimpleLogger — convenience wrapper
    class SimpleLogger final {
        void Out(const TLogFunction& logFunc) const noexcept;
        void OutWarning(const TLogFunction& logFunc) const noexcept;
        void OutError(const TLogFunction& logFunc) const noexcept;
        void Out(const char* text) const noexcept;
    };
};
```

### PrintArgs (`Engine/Log/PrintArgs.h`)

Variadic print utility for assertions and debugging.

```cpp
template<typename T, typename... TTypes>
void PrintArgs(const T& arg, TTypes&&... args) noexcept;
```

### LogLine (`Engine/Log/LogLine.h`)

```cpp
class LogLine final {
    StaticString category;
    ELogLevel level;
    StaticString text;
};
```

### LogUtil (`Engine/Log/LogUtil.h`)

```cpp
namespace LogUtil {
    StaticString GetLogLevelName(ELogLevel level);
    StaticString GetCurrentTimeString();
}
```

---

## 9. Configuration System

### BuildConfig (`Engine/Config/BuildConfig.h`)

Compile-time configuration defines:

| Define | Default | Purpose |
|---|---|---|
| `PLATFORM_LINUX` / `PLATFORM_OSX` / `PLATFORM_WINDOWS` | auto | Platform detection |
| `MAX_NUM_TASK_STREAMS` | 64 | Max task streams |
| `ENGINE_MIN_HARDWARE_THREADS` | 4 | Min CPU cores |
| `ENGINE_LOG_ENABLED` | 1 | Logging master switch |
| `MEMORY_VERIFICATION_ENABLED` | 0 | Memory integrity checks |
| `MEMORY_LOGGING_ENABLED` | 0 | Log memory operations |
| `MEMORY_INVESTIGATION_ENABLED` | 0 | Detailed memory debugging |
| `PROFILE_ENABLED` | 0 | Performance profiling |
| `LOG_ENABLED` | 1 | Logging system enable |
| `RIGHT_HANDED_COORDINATE` | 1 | Coordinate system |
| `VULKAN_SDK` | auto-detected | Vulkan availability |

### EngineConfig (`Engine/Config/EngineConfig.h`)

Runtime configuration constants:

```cpp
namespace Config {
    // Engine log levels
    static constexpr uint8_t EngineLogLevel = ...;
    static constexpr uint8_t EngineLogLevelPrint = ...;

    // Memory
    static constexpr size_t MemCapacity = 5ULL * 1024 * 1024 * 1024; // 5GB
    static constexpr size_t DefaultAlign = 16;

    // String
    static constexpr int MaxPathLength = 512;
    static constexpr int StaticStringBufferSize = 8 * 1024 * 1024;

    // Log
    static constexpr int LogLineLength = 1024;
    static constexpr int LogOutputBuffer = LogLineLength * 128;
    static constexpr int LogMemoryBlockSize = LogLineLength * 256;
    static constexpr int LogNumMemoryBlocks = 1024 * 12;
    static constexpr int LogForceFlushThreshold = 1024 * 8;

    // Task System
    static constexpr int MaxConcurrentTasks = 32;

    // Profile
    static constexpr float DebugTimeOutMultiplier = 2.0f;

    size_t GetMaxSystemMemoryTarget() noexcept;
}
```

### ConfigSystem (`Engine/Config/ConfigSystem.h`)

Runtime key-value configuration database.

```cpp
class ConfigSystem final {
    static ConfigSystem& Get() noexcept;

    void Register(TConfigParam<...>& engineParam);  // bool, int, size_t, float
    void SetBool(const StaticString& key, bool value);
    void SetInt(const StaticString& key, int value);
    void SetFloat(const StaticString& key, float value);

    bool GetBool(const StaticString& key) const noexcept;
    int GetInt(const StaticString& key) const noexcept;
    float GetFloat(const StaticString& key) const noexcept;

    void PrintAllParameters() const;
};
```

### ConfigParam (`Engine/Config/ConfigParam.h`)

```cpp
template<typename T, bool IsAtomic>
class ConfigParam {
    // Registered with ConfigSystem for runtime modification
};
```

### ConfigFile (`Engine/Config/ConfigFile.h`)

```cpp
class ConfigFile final {
    // Load/save configuration from/to files
};
```

---

## 10. OS Abstraction Layer (OSAL)

### Application (`Engine/OSAL/Application.h`)

```cpp
namespace OS {
class IApplication {
    virtual void Initialize() = 0;
    virtual void PollEvents() = 0;
};

std::unique_ptr<IApplication> CreateApplication();
}
```

### Window (`Engine/OSAL/Window.h`)

```cpp
namespace OS {
class IWindow {
    bool CreateWindow(const HString& title, int width, int height);
    void SetTitle(const HString& title);
    void SetSize(int width, int height);
    void SetVisible(bool visible);
    void PollEvents();
    void Close();

    int GetWidth() const;
    int GetHeight() const;
    bool IsVisible() const;
    bool IsClosed() const;
    intptr_t GetNativeHandle() const;
};

std::unique_ptr<IWindow> CreateWindow(const HString& title,
                                       int width, int height);
}
```

### File (`Engine/OSAL/File.h`)

```cpp
namespace OS {
class File final {
    const std::string& GetPath() const;
    bool operator<(const File& rhs) const;
};
using Files = std::vector<File>;
}
```

### Directory (`Engine/OSAL/Directory.h`)

```cpp
namespace OS {
class Directory final {
    const std::string path;
    const Files& FileList() const;
    const Dirs& DirList() const;
};
using Dirs = std::vector<Directory>;
}
```

### OSAbstractLayer (`Engine/OSAL/OSAbstractLayer.h`)

```cpp
namespace OS {
    HString GetFullPath(const HString& path);
    bool IsDirectory(const char* path);
    HVector<HString> ListFilesInDirectory(const char* path);
}
```

### OSDebug (`Engine/OSAL/OSDebug.h`)

```cpp
namespace OS {
    StaticString GetBackTrace(uint16_t startIndex = 0,
                              uint16_t maxDepth = 512);
}
```

### OSMemory (`Engine/OSAL/OSMemory.h`)

```cpp
namespace OS {
    bool IsValidAllocation(void* ptr) noexcept;
    template<typename T>
    bool CheckAligned(T* ptr, uint32_t alignBytes = Config::DefaultAlign) noexcept;
    constexpr size_t GetAligned(size_t size, uint32_t alignBytes) noexcept;
    size_t GetAllocSize(void* ptr) noexcept;
    size_t GetPageSize() noexcept;
    void* VirtualAlloc(size_t size);
    void VirtualFree(void* address, std::size_t n) noexcept;
    void ProtectMemory(void* address, std::size_t n) noexcept;
}
```

### OSThread (`Engine/OSAL/OSThread.h`)

```cpp
namespace OS {
    void Yield() noexcept;
    void Sleep(uint32_t milliseconds) noexcept;
    int GetCPUIndex() noexcept;
    int GetThreadPriority(std::thread& thread) noexcept;
    void SetThreadAffinity(std::thread& thread, uint64_t mask) noexcept;
    void SetThreadPriority(std::thread& thread, int priority) noexcept;
}
```

### OSInputOutput (`Engine/OSAL/OSInputOutput.h`)

```cpp
namespace OS {
    bool Open(FileHandle& outHandle, StaticString filePath,
              FileOpenMode openMode) noexcept;
    bool Close(FileHandle&& handle) noexcept;
    bool Exist(StaticString filePath) noexcept;
    bool Delete(StaticString filePath) noexcept;
    size_t Read(const FileHandle& handle, void* buffer,
                size_t size) noexcept;
    size_t Write(const FileHandle& handle, void* buffer,
                 size_t size) noexcept;
    bool Truncate(const FileHandle& handle, size_t size) noexcept;
    void* MapMemory(FileHandle& fileHandle, size_t size,
                    ProtectionMode protection, size_t offset) noexcept;
    bool MapSync(void* ptr, size_t size, MapSyncMode syncMode) noexcept;
    bool UnmapMemory(void* ptr, size_t size) noexcept;
}
```

### Platform-Specific Implementations

| Platform | Window | Application | File Handle | Debug | Memory | Thread |
|---|---|---|---|---|---|---|
| **macOS** | `OSXWindow.h/.mm` | `OSXApplication.h/.mm` | `OSXFileHandle.cpp` | `OSXDebug.cpp` | `OSXMemory.cpp` | `OSXThread.cpp` |
| **Linux** | `LinuxWindow.h/.cpp` | `LinuxApplication.h/.cpp` | `LinuxFileHandle.cpp` | `LinuxDebug.cpp` | `LinuxMemory.cpp` | `LinuxThread.cpp` |
| **Windows** | `Win32Window.h/.cpp` | `Win32Application.h/.cpp` | `Win32Window.cpp` | `WindowsDebug.cpp` | `WindowsMemory.cpp` | `WindowsThread.cpp` |

---

## 11. Resource Management

### Resource (`Engine/Resource/Resource.h`)

```cpp
class Resource final {
    Resource() noexcept;
    ~Resource() noexcept;
    // Handle to a loaded resource
};
```

### Buffer (`Engine/Resource/Buffer.h`)

```cpp
class Buffer final {
    Buffer();
    Buffer(Buffer&& rhs) noexcept;
    explicit Buffer(const TGenerateBuffer& genFunc);

    uint8_t* GetData() noexcept;
    size_t GetSize() const noexcept;

    template<typename T>
    T* GetDataAs();  // Type-safe access
    template<typename T>
    size_t TranslateSizeAs() const noexcept;
};
```

### ResourceManager (`Engine/Resource/ResourceManager.h`)

```cpp
class ResourceManager final {
    Resource RequestLoad(StaticString path);  // Async load
    Resource Load(StaticString path);         // Synchronous load
    void PostUpdate(Engine& engine) noexcept;
};
```

### BufferTypes (`Engine/Resource/BufferTypes.h`)

```cpp
namespace BufferTypes {
    using TSize = size_t;
    using TBufferData = uint8_t*;
    using TGenerateBuffer = std::function<TBufferData()>;
    using TReleaseBuffer = std::function<void(TBufferData)>;
}
```

---

## 12. HSTL — HardBop STL

Custom STL allocator wrappers for standard containers.

### HString (`Engine/HSTL/HString.h`)

```cpp
using HString = std::basic_string<char, std::char_traits<char>,
                                   DefaultAllocator<char>>;
using HInlineString<PoolSize = 128> = ...;  // InlinePool-based
using HInlinePathString = ...;  // MaxPathLength pool
```

### HVector (`Engine/HSTL/HVector.h`)

```cpp
template<typename T>
using HVector = std::vector<T, DefaultAllocator<T>>;

template<typename T, size_t PoolSize = 16>
using HInlineVector = std::vector<T, InlinePoolAllocator<T, PoolSize, 2>>;
```

### HUnorderedMap (`Engine/HSTL/HUnorderedMap.h`)

```cpp
template<class TKey, class T, class THash = std::hash<TKey>,
         class TPred = std::equal_to<TKey>>
using HUnorderedMap = std::unordered_map<TKey, T, THash, TPred,
                                          DefaultAllocator<std::pair<const TKey, T>>>;
```

---

## 13. Engine Core

### Engine (`Engine/Engine/Engine.h`)

Main entry point coordinating all subsystems.

```cpp
class Engine final {
    static Engine& Get();

    void Initialize(int argc, const char* argv[]);
    void Run();      // Main loop
    void ShutDown(); // Clean shutdown

    // Subsystem access
    MemoryManager& GetMemoryManager();
    Logger& GetLogger();
    TaskSystem& GetTaskSystem();
    SystemStatistics& GetStatistics();
    ResourceManager& GetResourceManager();
    OS::IApplication* GetApplication() const;

    void Log(ELogLevel level, const TLogFunc& func);
    void LogError(const TLogFunc& func);
    void ConsoleOutLn(const char* str);
};
```

### SystemStatistics (`Engine/Core/SystemStatistics.h`)

```cpp
class SystemStatistics final {
    void IncFrameCount() noexcept;
    void IncSlowFrameCount() noexcept;
    void IncEngineLogCount() noexcept;
    void Print() noexcept;
    void PrintAllocatorProfiles() noexcept;
    // Timing
    double GetTimeSinceStart() const noexcept;
    float GetDeltaTime() const noexcept;
};
```

### CommonUtil (`Engine/Core/CommonUtil.h`)

```cpp
template<typename Type>
Type GetAs(void* src) noexcept;     // Type-safe read from memory

template<typename Type>
void SetAs(void* dst, Type value) noexcept;  // Type-safe write to memory

template<typename Type>
void CopyAs(void* dst, void* src) noexcept;  // Type-safe copy

size_t ToAddress(void* ptr) noexcept;
template<typename T, size_t size>
size_t CountOf(T (&)[size]) noexcept;
```

### Debug (`Engine/Core/Debug.h`)

```cpp
void FlushLogs();

// Debug builds only (abort on failure)
void Assert(bool shouldBeTrue);
template<typename... Types>
void Assert(bool shouldBeTrue, Types&&... args);

// Always active
void FatalAssert(bool shouldBeTrue);
template<typename... Types>
void FatalAssert(bool shouldBeTrue, Types&&... args);
```

### Exception (`Engine/Core/Exception.h`)

```cpp
class Exception final {
    template<typename... Types>
    Exception(const char* file, int line, Types&&... args);
};
```

### Time (`Engine/Core/Time.h`)

```cpp
namespace hbe::time {
    using TMilliSec = uint64_t;
    using TStopWatch = std::chrono::high_resolution_clock;

    TTime GetNow() noexcept;
    float ToFloat(TStopWatch::duration duration) noexcept;
    double ToDouble(TStopWatch::duration duration) noexcept;
    TMilliSecs::rep ToMilliSeconds(TStopWatch::duration duration) noexcept;
    void Sleep(TMilliSec milli) noexcept;
}
```

### ScopedTime (`Engine/Core/ScopedTime.h`)

RAII scoped timer measuring execution time within a scope.

```cpp
namespace hbe::time {
class ScopedTime final {
    explicit ScopedTime(TDuration& outDeltaTime) noexcept;
    ~ScopedTime() noexcept;
};
}
```

### ScopedLock (`Engine/Core/ScopedLock.h`)

RAII lock wrapper with timeout profiling.

```cpp
template<CLockable TLockable>
class ScopedLock final {
    // Acquires lock on construction, releases on destruction
    // Profiles wait time if PROFILE_ENABLED
};
```

### CommandLineArguments (`Engine/Core/CommandLineArguments.h`)

```cpp
class CommandLineArguments final {
    CommandLineArguments(int argc, const char* argv[]);
    const Array<String>& GetArguments() const noexcept;
    void Print();
};
```

---

## 14. Renderer (RHI)

### IRenderer (`Engine/Renderer/IRenderer.h`)

```cpp
namespace hbe::Renderer {
class IRenderer {
    virtual bool Initialize(OS::IWindow* window) noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual void BeginFrame() noexcept = 0;
    virtual void EndFrame() noexcept = 0;
    virtual void Render(float deltaTime) noexcept = 0;
    virtual APIType GetAPIType() const noexcept = 0;
    virtual RenderCapabilities GetCapabilities() const noexcept = 0;
};
}
```

### RendererCommon (`Engine/Renderer/RendererCommon.h`)

```cpp
namespace hbe::Renderer {
enum class APIType : uint8_t { Unknown, Vulkan, Metal, DX12 };

struct Vertex {
    Vector3<float> position;
    Vector4<float> color;
};

class RenderCapabilities {
    APIType apiType;
    bool supportsGeometryShader;
    bool supportsTessellation;
    bool supportsComputeShader;
    uint32_t maxTextureSize;
    uint32_t maxUniformBuffers;
    uint32_t maxVertexAttribs;
};
}
```

### RendererFactory (`Engine/Renderer/RendererFactory.h`)

```cpp
namespace hbe::Renderer {
class RendererFactory final {
    static std::unique_ptr<IRenderer> Create(
        APIType preferredAPI = APIType::Unknown) noexcept;
    static std::unique_ptr<IRenderer> CreateWithFallback() noexcept;
};
}
```

### RHICapabilities (`Engine/Renderer/RHICapabilities.h`)

```cpp
namespace hbe::Renderer {
class RHICapabilities final {
    static bool IsVulkanSupported() noexcept;
    static bool IsMetalSupported() noexcept;
    static bool IsDX12Supported() noexcept;
    static APIType GetPreferredAPI() noexcept;
    static RenderCapabilities GetCapabilities(APIType api) noexcept;
};
}
```

### Vulkan Renderer (`Engine/Renderer/Vulkan/VulkanRenderer.h`)

### DX12 Renderer (`Engine/Renderer/DX12/DX12Renderer.h`)

### Metal Renderer (`Engine/Renderer/Metal/MetalRenderer.h`)

---

## 15. Test Framework

### TestCollection (`Engine/Test/TestCollection.h`)

```cpp
class TestCollection {
    using TTestFunc = std::function<void(TLogOut&)>;

    explicit TestCollection(const char* title);
    void AddTest(const char* testName, const TTestFunc& testCase);
    void Start();

    const char* GetName() const noexcept;
    bool IsDone() const noexcept;
    bool IsSuccess() const noexcept;

protected:
    virtual void Prepare() = 0;  // Override to add tests
};
```

### UnitTestCollection (`Engine/Test/UnitTestCollection.h`)

```cpp
namespace hbe::Test {
    void RunTests();  // Entry point for unit tests
}
```

### TestEnv (`Engine/Test/TestEnv.h`)

Test environment setup for unit tests.

---

## 16. Build Configuration

### BuildConfig (`Engine/Config/BuildConfig.h`) — Summary

| Category | Key Defines | Default |
|---|---|---|
| Platform | `PLATFORM_LINUX`, `PLATFORM_OSX`, `PLATFORM_WINDOWS` | Auto-detected |
| Engine | `MAX_NUM_TASK_STREAMS` | 64 |
| Debug | `__DEBUG__` | Auto (based on NDEBUG) |
| Memory | `MEMORY_VERIFICATION_ENABLED`, `MEMORY_LOGGING_ENABLED`, `MEMORY_INVESTIGATION_ENABLED` | 0 |
| Log | `LOG_ENABLED`, `LOG_BREAK_IF_WARNING`, `LOG_FORCE_PRINT_IMMEDIATELY` | 1, 0, 0 |
| Profile | `PROFILE_ENABLED` | 0 |
| Math | `RIGHT_HANDED_COORDINATE` | 1 |
| Vulkan | `VULKAN_SDK` | Auto-detected |

### Build System

See [BuildSystem.md](BuildSystem.md) for full documentation.

```bash
./build.sh <target> [-dev] [-debug] [-release] [-clean] [-notest]
# Example: ./build.sh Applications/TriangleExample -dev -debug -release -clean -notest
```

Tests: `./build/Applications/EngineTest/<Config>/EngineTest`

---

## Coding Standards

All classes are `final` unless designed for inheritance. See [CodingStandards.md](CodingStandards.md) and `Engine/CodingStandards.h` for full details.

### Naming Conventions

- **Type aliases**: `T`-prefix (`TByte`, `TInt`, `TFloat3`, `TRunnable`)
- **Template parameters**: `T`-prefix (`TNumber`, `TVec`, `TElement`)
- **Concepts**: `C`-prefix (`CLockable`, `CNext`, `CToZeroTerminateStr`)
- **Enum classes**: `E`-prefix (`ELogLevel`, `EHashEntryState`)
- **All other identifiers**: camelCase

### API Conventions

- All non-throwing functions: `noexcept`
- All value-returning getters: `[[nodiscard]]`
- Single-argument constructors: `explicit`
- RAII for resource management (ScopedLock, ScopedTime, AllocatorScope)
- Assertions in debug builds; FatalAssert always active
