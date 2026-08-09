# Plan: Apply CommonMacros.h Macros Across the Codebase

## Goal
Apply the new macros from `Engine/Core/CommonMacros.h` (`returnIf`, `breakIf`, `continueIf`, `ONCE`) across the entire codebase to replace verbose `if (...) return/break/continue` patterns.

## Scope
Apply macros in all `.h`, `.cpp`, `.inl`, `.mm` files under `Engine/` directory. **NOT** modifying: test files (`CodingStandards.h/cpp` test mocks), or `CommonMacros.h` itself.

## Macro Analysis
| Macro | Replaces | Condition | Value return support? |
|-------|----------|-----------|----------------------|
| `returnIf(...)` | `if (...) return` | void return only | No — `return` without value |
| `returnValueIf(val, ...)` | `if (...) return val` | condition + value return | Yes — return value is first arg |
| `breakIf(...)` | `if (...) break` | loop break (inside braces) | N/A |
| `continueIf(...)` | `if (...) continue` | loop continue (inside braces) | N/A |
| `ONCE()` | `do {} while(false)` | one-shot scope | N/A (no such patterns found) |

**Usage convention:**
- `returnIf(condition);` → replaces `if (condition) return;`
- `returnValueIf(value, condition);` → replaces `if (condition) return value;`
- `breakIf(condition);` → replaces `if (condition) break;`
- `continueIf(condition);` → replaces `if (condition) continue;`

**Key constraint:** `breakIf`/`continueIf` only apply when `if (...) { break; }` or `if (...) { continue; }` have the entire body being just the break/continue — i.e., `if (...) break;` or `if (...) continue;` on one line, or `if (...) { break; }`. When there are additional statements in the brace body, the macro still works: `if (cond) { stmt; break; }` → can't use because macro adds its own `{ }` — actually `breakIf(cond);` would be standalone. So only single-statement `if(cond) break;` patterns (no braces or single-line brace) qualify.

**Note on `breakIf`/`continueIf`:** The macro wraps in `if (cond) break` without braces. If the existing code has `if (cond) { continue; }` or `if (cond) { break; }`, the macro form `continueIf(cond);` is equivalent and safe — it replaces the entire if+body.

**Note on switch-case breaks:** `breakIf` is NOT suitable for switch-case statement breaks that follow non-empty case bodies, since `breakIf(cond)` adds an `if` wrapper. Switch-case breaks without condition cannot use `breakIf`.

## Files to Modify (15 files)

### 1. Engine/Container/Vector.h (1 site)
- `if (data == nullptr) return;` → `returnIf(data == nullptr);`
- `if (newCapacity <= capacity) return;` → `returnIf(newCapacity <= capacity);`

### 2. Engine/Container/HashMap.h (4 sites)
- Line ~271: `if (idx < 0 || states[idx] != EHashEntryState::Occupied) return end();` — **value return, skip**
- Line ~374: `if (newCapacity <= cap) return;` → `returnIf(newCapacity <= cap);`
- Line ~568: `if (entries == nullptr) return;` → `returnIf(entries == nullptr);`
- Line ~403: `if (cap == 0) return -1;` — **value return, skip**
- Line ~413: `if (states[slot] == EHashEntryState::Empty) return -1;` — **value return, skip**
- Lines ~68, ~207 in Vector.h: same as Vector.h above

### 3. Engine/Container/Deque.h (1 site)
- `if (data == nullptr) return;` → `returnIf(data == nullptr);`

### 4. Engine/Container/RingQueue.h (1 site)
- `if (data == nullptr) return;` → `returnIf(data == nullptr);`

### 5. Engine/Container/Map.h (2 sites)
- `if (entries == nullptr) return;` → `returnIf(entries == nullptr);`
- `if (count == 0) return -1;` — **value return, skip**

### 6. Engine/Container/LinkedList.h — N/A (all value returns)

### 7. Engine/Container/BoundedPriorityQueue.h — N/A (all value returns with std::nullopt/0)

### 8. Engine/String/Letter.h — N/A (all `return false` — value returns)

### 9. Engine/Math/MonteCarloIntegrator.h — N/A (all `return false` — value returns)

### 10. Engine/Math/ImportanceResampling.h (1 site)
- `if (weight <= 0) continue;` → `continueIf(weight <= 0);`

### 11. Engine/Math/MatrixCommonImpl.inl — N/A (value return `return false`)

### 12. Engine/Math/VectorCommonImpl.inl — N/A (value return `return Lerp(...)`)

### 13. Engine/OSAL/LinuxWindow.cpp (2 sites)
- `if (!display) return false;` — **value return, skip**
- `if (!window) return false;` — **value return, skip**
- `if (!display) return;` → `returnIf(!display);`

### 14. Engine/OSAL/Win32Window.cpp — 1 site
- `if (hwnd == nullptr) return false;` — **value return, skip**

### 15. Engine/OSAL/OSXWindow.mm (3 sites)
- `if (nsWindow != nullptr) return false;` — **value return, skip**
- `if (window == nullptr) return false;` — **value return, skip**
- `if (nsWindow == nullptr) return;` → `returnIf(nsWindow == nullptr);` (×3: lines 58, 67, 96)

### 16. Engine/OSAL/OSXApplication.mm — 1 site
- `if (appHandle == nullptr) return;` → `returnIf(appHandle == nullptr);`

### 17. Engine/OSAL/OSXAbstractLayer.cpp (3 sites) — continue patterns
- `if (element->d_name[0] == '.' || element->d_name[0] == '\0') continue;` → `continueIf(...)` (×3)

### 18. Engine/OSAL/LinuxAbstractLayer.cpp (3 sites) — continue patterns
- `if (element->d_name[0] == '.' || element->d_name[0] == '\0') continue;` → `continueIf(...)` (×3)

### 19. Engine/OSAL/Directory.cpp (1 site) — continue pattern
- `if (name[0] == '.') continue;` → `continueIf(name[0] == '.');`

### 20. Engine/OSAL/LinuxFileHandle.cpp — 1 site
- `if (fd < 0) return;` → `returnIf(fd < 0);`

### 21. Engine/OSAL/OSXFileHandle.cpp — 1 site
- `if (fd < 0) return;` → `returnIf(fd < 0);`

### 22. Engine/OSAL/OSXInputOutput.cpp — N/A (value returns)

### 23. Engine/OSAL/LinuxInputOutput.cpp — N/A (value returns)

### 24. Engine/OSAL/WindowsAbstractLayer.cpp — N/A (value returns)

### 25. Engine/OSAL/Win32FileHandle.cpp — need to check
- `if (fd < 0) return;`?

### 26. Engine/OSAL/Win32InputOutput.cpp — need to check

### 27. Engine/Core/ComponentSystem.cpp (1 site)
- `if (!isValid) return;` → `returnIf(!isValid);`
- `if (!testResult) return;` → `returnIf(!testResult);`

### 28. Engine/Test/TestCollection.cpp (1 site)
- `if (messages == nullptr) return;` → `returnIf(messages == nullptr);`

### 29. Engine/Resource/BufferInputStream.h — 1 site
- `if (length <= 0) return;` → `returnIf(length <= 0);`

### 30. Engine/Resource/BufferOutputStream.h (2 sites)
- `if (cursor >= size) return;` → `returnIf(cursor >= size);` (×2)

### 31. Engine/Resource/Buffer.cpp — 1 site
- `if (data == nullptr) return;` → `returnIf(data == nullptr);`

### 32. Engine/Resource/BufferOutputStream.cpp — 1 site
- `if (unlikely(str == nullptr)) return *this;` — **value return, skip**

### 33. Engine/Renderer/DX12/DX12Renderer.cpp — 2 sites (value returns, skip)

### 34. Engine/Renderer/Vulkan/VulkanRenderer.cpp — 2 sites (value returns, skip)

### 35. Engine/Renderer/Metal/MetalRenderer.mm — 2 sites (value returns in EndFrame — skip)
- But wait: let me re-check — some of these have `return;` (void)

### 36. Engine/Log/Logger.cpp (4 sites)
- `if (task.HasDone()) return;` → `returnIf(task.HasDone());`
- `if (filter != nullptr && !filter(level)) return;` → `returnIf(filter != nullptr && !filter(level));`
- `if (!hasInput.load(std::memory_order_acquire)) return;` → `returnIf(!hasInput.load(std::memory_order_acquire));`
- `if (swapBuffer.empty()) return;` → `returnIf(swapBuffer.empty());`

### 37. Engine/Log/LogLine.cpp (1 site)
- `if (likely(!isLong)) return;` → `returnIf(likely(!isLong));`

### 38. Engine/OSAL/OSMemory.cpp — 1 site
- `if (...) return;` → need to check

### 39. Engine/OSAL/WindowsMemory.cpp — 1 site
- `if (...) return;` → need to check

### 40. Engine/OSAL/UnixThread.cpp — check for continue/break
- `LinuxThread.cpp`: `if (...) continue;` → `continueIf(...)`

## Verification
1. Build Debug: `./build.sh Applications/EngineTest -dev -debug`
2. Run tests: `./build/Applications/EngineTest/Debug/EngineTest` (expect 51/51 pass)
3. Build TriangleExample if applicable
4. Verify no regressions
