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
