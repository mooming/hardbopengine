# PLAN — Fixed-size lock-free ring queue

Branch base: `origin/master`, own git worktree (never the current working tree, which carries
another session's uncommitted work). Nothing here is executed until the shape (MPSC / MPMC) is
chosen.

> **§0 supersedes that sentence and closes §7.** §1-§9 below stay as the design record — the
> refutations in §4c and the ordering table in §5 are why §0's shapes are what they are, and a
> reader who lands here needs them before §0 makes sense. The worktree instruction is retired
> because the only uncommitted files in this tree are this task's own session (`for(;;)` →
> `while (true)` in the queue header, a `JOURNAL.md` reorder, one `docs/Log/index.html` line);
> the work lands on `lock-free-mpsc-queue` in place, touches none of those three, and appends to
> `JOURNAL.md` rather than rewriting it.
>
> **Provenance of §1-§9.** Writing §0 overwrote this file, and the file is untracked, so §1-§9
> existed nowhere but in the writing session's context. They are re-typed verbatim from that
> context, which makes them *asserted* rather than *proved* unchanged — the only real proof is a
> diff against the owner's own copy. Nothing in §0 depends on a word of them being exactly right,
> and this sentence is here so the next reader knows which half of the file is on which footing.

## 0. Resolved shape (2026-10-15), and the five arms it ships

Owner decisions that closed §7 and the benchmark question:

| Question | Decision |
|---|---|
| Shape | **both** MPMC and MPSC, not one |
| Busy-wait | spin **N** attempts (default 8), then `yield` between further attempts — a capped spin, not an unbounded one |
| Baselines | a `std::mutex` ring pair, so the lock-free claim is measured and not asserted |
| Where the benchmark runs | its own target `Applications/RingBufferBenchmark`, **not** `EngineTest` — a benchmark in the suite slows every regression run and cannot control run order |
| Better than Vyukov for MPSC? | yes, on the producer side only → a 5th arm (§0c), because "does removing the CAS beat softening it?" is the real question |

### 0a. `AtomicRingBufferMPMC` — Vyukov's bounded MPMC queue

Per-slot tag holds the **absolute index**, exactly as §1 argued: no lap counter, no ABA, one
`FatalAssert` for a 2^64 overflow that cannot be reached.

```
state of slot s at lap L:  free ⟺ tag[s] == L        published ⟺ tag[s] == L + 1
released by the consumer:  tag[s] := (L - Capacity) + Capacity = L
```

| Side | Claim | Full / empty decision |
|---|---|---|
| producer | `pushPos.CAS(pos → pos+1)` | `tag < pos` → `Full`, answered **before** the claim is committed |
| consumer | `popPos.CAS(pos → pos+1)` | `tag < pos+1` → `Empty`, answered **before** the claim is committed |

Two properties worth stating because they are the reason this shape survives §3 and §4c:

- **The consumer never waits for a gap.** Readiness is decided *before* the index is committed, so
  a successful claim already implies a published payload. §3's rule is satisfied by construction,
  and there is no post-claim spin anywhere in this class.
- **`Blocked` is not observable here, and is not offered.** The tag carries two states — free and
  published — so a producer that has won a claim but not yet stored the tag is *invisible*. The
  shipped block-chained queue can report `EPopResult::Blocked` because its `claim` counter is a
  separate word; a fixed ring with a two-state tag cannot, and would need a third store on the hot
  path to buy it. Recorded as a real difference between the two containers, not as a defect.

### 0b. `AtomicRingBufferMPSC` — the same ring with the consumer CAS deleted

One consumer owns `popPos` outright: it becomes a plain member, and `Pop` is one `acquire` load of
the tag plus one plain increment. This is the algorithm named **Vyukov bounded MPSC queue** (the
single-consumer specialization of the bounded MPMC queue; `libcds` ships it as
`VyukovMPSCCycleQueue`), and §7 already called it "the most beautiful case of this design".

`Push` is unchanged from 0a, including the N-spin-then-yield, and for the same reason §3 gives: the
fullness test must sit inside the claim, so the claim stays a CAS and cannot become `fetch_add`.

### 0c. The 5th arm — `fetch_add` claim, spin-until-free (app-local, not engine API)

```
push: pos = pushPos.fetch_add(1)                 ← no retry, ever
      spin N, then yield, until tag[slot] == pos ← fullness becomes producer *latency*
      payload[slot] = v
      tag[slot].store(pos + 1, release)
```

It cannot answer `Full`: the index is already taken, and §4c is exactly what happens to a bounded
ring that burns a claim it cannot fill. So it converts the full-queue answer into waiting, which is
a different API and therefore **not engine surface** — it lives in the benchmark app, and the answer
it produces goes to the design document.

| | 0a / 0b | 0c |
|---|---|---|
| Producer atomics | 1 CAS, retried | 1 `fetch_add`, never retried |
| Full answer | immediate `Full` | deferred — the push returns when it returns |
| FIFO | strict | strict (index order is claim order) |
| Deadlock | — | none: the frontier producer is already unblocked, so the wait graph has no cycle |

Deliberate scope cut: the mutex baselines answer `Empty`/`Full` immediately, like the lock-free
arms, so the comparison is apples-to-apples. A `condition_variable` baseline would sleep instead of
returning, and that is a *different* trade (lower CPU, higher wake latency) — recorded as an open
item, not silently ignored, because the journal already records a mutex beating a lock-free
structure 5× on this machine.

### 0d. Memory order: ship the minimal gate, then try to break it

§5's table is the shipped ordering: `acquire` on the tag load, `release` on both tag stores,
`relaxed` on the position RMW. The position word only allocates an index — every payload handoff
crosses the slot word, and the slot word is the only place an edge is needed. A stale position
cannot double-claim, because the claim *is* the atomic RMW: a CAS whose expected value has moved
fails atomically whatever its order.

The stronger alternative (`acq_rel` on the claim) is near-free on x86, where `lock cmpxchg` is
already a full barrier, and costs a `dmb ish` on this machine's ARM cores. So the choice is
evidence's to make, per §8: ship the minimal gate, run the negative controls and a
`-fsanitize=thread` stress, and escalate to `acq_rel` **only** if a proof refuses it — recording
which proof, as the shipped queue did for its `seq_cst`.

### 0e. Wording correction the owner should know about

Vyukov's own page classifies this queue as *not* lock-free in the formal sense — "blocking producers
and consumers". Our headers may say "wait-free consumer" about 0b (true: one acquire load, one plain
store, no retry) and must not say "lock-free" about the whole structure without that caveat. An API
page that over-promises a progress guarantee is worse than one that says nothing.

## 0f. Files, and the gate each must clear

| File | Content | Proof |
|---|---|---|
| `Engine/Container/AtomicRingBufferMPMC.h` | 0a, template, header-only | `layout.py`, `blank_lines.py`, `includes.py`, `comments.py` 0 |
| `Engine/Container/AtomicRingBufferMPSC.h` | 0b | same |
| `Engine/Container/RingBufferMPMC.h` | mutex baseline, 0d shape API | same |
| `Engine/Container/RingBufferMPSC.h` | mutex baseline | same |
| `…/<same>.cpp` × 4 | `__UNIT_TEST__` test bodies only (templates belong in the header) | `EngineTest` green in Debug/Dev/Release |
| `Engine/Container/CMakeLists.txt` | 8 entries | 3-configuration build |
| `Applications/RingBufferBenchmark/{Main.cpp,Benchmark.h,Benchmark.cpp,CMakeLists.txt}` | 5 arms, two regimes | runs, prints the noise floor |
| `Applications/CMakeLists.txt` | `add_subdirectory (RingBufferBenchmark)` | the target is inside the gate, not beside it |
| `docs/Container/<Class>/…` × 4 + module index | pages via `docs_page.py`, `/// API reference:` pointer per header | `docs_coverage.py check-file` 0 debt per new file, `htmlcheck` clean |
| `docs/design/FixedRingBufferQueues_Design.html` | ordering table, the `Blocked` unobservability, the 5-arm result | `htmlcheck` |
| `JOURNAL.md` | one entry, appended | — |

## 0g. Checklist

| # | Step | Done |
|---|---|---|
| 1 | Four headers, standards-clean at write time (not "later") | ☐ |
| 2 | CMake wiring, three configurations compile | ☐ |
| 3 | Correctness testlets: FIFO, capacity boundary, wrap-and-refill, MP MPMC sum invariant, MPSC sum invariant, spurious-full path | ☐ |
| 4 | Four negative controls (§8 #3) actually run: each must turn a testlet red | ☐ |
| 5 | `-fsanitize=thread` stress, kernel-enforced watchdog, 0 data races | ☐ |
| 6 | Benchmark app: 5 arms × 2 regimes, interleaved, counterbalanced, controls | ☐ |
| 7 | Reference pages + design document + module index | ☐ |
| 8 | `gate.sh spawn --all --test`, `GATE_EXIT=0` | ☐ |
| 9 | JOURNAL entry with the measured numbers, noise floor stated | ☐ |

Residual carried forward from §8: the two regimes are where the answer lives, and a result inside
the control spread is **not** a win. If 0c does not beat 0a at 16 producers, the conclusion is that
the CAS is not the cost — which is the same sentence the last two journal entries had to write about
padding and about `std::function`.

---
---

## 1. Why a fixed ring, and what it removes

| | Shipped `AtomicRingQueueMultiProviderSingleConsumer` (509 lines) | Fixed-size ring |
|---|---|---|
| Slot lifetime | blocks allocated and recycled | static array, slots never die |
| Reclamation | hazard declaration + deferred list (62 lines of the file) | **none** |
| ABA expressible? | yes (block pointer identity) | **no** — index `h` belongs to one claim forever |
| Payload location | slot inside a block | slot in the static array |
| New problem introduced | — | emptiness test on the consumer side |

The slot state word holds the **absolute index**, not a lap counter, so no lap-distinguishing
machinery is needed. A false match needs 2^64 items — detectable with one `FatalAssert`, not a
protocol.

## 1b. Variant A: ownership before copying — and why

Scope: this section is about variant A (index claim, strict FIFO). Variant B in §4b reaches the
opposite conclusion for a defensible reason — the slot, not the index, is the unit of ownership.

In variant A, `payload[i]` is read only after `popPos` CAS commits the index. Copying before the commit reads a
slot another consumer may already have committed and marked `CONSUMED`, which lets a producer
legitimately write that slot while the copy is still in flight — the same shape as the
`AtomicStackView::Pop` defect fixed on this branch. It also costs more: with copy-after, only the
winner copies; with copy-before, every loser discards a copy.

The safety of copy-after rests on one requirement: **the producer's fullness test must consult the
slot's `CONSUMED` mark, not a `pushPos - popPos` counter difference.** A counter difference becomes
true the instant the consumer's CAS advances `popPos`, i.e. while the payload is still being read.

## 2. The rule that makes it correct

> A slot's payload is read only through a happens-before edge that passes through the state word.

Publication is a separate atomic store from the reservation. There is no atomic composition of
"advance the index" and "write the payload" — none is possible, since `T` is arbitrary-sized and
only `{ptr, stamp}`-sized values fit a double-width CAS. Measured earlier on this branch: a tag
rejects a stale *commit* but cannot prevent a stale *read*, so widening the CAS is not an answer.

Violation of this rule is exactly the defect fixed in `AtomicStackView::Pop`: it read the link
field of a node using a value obtained *before* the publication edge.

## 3. Corrected protocol: the empty/full test must live inside the claim

Retraction recorded on purpose: the first draft of this plan proposed `fetch_add` on both index
words, "zero retries". That is **wrong for a bounded queue**:

* Producer over-claim is recoverable in principle (write `ABORTED` into the slot it owns and let
  consumers skip it), but the chosen shape does not need it: the CAS-based claim never over-takes.
* Consumer over-claim is **not** recoverable — marking an index skipped means any item later
  published at that index is lost.

So emptiness must be decided by the consumer *before* it commits the index, which makes the index
word a CAS. Symmetrically for fullness.

| Push side | Pop side | Correct? | Cost |
|---|---|---|---|
| `fetch_add` | CAS | yes | spurious "full" (conservative snapshot) |
| **CAS** | **CAS** | **yes — the classic shape, no spurious answers** | two retry words |
| `fetch_add` | `fetch_add` | **no** — loses items or needs "push may fail" semantics | cheapest, weakest |

### Retry cost, measured context

This retry is not the pathology measured on this branch. The stack's retry cost
(a) re-acquired the head line exclusively, (b) performed a **dependent load** of `node->next` on
a line another core might own, (c) re-ran the whole preamble. Here a retry re-reads one counter
word and one slot state word the claimer already holds — no pointer chase. Still a retry, and the
design record must say so rather than claim "wait-free".

## 4. Skeleton

```
push(v):                                          pop(v?):
  t = pushPos.load(relaxed)                         h = popPos.load(relaxed)
  for(;;){                                          for(;;){
    s = state[t % N].load(acquire)                     s = state[h % N].load(acquire)
    if (s == READY(t))                                 if (s == READY(h))
      if (pushPos.CAS(t, t+1, relaxed)) break;           if (popPos.CAS(h, h+1, relaxed)) break;
    else if (s == NOT_CONSUMED(t)) return FULL;         else if (s == NOT_READY(h))   return EMPTY;
    t = pushPos.load(relaxed)                          h = popPos.load(relaxed)
  }                                                }
  payload[h % N] = v          ← plain                v = payload[h % N]            ← plain
  state[h % N].store(READY, release)                 state[h % N].store(CONSUMED, release)
```

## 4b. Variant B — the slot as the unit of ownership (order-relaxed)

Follow the copy-first instinct to its end and the fault turns out to be the *unit of ownership*,
not the copy order. Claim the slot instead of the index:

```
slot state[i] : EMPTY | READY(h) | TAKEN(h)          h = absolute index

push: h = pushPos.fetch_add(1)                        ← unconditional, no retry
      if (state[i].CAS(EMPTY → READY(h))) { payload[i] = v; }   else return FULL
pop:  copy payload[i] …                               ← safe: the slot is still READY
      if (state[i].CAS(READY(h) → TAKEN(h))) { state[i].store(EMPTY); return value; }
```

| | Variant A (index claim, FIFO) | Variant B (slot claim, relaxed) |
|---|---|---|
| CAS on a global counter | 2 | **0** — push is `fetch_add`, pop scans |
| Full / empty decision | counter snapshot → spurious answers possible | the slot CAS decides it **exactly** |
| Consumer over-claim | must be blocked by a CAS | **cannot be expressed** |
| Retry touches which line | one permanently hot word | per slot → contention self-shards |
| Cache lines touched per op | 2 | **1** |
| Copy before ownership is safe | ❌ | ✅ |
| Ordering | **strict FIFO** | not FIFO — newest-first makes it LIFO and warmer |

Discarded copies are still UB in variant A, and UB is not redeemed by discarding: a torn copy of a
non-trivially-copyable `T` can follow a pointer the producer already replaced, and a queue that
reports a race on every contended pop buries every other module's ThreadSanitizer output.

**Decision method**: implement whichever the use needs, and if the use is a work queue or a
reclaim backlog, prefer B and prove the difference with the two-regime latency sweep rather than
argument.

## 4c. Measured refutation of the `bool pub` + `fetch_add` pop proposal

Implemented literally (`/tmp/proposed_scheme.cpp`, `/tmp/deterministic.cpp`), payload a plain `int`
so nothing can be blamed on tearing:

| Observation | Value | Diagnosis |
|---|---|---|
| Single-threaded: pop #4 on an empty queue | returns `0`, reports **success** | a `bool` cannot distinguish never-published from already-consumed |
| Next pop after `push(40)` | returns **10** again | duplicate delivery; 40 parked under an index the consumer frontier has passed |
| 2 producers + 2 consumers, 200,000 pushes | **706,741 pops "succeeded"** | slots nobody published were delivered |
| Values never pushed, detected | **0** | stale values from the previous lap look valid — the corruption is **silent** |
| Pops that gave up waiting (spin added) | **593 in 0.2 s** | burned indices = lost items |
| Failed push CAS | 33,354 | each one burns an index even though nothing was enqueued |

Two root causes, and only two:

1. **The `pub` CAS is a reservation, not a publication.** As proposed the flag flips *before*
   `payload[i] = v`, so a consumer that passes the flag reads the previous lap's value and the
   producer's write lands afterwards. Publication must follow the payload write, not precede it.
2. **Two states where three are needed** — never-published, published(h), consumed(h).

The deeper lesson: **the two halves must agree on the unit of ownership.** The proposal put a
slot-claim on the push side and an index-claim on the pop side. Under slot ownership (§4b) a burned
push index is harmless, because consumers do not count; under index ownership it is fatal. Variant B
is the proposal kept intact with exactly two corrections: publish after the payload write, and let
the state word carry the absolute index.

## 5. Memory orders

| Site | Operation | Order | Why |
|---|---|---|---|
| `pushPos`, `popPos` | CAS success | `relaxed` | ordering is carried by the state word, not the index |
| `pushPos`, `popPos` | CAS failure | `relaxed` | only a fresher index is learned |
| `state[i]` | store READY | **`release`** | must not become visible before the payload write |
| `state[i]` | load | **`acquire`** | the only gate through which payload is read |
| `state[i]` | store CONSUMED | **`release`** | the payload read must precede slot reuse |
| `payload[i] = v` | plain | — | the reservation gives exclusive ownership of that slot |

## 6. Layout requirement (from measurement, not taste)

`pushPos`, `popPos` and the two arrays each get their own cache line
(`alignas(AlignSize)`, the existing convention). Two producers that touch different slots must not
share a line; the saturated-collapse we measured was cache-line ping-pong on a single word, and
the mutex beat the lock-free stack 5× for exactly that reason.

## 7. Shape decision still open

| Shape | Consequence |
|---|---|
| **MPSC** | consumer owns its index outright → no consumer CAS, no consumer retry. The most beautiful case of this design |
| MPMC | consumer CAS, plus the ownership-before-copy rule proved on both sides. One more invariant than MPSC, and it is the one that can lose items if written wrong |
| SPSC | no CAS at all on either side; genuine wait-freedom |

## 8. Verification plan

| # | Step | Proof |
|---|---|---|
| 1 | Header + member layout | `layout.py`, `blank_lines.py`, `includes.py`, `check.sh --staged` |
| 2 | Testlets: producer/consumer sum invariant, capacity boundary, spurious-full path, drain-then-refill | native + ThreadSanitizer, guarded run with an **external watchdog** (`kill -9` after N s — a signal-based `alarm` wrapper was proven unable to kill a hung TSan run) |
| 3 | Four negative controls, each must make a testlet fail | (a) publish `READY` with a plain store — severs the write-then-read edge; (b) store `CONSUMED` relaxed — severs the read-then-rewrite edge; (c) replace the per-slot `CONSUMED` fullness test with a `pushPos - popPos < N` counter difference — a producer then overwrites a slot a consumer is still copying from; (d) copy the payload *before* the `popPos` CAS — reads a slot the consumer does not own |
| 4 | Latency vs `Mutex`, vs the shipped block ring, in **both** regimes | open-system sweep (gap 0 / measured ratio / idle), p50 and p99, CPU-µs per op |
| 5 | Three-configuration build (Debug, Dev, Release) | head and every commit that touches engine code |

## 9. Honest residuals

| Residual | Treatment |
|---|---|
| Consumer waits for an un-published slot in strict FIFO | inherent to FIFO, not a defect — record it. Wait-freedom requires relaxing order and helping |
| 64-bit index width | no reclamation, no pointer identity, so no ABA; overflow is detectable, one `FatalAssert` |
| Retry on the index words under heavy contention | bounded by the number of competing threads, no dependent load — but real; must appear in the design record, and step 4 measures it rather than asserting it |

## 10. Shipped (2026-10-16)

| Step of §8 | Result |
|---|---|
| 1 layout / blanks / includes | `layout.py` 11 files clean, `blank_lines.py` 0 findings, `includes.py` 0 findings, `check.sh --staged` 0 grep-rule failures, 0 advisories |
| 2 correctness | 11 shapes (4/8/16 producers, capacities 8/64/1024) exactly-once + per-producer order + checksum, native `-O2` and under `-fsanitize=thread` with 0 race reports; `EngineTest` 65 collections / 406 testlets / 0 failures in Debug, Dev, Release |
| 3 negative controls | 4 of 4 detected. (a) relaxed publish → TSan race; (b) relaxed slot release → TSan race; (c) `tag == pos` loosened to `tag <= pos` → **hang**, the consumer strands on an unreachable lap; (d) MPMC reads the payload before the dequeue CAS → TSan race |
| 4 both regimes | five-arm sweep, 11 interleaved rounds, four same-code control cells as the noise floor (saturated ≤6.4%, sparse ≤0.2%): MPSC lock-free wins everywhere, MPMC lock-free wins above 4 producers only, `MPSCFetchAdd` wins the middle of the curve and loses at 16 producers |
| 5 three configurations | Debug, Dev, Release build for `EngineTest`, `VulkanExample`, `CodingStandards` and `RingBufferBenchmark` |

Two places where the implementation and this plan part company, both recorded rather than hidden:

- **§9's overflow row is unimplementable as written.** No `FatalAssert` can catch a wrapped tag, because a wrapped
  index is indistinguishable from a full ring without extra state, and an assert that cannot fire is decoration. The
  width of the index plus the exact `tag == pos` comparison is the argument; the design record says so.
- **§9's FIFO residual was removed rather than accepted.** Readiness is decided *before* the dequeue claim commits, so
  the consumer never waits on an unpublished slot — the price is that an unpublished claim is invisible, so `Pop`
  cannot answer `Blocked` and `Empty` is allowed to be conservative in that one direction. That trade is what
  `AtomicRingQueueMultiProviderSingleConsumer` still does better.

Open, in priority order: the spin default (`8` measured wrong under oversubscription, wants a sweep on a machine with
more cores than threads), a condition-variable baseline, `WaitUntil`'s extra predicate call, and `check.sh` naming a
`WindowExample` target that does not exist in this checkout.
