# hb-standards per-file verdict

**Generated — do not edit.** Every row measures a tracked source **as it stands on disk**, produced by
`python3 .pi/skills/hb-standards/scripts/file_ledger.py --write`. Measured against 6a6c7d9 (tree dated 2026-10-04) with
`Homebrew clang-format version 22.1.8`.

The previous edition of this file was hand-written and its labels had rotted: 246 of its 256
measurable rows described formatting work that had already been done, and 19 tracked sources had no
row at all. That is why the command above is the only way this table changes.

| column | measured by | value |
|---|---|---|
| clang-format | `clang-format --style=file` compared through `blank_lines.py --collapse-seam`, as `check.sh` does | `clean`, `clean apart from A3`, what else would change, or `n/a` |
| blank lines | `blank_lines.py` (rule set A) | finding count |
| include preamble | `includes.py` (rule set B) | finding count |
| comments | `comments.py` (the comment ban) | finding count |
| member layout | `layout.py` (twelve-block order from the clang AST) | finding count, `n/a`, or `unmeasured` |

`n/a` means the layer has nothing to judge here; `unmeasured` means it has something to judge and could
not — the two are never printed alike, and neither is a pass. **This table says nothing about the**
**docs coverage, API reference, or build-and-test layers** of `SKILL.md`, which are gated by
`docs_coverage.py` and `gate.sh` and are not per-file verdicts.

Two counts a reader should not act on alone, both named here because the rows cannot say them:

* `Engine/CodingStandards.h` reports every comment it holds, because `comments.py` exempts only
  `Engine/CodingStandards.cpp`. `docs/CodingStandards.md` exempts the `.cpp` outright and the `.h`
  only "the BAD EXAMPLE blocks" of it, while `check.sh` skips `Engine/CodingStandards.*` whole. So
  the count is true under the checker and unreachable through the gate, and which of the two should
  win is an open owner decision, not a fact this table can settle.
* `unmeasured` in *member layout* is itself a finding about that file. `Engine/Memory/ScopedAllocator.h`
  is rejected by clang when compiled as its own translation unit because it names `std::forward` on
  line 28 and includes no `<utility>` — it builds today only because its consumers reach that header
  first, which is the same latent shape `Engine/Core/TaskSystem.cpp` carried before `50efdaa`.

6 path(s) carried uncommitted edits while this ran, so those rows describe work in progress rather
than 6a6c7d9.

3 file(s) carry a column this run could not measure: unmeasured 3.

| file | clang-format | blank lines | include preamble | comments | member layout |
|---|---|---|---|---|---|
| Applications/EngineTest/TestMain.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Applications/VulkanExample/Main.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/CodingStandards.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/CodingStandards.h | clean | 0 | 0 | 120 | 0 |
| Engine/Config/BuildConfig.h | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Config/ConfigFile.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Config/ConfigFile.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Config/ConfigParam.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Config/ConfigParam.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Config/ConfigSystem.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Config/ConfigSystem.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Config/EngineConfig.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Config/EngineConfig.h | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/Array.cpp | clean | 2 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/Array.h | clean | 9 | 0 | 2 | 0 (partial) |
| Engine/Container/AtomicStackView.cpp | clean | 10 | 0 | 0 | 0 (partial) |
| Engine/Container/AtomicStackView.h | clean | 7 | 0 | 1 | 3 (partial) |
| Engine/Container/BoundedPriorityQueue.cpp | clean | 31 | 0 | 5 | 0 (partial) |
| Engine/Container/BoundedPriorityQueue.h | clean | 5 | 0 | 33 | 15 (partial) |
| Engine/Container/Deque.cpp | clean | 24 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/Deque.h | clean | 14 | 0 | 0 | 9 (partial) |
| Engine/Container/HashMap.cpp | clean | 23 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/HashMap.h | clean | 12 | 0 | 0 | 11 (partial) |
| Engine/Container/LinkedList.cpp | clean | 11 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/Container/LinkedList.h | clean | 7 | 0 | 14 | 0 (partial) |
| Engine/Container/Map.cpp | clean | 20 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/Map.h | clean | 8 | 0 | 0 | 9 (partial) |
| Engine/Container/Queue.cpp | clean | 17 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/Queue.h | clean | 6 | 0 | 0 | 1 (partial) |
| Engine/Container/RingQueue.cpp | clean | 18 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Container/RingQueue.h | clean | 13 | 0 | 1 | 5 (partial) |
| Engine/Container/Vector.cpp | clean | 23 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Container/Vector.h | clean | 11 | 0 | 0 | 7 (partial) |
| Engine/Core/CPUBudget.cpp | clean apart from A3 | 0 | 0 | 4 | n/a (no class or struct defined here) |
| Engine/Core/CPUBudget.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/CommandLineArguments.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/CommandLineArguments.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/CommonMacros.h | clean | 0 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Core/CommonUtil.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/Component.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/Component.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/ComponentState.h | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/ComponentSystem.cpp | clean | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/ComponentSystem.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/Constants.h | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/Debug.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/Debug.h | clean | 0 | 0 | 11 | n/a (no class or struct defined here) |
| Engine/Core/Exception.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/MainThreadTaskQueue.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/MainThreadTaskQueue.h | clean apart from A3 | 0 | 0 | 0 | 0 (+1 waived) |
| Engine/Core/ResultPacket.cpp | clean | 0 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/Core/ResultPacket.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/Runnable.h | clean apart from A3 | 0 | 0 | 9 | n/a (no class or struct defined here) |
| Engine/Core/ScopedLock.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/ScopedLock.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/ScopedTime.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/StreamDrainPolicy.cpp | clean apart from A3 | 0 | 0 | 16 | 0 (partial) |
| Engine/Core/StreamDrainPolicy.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/SystemStatistics.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/SystemStatistics.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/Task.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/Task.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/TaskID.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Core/TaskProvider.cpp | clean apart from A3 | 0 | 0 | 17 | 0 (partial) |
| Engine/Core/TaskProvider.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/TaskRegistry.cpp | clean apart from A3 | 0 | 0 | 11 | n/a (no class or struct defined here) |
| Engine/Core/TaskRegistry.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/TaskStream.cpp | clean apart from A3 | 0 | 0 | 34 | 0 |
| Engine/Core/TaskStream.h | clean apart from A3 | 0 | 0 | 0 | 0 (+1 waived) |
| Engine/Core/TaskStreamAffinity.cpp | clean | 0 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Core/TaskStreamAffinity.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/TaskStreamIndex.h | clean apart from A3 | 0 | 0 | 11 | n/a (no class or struct defined here) |
| Engine/Core/TaskSystem.cpp | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/TaskSystem.h | clean apart from A3 | 0 | 0 | 0 | 0 (partial) |
| Engine/Core/Time.cpp | clean apart from A3 | 0 | 0 | 7 | n/a (no class or struct defined here) |
| Engine/Core/Time.h | clean apart from A3 | 0 | 0 | 35 | 0 (partial) |
| Engine/Core/Types.h | clean apart from A3 | 0 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Core/WorkItem.cpp | clean apart from A3 | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Core/WorkItem.h | clean apart from A3 | 0 | 0 | 0 | 0 |
| Engine/Engine/Engine.cpp | clean | 7 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Engine/Engine.h | clean | 2 | 0 | 0 | 0 |
| Engine/Engine/EngineInitLevel.h | clean | 2 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/HSTL/HString.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/HSTL/HString.h | clean | 1 | 0 | 0 | 0 |
| Engine/HSTL/HUnorderedMap.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/HSTL/HUnorderedMap.h | clean | 3 | 0 | 0 | 0 (partial) |
| Engine/HSTL/HVector.h | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Log/LogLevel.h | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Log/LogLine.cpp | clean | 4 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/Log/LogLine.h | clean | 4 | 0 | 0 | 0 |
| Engine/Log/LogUtil.cpp | clean | 4 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Log/LogUtil.h | clean | 6 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Log/Logger.cpp | clean | 16 | 1 | 0 | 0 |
| Engine/Log/Logger.h | clean | 3 | 0 | 0 | 0 |
| Engine/Log/PrintArgs.h | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/AABB.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/AABB.h | clean | 5 | 0 | 1 | 2 (partial) |
| Engine/Math/CoordinateOrientation.h | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/ImportanceResampling.h | clean | 1 | 0 | 35 | 0 (partial) |
| Engine/Math/ImportanceSampling.cpp | clean | 21 | 0 | 7 | n/a (no class or struct defined here) |
| Engine/Math/MathUtil.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/MathUtil.h | clean | 8 | 0 | 0 | 0 (partial) |
| Engine/Math/Matrix2x2.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Matrix2x2.h | clean | 2 | 0 | 1 | 2 |
| Engine/Math/Matrix3x3.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Matrix3x3.h | clean | 3 | 0 | 1 | 2 (partial) |
| Engine/Math/Matrix4x4.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Matrix4x4.h | clean | 3 | 0 | 1 | 2 |
| Engine/Math/MatrixCommonImpl.inl | n/a (include body: clang-format needs --assume-filename) | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/MonteCarloIntegrator.cpp | clean | 7 | 0 | 10 | 0 (partial) |
| Engine/Math/MonteCarloIntegrator.h | clean | 3 | 0 | 6 | 0 (partial) |
| Engine/Math/OBB.cpp | clean | 3 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Math/OBB.h | clean | 3 | 0 | 4 | 0 (partial) |
| Engine/Math/PerlinNoise.cpp | clean | 6 | 0 | 17 | n/a (no class or struct defined here) |
| Engine/Math/PerlinNoise.h | clean | 2 | 0 | 22 | 6 (partial) |
| Engine/Math/Quaternion.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Quaternion.h | clean | 3 | 0 | 4 | 2 (partial) |
| Engine/Math/RigidTransform.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/RigidTransform.h | clean | 3 | 0 | 1 | 0 (partial) |
| Engine/Math/StratifiedSampling.cpp | clean | 9 | 0 | 7 | 0 (partial) |
| Engine/Math/StratifiedSampling.h | clean | 2 | 0 | 5 | 0 (partial) |
| Engine/Math/Transform.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Transform.h | clean | 3 | 0 | 1 | 0 (partial) |
| Engine/Math/UniformTransform.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/UniformTransform.h | clean | 2 | 0 | 1 | 0 (partial) |
| Engine/Math/Vector2.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Vector2.h | clean | 2 | 0 | 1 | 4 (partial) |
| Engine/Math/Vector3.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Vector3.h | clean | 2 | 0 | 1 | 2 (partial) |
| Engine/Math/Vector4.cpp | clean | 2 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Math/Vector4.h | clean | 2 | 0 | 1 | 4 (partial) |
| Engine/Math/VectorCommonImpl.inl | n/a (include body: clang-format needs --assume-filename) | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/AllocStats.cpp | clean | 4 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/AllocStats.h | clean | 4 | 0 | 0 | 0 |
| Engine/Memory/AllocatorID.h | clean | 3 | 0 | 5 | n/a (no class or struct defined here) |
| Engine/Memory/AllocatorProxy.h | clean | 4 | 0 | 5 | 0 |
| Engine/Memory/AllocatorScope.cpp | clean | 5 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/AllocatorScope.h | clean | 5 | 0 | 3 | 2 (partial) |
| Engine/Memory/DefaultAllocator.cpp | clean | 4 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/DefaultAllocator.h | clean | 7 | 0 | 7 | 1 (partial) |
| Engine/Memory/GlobalAllocation.cpp | clean | 3 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/Memory/InlineMonotonicAllocator.cpp | clean | 6 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/InlineMonotonicAllocator.h | clean | 8 | 0 | 0 | 0 (partial) |
| Engine/Memory/InlinePoolAllocator.cpp | clean | 4 | 0 | 0 | 0 (partial) |
| Engine/Memory/InlinePoolAllocator.h | clean | 11 | 0 | 7 | 5 (partial) |
| Engine/Memory/Memory.h | clean | 5 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/MemoryManager.cpp | clean | 13 | 0 | 7 | n/a (no class or struct defined here) |
| Engine/Memory/MemoryManager.h | clean | 4 | 0 | 19 | 10 |
| Engine/Memory/MonotonicAllocator.cpp | clean | 14 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/MonotonicAllocator.h | clean | 4 | 0 | 3 | 0 (partial) |
| Engine/Memory/MultiPoolAllocator.cpp | clean | 16 | 0 | 4 | n/a (no class or struct defined here) |
| Engine/Memory/MultiPoolAllocator.h | clean | 2 | 0 | 5 | 2 (partial) |
| Engine/Memory/MultiPoolAllocatorConfig.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/MultiPoolAllocatorConfig.h | clean | 3 | 0 | 2 | 0 |
| Engine/Memory/MultiPoolConfigCache.cpp | clean | 5 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/MultiPoolConfigCache.h | clean | 2 | 0 | 2 | 1 |
| Engine/Memory/Optional.cpp | clean | 1 | 0 | 0 | 0 (partial) |
| Engine/Memory/Optional.h | clean | 5 | 0 | 2 | 0 (partial) |
| Engine/Memory/PoolAllocator.cpp | clean | 14 | 0 | 21 | 0 (partial) |
| Engine/Memory/PoolAllocator.h | clean | 3 | 0 | 3 | 0 (partial) |
| Engine/Memory/PoolConfig.h | clean | 3 | 0 | 1 | 0 |
| Engine/Memory/PoolConfigUtil.cpp | clean | 6 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/PoolConfigUtil.h | clean | 6 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Memory/ScopedAllocator.h | clean | 2 | 0 | 3 | unmeasured (clang rejected this file: [1m/Users/anav/atelier/hardbopengine/Engine/Memory/ScopedAllocator.h:28:15: [0m[0;1;31merror: [0m[1muse of undeclared identifier 'std'[0m) |
| Engine/Memory/Shareable.h | clean | 3 | 0 | 6 | 0 |
| Engine/Memory/StackAllocator.cpp | clean | 22 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/StackAllocator.h | clean | 3 | 0 | 11 | 0 (partial) |
| Engine/Memory/SystemAllocator.cpp | clean | 1 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/Memory/SystemAllocator.h | clean | 1 | 0 | 2 | 0 (partial) |
| Engine/Memory/ThreadSafeMultiPoolAllocator.cpp | clean | 18 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/Memory/ThreadSafeMultiPoolAllocator.h | clean | 1 | 0 | 4 | 2 (partial) |
| Engine/OSAL/Application.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/Application.h | clean | 3 | 0 | 7 | 1 |
| Engine/OSAL/Directory.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/Directory.h | clean | 1 | 0 | 1 | 2 |
| Engine/OSAL/File.h | clean | 2 | 0 | 1 | 1 |
| Engine/OSAL/Intrinsic.h | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxAbstractLayer.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxApplication.cpp | clean | 2 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxDebug.cpp | clean | 1 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxFileHandle.cpp | clean | 3 | 1 | 0 | n/a (no class body reached by clang in this file) |
| Engine/OSAL/LinuxFileOpenMode.cpp | clean | 1 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxInputOutput.cpp | clean | 5 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxIntrinsic.h | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxMapSyncMode.cpp | clean | 3 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxMemory.cpp | clean | 0 | 0 | 2 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxProtectionMode.cpp | clean | 3 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/LinuxThread.cpp | clean | 1 | 1 | 0 | unmeasured (no namespace and no declared name to filter the AST by) |
| Engine/OSAL/LinuxWindow.cpp | clean | 2 | 0 | 2 | n/a (no class or struct defined here) |
| Engine/OSAL/OSAbstractLayer.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSAbstractLayer.h | clean | 4 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/OSAL/OSDebug.cpp | clean | 2 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/OSAL/OSDebug.h | clean | 5 | 0 | 2 | 0 (partial) |
| Engine/OSAL/OSFileHandle.h | clean | 3 | 0 | 0 | 0 |
| Engine/OSAL/OSFileOpenMode.h | clean | 3 | 0 | 1 | 0 |
| Engine/OSAL/OSInputOutput.cpp | clean | 10 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSInputOutput.h | clean | 5 | 0 | 1 | 0 (partial) |
| Engine/OSAL/OSMapSyncMode.h | clean | 3 | 0 | 1 | 0 |
| Engine/OSAL/OSMemory.cpp | clean | 7 | 1 | 27 | n/a (no class or struct defined here) |
| Engine/OSAL/OSMemory.h | clean | 7 | 0 | 3 | 0 (partial) |
| Engine/OSAL/OSProtectionMode.h | clean | 3 | 0 | 1 | 0 |
| Engine/OSAL/OSThread.cpp | clean | 4 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSThread.h | clean | 3 | 0 | 17 | 0 (partial) |
| Engine/OSAL/OSXAbstractLayer.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXApplication.mm | n/a (Objective-C++: .clang-format declares Language: Cpp only) | 3 | 1 | 12 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXDebug.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXFileHandle.cpp | clean | 7 | 1 | 0 | n/a (no class body reached by clang in this file) |
| Engine/OSAL/OSXFileOpenMode.cpp | clean | 3 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXInputOutput.cpp | clean | 8 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXIntrinsic.h | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXMapSyncMode.cpp | clean | 3 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXMemory.cpp | clean | 1 | 0 | 2 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXProtectionMode.cpp | clean | 3 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/OSXThread.cpp | clean | 0 | 0 | 2 | unmeasured (no namespace and no declared name to filter the AST by) |
| Engine/OSAL/OSXWindow.mm | n/a (Objective-C++: .clang-format declares Language: Cpp only) | 5 | 1 | 6 | n/a (no class or struct defined here) |
| Engine/OSAL/SourceLocation.h | clean | 4 | 0 | 3 | 4 |
| Engine/OSAL/Win32Application.cpp | clean | 2 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/OSAL/Win32Window.cpp | clean | 2 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/Window.cpp | clean | 24 | 0 | 0 | 0 (partial) |
| Engine/OSAL/Window.h | clean | 5 | 0 | 7 | 6 (partial) |
| Engine/OSAL/WindowsAbstractLayer.cpp | clean | 2 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/WindowsDebug.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/WindowsInputOutput.cpp | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/WindowsIntrinsic.h | clean | 0 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/OSAL/WindowsMemory.cpp | clean | 2 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/OSAL/WindowsThread.cpp | clean | 2 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Renderer/RHICapabilities.cpp | clean | 6 | 0 | 12 | n/a (no class or struct defined here) |
| Engine/Renderer/RHICapabilities.h | clean | 5 | 0 | 8 | 0 (partial) |
| Engine/Renderer/RenderCapabilities.cpp | clean | 3 | 0 | 6 | n/a (no class or struct defined here) |
| Engine/Renderer/RenderCapabilities.h | clean | 3 | 0 | 106 | 0 |
| Engine/Renderer/RendererTest.cpp | clean | 1 | 0 | 7 | n/a (no class or struct defined here) |
| Engine/Renderer/RendererTest.h | clean | 2 | 0 | 0 | 0 (partial) |
| Engine/Renderer/Vertex.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Renderer/Vertex.h | clean | 3 | 0 | 1 | 0 |
| Engine/Renderer/Vulkan/ShadersSpv.h | clean | 4 | 0 | 1 | n/a (no class or struct defined here) |
| Engine/Renderer/Vulkan/VulkanCapabilities.cpp | clean | 10 | 0 | 23 | n/a (no class or struct defined here) |
| Engine/Renderer/Vulkan/VulkanCapabilities.h | clean | 3 | 0 | 16 | n/a (no class or struct defined here) |
| Engine/Renderer/Vulkan/VulkanRenderer.cpp | clean | 55 | 0 | 47 | n/a (no class or struct defined here) |
| Engine/Renderer/Vulkan/VulkanRenderer.h | clean | 3 | 0 | 22 | 44 |
| Engine/Renderer/Vulkan/VulkanRenderer.mm | n/a (Objective-C++: .clang-format declares Language: Cpp only) | 7 | 0 | 10 | n/a (no class or struct defined here) |
| Engine/Resource/Buffer.cpp | clean | 17 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/Buffer.h | clean | 4 | 0 | 0 | 0 (partial) |
| Engine/Resource/BufferInputStream.cpp | clean | 12 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/BufferInputStream.h | clean | 11 | 0 | 0 | 0 (partial) |
| Engine/Resource/BufferOutputStream.cpp | clean | 7 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/BufferOutputStream.h | clean | 9 | 0 | 0 | 0 (partial) |
| Engine/Resource/BufferTypes.h | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/BufferUtil.cpp | clean | 5 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/BufferUtil.h | clean | 2 | 0 | 0 | 0 |
| Engine/Resource/Resource.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/Resource.h | clean | 5 | 0 | 0 | 0 |
| Engine/Resource/ResourceManager.cpp | clean | 3 | 1 | 0 | n/a (no class or struct defined here) |
| Engine/Resource/ResourceManager.h | clean | 3 | 0 | 0 | 0 |
| Engine/String/EndLine.h | clean | 3 | 0 | 2 | 0 |
| Engine/String/InlineStringBuilder.cpp | clean | 4 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/String/InlineStringBuilder.h | clean | 4 | 0 | 1 | 3 (partial) |
| Engine/String/Letter.h | clean | 1 | 0 | 1 | 0 |
| Engine/String/StaticString.cpp | clean | 5 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/String/StaticString.h | clean | 7 | 0 | 1 | 1 (partial) |
| Engine/String/StaticStringID.h | clean | 2 | 0 | 1 | 0 |
| Engine/String/StaticStringTable.cpp | clean | 13 | 1 | 0 | 0 |
| Engine/String/StaticStringTable.h | clean | 4 | 0 | 1 | 2 |
| Engine/String/String.cpp | clean | 9 | 0 | 6 | n/a (no class or struct defined here) |
| Engine/String/String.h | clean | 5 | 0 | 1 | 2 (partial) |
| Engine/String/StringBuilder.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/String/StringBuilder.h | clean | 24 | 0 | 1 | 3 (partial) |
| Engine/String/StringUtil.cpp | clean | 6 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/String/StringUtil.h | clean | 5 | 0 | 0 | 0 (partial) |
| Engine/Test/TestCollection.cpp | clean | 5 | 0 | 5 | n/a (no class or struct defined here) |
| Engine/Test/TestCollection.h | clean | 2 | 0 | 40 | 14 |
| Engine/Test/TestEnv.cpp | clean | 5 | 0 | 3 | n/a (no class or struct defined here) |
| Engine/Test/TestEnv.h | clean | 2 | 0 | 42 | 14 |
| Engine/Test/TestHelper.cpp | clean | 3 | 0 | 0 | n/a (no class or struct defined here) |
| Engine/Test/TestHelper.h | clean | 2 | 0 | 0 | 0 |
| Engine/Test/Testlet.h | clean | 2 | 0 | 17 | 4 |
| Engine/Test/UnitTestCollection.cpp | clean | 2 | 0 | 8 | n/a (no class or struct defined here) |
| Engine/Test/UnitTestCollection.h | clean | 0 | 0 | 10 | n/a (no class or struct defined here) |
| Examples/WindowExample/Main.cpp | clean apart from A3 | 0 | 0 | 0 | 0 |

278 row(s) for 278 tracked source(s); 278 row(s) reconcile(s) by path with `git ls-files`.
