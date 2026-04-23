# TaskSystem Guide

The TaskSystem is a multi-threaded task execution framework that manages worker threads and distributes tasks across multiple streams for parallel processing.

## Architecture Overview

```
TaskSystem
├── Base Stream (index 0) - Main application thread
├── IO Stream (index 1)   - I/O-bound operations
└── Worker Streams       - Additional threads for parallel task execution
```

## Core Components

### 1. Task (`Task.h`)
A task represents a unit of work that can be split into multiple sub-tasks for parallel execution.

**Key Features:**
- Encapsulates a runnable function with user data
- Tracks the number of sub-tasks and finished sub-tasks using atomic counters
- Can generate ranged sub-tasks for parallel processing
- Provides both busy-wait and interval-based wait methods for synchronization

**Usage:**
```cpp
auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t {
    // Process range [start, end)
    // Return the number of indices processed
    // If less than (end - start), the task will be re-scheduled
    return end - start;
};

Task task("MyTask", func, &userData);

// Split into sub-tasks with automatic range distribution
task.Start(numSubTasks, startIndex, endIndex);
task.BusyWait();  // Spin until all sub-tasks complete
```

### 2. RangedTask (`RangedTask.h`)
A sub-task with a specific range `[start, end)` that belongs to a parent Task.

**Key Features:**
- Contains priority, affinity, and range information
- Tracks current index during incremental execution
- Automatically reports completion to the parent Task when finished

**Important:** A RangedTask can be re-added to the queue if its runnable doesn't process the entire range in one call. This enables incremental task execution.

### 3. Runnable (`Runnable.h`)
A function pointer type for task execution:
```cpp
using Runnable = std::size_t(*)(void* userData, std::size_t startIndex, std::size_t endIndex);
```
**Returns:** The number of indices processed. If less than `(endIndex - startIndex)`, the RangedTask will be re-scheduled with an updated `startIndex`.

### 4. TaskStream (`TaskStream.h`)
A dedicated thread that processes a queue of RangedTasks.

**Key Features:**
- Owns a dedicated thread for task execution
- Uses a `BoundedPriorityQueue` for efficient task scheduling
- Supports task affinity (restricts which streams can execute a task)
- Uses `MultiPoolAllocator` for efficient memory allocation
- Implements a re-adding buffer for incremental task execution

**Behavior:**
- Waits on a condition variable when the queue is empty
- Processes tasks in priority order (higher priority first)
- Removes finished tasks and re-adds incomplete tasks via an internal buffer
- Supports wake-up signals for urgent tasks
- Logs warnings for tasks exceeding the duration threshold (configurable, default 0.16s)

### 5. TaskStreamAffinity (`TaskStreamAffinity.h`)
A bitmask template class that tracks which task streams have processed a task.

**Implementation:**
- Uses a 64-bit bit array by default (`TaskStreamAffinityBase<64>`)
- `Set(bitIndex)` marks a stream as having processed the task (affinity granted)
- `Get(bitIndex)` checks if a stream has processed the task
- Initially all bits are unset (task can run on any stream)

**Purpose:** Prevents stream starvation by ensuring global queue tasks are distributed across different streams.

### 6. TaskSystem (`TaskSystem.h`)
The central system that manages all task streams and coordinates task execution.

**Responsibilities:**
- Creates and manages worker threads based on hardware concurrency
- Provides the base stream (index 0) and IO stream (index 1)
- Maintains a global task queue with priority scheduling
- Supports dispatching tasks for execution on the main thread
- Thread-local storage for stream names and indices

**Static Methods:**
```cpp
TaskSystem::GetNumHardwareThreads();       // Returns the number of CPU cores
TaskSystem::IsBaseThread();                // Check if running on base stream (was "IsMainThread")
TaskSystem::IsIOThread();                  // Check if running on IO stream
TaskSystem::GetCurrentStreamIndex();       // Get current stream index
TaskSystem::GetCurrentThreadName();        // Get thread name
TaskSystem::GetBaseTaskStreamIndex();      // Returns 0
TaskSystem::GetIOTaskStreamIndex();        // Returns 1
```

## Task Distribution

### Stream-Specific Tasks
Tasks can be enqueued to specific streams:
```cpp
taskSystem.Enqueue(TaskSystem::GetIOTaskStreamIndex(), rangedTask);  // IO stream
taskSystem.Enqueue(TaskSystem::GetBaseTaskStreamIndex(), rangedTask);  // Base stream
taskSystem.Enqueue(2, rangedTask);  // Worker stream (use named constants for clarity)
```

### Global Task Queue
Tasks can be enqueued to the shared global queue:
```cpp
taskSystem.Enqueue(rangedTask);  // Added to global priority queue
```

### Affinity-Based Dequeue
When a TaskStream's local queue is empty, it attempts to dequeue from the global queue. Before executing a task from the global queue, the stream sets its affinity bit on the task. If a task already has affinity for the current stream, it cannot be taken from the global queue—preventing any single stream from monopolizing global queue tasks.

## Pre-defined Streams

| Index | Name  | Purpose                        |
|-------|-------|--------------------------------|
| 0     | Base  | Main application thread        |
| 1     | IO    | I/O-bound operations          |
| 2+    | Worker| Additional parallel workers   |

## Example Usage

### Creating and Running Tasks
```cpp
auto& taskSystem = engine.GetTaskSystem();

auto computeFunc = [](void* data, std::size_t start, std::size_t end) -> std::size_t {
    auto* result = static_cast<double*>(data);
    for (std::size_t i = start; i < end; ++i) {
        *result += 1.0 / (i + 1);
    }
    return end - start;  // All indices processed
};

Task task("Compute", computeFunc, &result);

// Split into 10 sub-tasks
constexpr std::size_t count = 1000000;
constexpr std::size_t numSubtasks = 10;
constexpr std::size_t increment = count / numSubtasks;

for (std::size_t i = 0; i < count; i += increment) {
    taskSystem.Enqueue(task.GenerateSubTask(i, i + increment));
}

task.BusyWait();  // Wait for all sub-tasks to complete
```

### Incremental Task Processing
A runnable can return before processing the entire range, enabling time-slicing:
```cpp
auto incrementalFunc = [](void* data, std::size_t start, std::size_t end) -> std::size_t {
    auto* result = static_cast<double*>(data);
    std::size_t processed = 0;
    
    // Process a chunk at a time
    for (std::size_t i = start; i < end && processed < chunkSize; ++i, ++processed) {
        *result += 1.0 / (i + 1);
    }
    
    return processed;  // Less than full range = task will resume later
};
```

### Dispatching to Main Thread
For tasks that must run on the main thread (e.g., UI updates):
```cpp
taskSystem.DispatchToMainThread([](void* userData) {
    // This runs on the main thread
    auto* data = static_cast<MyData*>(userData);
    data->ui->Update();
}, userData, 0);  // priority parameter

// Call periodically from main thread
taskSystem.ProcessMainThreadTasks();
```

## Thread Safety

- Task queues use `std::mutex` for synchronization
- Condition variables coordinate thread sleeping and waking
- `std::atomic` tracks sub-task completion counts
- Thread-local storage maintains per-thread stream index and name
- Memory barrier (seq_cst) ensures proper synchronization in `ReportFinishedSubTask()`

## Memory Management

- TaskStream uses `MultiPoolAllocator` for efficient task allocation
- Reduces allocation overhead for frequent task creation and destruction
- A re-adding buffer (`HVector<RangedTask>`) minimizes allocations during incremental execution

## Integration with Engine

The TaskSystem is initialized by the Engine:
```cpp
auto& engine = Engine::Get();
auto& taskSystem = engine.GetTaskSystem();

taskSystem.Initialize();
// ... run tasks ...
taskSystem.RequestShutDown();
taskSystem.JoinAndClear();  // Wait for all streams to finish and clean up
```

## Testing

Unit tests in `TaskSystem.cpp` include:
- Empty task verification (`Empty Task`)
- Zero-size task handling (`Task of size 0`)
- Parallel computation verifying the Basel problem solution (`Bagel Problem`)
- Incremental task processing (`Bagel Problem (Incremental Task)`)