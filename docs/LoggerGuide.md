# HardBop Engine - Logger Guide

## Overview

The HardBop Engine provides an asynchronous, thread-safe logging system designed for high-performance game engines. The Logger is implemented as a singleton that runs on a dedicated IO thread, allowing main application threads to log without blocking.

## Architecture

### Thread Model

The Logger operates on a producer-consumer pattern:

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Main Threads   │────▶│  Input Buffer   │────▶│   IO Thread     │
│  (Producers)    │     │  (Thread-safe)  │     │  (Consumer)     │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                                        │
                                                        ▼
                                                ┌─────────────────┐
                                                │  Console + File │
                                                └─────────────────┘
```

- **Main threads** (producers): Add log entries to a thread-safe input buffer
- **Input buffer**: Lock-protected queue for cross-thread communication
- **IO thread** (consumer): Processes buffered logs and writes to console/file

### Key Components

| Component | Description |
|-----------|-------------|
| `inputBuffer` | Thread-safe queue for incoming log entries (via `inputAlloc`) |
| `swapBuffer` | Temporary buffer used during buffer swap (via `allocator`) |
| `textBuffer` | Formatted text ready for output |
| `flushFuncs` | Output handlers (file + console via `PrintStdIO`) |
| `filters` | Category-based log level filters |
| `inputAlloc` | Thread-safe allocator for input buffer |
| `allocator` | Multi-pool allocator for internal data structures |

---

## Usage

### Basic Logging

```cpp
#include "Log/Logger.h"

auto log = Logger::Get("MyCategory");
log.Out([](auto& ls) { ls << "Hello World"; });
```

### Log Levels

```cpp
auto log = Logger::Get("Game");

log.Out(ELogLevel::Debug, [](auto& ls) { ls << "Debug info"; });
log.Out(ELogLevel::Info, [](auto& ls) { ls << "Info message"; });
log.Out(ELogLevel::Significant, [](auto& ls) { ls << "Significant event"; });
log.Out(ELogLevel::Warning, [](auto& ls) { ls << "Warning!"; });
log.Out(ELogLevel::Error, [](auto& ls) { ls << "Error occurred"; });
log.Out(ELogLevel::FatalError, [](auto& ls) { ls << "Fatal error!"; });
```

**Log Level Enum** (from `Engine/Log/LogLevel.h`):
| Level | Description |
|-------|-------------|
| `Verbose` | Verbose debug information |
| `Info` | General informational messages |
| `Significant` | Significant events worth noting |
| `Warning` | Potential issues |
| `Error` | Errors that need attention |
| `FatalError` | Critical errors causing termination |

### Convenience Methods

```cpp
auto log = Logger::Get("Game");

log.OutWarning("This is a warning");      // ELogLevel::Warning
log.OutError("This is an error");         // ELogLevel::Error
log.OutFatalError("Fatal error!");       // ELogLevel::FatalError
```

### Fallback Logging

If `Logger::Get()` returns null (logger not initialized), logs fall back to Engine's console output:

```cpp
void FallbackLog(StaticString category, ELogLevel level, const TLogFunction& logFunc)
{
    auto& engine = Engine::Get();
    Logger::TLogStream str;
    str << '[' << category << "] ";
    logFunc(str);
    engine.Log(level, [&str](auto& ls) { ls << str.c_str(); });
}
```

---

## Configuration

### Build-Time Options

Defined in `Engine/Config/BuildConfig.h`:

| Option | Description |
|--------|-------------|
| `LOG_ENABLED` | Master switch for logging (default: 1) |
| `LOG_FORCE_PRINT_IMMEDIATELY` | Bypass async, print immediately to console |
| `LOG_BREAK_IF_WARNING` | Debug break on warnings |
| `LOG_BREAK_IF_ERROR` | Debug break on errors |
| `LOG_FORCE_FLUSH_THRESHOLD` | Force flush when buffer reaches this size |

### Config Constants

From `Engine/Config/EngineConfig.h`:

| Constant | Value | Description |
|----------|-------|-------------|
| `LogLineLength` | 1024 | Maximum characters per log line |
| `LogOutputBuffer` | 131072 (128KB) | Output string builder buffer size |
| `LogMemoryBlockSize` | 262144 (256KB) | Memory block size for logger allocator |
| `LogNumMemoryBlocks` | 12288 | Number of memory blocks |
| `LogForceFlushThreshold` | 8192 | Auto-flush when buffer reaches this size |

### Runtime Configuration

```cpp
#include "Config/ConfigParam.h"

auto logLevel = TAtomicConfigParam<uint8_t>("Log.Level", "Description", 
    static_cast<uint8_t>(ELogLevel::Info));
logLevel.Set(static_cast<uint8_t>(ELogLevel::Debug));
```

### Category Filtering

```cpp
#include "Log/Logger.h"

Logger::Get().SetFilter("Audio", [](ELogLevel level) {
    return level >= ELogLevel::Warning;  // Only Warning and above
});
```

---

## Memory Management

### Internal Allocators

The Logger uses two internal allocators:

1. **LoggerMemoryPool**: For internal data structures (swapBuffer, textBuffer)
2. **LoggerInputPool**: For input buffer allocation (thread-safe)

### Allocator Usage Tracking

Each log output shows memory usage:

```
[599:23:36.62][Main][Memory][Info] [64] Memory = 0 / 1048576
```

Format: `[allocationSize] Memory = used / total`

---

## Thread Safety

### Lock Strategy

The Logger uses minimal locking:

1. **inputLock**: Protects `inputBuffer` during add operations
2. **filterLock**: Protects filter map access

### Atomic Flags

| Flag | Purpose |
|------|---------|
| `isRunning` | Logger task active state |
| `hasInput` | Input buffer has pending logs |
| `needFlush` | Output needs to be flushed |

### Flush Behavior

```cpp
void Logger::Flush();      // Wait for current flush to complete
```

**Important**: `Flush()` only works if called from a thread other than the logger thread. It polls `needFlush` with 10ms intervals.

---

## Log Format

### Output Format

```
[Timestamp][ThreadName][Category][Level] Message
```

### Example

```
[599:23:36.62][Main][Engine][Info] Engine has been initialized.
[599:23:36.62][Main][Memory][Warning] Memory usage high.
[599:23:36.62][Worker1][Game][Error] Failed to load resource.
```

### Timestamp Format

- Hours:Minutes:Seconds.Milliseconds
- Relative to engine start time

### Special Behaviors

#### Fatal Error Handling

When `ELogLevel::FatalError` is logged:
1. Immediate flush via `Flush()`
2. `debugBreak()` is called
3. `Assert(false)` terminates in debug builds

#### Error on Logger Thread

If an error is logged from the logger's own IO thread:
- Output directly to console (bypasses buffer)
- Uses `SystemAllocator` for text formatting

#### Immediate Output Options

Build-time flags for immediate logging (from `Engine/Config/BuildConfig.h`):
- `LOG_FORCE_PRINT_IMMEDIATELY`: All logs go directly to console
- `LOG_BREAK_IF_WARNING`: Debug break on Warning or higher
- `LOG_BREAK_IF_ERROR`: Debug break on Error or higher

---

## Integration with Engine

### Initialization

```cpp
// Engine.cpp
Logger logger(engine, path, filename);
logger.StartTask(taskSystem);  // Starts IO thread
```

### Shutdown Sequence

```cpp
// Engine.cpp - SignalHandler
engine.GetLogger().StopTask(engine.GetTaskSystem());
engine.CloseLog();
```

### StopTask Process

1. Sets `isRunning = false`
2. Waits for logger task to complete
3. Processes remaining buffer
4. Flushes and closes file stream
5. Logs "Logger shall be terminated."

---

## Best Practices

### 1. Use Lambdas for Lazy Evaluation

```cpp
// Good - string construction deferred
log.Out([](auto& ls) { ls << "Player " << player.name << " at " << player.pos; });

// Avoid - always constructs string
log.Out("Player " + player.name + " at " + player.pos);
```

### 2. Avoid Logging in Signal Handlers

Signal handlers have limited functionality. Use `Engine::ShutDown()` instead.

### 3. Set Filters for Noisy Categories

```cpp
Logger::Get().SetFilter("Debug", [](ELogLevel level) {
    return level >= ELogLevel::Error;
});
```

### 4. Use Flush for Critical Output

Before application exit:

```cpp
Logger::Get().Flush();
```

### 5. Prefer StaticStrings for Categories

```cpp
// Good
static StaticString category = "MyCategory";
Logger::Get(category);

// Avoid - creates string each time
Logger::Get("MyCategory");
```

---

## Troubleshooting

### Logs Not Appearing

1. **Logger not started**: Ensure `logger.StartTask()` was called
2. **Log level too high**: Check `Log.Level` config
3. **Category filtered**: Verify no filter is blocking
4. **Buffer not flushed**: Call `Flush()` before exit

### Performance Issues

1. **High log volume**: Increase buffer sizes or `LOG_FORCE_FLUSH_THRESHOLD`
2. **Thread contention**: Reduce log frequency in hot paths
3. **IO blocking**: Use async logging (default)

### Crashes on Exit

1. **Logger stopped too early**: Ensure all threads finished logging
2. **Flush from logger thread**: Only works correctly, other threads need to wait

---

## API Reference

### Logger

```cpp
class Logger final
{
    // Singleton access
    static Logger& Get();
    static SimpleLogger Get(StaticString category, ELogLevel level = ELogLevel::Info);
    
    // Lifecycle
    void StartTask(TaskSystem& taskSys);
    void StopTask(TaskSystem& taskSys);
    
    // Configuration
    void SetFilter(StaticString category, TLogFilter&& filter);
    
// Output
void Flush();
};
```

### SimpleLogger

```cpp
struct SimpleLogger
{
    const StaticString category;
    const ELogLevel level;
    
    // Output methods
    void Out(const TLogFunction& logFunc) const;
    void Out(ELogLevel level, const TLogFunction& logFunc) const;
    
    // Convenience methods with lambda
    void OutWarning(const TLogFunction& logFunc) const;
    void OutError(const TLogFunction& logFunc) const;
    void OutFatalError(const TLogFunction& logFunc) const;

    // Convenience methods with string
    void Out(const char* text) const;
    void Out(ELogLevel level, const char* text) const;
    void OutWarning(const char* text) const;
    void OutError(const char* text) const;
    void OutFatalError(const char* text) const;
};
```

---

## Related Files

- `Engine/Log/Logger.h` - Header file
- `Engine/Log/Logger.cpp` - Implementation
- `Engine/Log/LogLevel.h` - Log level definitions
- `Engine/Log/LogLine.h` - Log line structure
- `Engine/Log/LogUtil.h` - Logging utilities
- `Engine/Engine/Engine.cpp` - Logger lifecycle integration

---

## Known Issues & Potential Bugs

### 1. Static tmpTextBuffer Thread Safety Issue (Line 304-320)

```cpp
static TTextBuffer tmpTextBuffer;
tmpTextBuffer.reserve(1);
```

**Problem**: Using a static buffer for error logging from the logger thread creates a race condition. If multiple errors occur simultaneously on the logger thread, they will overwrite each other's content.

**Impact**: Only the last error message may be logged if multiple errors occur in quick succession from the logger thread.

**Recommendation**: Use a thread-local buffer or scoped allocation instead.

---

### 2. AddLog Recursive Call on Null logFunc (Line 244)

```cpp
if (unlikely(logFunc == nullptr))
{
    AddLog(GetName(), ELogLevel::Warning, [](auto& ls) { ls << "Null log function!"; });
    return;
}
```

**Problem**: When `logFunc` is null, `AddLog` calls itself recursively with a valid lambda. If the filter or log level settings cause this warning to be filtered out, no log is produced. However, if there's a bug in the filter logic, this could cause infinite recursion.

**Impact**: Low - but creates a potential stack overflow risk if filter logic has bugs.

**Recommendation**: Use FallbackLog directly instead of AddLog for this internal warning.

---

### 3. Flush() Busy Wait on Different Thread (Line 373-377)

```cpp
constexpr auto period = std::chrono::milliseconds(10);
while (needFlush.load(std::memory_order_relaxed))
{
    std::this_thread::sleep_for(period);
}
```

**Problem**: When called from a non-logger thread, this polls with 10ms sleep intervals. During heavy logging, this could cause significant delays (up to ~80ms if buffer fills completely).

**Impact**: Performance degradation when calling Flush() under heavy load.

**Recommendation**: Consider using a condition variable for more efficient waiting.

---

### 4. StopTask AddLog After isRunning = false (Line 206, 224)

```cpp
void Logger::StopTask(TaskSystem& taskSys)
{
    isRunning.store(false, std::memory_order_release);  // Line 206
    ...
    AddLog(GetName(), ELogLevel::Info, [](auto& ls) { ls << "Logger shall be terminated." << hendl; });  // Line 224
```

**Problem**: `AddLog` can still add entries after `isRunning` is set to false because:
- The check `if (unlikely(instance == nullptr))` passes (instance is valid)
- The `LOG_ENABLED` check passes
- The runnable in `ProcessBuffer` checks `isRunning` but `AddLog` doesn't

**Impact**: Last log message may be lost if it gets added after the logger thread has already stopped checking for new work.

---

### 5. Null Instance Check in SimpleLogger::Out (Line 57-61, 68-72)

```cpp
if (unlikely(instance == nullptr))
{
    FallbackLog(category, level, logFunc);
    return;
}
```

**Problem**: Between logger construction and `StartTask`, or after `StopTask` but before destructor, the logger is in an inconsistent state. Logs will go to FallbackLog which uses Engine's synchronous logging.

**Impact**: During startup/shutdown phases, logging behavior differs from normal operation.

---

### 6. LogLine Text Buffer Overflow (LogLine.h)

```cpp
union
{
    char text[Config::LogLineLength];  // 1024 bytes
    ...
};
```

**Problem**: If log message exceeds 1024 characters, only the first 1024 characters are stored (depending on LogLine implementation).

**Impact**: Long log messages may be truncated without indication.

---

### 7. Memory Allocation in Hot Path (Line 334)

```cpp
{
    std::lock_guard lock(inputLock);
    AllocatorScope inputAllocScope(inputAlloc);
    inputBuffer.emplace_back(level, threadName, category, ls.c_str(), ls.Size());
}
```

**Problem**: Every log message performs a heap allocation via `inputAlloc` even when logging is disabled at the filter level.

**Impact**: Performance overhead in high-frequency logging scenarios.

---

### 8. Missing Return After FatalError Flush (Line 342-347)

```cpp
if (unlikely(level >= ELogLevel::FatalError))
{
    Flush();
    debugBreak();
    Assert(false);
    return;
}
```

**Note**: The code does have `return;` after `Assert(false)` in most cases, but the control flow could be confusing. The `Assert(false)` in debug builds does not return, so the explicit `return` is necessary.

---

These issues represent edge cases in the Logger implementation. Most normal usage patterns will not encounter these problems, but they are documented here for completeness and for developers who need to debug unusual logging behavior.
