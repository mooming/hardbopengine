# PLAN — `RingBuffer<T, U>`: the chain kept, the block replaced by a fixed ring

Confirmed with the owner (2026-10-17):

1. The class is renamed to **`RingBuffer<T, U>`**, `T` the value type, `U` a fixed-size ring buffer class.
2. **The linked-list chain stays.** Only the *block* becomes `U`: the hand-written per-slot tag protocol inside
   `LinkNode` is deleted and `U` does that job.
3. The four fixed rings are renamed into the **`FixedRing` family** so `RingBuffer` is the only chain name in `hbe`.
4. "Use the backends selectively" is expressed by `U`: MPSC when one consumer drains it, MPMC when many do, mutex
   variants where a lock is the better program. The chain does not guess the shape at runtime — it cannot, and a guess
   that is wrong is a data race.

## 1. Why this is worth doing

| Today | After |
|---|---|
| The chain hand-rolls a slot protocol (`claim`, `tags[]`, `SlotAt`, publish/reset loops) that `AtomicFixedRingMPSC` already implements and that 4 negative controls proved necessary | the chain keeps block-level bookkeeping only: `claim`, `next`, allocation |
| The four fixed rings are load-bearing only for the benchmark | they are the engine's slot engine, used by the engine's growable queue |
| Two vocabularies for one idea (`tags`, `SlotMask`, `storage` vs `readyIndex`, `IndexMask`, `payload`) | one vocabulary, in the backends |

## 2. Shape of `U` — settled by compilation, not by preference

| Candidate | Result |
|---|---|
| `template <typename T, typename U>` — `U` is a concrete ring type, e.g. `AtomicFixedRingMPSC<Msg, 1024>` | **works**; admits all four backends; needs `U::ValueType`, `U::SlotCount`, `U::ClaimedSlotCount()` |
| `template <typename T, template <typename, size_t, size_t> class U>` | rejected: `RingBufferMPSC<T, Capacity>` has two parameters and will not bind — *"template template argument has different template parameters"*. Accepting it would mean bolting an unused third parameter onto the mutex rings |

```cpp
template <typename T, typename U, class TAllocator = DefaultAllocator<T>>
	requires CFixedRing<T, U>
class RingBuffer final
{
};
```

The constraint is a named concept, not an assertion in a member body — §2.1.

### 2.1 The concept that says what `U` is

The constraint is named, not implied by a `static_assert` buried in a member function. House precedent: `CLockable`
in `Engine/Core/ScopedLock.h`, `CNext` in `Engine/Container/AtomicStackView.h`, `CToZeroTerminateStr` in
`Engine/String/StaticString.h` — each declared in the header immediately above the class that consumes it, and each
documented on that class's page under *Template parameters*, not in its own folder.

```cpp
template <typename T, typename U>
concept CFixedRing =
	std::same_as<T, typename U::ValueType> &&
	std::default_initializable<U> &&
	!std::copy_constructible<U> &&
	requires(U& ring, const T& item, T&& movedItem, T& outItem)
	{
		{ U::SlotCount } -> std::same_as<const std::size_t&>;
		{ ring.Push(item) } -> std::same_as<typename U::EPushResult>;
		{ ring.Push(std::move(movedItem)) } -> std::same_as<typename U::EPushResult>;
		{ ring.Pop(outItem) } -> std::same_as<typename U::EPopResult>;
		{ ring.IsEmpty() } -> std::same_as<bool>;
		{ ring.ClaimedSlotCount() } -> std::same_as<std::size_t>;
	} &&
	requires {
		U::EPushResult::Pushed;
		U::EPushResult::Full;
		U::EPopResult::Element;
		U::EPopResult::Empty;
	} &&
	(U::SlotCount > 1) && ((U::SlotCount & (U::SlotCount - 1)) == 0);

template <typename T, typename U, class TAllocator = DefaultAllocator<T>>
	requires CFixedRing<T, U>
class RingBuffer final
{
};
```

What each clause buys, in one line each:

| Clause | Why it belongs in the constraint rather than in prose |
|---|---|
| `same_as<T, U::ValueType>` | the queue and its blocks disagreeing about the element type is a type pun, not a bug report |
| `default_initializable<U>` | the chain placement-news a block at an address chosen by the allocator |
| `!copy_constructible<U>` | a ring is pinned: copying one in use would strand every producer holding a reference to the original |
| the two `Push` overloads | the chain forwards both an lvalue and an rvalue; a backend with only one would silently decay moves to copies |
| `EPushResult::{Pushed,Full}`, `EPopResult::{Element,Empty}` | these four answers are what the chain's `Push`/`Pop` are built from; a backend that renames one is a compile error, not a wrong `Full` at 3 a.m. |
| `ClaimedSlotCount()` | it is the one-lap cap (§3.1) — the chain cannot enforce single use without it |
| `SlotCount` power of two, `> 1` | the same condition each backend already `static_assert`s on its own `Capacity`, now stated where a caller learns it |

Proven by compilation (`/tmp/con/c3.cpp`, C++23, `-Wall -Wextra`): the concept accepts a compliant ring and rejects a
wrong value type, `SlotCount = 12`, `SlotCount = 1`, a copy-constructible ring, and a ring without `ClaimedSlotCount()`.

**One trap, recorded so nobody "simplifies" into it.** The shorthand `template <typename T, CFixedRing<T> U>` does not
mean `CFixedRing<T, U>`: a constrained-parameter concept receives the *constrained parameter first*, so clang expands it
to `CFixedRing<U, T>` and the constraint silently evaluates the wrong pair. Two spellings are correct — the trailing
`requires CFixedRing<T, U>` clause above, which is what this plan uses, or a concept declared `<typename U, typename T>`
to suit the shorthand. With no arguments written at all the compiler refuses outright (*"requires more than 1 template
argument"*), which is the only reassuring case.

All four backends are checked against the concept by `static_assert(CFixedRing<T, Backend<T, N>>)` inside the
`__UNIT_TEST__` region of `RingBuffer.h` — it proves compliance without the backends having to include the queue header.

Preset alias, replacing `AtomicRingQueueMPSC`:

```cpp
template <typename T, size_t BlockSlots = 1024>
using RingBufferMPSC = RingBuffer<T, AtomicFixedRingMPSC<T, BlockSlots>>;
```

`BlockSlots` moves out of the queue's own template list and into the backend, which is where capacity already lives.

## 3. What a block becomes

```cpp
struct Block final // hb-standards:ignore
{
	alignas(LineSpacing) std::atomic<size_t> claim;   // single-use reservation: never > U::SlotCount
	AlignedStorage<U> ring;                           // placement-new'd, one lap, never wrapped
	std::atomic<Block*> next;
};
```

Deleted from the block: `tags[BlockSlots]`, `storage[]`, `SlotAt`, the tag init loop, `InvalidTag`.

### 3.1 The one-lap rule, and why the chain must enforce it atomically

A ring recycles: after `SlotCount` pushes **and** that many pops, slot `i` is free again at absolute index
`SlotCount + i`, so a `(SlotCount+1)`-th push succeeds. A recycled block can never be retired — retirement needs "this
block is finished forever", which recycling makes undecidable. So the chain caps each block at exactly one lap, and the
cap has to be **atomic**: two producers that both read `ClaimedSlotCount() == SlotCount - 1` and then both push recycle
a slot and lose an element. Therefore `claim` stays as the reservation, and the ordering is:

1. CAS `claim` `n → n+1` while `n < SlotCount`; `n >= SlotCount` ⇒ link-or-take the next block (today's code, unchanged).
2. `ring->Push(item)` — cannot fail while the reservation holds, because fewer than `SlotCount` pushes have been made.
   A `Full` answer here is a contract violation: count it, and report `Full` to the caller.
3. `ClaimedSlotCount()` is the ring's own absolute write index, so it is the same number `claim` reached — the two
   counters agree by construction, which is what makes them safe to keep both.

Cost, stated before measuring: **one extra atomic RMW per push** (the chain's `claim` CAS plus the ring's own
`writeIndex` CAS). Considered and rejected for now: a `PushReserved(index, item)` entry point that lets the chain's CAS
*be* the ring's claim — it removes the RMW but hands the backend's central invariant ("only the ring reserves slots") to
the caller, which is exactly the class of bug the tag protocol exists to prevent. If §7 measures the extra RMW as
material, that option comes back as its own decision with the number attached.

### 3.2 `Pop` keeps its honest three-case answer with no new backend API

| Condition | Answer | How |
|---|---|---|
| ring yields an element | `Element` | `ring->Pop(out)` |
| ring says Empty, block not drained (`!ring->IsEmpty()`) | `Blocked` | a reservation is mid-publish — transient, never abandoned (engine is exception-free) |
| ring says Empty, block drained, successor exists | advance | `head = next`, `RetireBlock`, retry |
| ring says Empty, block drained, no successor | `Empty` | today's answer |

The chain's `headCursor` disappears: the ring's own read index is the cursor, and `IsEmpty()` is the drained test.

### 3.3 Behavior change that must be documented, not hidden

The chain's claim loop today retries with a bare `continue`. Pushing through `U` inherits the ring's
`MaxSpinAttempts` — spin 8, then `yield()` — so block-level claim contention now yields where it previously spun free.
That is the spin-cap lesson arriving in the chain; it is measured in §7 and recorded in the design document either way.

## 4. Backend API delta (all four, identical)

| Addition | Implementation | Note |
|---|---|---|
| `using ValueType = T;` | typedef | lets `RingBuffer` check `T` against `U` |
| `static constexpr size_t SlotCount = Capacity;` | constant | the chain's one-lap cap; the class property, not a new concept |
| `[[nodiscard]] size_t ClaimedSlotCount() const noexcept` | lock-free: `writeIndex.load(relaxed)`; mutex: the write counter under `guard` | one method page per backend; the chain's only new requirement on `U` |

## 5. Names

| Now | After |
|---|---|
| `AtomicRingQueueMultiProviderSingleConsumer.h/.cpp` | `RingBuffer.h/.cpp` |
| class `AtomicRingQueueMultiProviderSingleConsumer<T, BlockSlots, TAllocator>` | `RingBuffer<T, U, TAllocator>` |
| alias `AtomicRingQueueMPSC<T, BlockSlots>` | `RingBufferMPSC<T, BlockSlots>` |
| test collection `…Test` | `RingBufferTest` |
| `AtomicRingBufferMPSC` | `AtomicFixedRingMPSC` |
| `AtomicRingBufferMPMC` | `AtomicFixedRingMPMC` |
| `RingBufferMPSC` | `FixedRingMPSC` |
| `RingBufferMPMC` | `FixedRingMPMC` |
| perf arms `AtomicRingBufferMPSC`, `RingBufferMPSC (mutex)`, … | same words with the new type names |

The chain has no other users: `grep` finds only its own header/source, its testlet, `UnitTestCollection.cpp`'s
registration, CMake, `README.md` and documents.

## 6. Files and documents touched

| Area | Work |
|---|---|
| `Engine/Container/*.h/.cpp` | 4 backend renames (`git mv`, history preserved) + §4 additions; chain `git mv` to `RingBuffer.*` and §3 rewrite |
| `Engine/Container/CMakeLists.txt` | regenerated by `generate_cmake_files.sh` (directory-driven), never hand-edited |
| `Engine/Test/UnitTestCollection.cpp` | include + registration rename |
| `Engine/Test/RingBufferPerf.{h,cpp}` | backend names; one new arm pricing the chain against a bare ring |
| `docs/Container/{AtomicRingBufferMPSC,AtomicRingBufferMPMC,RingBufferMPSC,RingBufferMPMC}/` | folders renamed, titles/paths/links rewritten, `ClaimedSlotCount` page + two class properties each |
| `docs/Container/AtomicRingQueueMultiProviderSingleConsumer/` (18 pages) | becomes `docs/Container/RingBuffer/`, re-addressed; `Signature` sections re-verified verbatim against the new header |
| `docs/Container/index.html`, `docs/index.html`, `README.md` | rows, counts, the `AtomicRingQueueMPSC` line |
| `docs/design/FixedRingBufferQueues_Design.html` + the queue's design doc | the one-lap rule, the +1 RMW and its measurement, the three-case `Pop` under a ring backend, the yield-under-claim change |

## 7. Checklist (each step ends with a commit)

- [x] 1. Add `ValueType`, `SlotCount`, `ClaimedSlotCount()` to the four backends + pages; build, `EngineTest`. (`80333ee`)
- [x] 2. Rename the four backends across code, perf cells and `docs/`; regenerate CMake; gates; commit. (`a75bd07`)
- [x] 3. Rewrite the chain as `RingBuffer<T, U, TAllocator>` per §3; keep every existing testlet passing unchanged. (`d30a89c`)
- [x] 4. Docs for `RingBuffer` (class page + one per method name), `docs/Container/index.html`, `README.md`. (with `d30a89c`)
- [x] 5. Measure: chain vs bare `AtomicFixedRingMPSC` (the price of growth), and claim-contention before/after the
      inherited yield; write the numbers into the design document. (the fair number came in only with the
      producer-token fix; see JOURNAL 2026-10-19)
- [x] 6. Full verification matrix §8; JOURNAL entry; commit. (§8 as written passed only in Release; see §10's last two
      rows for why the matrix itself was not strong enough, and the 2026-10-19 15:47 entry for what closed it)

## 8. Verification matrix

| Gate | Proof |
|---|---|
| Concept | `static_assert(CFixedRing<T, …>)` for all four backends, in the `__UNIT_TEST__` region of `RingBuffer.h` |
| Correctness | existing 11 stress shapes (4/8/16 producers, capacities 8/64/1024): exactly-once, per-producer order, checksum — unchanged, and they must pass without edit |
| One-lap invariant | **new** testlet: push `SlotCount * 3` items through one queue, assert `GetGrowCount() >= 3`, no duplicate and no lost item — a recycled slot shows up as a duplicate |
| Three-case `Pop` | existing `Blocked` testlets plus one that forces mid-publish contention |
| Races | `-fsanitize=thread` over the stress shapes, 0 reports |
| Suite | `EngineTest` in Debug, Dev, Release: 66 collections / 409 testlets / 0 failures |
| Benchmark | `RingBufferBenchmark` Release: 22 cells, 0 violations, all graded claims `GREEN`, max cell < 3 s |
| Standards | `layout.py`, `blank_lines.py`, `includes.py`, `comments.py`, `clang-format`, `check.sh --staged --no-build` at 0 grep failures |
| Docs | `docs_coverage.py`, `docs_pass.py signatures`, `htmlcheck.py` 0 problems |
| Generator | `generate_cmake_files.sh` run twice, second pass changes nothing |

## 9. Risks

| Risk | Mitigation |
|---|---|
| A recycled slot silently loses an element if the one-lap cap is wrong | the cap is one CAS in one place; new duplicate-detecting testlet at `SlotCount * 3` items; TSan run |
| +1 atomic RMW per push | measured in §7 step 5 before the change is called done; `PushReserved` documented as the fallback decision |
| Claim contention now yields (`MaxSpinAttempts`) instead of spinning free | measured at 4/8/16 producers; if it hurts, the backend is instantiated with a spin count chosen for the chain and recorded |
| Block grows by the ring's own index words and line padding | printed with `offsetof`/`sizeof` in the design document, same as the layout audit that preceded this queue's last layout change |
| Renaming four classes touches ~40 reference pages | `docs_coverage.py` + `docs_pass.py signatures` + `htmlcheck.py` prove every page still addresses a real declaration |
| Scope: the engine's growable queue changes hands under its users | it has none today (`grep` proves it); the change is therefore contained to tests, docs and the benchmark |

## 10. What the plan got wrong, found by implementing it

Recorded because a plan that is quietly corrected is not a plan, and the next reader cannot tell
which parts of it were ever true.

| Plan said | What the code needed | Evidence |
|---|---|---|
| §4: add `static constexpr size_t SlotCount = Capacity;` to each backend | do not add it — every backend already answers `GetCapacity()`, so `SlotCount` would have been a second name for one fact, and the concept binds to `GetCapacity()` | `Engine/Container/RingBuffer.h` concept clause `{ U::GetCapacity() } -> std::same_as<std::size_t>` |
| §3.2: `Pop` decides `Blocked` from `!ring.IsEmpty()` alone | two tests are needed, not one. `!IsEmpty()` sees a ring slot claimed but unpublished; it cannot see a chain `claim` reservation made *before* `ring.Push` is entered, and retiring a block in that window frees a block a producer is still about to write into — an element `Push` reported as accepted is lost | `RingBuffer::Pop`: `reservedAhead = claim.load(acquire) > ring.ClaimedSlotCount()` then `reservedAhead \|\| !ring.IsEmpty()` |
| §3.1: enforce one lap per block (how was left open) | the cap must be taken by the block's atomic `claim` compare-exchange *before* `ring.Push`, never read-then-push. Two producers that both read `capacity - 1` wrap the ring, and a wrap is only refused once a slot has been consumed — which is exactly when a successor-bearing block can be retired, so the wrap lands in a retired block | negative control: forcing `if (claimIndex >= BlockCapacity)` false makes the queue's own `Assert` fire — `[Assert] RingBuffer: the block answered Full although its claim had room, so one lap is not enforced.` — and the Dev `EngineTest` dies with SIGTRAP |
| §2.1: write the constraint as a trailing `requires CFixedRing<T, U>` clause | keep the clause, but it must sit on the `template <...>` line. `clang-format` forces `requires` onto its own line, and `comments.py` then refuses the `/// API reference` pointer ("the next declaration is `requires CFixedRing<T, U>`"). Resolution: formatter gets the clause position, the pointer moves directly above `class RingBuffer final`, which `comments.py` accepts and which is literally where the declaration it names begins | both gates report 0 on `Engine/Container/RingBuffer.h` |

Also found: `FreeBlock` had to start running the block's destructor. It used to free raw memory after
the caller had walked the tags and destroyed each live element itself; there are no tags to walk now,
so the ring's own destructor is the only thing that knows which slots hold constructed objects.

Plan §5's `IsSlotFree` question is closed without adding it: nothing in the chain needs that question,
and a public accessor nobody calls is API the concept would then have to demand of every backend.

Two more ways this plan was wrong, found after it was "done":

| Plan said | What the code needed | Evidence |
|---|---|---|
| §8 "Suite": one green `EngineTest` run per configuration proves the change | a flaky suite needs N runs before any green claim, and the N was five — the same tree that "passed" three configurations crashed 5/5 an hour later | JOURNAL 2026-10-19 15:47; Debug 5×413 before the claim could be restated |
| §8 "Races": TSan over the stress shapes is the memory-safety gate | the gate cannot see what the allocator hides: the crashes were bank-vector tearing in shared engine infrastructure, which a green suite under a lucky size-class layout read as safety | the trio of wandering pool-allocator asserts; design document `#allocator` |
