# TaskSystem Guide

The TaskSystem is a multi-threaded task execution framework that manages worker threads and task distribution across multiple streams.

## Architecture Overview

```
TaskSystem
├── Main Stream (index 0) - Main application thread
├── IO Stream (index 1)   - I/O operations
└── Worker Streams        - Worker threads for parallel task execution
```

## Core Components

### 1. Task (`Task.h`)
A task represents a unit of work that can be split into multiple sub-tasks for parallel execution.

**Key Features:**
- Contains a runnable function with user data
- Tracks number of sub-tasks and finished sub-tasks
- Can generate ranged sub-tasks for parallel processing
- Provides busy-wait and interval-based wait methods

**Usage:**
```cpp
auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t {
    // Process range [start, end)
    return end - start;
};

Task task("MyTask", func, &userData);
task.Start(numSubTasks, startIndex, endIndex);
task.BusyWait();  // Wait for completion
```

### 2. RangedTask (`RangedTask.h`)
A sub-task with a specific range `[start, end)`. Each RangedTask belongs to a parent Task.

**Key Features:**
- Contains priority, affinity, and range information
- Tracks current index during execution
- Automatically reports completion to parent Task

### 3. TaskStream (`TaskStream.h`)
A single thread that processes a queue of RangedTasks.

**Key Features:**
- Owns a dedicated thread for task execution
- Uses priority queue for task scheduling
- Supports task affinity (can bind tasks to specific streams)
- Uses MultiPoolAllocator for efficient memory allocation

**Behavior:**
- Waits on a condition variable when queue is empty
- Processes tasks in priority order
- Supports wake-up signals for urgent tasks

### 4. TaskStreamAffinity (`TaskStreamAffinity.h`)
Template class that tracks which task streams have processed a task.

**Implementation:**
- Uses a 64-bit bit array
- `Set(bitIndex)` marks a stream as having processed the task
- `Get(bitIndex)` checks if a stream has processed the task

### 5. TaskSystem (`TaskSystem.h`)
Main system that manages all task streams.

**Responsibilities:**
- Creates and manages worker threads based on hardware concurrency
- Provides main stream (index 0) and IO stream (index 1)
- Handles global task queue with priority scheduling
- Thread-local storage for stream names and indices

**Static Methods:**
```cpp
TaskSystem::GetNumHardwareThreads();       // Get CPU core count
TaskSystem::IsMainThread();                 // Check if on main thread
TaskSystem::IsIOThread();                    // Check if on IO thread
TaskSystem::GetCurrentStreamIndex();        // Get current stream index
TaskSystem::GetCurrentThreadName();         // Get thread name
```

## Task Distribution

### Stream-Specific Tasks
Tasks can be enqueued to specific streams:
```cpp
taskSystem.Enqueue(streamIndex, rangedTask);
```

### Global Task Queue
Low-priority tasks can be enqueued to the global queue:
```cpp
taskSystem.Enqueue(task);  // Added to global priority queue
```

### Affinity-Based Dequeue
When dequeuing from the global queue, tasks can be automatically assigned affinity to prevent stream starvation:
```cpp
std::optional<RangedTask> task;
taskSystem.Dequeue(task);  // Gets task with stream affinity
```

## Pre-defined Streams

| Index | Name  | Purpose                    |
|-------|-------|----------------------------|
| 0     | Main  | Main application thread    |
| 1     | IO    | I/O operations            |
| 2+    | Worker| Parallel task execution   |

## Example Usage

### Creating and Running Tasks
```cpp
auto& taskSystem = engine.GetTaskSystem();

auto computeFunc = [](void* data, std::size_t start, std::size_t end) -> std::size_t {
    auto* result = static_cast<double*>(data);
    for (std::size_t i = start; i < end; ++i) {
        *result += 1.0 / (i + 1);
    }
    return end - start;
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

### Enqueueing to Specific Stream
```cpp
// Enqueue to IO stream for I/O-bound tasks
taskSystem.Enqueue(TaskSystem::GetIOTaskStreamIndex(), rangedTask);

// Enqueue to worker stream
taskSystem.Enqueue(2, rangedTask);
```

## Thread Safety

- Task queues use `std::mutex` for synchronization
- Condition variables coordinate thread sleeping/waking
- `std::atomic` tracks task completion
- Thread-local storage stores per-thread stream index and name

## Memory Management

- TaskStream uses `MultiPoolAllocator` for efficient task allocation
- Reduces allocation overhead for frequent task creation/destruction

## Integration with Engine

The TaskSystem is initialized by the Engine:
```cpp
auto& engine = Engine::Get();
auto& taskSystem = engine.GetTaskSystem();

taskSystem.Initialize();
// ... run tasks ...
taskSystem.RequestShutDown();
taskSystem.JoinAndClean();  // Wait for all streams to finish
```

## Testing

Unit tests in `TaskSystem.cpp` include:
- Empty task verification
- Zero-size task handling
- Parallel computation (Bagel problem)
- Incremental task processing
