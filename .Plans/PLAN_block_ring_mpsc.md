# PLAN — block-ring MPSC queue (PR #15 redesign)

**Status: awaiting approval. No container code has been written.**
Origin: PR #15 review thread — `AtomicQueueViewMultiProviderSingleConsumer` is an intrusive Treiber
stack plus a consumer-side reversal; the reviewer's concern is drain locality and refill cost at
thousands-to-millions of records, and the direction agreed is **a linked list of fixed-size,
power-of-two lock-free ring buffers, storing the payload in the slot**.

Decision log so far:

| question | answer | source |
| --- | --- | --- |
| shape | chain of fixed-size blocks, not one growable array | reviewer, 2026-10-12 |
| block slot count | power of two | reviewer, 2026-10-12 |
| slot contents | the payload itself, not a pointer | reviewer, 2026-10-12 |
| correction to that decision | "reserve every block at construction" contradicts "variable size" — see §2 | this plan |
| block allocation | **delegated to `MemoryManager`** — the container owns no arena and no block pool | reviewer, 2026-10-12 |
| consequence of delegating | allocation is now somebody else's problem; **the timing of a `free` is still the container's** — §2.3 | this plan |
| block lifecycle | **acquire and release take a mutex** — one event per `N` elements, amortised to noise | reviewer, 2026-10-12 |
| recycling | **enabled**, so memory shrinks with the backlog | reviewer, 2026-10-12 |
| lifetime guard | **per-producer hazard slots** — the mutex covers list surgery, not a producer parked with a copy of `tail` | reviewer, 2026-10-12 |

## 1. What is being replaced, and what it costs today

| | today (`980b150`) | after |
| --- | --- | --- |
| `Push` | 1 relaxed load + 1 CAS on one shared word, all producers on one cache line | 1 CAS claim + 1 payload store + 1 release tag store, spread over `N` slots, + 2 relaxed hazard stores on a private line |
| `Pop`, warm | 2 pointer reads, 1 write, no atomic | 1 tag load, 1 payload load, sequential |
| `Pop`, refill | `exchange` + O(k) reversal concentrated in **one** call | none — the block boundary is a pointer-sized handoff |
| iteration | one pointer chase per scattered node | `N` contiguous slots, then the next block |
| storage | the caller's | the queue's, `sizeof(T) + 8` bytes per element |
| backpressure | a caller cannot queue more nodes than it owns | none — a stalled consumer grows the queue |
| FIFO | reverse of CAS-completion order | claim order, which is the same thing, but now explicit |

## 2. Capacity: allocation is delegated, reclamation is not

### 2.1 What the delegate fixes

Blocks are allocated and freed through `MemoryManager`, under an `AllocatorScope` on the id captured at
construction (`parentID = MemoryManager::GetCurrentAllocatorID()`, the idiom every other allocator in
the module uses). The container therefore owns no arena, no block pool, no size bookkeeping, and the
ledger counts its growth like anything else.

Two consequences the reference has to state, because they come from that choice:

| consequence | detail |
| --- | --- |
| **the parent allocator must be thread-safe** | a producer allocates a block, so the parent has to be `ThreadSafeMultiPoolAllocator` or the system allocator — not `MultiPoolAllocator`, whose own documentation says it is not safe from two threads |
| **growth is not lock-free** | `ThreadSafeMultiPoolAllocator::Allocate` takes the instance mutex, so a block-growth event blocks. Steady state stays lock-free; the contract is "lock-free except once per `N` elements", measured by a growth counter |

### 2.2 What the delegate cannot fix

A thread-safe allocator answers *"may two threads allocate at once"*. It cannot answer *"is any thread
still inside this block"* — that is a question about the queue's producers, and the allocator has no
knowledge of them. Recycling a block back into a pool that hands the same address straight back is the
one failure mode a thread-safe allocator actively makes worse, because reuse is exactly what makes the
stale pointer dangerous.

### 2.3 The decision that is left, and the choice taken

| when may a block be freed? | lifetime risk | memory |
| --- | --- | --- |
| only at destruction | none — by the destruction contract producers are quiesced | grows to the high-water mark, then flat |
| **whenever the consumer finishes a block, under the guard of §2.5** *(chosen)* | closed by hazard slots, and proven by control 4 | grows and shrinks |

The interleaving that makes the second row unsafe:

| step | thread | event |
| --- | --- | --- |
| 1 | P | reads `tail = B`, sees `claim = 31 < N`, is preempted **before** the claim CAS |
| 2 | C | drains B, B is full and empty of consumers' work, frees B to `MemoryManager` |
| 3 | — | the thread-safe pool, whose job it is to reuse slots, hands B's address to the next allocation |
| 4 | P | resumes, CAS on `B->claim` succeeds — the address is still valid memory, so nothing faults |
| 5 | C | receives an element that belongs to a different block, or never receives one at all |

Step 4 is silent, which is why this project's methodology demands the negative control (§4 control 4)
before any recycling is believed. Protection, if it is ever wanted, is global implicit indices so a
stale producer names a slot range that is provably not the recycled block's — which is model C below and
is the reclamation machinery `docs/TaskSystemRedesign.md` §9 declined to buy speculatively.

### 2.5 The guard that makes recycling safe: per-producer hazard slots

The mutex decides who may modify the list. It says nothing about a producer parked with a copy of
`tail`, because that thread is on the lock-free fast path **by design** — the dangerous CAS happens
after the critical section has been left and released.

```text
   [mutex] read tail ─ attach next ─ move tail ─ mark dead [unlock]      ◄ the lock's reach

   read tail=B ──► ▓▓ parked ▓▓ ──► CAS(B.claim) ──► write payload      ◄ outside it, always
                                        ▲
                                 B may have been freed and re-allocated
```

The protocol, and it costs the hot path two relaxed stores:

```text
   push  : hazard[token] = B ; claim and publish in B ; hazard[token] = nullptr
   free  : [holding the mutex] scan hazard[0..P-1]
             any == B  →  defer this block, retry on the next retire
             none == B →  free(B) through MemoryManager
```

| property | detail |
| --- | --- |
| hot-path cost | two `relaxed` stores per push, into a slot **nobody else writes** — one cache line per producer, so it does not join the contention the design is removing |
| free-path cost | at most `P` word reads, once per retired block, already inside a mutex that is held anyway |
| producer identity | `thread_local` token, allocated from a registry under the same rare mutex — the idiom is already in the engine as `MemoryManager::scopedAllocatorID` (`MemoryManager.h:43`) |
| bound | `P` is bounded by `TaskSystem`'s `numHardwareThreads` (`TaskSystem.h:40`) for engine producers; a foreign thread takes a token on first use and keeps it |
| does it block? | never. a deferred free is retried on the next retire; the consumer keeps draining. The only blocking left in the class is the acquire/release mutex itself |
| what it does NOT protect | a producer that has *not yet* published its hazard slot for B — the window is closed by publishing the slot **before** the CAS that touches B, and by the free path scanning after marking the block dead, never before |

The invariant this buys, restated: **a block is freed only when `claim == N`, all `N` slots are drained,
the block is marked dead, and no hazard slot names it.** Control 4 in §4 is the test that must fail if
any clause is dropped.

### 2.4 The three lifecycle models, kept for the record

Fixed blocks plus a payload-owning queue plus a growing queue leaves three coherent answers:

| model | size behaviour | lock-free? | memory | complexity |
| --- | --- | --- | --- | --- |
| **A. segmented ring** — `maxBlocks x N` reserved at construction, blocks indexed `idx >> b` and wrapped | fixed capacity chosen once | always | all of it, up front | lowest — no chain, no lifetime question |
| **B. growing chain, no recycling** *(recommended)* | grows on demand, never shrinks within a run | steady state yes; **growth takes a mutex** | grows with the backlog, bounded only by the allocator | medium |
| C. growing chain with block recycling | grows and shrinks | needs stale-tail protection: global implicit indices, so a producer whose cached tail went stale is still naming a valid slot | bounded | highest — this is where moodycamel's complexity lives |

A capped chain *is* model A (a ring split into cache-friendly segments), so "variable size" and
"bounded memory" are the two ends of this choice and C is the expensive middle. Recommendation: **B**,
with the contract stated in the reference — *`Push` is lock-free except on the block-growth event,
which is one allocation per `N` elements and measurable* — and a counter on growth events so a
consumer that never drains shows up as a number, not a surprise.

## 3. Shape

```text
queue:  head : Block*                 consumer-private, plain pointer (affinity, as `oldest` today)
        tail : atomic<Block*>         producers claim into this block
        grow : mutex                  growth path only

Block:  alignas(64) claim : atomic<size_t>          // 0..N, producers CAS-bounded on it
        alignas(64) tag[N] : atomic<size_t>         // generation per slot
        alignas(64) slots[N] : storage              // stride = align_up(sizeof(T), line/slotsPerLine)
        next : atomic<Block*>                       // written once, CAS-installed
```

Index arithmetic uses one global monotone ordinal per block (`ordinal = idx >> b`) and a slot index
(`idx & (N-1)`), so no per-block cursor is ever divided by anything.

## 4. Invariants, each with the negative control that catches its violation

| # | invariant | control that must fail a testlet if broken |
| --- | --- | --- |
| 1 | a slot's payload is written before its tag is published | publish the tag first, or write the payload after: the consumer reads a torn/old element — delivery count and a payload checksum both fail |
| 2 | a slot is claimed by exactly one producer (`claim` advances by one per claim) | drop the bound check on `claim == N`: two producers write one slot, the delivered multiset differs from the pushed multiset |
| 3 | the consumer reads slot *i* only after `tag == generation(i)` | read unconditionally: a blocked slot is delivered as garbage |
| 4 | a block is freed only when `claim == N`, all slots are drained, it is marked dead, **and no hazard slot names it** | park a producer between reading `tail` and its claim CAS, drain and retire that block, force a re-allocation that reuses the address, release the producer: without the hazard scan an element is lost or corrupted and the multiset check fails; with it, every element arrives |
| 5 | `next` is installed once and never rewritten | install twice: the reversal-free drain skips a block |

Existing shipped controls stay: single-field `IsEmpty`, and the late-write race that delivered 1 of
4 000 nodes. The queue's testlets are re-proved against each new invariant, per the project rule that
a test must be shown to fail on the defect it names.

## 5. Memory ordering

| action | ordering | why |
| --- | --- | --- |
| claim `CAS(claim, c, c+1)` | `acq_rel` | publishes the claim to the consumer and acquires the block's state |
| tag publish `store(gen)` | `release` | the payload store before it must be visible |
| tag read `load()` | `acquire` | pairs with the publish; the payload read after it is ordered |
| `next` install `CAS(nullptr, b)` | `acq_rel` | the block is fully constructed before it is visible |
| `tail` read | `acquire` | must not see a stale tail after a handoff |
| `claim` read for emptiness | `relaxed` is **not** enough — use `acquire` | emptiness is a cross-thread claim |

## 6. API sketch (names provisional, standards-shaped)

```cpp
enum class EPushResult { Pushed, Full };                 // model A/B cap
enum class EPopResult  { Element, Empty, Blocked };      // Blocked = claimed, unpublished
template <typename T, size_t InBlockSlots = 1024>
class AtomicRingQueueMultiProviderSingleConsumer final;  // alias AtomicRingQueueMPSC
[[nodiscard]] EPushResult Push(const T& inItem);
[[nodiscard]] EPopResult Pop(T& outItem) noexcept;       // never spins
[[nodiscard]] bool IsEmpty() const noexcept;             // reads claim AND head state
```

`Blocked` is the honest form of the one thing this shape costs: a producer that claimed slot *i* and
has not published it can hold the FIFO boundary. Returning it, rather than spinning on the tag, keeps
the consumer's non-blocking property and turns the event into a counter.

## 7. Files

| file | change |
| --- | --- |
| `Engine/Container/AtomicQueueViewMultiProviderSingleConsumer.{h,cpp}` | rewritten, and renamed if the rename is approved |
| `Engine/Container/CMakeLists.txt` | name change only |
| `Engine/Test/UnitTestCollection.cpp` | name change only |
| `docs/Container/<Class>/…` | class page + one page per method name; `reversenewest.html` dies, `push`/`pop`/`isempty` rewritten |
| `docs/design/MultiProviderSingleConsumerQueue_Design.html` | rewritten around the new invariants; its "Open items" get closed by §8 |
| `docs/Container/index.html`, `docs/index.html` | link updates |
| `docs/PerformanceAnalysis.md` | the four numbers below, where such figures go |

Guard note: this branch is based on `master`, where the test macro is `__UNIT_TEST__`. The test region
keeps that guard on this branch — renaming it here would compile the testlets out silently.

## 8. Measurement, which the design document lists as unmeasured today

| axis | question it settles |
| --- | --- |
| push rate vs 1, 2, 4, 8 producers | whether the single shared word was the ceiling |
| `Pop` latency p50/p99/max at k = 10³, 10⁴, 10⁶ | the old refill spike vs the new block handoff |
| drain throughput at 8-byte and 64-byte payloads | whether ring locality is the win, and where it stops paying |
| block size 256 vs 1024 vs 4096 | the power-of-two `N`, by number rather than by argument |
| `Blocked` count under 4 producers | how often a producer at the FIFO boundary would have stalled a spinning consumer |

## 9. Verification protocol

`check.sh` at every commit touching C++ · three-configuration build with `-test` · EngineTest in Debug,
Dev, Release · a ThreadSanitizer pass over the contention testlets (`build-tsan/` exists) · the five
negative controls of §4 run one at a time, each expected to fail a testlet · `docs_coverage.py
check-file` and `docs_methods.py` for the renamed class's page set.

## 10. Checklist

- [x] block lifecycle chosen: mutex-guarded acquire and release, recycling enabled
- [x] lifetime guard chosen: per-producer hazard slots (§2.5)
- [ ] producer token registry sized, and the hazard array laid out one line per slot
- [ ] retire path: mark dead → scan → free or defer-and-retry
- [ ] fuse cap chosen (a `Full` result past it, as a safety valve rather than a capacity plan)
- [ ] rename approved (`AtomicRingQueue…`, files + docs folder follow)
- [ ] `Blocked` as a `Pop` outcome approved, or spin chosen instead
- [ ] header: block layout, tag protocol, ordering as §5
- [ ] source: push, pop, growth path, destruction contract
- [ ] testlets: multiset equality, boundary contention, blocked-slot, growth-under-load
- [ ] negative controls 1-5 written and each proven to fail
- [ ] measurement testlets, numbers recorded in `docs/PerformanceAnalysis.md`
- [ ] reference pages: class + every method name; `reversenewest` removed
- [ ] design document rewritten; open items closed
- [ ] gates: `check.sh`, three configs, EngineTest, TSAN

## 11. What this plan does not do

It does not rebase #15 onto `master` or onto `build/single-test-macro` (that is the macro thread's
decision), it does not touch the task system's adoption of the queue (the design document keeps that
out of scope), and it does not add a multi-consumer variant — that needs a reclamation scheme the
guardrail in `docs/TaskSystemRedesign.md` §9 has not yet authorised buying.
