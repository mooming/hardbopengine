# PLAN: lock-free queue in Engine/Container

Owner approval: 2026-07-14 session. The owner chose the name
`AtomicQueueViewMultiProviderSingleConsumer` with the alias `using AtomicQueueViewMPSC = …`, which also
confirmed the multi-producer / single-consumer topology. The seed-node question is overtaken: the chosen
algorithm needs no sentinel.

## 1. Goal

Add one lock-free first-in-first-out container to `Engine/Container` that serves many producer threads
enqueueing while one consumer thread dequeues, on caller-owned nodes, with no allocation inside the
container and no memory-reclamation subsystem.

## 2. Constraint carried in from the project's own documents

| Source | Constraint | Effect on this task |
|---|---|---|
| `docs/TaskSystemRedesign.md` §9 guardrail 3 | Never multi-producer / multi-consumer lock-free unless profiling justifies it behind a seam; single-producer or single-consumer affinity must be provable by construction and stated where the queue is declared | The class is multi-producer, single-consumer, and says so in its name |
| `Engine/Memory/MultiPoolAllocator.h` | Not thread-safe; `ThreadSafeMultiPoolAllocator` is mutex-based | An internally-allocating linked queue would reintroduce a lock, so nodes stay caller-owned |
| `AGENTS.md` | No comments in `.h`/`.cpp`; every entry owns a reference page | Contract prose goes to `docs/Container/…`, gated by `docs_coverage.py check-file` |

## 3. Algorithm chosen, and the derivation that changed the plan

Chosen: the two-stack queue (AMQ) of Michael and Scott, restricted to a single consumer so that the
consumer-side half needs no lock and no atomic. Producers push onto one atomic Treiber stack; the single
consumer steals the whole stack with one `exchange` and reverses it locally, which restores first-in
first-out order.

The plan originally named Vyukov's intrusive multi-producer / single-consumer queue, whose publish is
`prev = head.exchange(node); prev->next = node;`. Deriving that sequence against the ownership rule found
two properties the container would have had to document as defects:

| Finding | Why it follows from the sequence |
|---|---|
| A sentinel node the queue must own is unavoidable | The consumer's frontier node is read through `tail->next` before the queue knows whether an item exists; the frontier must therefore be memory the queue owns, and a view may allocate nothing |
| The node at the head is not dequeueable until one more node is enqueued | The consumer may hand back only a node whose single `next` write has already landed, which is provable only by observing a non-null `next`, so the newest item waits for a successor |

The chosen shape has neither. Both properties are proved empirically in §5, not asserted.

## 4. Properties to prove, each with the test that proves it

| # | Property | Proof |
|---|---|---|
| 1 | First-in first-out per producer, and one total order across producers | TC2: four producers, tagged sequence numbers, single consumer asserts each producer's subsequence rises |
| 2 | No item lost or duplicated under contention | TC2 asserts the exact multiset of 4000 values arrives once each |
| 3 | A node returned by `Pop` is never touched by the queue again, so the owner may free or recycle it | TC3 recycles every popped node immediately into a shared slot array and re-pushes it; ThreadSanitizer plus the value check would report the corruption |
| 4 | `IsEmpty` is false while a node sits in either the producer stack or the consumer's private list | TC4 pushes, steals into the private list with one `Pop`, and checks `IsEmpty` across both halves |
| 5 | `Pop` returning null never strands an item that was already enqueued | TC2's drain loop terminates on a counted total, not on the first null |
| 6 | Destruction with items still queued releases nothing and leaves the caller's nodes intact | TC5 destroys a non-empty queue and then reads the surviving nodes back |
| 7 | No data race | ThreadSanitizer harness over TC2/TC3 shapes |
| 8 | Tree still builds and no test regresses | Debug, Dev, Release with `-test`, then `EngineTest` |

## 5. Deliverables

| Item | Path |
|---|---|
| Header and source | `Engine/Container/AtomicQueueViewMultiProviderSingleConsumer.{h,cpp}` |
| Build registration | `Engine/Container/CMakeLists.txt` |
| Test registration | `Engine/Test/UnitTestCollection.cpp` |
| Reference pages | `docs/Container/AtomicQueueViewMultiProviderSingleConsumer/{index,constructors,push,pop,isempty}.html` plus the `AtomicQueueViewMPSC` alias row |
| Module rows | `docs/Container/index.html`, `docs/index.html`, `README.md` |
| Gates | `check.sh --fix`, `layout.py`, `blank_lines.py`, `includes.py`, `comments.py`, `docs_coverage.py check-file`, `docs_methods.py`, `htmlcheck.py`, three-configuration build |

## 6. Naming note recorded for the owner

`provider` already names the task-supplying actor in this engine (`TaskProvider`, `docs/TaskSystemRedesign.md`
§5). The concurrency term for a thread that enqueues is `producer`, and `MPSC` abbreviates it. The owner
chose `MultiProvider`; a rename to `MultiProducer` is mechanical (one pair, one docs folder, one module row)
and was raised before the commit rather than after.

## 7. Out of scope

Multi-consumer dequeue, cache-line padding without a measurement that asks for it, and any change to the
task system's enqueue path.
