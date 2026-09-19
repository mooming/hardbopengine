# Plan — TaskRegistry: identity as index plus generation (R7, sized by R22)

Owner decisions this implements: **R7** (task identity is index plus generation in a registry; Tasks stop
being stack objects; a packet naming a dead task is recognised by generation and dropped) and **R22/R22a**
(capacity tunable, adjustable, growable like a result container: initial 4096 records, `growBy` 4096, ceiling
`0` = none, and `Create` never grows the table).

## Two measurements that decided the shape

- **`Task` is not movable.** It holds `std::atomic<TNumSubTasks> numFinishedSubTasks`, so its move constructor is
  deleted. `Array<T>::Resize` move-constructs every element into the new buffer, so `Array<Record>` cannot grow
  while tasks are live - the storage can move at most once, and only if nothing references it.
- **A pool bank is the wrong unit for a table this size.** A 160 KiB request to `MultiPoolAllocator` gives a
  block size of 262,144, which `CalculateNumberOfBlocks` raises to `MinNumberOfBlocks = 16`, so a bank is 4 MiB -
  a 25-times reservation for a table nobody has filled. This is the same amplification R20 understated for the
  result container, and at registry size it is unacceptable rather than merely untidy.

Therefore: **the registry is a list of fixed-size banks**, one bank per `growBy`, allocated from the engine's
scoped allocator (`MemoryManager::Allocate`) with no bank rounding. Records never move, which buys three things
a reallocating array cannot: `Task` needs no move constructor, an index handed out stays valid across growth,
and a pointer from `Find` cannot be invalidated by another thread growing the table.

| Shape | Verdict |
|---|---|
| `Array<Record, NamedPoolAllocator>` | rejected - needs a movable `Task`, which the atomic forbids |
| `MultiPoolAllocator` banks | rejected - 4 MiB reserved for a 160 KiB table |
| Bank list over the scoped allocator | chosen - no moves, exact sizing, one allocation per grow |

## Shape as built

| Piece | Detail |
|---|---|
| `TaskID` | `{ index, generation }`, `IsNull()` on the house sentinel pattern, `uint32` generation |
| `TaskRegistry` | banks of `RecordsPerBank == growBy` records; `Create`, `Find`, `Release`, `Grow`, capacity, count, ceiling |
| Capacity | initial 4096 records = one bank; `Grow()` adds exactly one bank, so capacity is always exact |
| Ceiling | default `0` = unlimited (R21's inert convention); `Grow` refuses past it and says so |
| Free list | singly linked through the free records by index, head under one mutex - `Logger` creates tasks from arbitrary threads today |
| `Find` | lock-free: bank + offset, then acquire-load the generation and in-use flag. Those two are atomic so the read is not a data race. |
| `Task` | carries its own `TaskID`, so `GenerateSubTask` and all 12 call sites keep their shape |
| `RangedTask` | holds `TaskID` instead of `std::reference_wrapper<Task>`, plus the `numResults` its task declared at generation time - so the ceiling guard costs no lookup, and a later `SetNumResults` cannot rewrite a promise already made |
| `RangedTask::Run` | takes the resolved `Task&` from the stream, which is where the registry is already to hand; no global access from inside a work item |

## Checklist

| # | Step | Verification | Done |
|---|---|---|---|
| 1 | `TaskID` + `Task` carries its ID | builds; ID round-trips | ☐ |
| 2 | `TaskRegistry` over banks, R22 defaults | builds; record size measured, not assumed | ☐ |
| 3 | `RangedTask` holds an ID and a declared-results snapshot | builds | ☐ |
| 4 | `RangedTask::Run(Task&)`; stream resolves the ID | builds | ☐ |
| 5 | `TaskSystem` owns the registry; create/find/release wrappers | builds | ☐ |
| 6 | `Logger` and `UnitTestCollection` stop holding `Task` objects | builds, suite green | ☐ |
| 7 | Dead-task test: release, then the same ID does not resolve | must go red when the generation check is removed | ☐ |
| 8 | Reuse test: a new task in a released slot has a new generation | must go red if generation is not bumped | ☐ |
| 9 | Ceiling test: `Grow` refuses past it, unlimited by default | must go red on an inverted ceiling test | ☐ |
| 10 | Growth keeps earlier IDs resolvable | must go red if IDs break across banks | ☐ |
| 11 | Exhaustion test: a table with no free record returns invalid and logs | must go red if `Create` grows implicitly | ☐ |
| 12 | Three-configuration gate, `check.sh --staged`, commit | 58 collections, exit 0, 0 violations | ☐ |

## Stated before writing, so it cannot be quietly assumed

- Nothing in production creates a task except `Logger`. Everything below it is exercised by the suite, so suite
  coverage is the real test surface and its assertions must be able to fail.
- A released record's fields stay readable to anyone who already resolved it - `Task` owns no memory, so this is
  stale data rather than a dangling pointer, and it is documented instead of papered over.
- `Grow` is only legal when no lookup is in flight. Nothing calls it in production yet; the tests drive it
  directly on one thread, which is the documented rule rather than a pretence of thread safety.
- `Wait`, `BusyWait` and a public `HasDone` survive this commit. Their removal is a later item and rewrites
  five test call sites.


## Result, and where this plan was wrong

Landed as `3be27c3`, gate green in Debug/Dev/Release, 59 collections, `check.sh --staged` 0 violations.

* The plan's storage shape survived implementation only because `Task` is immovable. If `Task` ever becomes
  movable, the bank design does not become optional — it becomes wrong in the same way `Array` was, and the
  reason is `std::atomic` being its own address, not a style preference.
* The plan's cost figures were mine, written before the class existed, and were wrong: 64 bytes per record and
  256 KiB per table, not 40 bytes and 160 KiB. Corrected in R22, and the figure is now a published constant that
  a test prints.
* Two plan steps I had not written and had to add mid-flight: the sizing parameters must be atomic (the
  cross-thread assert is the engine telling you), and legacy banners whose line 1 is not the copyright fail file
  hygiene the moment a commit touches the file.
* One test assertion of mine was wrong and passed nothing: I asserted a creation must be refused when three free
  records remained. The suite caught it on the first run. The replacement asserts the invariant — fill the table
  and require exactly the records it owns.
