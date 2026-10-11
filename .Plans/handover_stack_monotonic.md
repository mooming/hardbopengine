# Handover: `StackAllocator` and `MonotonicAllocator` reference pages (Memory comment migration, wave 3)

Author stopped on instruction after roughly 25 tool calls, mid-class. This file exists so the resumption does not
re-read the world. Everything below was read from source in this checkout; nothing was built and nothing was committed.

## 1. State on disk right now

| Path | State |
|---|---|
| `docs/Memory/StackAllocator/index.html` | **written and validated** by `docs_page.py` (20971 bytes). Its own method links are still pending. |
| `docs/Memory/StackAllocator/*.html` method pages | **not written** — every attempt so far was rejected before landing (causes in section 7). |
| `docs/Memory/MonotonicAllocator/` | **does not exist** — no page written. |
| `Engine/Memory/StackAllocator.h`, `Engine/Memory/MonotonicAllocator.h` | **unmodified.** The `/// API reference:` pointer line is NOT yet added to either header. Add it only after the class page passes its gates. |
| `/tmp/hbe-frag/Memory/StackAllocator/{index,constructors,destructor,allocate}.body.html` | drafted fragments. Copied into `.Plans/handover_stack_monotonic_fragments/` so they survive `/tmp`. The `index` fragment is the one already rendered into the class page; the other three are unwritten prose. |

Fragments saved in the tree: `.Plans/handover_stack_monotonic_fragments/index.body.html`,
`constructors.body.html`, `destructor.body.html`, `allocate.body.html`. Regenerate with:

```
python3 .pi/skills/hb-docs/scripts/docs_page.py method Memory StackAllocator constructors \
    --source Engine/Memory/StackAllocator.h --label StackAllocator --summary "..." \
    --body /tmp/hbe-frag/Memory/StackAllocator/constructors.body.html
```

`docs_page.py` writes `<name>.html.new`, validates it with `htmlcheck.py`, and moves it only on success — so a
rejection means nothing landed, which is why the fragments must be kept.

## 2. Every entry the two headers declare, with the exact signature text

### `Engine/Memory/StackAllocator.h` — `class StackAllocator final`, namespace `hbe`

Aliases (public, header order): `using This = StackAllocator;` · `using SizeType = size_t;` ·
`using TSrcLoc = hbe::source_location;` (inside `#if PROFILE_ENABLED`).

Data (private, header order): `TAllocatorID id;` · `TAllocatorID parentID;` · `SizeType capacity;` ·
`SizeType cursor;` · anonymous `union { Byte* buffer; Pointer bufferPtr; };` ·
`TSrcLoc srcLocation;` (inside `#if PROFILE_ENABLED`).

Functions (public unless noted):

```
static size_t GetSize(Pointer)                    // header comment: "// Not Supported" — body returns 0
StackAllocator(const char* name, SizeType capacity, const TSrcLoc& location = TSrcLoc::current());   // #if PROFILE_ENABLED
StackAllocator(const char* name, SizeType capacity);                                               // #else
~StackAllocator();
[[nodiscard]] Pointer Allocate(size_t size);
void Deallocate(Pointer ptr, SizeType size) noexcept;
[[nodiscard]] size_t GetAvailable() const;
[[nodiscard]] size_t GetUsage() const;
[[nodiscard]] auto GetID() const { return id; }   // defined in the class body
bool IsMine(Pointer ptr) const;                   // private, no [[nodiscard]]
```

Test-only, inside `#ifdef __UNIT_TEST__`: `class StackAllocatorTest : public TestCollection` with
`void Prepare() override;`. Owns no page; gets a Coverage row.

### `Engine/Memory/MonotonicAllocator.h` — `class MonotonicAllocator final`, namespace `hbe`

Aliases (public): `using TSize = size_t;` · `using TPointer = void*;`.
Data (private, header order): `TAllocatorID id;` · `TAllocatorID parentID;` · `TSize cursor;` ·
`TSize capacity;` · anonymous `union { uint8_t* buffer; TPointer bufferPtr; };`. No profile member, no name member.

```
MonotonicAllocator(const char* name, TSize capacity);
~MonotonicAllocator();
[[nodiscard]] TPointer Allocate(size_t size);
void Deallocate(const TPointer ptr, TSize size) noexcept;
[[nodiscard]] size_t GetAvailable() const;
[[nodiscard]] size_t GetUsage() const;
[[nodiscard]] auto GetID() const { return id; }   // defined in the class body
[[nodiscard]] bool IsMine(const TPointer ptr) const;   // private, and it IS [[nodiscard]] here
```

No `GetSize` on this class — `StackAllocator` has one, `MonotonicAllocator` does not. Test-only
`MonotonicAllocatorTest : public TestCollection` with `void Prepare() override;`.

### What `docs_pass.py declarations()` extracts (the text the signatures gate will paste)

Verified by running the parser directly. It keeps preprocessor lines, so the Signature blocks are noisy and that is
correct — the gate owns that text:

* `StackAllocator::GetSize` → chunk starts `#endif`, then a blank line, then `public:`, then
  `static size_t GetSize(Pointer)`.
* `StackAllocator` → two entries: `#if PROFILE_ENABLED` + the 3-argument declaration; `#else` + the 2-argument one.
* `~StackAllocator` → starts `#endif` then `~StackAllocator()`.
* All `MonotonicAllocator` groups are clean single lines, including
  `[[nodiscard]] bool IsMine(const TPointer ptr) const`.

Each method page must therefore carry `<h2 id="signature">Signature</h2>`, a blank line, then one
`<pre><code>…</code></pre>`; the gate replaces it byte-identically. Run the gate after the prose is written.

## 3. Established facts, with the file and symbol they came from

### 3.1 Reset, and what it does to pointers still held

* Neither class declares `Reset`, `Rewind`, `ReleaseAll` or any mass-release entry point. The only mass release is the
  destructor. (`Engine/Memory/StackAllocator.h`, `Engine/Memory/MonotonicAllocator.h` — full member lists in section 2.)
* `StackAllocator::~StackAllocator()` (`Engine/Memory/StackAllocator.cpp`) is
  `mmgr.Deallocate(bufferPtr, capacity);` then `mmgr.DeregisterAllocator(GetID())`. Every pointer into the buffer dies at
  that call, released or not; blocks that came from the overflow path are not released there and survive the object.
* `MonotonicAllocator::~MonotonicAllocator()` is `ReportDeallocation(id, bufferPtr, 0, cursor)` under `PROFILE_ENABLED`,
  then `mmgr.Deallocate(bufferPtr, capacity); mmgr.DeregisterAllocator(GetID());`. Same mass death.
* The single release that keeps the rest alive is a stack rewind: bytes below the released block keep their addresses;
  the released address is immediately reusable by the next `Allocate`; nothing is cleared or poisoned first.
* Misrouted buffer release: `MemoryManager::Deallocate(void* ptr, size_t nBytes)` (`Engine/Memory/MemoryManager.cpp`) is
  `Deallocate(GetScopedAllocatorID(), ptr, nBytes);` — the destructor's call resolves to *that* overload, so the buffer
  goes back through whichever scope is open at destruction, not through the recorded `parentID`. `PoolAllocator::~PoolAllocator()`
  (`Engine/Memory/PoolAllocator.cpp`) releases through `parentID` instead; these two do not. That is the headline asymmetry
  between the already-documented `PoolAllocator` page and these two.
* Consequence of the reverse ordering (allocator destroyed while its own `AllocatorScope` is open): the release dispatches
  into that allocator's own `Deallocate`, which reports the pointer as mismatched — `Assert(cursor >= size)` fires first.
  `ScopedAllocator<TAlloc>` (`Engine/Memory/ScopedAllocator.h`) declares `allocator` before `scope`, so the scope dies
  first and the ordering is safe; the `StackAllocatorTest` testlets declare `stack` before `scope` for the same reason.
  No testlet covers the unsafe order.

### 3.2 Is an individual deallocation legal, and what happens if attempted

* `StackAllocator::Deallocate(Pointer ptr, SizeType size) noexcept` — legal, but last-in-first-out only, and only with a
  byte count that rounds up to the size the block was charged. Order of checks: `IsMine` first; a foreign pointer goes to
  `mmgr.Deallocate(parentID, ptr, requested)` (the caller's raw count, not the rounded one). For a pointer inside the
  buffer: round the count, `Assert(cursor >= size)`, then compare `buffer + cursor` with `ptr + size`; on mismatch
  `mmgr.LogError(...)` naming `ptr`, the expected and the provided address, then `Assert(false)`, then `return` — the
  cursor is left where it was, so the bytes stay charged. On a match `cursor -= size`.
* Release-build severity: `Assert` is an empty inline function when `RELEASE_BUILD` (`Engine/Core/Debug.h`), and
  `MemoryManager::Log` (`Engine/Memory/MemoryManager.cpp`) has its entire body inside `#if MEMORY_LOGGING_ENABLED`,
  which `Engine/Config/BuildConfig.h` defines as 0. So a mismatched release in a shipped build is a silent leak.
* Double release / stale pointer, derived from the same code and **not covered by any testlet**: allocate A, release A,
  allocate C (which returns the same address at the same offset), then release the stale A again — `IsMine` is true,
  `cursor >= size` holds, `buffer + cursor == A + size` matches, and the cursor rewinds, freeing C while its holder is
  still using it. There are no block headers and no generation counter, so nothing can detect it.
* `MonotonicAllocator::Deallocate(const TPointer ptr, TSize size) noexcept` — legal and **inert**. Foreign pointer →
  `mmgr.Deallocate(parentID, ptr, requested)`. Otherwise: a `Verbose` log (`MEMORY_LOGGING_ENABLED`, 0 here) saying the
  call "shall be ignored", a `ReportDeallocation(id, ptr, requested, 0)` under `PROFILE_ENABLED` (0 here), and nothing
  else. No assert, no error, no cursor movement. Usage never decreases before destruction.
* Null pointer: `IsMine(nullptr)` is false for a non-null buffer (`nullptr < buffer`), so `Deallocate(nullptr, n)` reaches
  `MemoryManager::Deallocate(TId, void*, size_t)`, whose first act is `if (ptr == nullptr) { Assert(nBytes == 0); return; }`.
  Releasing null with a non-zero count asserts in Debug/Dev; with 0 it is a clean no-op. Contrast `PoolAllocator::Deallocate`,
  which ignores null silently before any dispatch.
* Nothing detects a misdirected release: `AllocatorProxy` (`Engine/Memory/AllocatorProxy.h`) stores `id`, `next`,
  `allocator`, `allocate`, `deallocate` — no base address, no range, no owner. The manager's only release-path checks are
  `IsValid(id)` and `deallocate != nullptr`. `MEMORY_VERIFICATION_ENABLED` (0) is the only owner/thread stamping.

### 3.3 Alignment above the engine default

* No alignment parameter exists on either class, in either direction (no aligned allocate, no aligned allocate-and-size).
* Both `Allocate` functions round the request inline: `constexpr auto AlignUnit = Config::DefaultAlign;` /
  `const auto multiplier = (size + AlignUnit - 1) / AlignUnit;` / `size = multiplier * AlignUnit;`
  (`StackAllocator::Allocate`, `MonotonicAllocator::Allocate`, both in `Engine/Memory/`). The same rounding is applied to
  the byte count on the stack's release path, which is what makes the size pairing tolerant: allocate 20 (charged 32) and
  release with 17 (rounds to 32) matches.
* `Config::DefaultAlign` is `static constexpr size_t DefaultAlign = 16;` in `Engine/Config/EngineConfig.h`.
* Constructors round `capacity` the same way, so the buffer size is always a multiple of 16.
* What is *not* guaranteed: the alignment of the buffer itself. Addresses are `buffer + cursor` with `cursor` a multiple
  of 16, so blocks are 16 bytes apart, but the base is whatever the parent's answer happened to be — default path
  `SystemAllocator<T>::AllocateBytes` → `malloc` (`Engine/Memory/SystemAllocator.h`). No test asserts an address modulus.
  Over-aligned requests (32/64/64-byte SIMD types) are served with 16-byte spacing and no check. `PoolAllocator`'s
  Coverage records the same shape of gap as review finding 9 (2026-10-05), still open.

### 3.4 Behaviour at exhaustion

* `StackAllocator::Allocate`: `if (unlikely(size > freeSize)) return mmgr.FallbackAllocate(GetID(), parentID, requested);`
  — note `requested`, not the rounded `size`. `MemoryManager::FallbackAllocate` is `Allocate(parentID, requestedSize)` plus
  a `PROFILE_ENABLED` `ReportFallback`. No log on this path at all.
* `MonotonicAllocator::Allocate`: `mmgr.LogWarning([size, freeSize]…)` reading `"The requested size N is exceeding its
  limit, F."` then `auto ptr = mmgr.Allocate(parentID, requested);`. Different helper, so under `PROFILE_ENABLED` the
  monotonic overflow is recorded as the parent's allocation and never as a fallback; the warning itself is dead code here
  (`MEMORY_LOGGING_ENABLED` 0).
* Both comparisons use the rounded size, so the last partial 16 bytes of a buffer are unreachable for a request that would
  otherwise fit.
* Neither ever returns "out of memory" itself. A null can still arrive: `MemoryManager::Allocate(TId id, size_t nBytes)`
  returns `nullptr` for a 0-byte request, and ends `Assert(ptr != nullptr, "Allocation Failed")` — abort in Debug/Dev,
  silent null in Release.
* `GetAvailable()`/`GetUsage()` count buffer bytes only, so an allocator reporting 0 available still allocates, and
  `GetUsage()` under-reports everything a caller holds that came from the parent.
* `GetUsage()` asserts `cursor < capacity` (strict) in both classes, and `GetAvailable()` asserts `capacity >= cursor`.
  A exactly-full buffer therefore trips `GetUsage()`'s assert in Debug/Dev — as does any zero-capacity allocator
  (`0 < 0` is false). Derived from source; untested.

### 3.5 Who owns the backing store

* Both constructors: round capacity → `parentID = hbe::MemoryManager::GetCurrentAllocatorID();` →
  `bufferPtr = mmgr.Allocate(capacity);` → `id = mmgr.RegisterAllocator(this, name, false, capacity, allocFunc, deallocFunc);`
  (`StackAllocator::StackAllocator`, `MonotonicAllocator::StackAllocator`-equivalent, both in `Engine/Memory/`).
* `MemoryManager::GetCurrentAllocatorID()` returns the thread-local `scopedAllocatorID`, defined
  `thread_local MemoryManager::TId MemoryManager::scopedAllocatorID = 0;` in `Engine/Memory/MemoryManager.cpp`; `0` is
  `SystemAllocatorID`. `MemoryManager::Allocate(size_t nBytes)` is `Allocate(GetScopedAllocatorID(), nBytes)`. So the scope
  open at construction supplies the buffer, and both classes record the same id as `parentID`.
* `parentID` is assigned in the constructor **body**, not the member initializer list, in the configuration this tree
  compiles (for `StackAllocator` the initializer list names it only under `PROFILE_ENABLED`).
* The object owns the buffer for its whole life: no `GetBuffer()` accessor exists on either class (unlike `PoolAllocator`),
  callers never see it, and only the destructor returns it. Callers own only the blocks inside it.
* `RegisterAllocator` stores `this` in the proxy, so the object must outlive every allocation made through its id. Name
  is passed to the registration only (`nullptr` becomes `"None"`); neither class stores a name and neither has `GetName()`.
* Registration failure: `proxyPool.Pop()` empty → `Log(ELogLevel::FatalError, …)` and return `InvalidAllocatorID`; the
  constructor stores `-1` and keeps the buffer. Id space is `MaxNumAllocators = 256` (`Engine/Memory/AllocatorID.h`,
  wave 4 owns that page — cite, do not author).
* `MemoryManager::GetInstance()` guards with `FatalAssert(mmgrInstance != nullptr)` and `FatalAssert` aborts in every
  configuration (`Engine/Core/Debug.h`, it sits outside the `#if !RELEASE_BUILD` block) — constructing before the manager
  exists is fatal.
* `MemoryManager::DeregisterAllocator(TId id)` clears the proxy's function pointers and ends `proxyPool.Push(allocator);`,
  so ids are recycled. Under `PROFILE_ENABLED` it warns `"Memory leak is detected!"` when `stats.usage > 0` and returns
  early — skipping the push, so that id is never reused.
* `StackAllocator::GetSize(Pointer)` is `static`, ignores its argument and returns 0. No caller anywhere: the only
  allocator `GetSize` call site in the tree is `PoolAllocator`'s own test on a `PoolAllocator`
  (`Engine/Memory/PoolAllocator.cpp`).

## 4. Usage in the engine (for the example sections)

Neither class is instantiated by any engine or application source. The only users are their own test collections,
registered in `Engine/Test/UnitTestCollection.cpp` (`AddTestCollection<StackAllocatorTest>()`,
`AddTestCollection<MonotonicAllocatorTest>()`). Real snippets already lifted:

* `Engine/Memory/StackAllocator.cpp`, `StackAllocatorTest::Prepare()` —
  `StackAllocator stack("Test::StackAllocator", 1024 * 1024); { AllocatorScope scope(stack.GetID()); HVector<int> a; a.push_back(0); }`
  and the `Nested Usage` testlet's `ScopedAllocator<TAlloc> scope0("NestedStack0", 1024);` with `New<long double>(0)` /
  `Delete(ptr)` six levels deep; and the `Deallocation` testlet that checks `stack.GetUsage() != 0` after the scope closes.
* `Engine/Memory/MonotonicAllocator.cpp`, `MonotonicAllocatorTest::Prepare()` — same shape with
  `MonotonicAllocator alloc("Test::MonotonicAllocator", 1024 * 1024);` and the assertion reading
  `"Monotonic Allocator doesn't provide deallocation."` — the test *requires* usage to stay non-zero after the scope closes.
* The only real call sites of `Allocate`/`Deallocate` are the two registration lambdas handed to `RegisterAllocator` in
  each constructor.

## 5. Page list intended (19 pages, 2 classes)

`docs/Memory/StackAllocator/`: `index.html` (done) + `constructors.html`, `destructor.html`, `allocate.html`,
`deallocate.html`, `getsize.html`, `getavailable.html`, `getusage.html`, `getid.html`, `ismine.html` → 10.

`docs/Memory/MonotonicAllocator/`: `index.html` + `constructors.html`, `destructor.html`, `allocate.html`,
`deallocate.html`, `getavailable.html`, `getusage.html`, `getid.html`, `ismine.html` → 9.

File names are what `docs_methods.py` accepts (`norm()` strips case and punctuation; `constructors`/`destructor` are
special-cased; no operators in either class). Private `IsMine` still needs its page — `PoolAllocator` sets that precedent.

After all pages exist: `docs_pass.py signatures`, then add the one pointer line to each header
(`/// API reference: docs/Memory/StackAllocator/index.html` above `class StackAllocator final`, likewise for
`MonotonicAllocator`), then re-run `docs_coverage.py check-file`. Do not touch `docs/Memory/index.html` or
`docs/index.html`.

## 6. Gate baseline measured before anything was written

```
docs_coverage.py check-file Engine/Memory/StackAllocator.h
  [BLOCKS STRIP] … StackAllocator has no reference page (docs/Memory/StackAllocator/index.html)
  file docs check: Engine/Memory/StackAllocator.h - 1 blocker(s)        (same for MonotonicAllocator, 1 blocker)
docs_methods.py Memory --only StackAllocator,MonotonicAllocator
  method coverage Memory (filtered …): 0 method(s) without a page … / method pages missing: 0
```
The zero is a baseline artefact: `docs_methods.py` skips classes that own no folder, so the demands appear the moment
the folders exist. Expect ~17 `DOC-METHOD` rows until every page lands.

## 7. Open questions already identified as named omissions, and two tool traps

Named omissions (each already written into the `StackAllocator` class page's Coverage section; repeat them on the
`MonotonicAllocator` class page):

1. Thread-safety: neither `.cpp` takes a lock or touches an atomic, `cursor` is a plain member, no testlet runs either
   allocator from two threads, and `MEMORY_VERIFICATION_ENABLED` (0) is what would stamp a thread id. Whether sharing is
   "merely" racy or checked somewhere else is unknown; the manager's rules belong to the module page.
2. Whether an implicit copy or move of either class compiles — a build claim, and this step runs no compiler. What is
   certain: the destructor frees the buffer and deregisters the id, so two live objects over one buffer is a double free,
   and (unlike `PoolAllocator`) neither class declares anything about the matter.
3. The alignment a returned pointer actually carries beyond "16 bytes apart inside one buffer".
4. Every `#if PROFILE_ENABLED` path: `TSrcLoc`, `srcLocation`, the `location` parameter, `ReportAllocation`,
   `ReportDeallocation`, `ReportFallback`, the 2-argument `DeregisterAllocator`, the leak warning and the skipped
   `proxyPool.Push`. `BuildConfig.h` defines the macro 0; `JOURNAL.md` (2026-10-05) records that turning it on breaks the
   build before any Memory file. Described from source, never executed.
5. Zero-capacity construction and failed construction (null buffer, `InvalidAllocatorID` while holding a buffer) — read
   from code, no testlet, so named rather than documented as a contract.
6. What a mismatched or out-of-order release leaves behind in a Release build — the code path is documented, the size of
   the leak and the survival of any diagnostic are not demonstrated.
7. The stale-pointer release described in 3.2 (freeing a live block at a reused offset) — derived from code only.
8. `TAllocatorID`, `InvalidAllocatorID`, `MaxNumAllocators`, `PoolConfig` — wave 4's pages; cite by name and file only.
9. `MemoryManager` contracts these classes lean on (`RegisterAllocator`, `DeregisterAllocator`, `FallbackAllocate`,
   `AllocatorProxy` dispatch, the thread-local scope) — owned by the module page.
10. `StackAllocatorTest` / `MonotonicAllocatorTest` are test-only and own no page; note that all their testlets reach the
    allocator through an `AllocatorScope` and a container, so the two-argument release path this wave documents most heavily
    is the one the tests exercise least.

Findings worth reporting to the integrator:

* `docs/Memory/index.html`'s allocator-catalogue row says `MonotonicAllocator` … "No — reset only", but no reset method
  exists on the class; the only release is the destructor. Worth a wording fix in the file the integrator owns.
* These two destructors release through the ambient scope while `PoolAllocator`'s releases through its recorded parent —
  an asymmetry the catalogue table does not show.

Tool traps hit or anticipated, so the next author does not lose pages:

* `htmlcheck.py` rejects a stray `</p>` with no opener; two fragments needed fixing before they could validate.
* On a method page, `<a href="#coverage">` is a finding — method pages have no `coverage` id. Link `index.html#coverage`.
* Dead links are tolerated **only** when every finding is a dead link into the page's own folder. A link to
  `../MonotonicAllocator/index.html` from a `StackAllocator` page is rejected until that folder exists, so author both
  class pages first, then the method pages, then re-render the class pages last so their sibling links resolve clean.
* `docs_pass.py signatures` owns the Signature block and pastes preprocessor noise verbatim; do not "tidy" it.
