# Plan: results embedded in the task, delivered by the base stream

Owner decisions of 2026-09-19 (fourth round), which supersede the container design:

| Ruling | Effect |
|---|---|
| The base stream retrieves results and rewinds; it also triages and enqueues a deliver task to the destination stream | The pass has a payload — this is the thing "fold" named but never had |
| The destination stream ID is on the **result**, and a Task fills the value | It belongs in R5's 8-byte header, which R9 already noted "did not have to grow" |
| **Embed one packet payload only**; a task handles bigger results itself | The engine stores exactly one 128-byte packet per task. Anything larger is caller-owned memory, and the packet carries a handle to it — which is the design's existing size-class rule ("exceeds payload → `new` a `unique_ptr`, move it") |
| Drop the janitor/refcount model as too complex | No third container, no use counts |

## 1. Measured layout this plan is built on

Probe run against the committed tree, mirror verified by `static_assert(sizeof(mirror) == sizeof(Task))`, then
reverted byte-identical.

| Quantity | Measured |
|---|---|
| `sizeof(Task)` | **48** — id@0, name@16, numResults@24, numSubTasks@28, numFinishedSubTasks@29, func@32, userData@40 |
| `Task` + one 128-byte packet | **176** |
| Record around it | **192** = 3 cache lines, and `192 % 64 == 0` so every record is line-aligned |
| Table at the default 4096 records | **768 KiB** (from 256 KiB) |
| Packet padded to a line boundary | record **208** — `208 % 64 == 16`, records misaligned. Rejected |
| `sizeof(RangedTask)` | **128** already |
| Result containers reserved today | **24 MiB** across 12 streams (2 MiB pool bank per stream, R21's arithmetic correction) — for a buffer with zero producers |

## 2. What this retires, stated plainly rather than silently

The container stops being load-bearing, so the rules that exist to protect it lose their subject:

| Row | Content | Why it dies |
|---|---|---|
| R3 | Two containers per stream, base swap + drain | No container |
| R4 | 128-byte packets from a thread-safe pool, released on the base stream, **valid for one frame** | Result lives in the record until the task is released; the pool is caller-side for big results |
| R11 | Bump allocator over slots, rewind = `count = 0` | No slots |
| R12 | `Reset()` grows the buffer from a `TaskDescriptor` | No buffer to grow; `TaskDescriptor` never gets invented (R19 predicted this) |
| R13 | Capacity declared per task, stream will not proceed without room | One packet per record is static — there is nothing to run out of |
| R14 | Lane closes at dequeue when the head does not fit | Nothing can fail to fit |
| R15 | Closed lane falls back to the other lane | No closure |
| R20/R21 | Container initial 1024 / growBy 1024; per-stream ceiling, `0` = unlimited | Sizing a thing that no longer exists |
| **Guard (a)** | Refuse at dispatch any task whose `NumResults` could never be admitted | **My own guard** — capacity is now static, so the predicate has no subject. `CanAdmitResults`, the `[[nodiscard]] bool` on the three enqueue paths and the six `(void)` casts go with it |
| **Guard (b)** | Log a capacity-closed lane | **My own guard** — R14's closure is gone, so there is nothing to log. Never implemented; now it never will be |
| R8's *stated reason* | "completions fold on the base thread" | Fold is gone. **R8's rule survives** on a new rationale: the pass reopens every stream's budget and performs every delivery, so blocking on that thread stalls the whole engine. Must be re-grounded, not left as a sentence with a false premise |

Kept and doing real work: R1, R2, R5 (the 8-byte header inside the 128), R6 (kind split), R7 (identity — this is
what makes reading a result in place safe), R9 (routing lives in the registry), R16 (completion optional), R22.

Code that becomes dead and is deleted, not left behind: `Engine/Core/ResultContainer.h/.cpp`, `NamedPoolAllocator.h`
if nothing else uses it, and `TaskStream`'s `firstResultContainer` / `secondResultContainer` / `growBySlots` /
`maxResultCapacitySlots` / `CanAdmitResults` / `ReportRefusal`. The mutation-proved ceiling tests from `2e087a0` go
with it — they were proven, and they are being deleted because their subject is gone, which must be said in the
commit message rather than glossed.

## 3. The new parts

### 3.1 One packet in the record

```
Task (48 B, unchanged)                    ResultPacket (128 B, new, appended → record 192 B)
 id@0  name@16  numResults@24             ┌── header, 8 B (R5) ──┬─────── payload, 120 B ──────┐
 numSubTasks@28 numFinished@29            │ kind 1B │ dest 1B    │                              │
 func@32  userData@40                     │ R6 split│ 12 streams │  written by the producing    │
                                         │ 0 = no  │ today, 1 B │  task itself, read in place  │
                                         │ packet  │ reaches 255│  by a deliver task           │
                                         └─────────┴────────────┴──────────────────────────────┘
```

- `dest` is filled by the task (owner's rule). `0xFF` means "no destination": R16's fire-and-forget and the caller
  that polls through its own `TaskID`.
- `kind == 0` is the "no packet was written" marker, so a deliver task never reads uninitialised bytes and R16's
  zero-output task stays expressible.
- `numResults` leaves the engine's decision path: it no longer sizes anything, admits anything or closes anything.
  Proposed: delete the field, and with it `RangedTask::declaredResults`. A task with a bigger result is a caller
  concern (R9), expressed as a handle in the payload.

### 3.2 Completion list

A finishing task (its last subtask finishing, whichever stream that was) records its `TaskID` for the base pass.

- **Bounded so that overflow is impossible, by proof not by retry**: a record can be live once, and a task records a
  completion only at its finish, so outstanding entries ≤ live tasks ≤ registry capacity. Size it from the registry.
- A mutex over a pre-allocated array, not a lock-free stack. R4 already made this exact argument for the old payload
  path and it applies verbatim: one push per completed task is far cheaper than the CPU-time syscall the stream
  already pays per task, and an atomic Treiber stack carries an ABA defect for no measurable gain.

### 3.3 The pass, per quantum, on the base stream

| Step | Action |
|---|---|
| ① | Reset every stream's budget — **R1/R2, a live defect today: `CPUBudget::Reset` has no production caller, so a stream with a configured allowance stops dequeuing permanently** |
| ② | Drain the completion list into a local batch |
| ③ | Per entry: `Find(TaskID)`; read the header; `kind == 0` or `dest == 0xFF` → skip |
| ④ | Enqueue a deliver work item into `dest`'s stream |
| ⑤ | Nothing. No rewind, no copy, no expiry — the result belongs to the task now |

## 4. Split into three commits, each with the full gate

| Commit | Content | Independent value |
|---|---|---|
| C3a | Packet + header + accessors in `Task`, record 192 B with `static_assert` that the size is a multiple of 64, tests | No behaviour change; the layout is committed and priced before anything depends on it |
| C3b | Completion list + the pass + **budget reopen (fixes R2)** + triage → deliver work item | Fixes the live defect; delivery works end to end |
| C3c | Delete `ResultContainer`, `TaskStream`'s capacity members, guard (a) and the ceiling tests; mark R3/R4/R11–R15/R20/R21 superseded, re-ground R8 | The tree stops carrying 24 MiB of dead reservation and the doc stops stating rules with no subject |

Gate on each: Debug/Dev/Release build with 0 `error:`, runner exit 0, every collection passing, `check.sh --staged`,
and a mutation with per-test attribution for every new check.

## 7. C3a landed — what is true now

`ResultPacket` is a real type (`Engine/Core/ResultPacket.h`, header-only) and one packet is embedded in `Task`.
Measured by the suite, not by hand: packet **128** bytes, `Task` **176**, record **192** = 3 cache lines, default
table **768 KiB**. `ResultPacketTest` joins the suite, which is **60 collections** now. Four proofs: dropping
`result.Clear()` from `LoadIntoRecord` reddens the reuse test; pointing `SetKind` at the destination byte reddens the
same test; widening the payload to 128 and dropping `alignas(std::uint64_t)` are caught by `static_assert`, in the
second case by two of them at once - the packet's own width and the record's price of 192.

Behaviour change: none. Nothing reads a packet yet, and nothing writes one outside these tests. The next commit
(C3b) is where a producer writes and a reader arrives.

Gate: Debug, Dev and Release each built with 0 `error:` and ran with exit 0 and `all 60 collections passed`.
`check.sh --staged` 0 violations, 1 advisory - the `using TIndex = std::size_t;` false positive in `Task.h`.

## 5. Open before C3a starts

1. **Which thread runs the pass.** R1 says "the *base stream* resets each stream's budget by calling into it", and
   stream 0 is named "Main" and has its own thread, while the OS thread named "Base" is the one running
   `Engine::Run()`. These are different threads. Measured: `SetThreadName("Base")` + `SetStreamIndex(-1)` on the
   caller, `streams.Emplace(0, "Main", 0)`, and `Start()` gives every stream a real `std::thread`. If the pass runs
   on `Engine::Run()`'s thread it races consumers on stream 0; on stream 0's own thread the read window is exact.
   **Asked twice, not answered — it is load-bearing now.**
2. **Deliver granularity**: one work item per completed task (RangedTask stays 128 B, costs a dispatch per result) or
   batches of up to 8 TaskIDs inside a grown work item (256 B, one dispatch per 8 results, doubles queue footprint).
3. Whether `Task::numResults` is deleted or kept as an advisory declaration of caller-managed outputs.
