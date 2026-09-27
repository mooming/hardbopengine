# PLAN — dynamic priority: derived keys, bucket fronts, and what to build first

Written 13:20 at `073e746`, from the owner's question "can the priority lane support dynamic priority?" followed by
"items could be thousands or millions, and it shouldn't consume computation power too much; if it's heavy we can reprioritise every
10th frame". **Status: specified, blocked on standing decision 6.** Everything about the current code below was read from the tree.

## 1. What the container actually is

`Engine/Container/BoundedPriorityQueue.h`:

| Fact | Where | Why it matters here |
|---|---|---|
| `std::array<std::optional<Deque>, MaxPriority>`, `MaxPriority = 256` | `:28,:32` | Bucketed, **not a heap**: no comparisons, no tree, no per-element index |
| Buckets created on first push to a level, **released when the level empties** | `:18-21` | Memory is paid only for levels in use |
| Highest number = most urgent; **oldest-first within a level** | `:15-17` | A documented contract; the main thing candidate algorithms silently destroy |
| `Push` files an item by `item.priority`, updates `highestBucket` | `:104-121` | **The byte is read exactly once, at insertion** |
| `Pop` serves from `buckets[highestBucket]` | `:125-139` | The level it served from is the item's real priority |
| `highestBucket` re-tracked **downward, lazily, at pop** | `:74-76` | A removal that empties the top level self-corrects |
| `Remove(predicate)` scans levels then items | `:156-169` | O(256 + N): fine when rare, fatal per frame |

## 2. The lemma that makes a per-frame refresh cheap

Within a level, items sit in arrival order, and aging is monotone in arrival time ⇒ **effective keys inside a bucket are
non-decreasing from front to back** ⇒ the globally best item is always **one of the at most 256 bucket fronts**. You never need to
touch the N-256 items behind them. So:

```
effectiveKey(item) = clamp(basePriority + f(now - offerTime, budgetRemaining))   // derived, never stored
candidate          = front of every non-empty bucket                              // ≤ 256
Pop                = argmax by (effectiveKey, then oldest offerTime)              // tie rule preserved
```

**Refresh cost is O(256), independent of N** — or exactly zero, because if every item ages at the same rate, bucket order is
invariant and nothing needs re-evaluating at all.

## 3. Cost model at the owner's stated scale (arithmetic, **not measurements**)

N = 10⁶ queued items, 80 B each (72 today, +8 if B3d's offer stamp lands), 60 Hz.

| Approach | Per refresh | Per dispatch | Refresh cost/sec | Verdict |
|---|---|---|---|---|
| Maintained key, **re-bucket sweep every 10th frame** | 10⁶ moves ≈ **80 MB touched** | O(1) | ~6 sweeps/s ≈ **~480 MB/s**, tens of ms each | ❌ blows a 16.7 ms frame, thrashes cache, churns bucket `Deque` growth |
| Derived key, **scan 256 fronts per dispatch** | none | 256 key computations | 0 | ✅ exact; ~10⁵ dispatches/s ≈ 2.6·10⁷ int ops/s ≈ ~1% of a core *(estimate)* |
| Derived key + **256-entry front tournament, refreshed every R frames** | **256 recomputes** | ~8 ops | R=10 → ~1.5k ops/s, noise | ✅✅ best fit for "every 10th frame": the batch is over **levels, not items** |
| **Pending-reprioritise list** (caller-driven changes) | O(changed) | 0 | proportional to changes | ✅ how explicit changes should land; **zero item-size growth** |

"Every 10th frame" is the right instinct pointed at the wrong object: refresh the ≤256-entry tournament, never the queue.

## 4. Derived keys vs maintained keys — the actual trade

| | Maintained | Derived |
|---|---|---|
| Correct as time passes | only if every change is written | by construction |
| Cost when nothing changed | O(N) — you must look | O(0) |
| **Where cost lands** | **a sweep you schedule, bill and watch** | **at dispatch**, inside the window `CPUBudget` charges to a lane accumulator |
| Ordering view | physical order **is** priority order — a debug dump shows truth | buckets only locally sorted; `highestBucket` stops being a valid summary of "most urgent" |
| Failure mode | **staleness, silent** | recomputation cost + two disciplines: clamp and within-bucket monotonicity |
| Item size | the field, plus a position handle for cheap moves | the *inputs* (`basePriority`, `offerTime`) — `offerTime` already exists in the B3d plan |

> **Deciding question: does this number change after the item is queued?** Time, budget headroom and deadline nearness → derive.
> A caller's explicit "this one matters" → maintain, because it is a decision and not a computation. Spend ledgers
> (`ChargeFifo`/`ChargePriority`) and `IsRoundExhausted()` are correctly maintained today: they record history, and history does not
> change retroactively.

**Recommended hybrid**
```cpp
effectivePriority = clamp( basePriority            // maintained: caller intent
                         + (now - offerTime) / step // derived: time, never stored
                         + externalAdjustment )     // maintained: rare, caller-set, counted
```
If you choose derived, buy back the two things you lose: do the derivation **outside** the charged runnable window so the lane
accumulator is not billed for a container comparison, and add a debug dump printing the ≤256 fronts with their effective keys.

## 5. Build order (each step lands green on its own)

1. **`Pop` reports the level it served from** — makes "filed under 2, reports 7" unrepresentable instead of documented. Four lines,
   already attempted and reverted at `073e746`. **The exact fix:** inside the template
   `auto item = std::move(bucket.Front());` deduces a **non-optional value**, so the member access is `item.priority` and **not**
   `item->priority`; capture `const std::size_t servedFromLevel = highestBucket;` **before** `RetrackHighestBucket()`, pass that to
   `ReleaseEmptyBucket(servedFromLevel)`, and cast with `static_cast<decltype(item.priority)>` — not `uint8_t`, because the template is
   instantiated for other element types, and not `std::remove_reference_t`, because this header deliberately includes nothing from
   `<type_traits>`. Do **not** add an `Assert` here: it would make Container depend on Core and invert the layering, and an assert only
   fires in a checked build.
2. **Aging as a derived term with the clamp, plus a wrap-boundary test.** `uint8_t` + `MaxPriority = 256` means aging past 255 **wraps
   to 0** — the most urgent queued work becomes the least urgent, silently, and only after a long stall. Test the boundary at 254/255
   and across it, not just the happy path.
3. **256-entry front tournament**, refreshed every R frames (R=10 default, a named constant), O(log 256) per dispatch. Staleness is
   bounded by R frames of aging, which is a policy statement you can write down and test.
4. **Pending-reprioritise list + `GetReprioritiseCount()`**, so "explicit changes are rare" is a measurement and not a hope. Report the
   outcome — `movedToNewBucket` / `alreadyAtThatPriority` / `notQueued` / `runningElsewhere` / `unknownTask`. Never let "did nothing"
   and "moved" look alike; that ambiguity is what this session has been cutting out all day.

Rejected for this scale: **indexed binary heap** (O(N) heapify per refresh, +4·N index array, and **not stable**, so it breaks the
documented oldest-first tie rule); **pairing/Fibonacci** (per-node pointers and allocation, which this codebase refuses);
**sorted array** (O(N²) per refresh); **radix re-bucket sweep** (the ~480 MB/s above).

## 6. Two constraints that outscale the queue

* **`RecordSizeBytes == 256` (static-asserted) with a 4096-record / 1 MiB default table.** At 10⁶ queued tasks that is **~256 MB of
  registry records** — three times what the queue items would cost. At "millions" the record, not the bucket structure, is the
  bottleneck, and that is a different decision with the owner in it.
* **Turnover.** Millions of create/release cycles put `TaskID::TGeneration` reuse and the **open `AtomicStackView` Treiber ABA**
  (`TODO_task_system.md` #7 D3, a confirmed true positive in the Memory module) under real load rather than theoretical load. That fix
  belongs to the Memory owner, and it becomes urgent before a dynamic-priority rewrite would.
* **`BucketSizeHint`** exists precisely to pre-size hot levels; at 10⁶ items let the top levels grow by reallocation and you pay for it
  every frame.

## 7. What is already true, so nobody rebuilds it

`StreamDrainPolicy::ChooseLane` is **pure** (neither charges budget nor mutates rotation state) and has two regimes: unlimited
allowance serves whichever lane has work, and when both have work it returns `fifoCredit >= priorityCredit ? Fifo : Priority`, credit
from `ConfigureRate` spent by `CommitTake`, with a zero weight treated as one and both falling back to **1:1** — a zero weight is
explicitly *not* "never serve this lane", because that would strand the lane's queued tasks. With a finite allowance it uses per-lane
shares and returns `ELane::None` once the round is exhausted. Callers choose the lane (`Enqueue(..., ELane)`);
`WorkItem::priority` orders inside the priority queue only and is **not consulted at all on the FIFO lane** (`073e746`, guide §15).
Keep lane choice and priority apart: the two lanes carry **separate accumulators**, so a lane migration moves cost between budget
shares, and that is a different operation from reprioritising.

## 8. Gate for every step

```bash
./build.sh Applications/EngineTest -test -debug -dev -release
for C in Debug Dev Release; do .pi/skills/hb-standards/scripts/runtest.sh $C 280; done
bash .pi/skills/hb-standards/scripts/check.sh
```
`-test` is not optional; a binary built without `-D__TEST__` prints advice and exits 1, so any verdict from it is vacuous. Tests for
lane behaviour are **duplicated per lane**, never one parameterised body — this project proved twice that a shared body lets one lane
go unwitnessed. **Commit before mutating, never after.** Stage paths by name; never touch `docs/Core` or `docs/OSAL`; never push.

## 9. The decision this plan is waiting on

**Standing decision 6:** (a) derived keys or maintained keys, or the hybrid above; (b) the **aging step** — frames of waiting per level
of promotion, which is a starvation policy, not an implementation detail. Nothing in section 5 beyond step 1 should start before
those two answers exist.
