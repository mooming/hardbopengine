# PLAN — optional abandonment notice to the requestor (decision 4, owner's call 08:10)

Owner's decision: **provide an optional callback** — a function pointer, set only by requestors who want to know.
Status: **partly landed at the next commit - see "What landed" at the bottom of this file.** Written at `ffa14fa` by the session that established the gap.

## The fact that decides the shape (read from the tree, not inferred)

An abandonment notice **cannot live on the `Task`**, because at the moments it must fire, the task is exactly the thing
that is not there:

| Site | What it actually has | Task resolvable? |
|---|---|---|
| `TaskStream.cpp:500` — `ReportReleasedTask(*workItem)` | the item; `FindTask` already returned `nullptr` | **no** |
| `TaskStream.cpp:566` `CloseDrivenStream` / `:622` `CloseOtherStream` | only `CountPendingItems()` — a **count**, no item is ever examined | **no** |
| `PLAN_b3d_max_age.md` Step 3 — max-age drop | the item, and a live `task` pointer | yes |

Two of three sites cannot reach a task-stored handler, so the notice must be **carried by the `WorkItem`**. That is the
whole design in one sentence, and it is why the obvious "put it next to `successor` in the record" version does not work:
`SetSuccessor` survives only as long as the record does, and abandonment is where the record is already gone.

The current log text at both shutdown sites — *"any customer awaiting them will not receive one"* — is honest today, and
becomes a **lie** the moment this ships. It is in Step 5 on purpose.

## Cost, which the owner has to accept before Step 1 is committed

`WorkItem` is 56 bytes and the size is guarded by `decidedWorkItemBytes` in `ResultPacket.cpp:35`, precisely so that width
changes are loud: an item is copied on every enqueue, every re-add after a partial run and every lane change, so the width
is paid per lane change rather than per task.

```
56  today
72  + function pointer (8) + userData (8)          <- this plan
80  + PLAN_b3d_max_age.md's offerTime (8)          <- this plan + max age together
```

**If 72/80 bytes per queued item is refused**, the fallback is honest but narrower: fire only where the item is already in
hand during a normal pass (`:500` and the max-age drop), leave the shutdown-close sites as a count-only loss, and leave
their log lines saying that a waiting customer gets nothing. Do not fake the shutdown case to make the feature look
complete — walking every queue at teardown to resolve tasks that may already be released is how a shutdown deadlock gets
re-imported, and this project spent a session removing those.

## Step 1 — the notice type and the two fields

`Engine/Core/WorkItem.h`, above the class (it belongs to the item, not to `Task`):

```cpp
/// @brief Called when work is dropped without running, on the thread that dropped it.
/// @param abandonedTask the ID the item carried, which is the only identity that still means something
/// @param userData the pointer the requestor supplied, opaque to the engine
/// @note At most one call per work item, and never for work that ran. The callee must not block, must not
///       acquire a task-stream lock, and must not touch the task record — it is being dropped around this call.
using FAbandonedNotice = void (*)(TaskID abandonedTask, void* userData) noexcept;
```

Inside `WorkItem`, beside `priority`/`affinity` so the notice sits with the identity it names:

```cpp
	/// @brief Optional notice fired if this item is dropped without running. `nullptr` means nobody is listening.
	/// @details Copied like everything else, so a re-add after a partial run and a split slice both still notify —
	///          a requestor that asked once is told once per drop, and the notice for a slice is not the notice for
	///          the whole job. Set on the task before it is offered; see `TaskSystem::SetAbandonedNotice`.
	FAbandonedNotice abandonedNotice{ nullptr };
	/// @brief The requestor's context, passed through untouched. Its lifetime is the requestor's problem, not the
	///        engine's: it must outlive every drop this item can suffer.
	void* abandonedUserData{ nullptr };
```

`WorkItem`'s copy semantics are already `= default`, so **copies and lane changes carry the notice for free** — that is
the property that makes this design cheap, and it is also why `std::function` is not on the table: 16 bytes of pointer
pair versus a 32-byte type that may allocate per item copy.

## Step 2 — set it on the task, stamp it into the item at offer time

A requestor cannot set a field on an item it never sees, and setting it after `EnqueueTask` races the first pass. So the
public API is on the task, and the constructor copies it inward — one call per offer, no lock, no new container:

`Engine/Core/TaskSystem.h`, beside `SetSuccessor` (`:143`):

```cpp
	/// @brief Ask to be told if this task's work is dropped without running.
	/// @param handler `nullptr` clears the notice — the default, and the state every task is in unless asked.
	/// @note Call before the task is offered. After it is queued, the engine may drop the item before this lands,
	///       which is not an error but means no notice arrives; there is deliberately no lock to close that window.
	void SetAbandonedNotice(TaskID task, FAbandonedNotice handler, void* userData = nullptr) noexcept;
```

Store the pair on `Task` (`Engine/Core/Task.h`), where the record's existing `reservedToCacheLine[48]` absorbs it.
Then in `WorkItem`'s private constructor (`WorkItem.cpp:35`) copy it out of the task, next to where `taskID` comes from —
which is the same mechanism `PLAN_b3d_max_age.md` uses for `offerTime`, so the two plans compose: **sub-slices inherit
both the stamp and the notice from the parent**, and `Task::GenerateSubTask` copies the notice for the parent's reason
(a slice is not a new request).

## Step 3 — one firing point, so the three sites cannot drift

`Engine/Core/TaskStream.h` / `.cpp`, beside `ReportReleasedTask` (`:186`):

```cpp
	/// @brief Report that an item will not run, and fire its notice if it has one.
	/// @param reason text for the log line, naming the site: "released", "aged out", "stream closed"
	void DiscardWorkItem(const WorkItem& item, const char* reason) const noexcept;
```

`DiscardWorkItem` does the log line **and then** `if (item.abandonedNotice != nullptr) item.abandonedNotice(item.taskID,
item.abandonedUserData);` — one place, so no future drop site can log without notifying or notify without logging. The
`reason` parameter exists because a notice with no explanation in the log is unanswerable at 3 a.m.; it is the same
argument as the counters in `GetLaneWorkRefusalCount`.

Call sites: the released branch at `TaskStream.cpp:500` (replacing the bare `ReportReleasedTask` call), the max-age branch
from `PLAN_b3d_max_age.md` Step 3, and — **only if the owner accepted the width** — the queue-discarding path behind
`CloseDrivenStream`/`CloseOtherStream`, which today reports a count without looking at a single item.

## Step 4 — update the decided-size report or the build is wrong, not the test

`Engine/Core/ResultPacket.cpp:35`: `decidedWorkItemBytes` 56 → **72**, and extend the surrounding message to say what the
16 bytes bought: a requestor can now be told its work was dropped, which nothing in the engine could do before. Keep the
existing sentence about width being paid per lane change — it is why the number is worth guarding at all.

## Step 5 — the log lines that would start lying

`TaskStream.cpp:570` and `:627` both end with *"any customer awaiting them will not receive one"* / *"a task that could
not ... "*. Once `DiscardWorkItem` is wired through those paths, that clause must be replaced with the truth: work is
abandoned, not requeued, and a requestor that asked to be told is told there. Leaving the old sentence in place after the
feature exists is the documentation half of the same defect the callback fixes.

## Step 6 — the test, its control, and the mutants that must die

New collection in `Engine/Test/UnitTestCollection.cpp`, with the in-band sentinel rule applied to both directions:

1. **Control — ran means silent.** Work that completes normally fires **nothing**. Without this case, a test asserting
   "notice fired on abandon" can pass on a build that fires always, which is a notification-shaped no-op.
2. **Released-task drop.** Enqueue work that is not driven, release the task, drive the stream → the handler runs exactly
   once, receives this task's ID and this requestor's `userData` value (a distinctive pointer, not a boolean), and the
   sentinel shows the runnable never executed.
3. **Unset stays free.** The same fixture with no notice set → nothing fires, nothing crashes. `nullptr` is the default and
   most tasks never ask; that path is what every existing caller depends on.
4. **Sub-slice inheritance.** A split job whose slices are dropped fires the parent's notice — proving Step 2's copy
   happens. Without it, a task could dodge notification by splitting, exactly like the max-age dodge in the sibling plan.

Named mutants, per the standing standard that a mutation is not accepted until a named test names the gate: remove the
`if (item.abandonedNotice != nullptr)` call from `DiscardWorkItem`; remove the notice copy from `WorkItem`'s constructor;
remove the inheritance line from `Task::GenerateSubTask`. Each must be killed by a named collection above.
**Commit before mutating, never after** — that rule exists because undoing a mutant with `git checkout` destroyed new work
three times.

## Step 7 — gate

```bash
./build.sh Applications/EngineTest -test -debug -dev -release
for C in Debug Dev Release; do .pi/skills/hb-standards/scripts/runtest.sh $C 280; done
bash .pi/skills/hb-standards/scripts/check.sh
```

`-test` is not optional — only `-D__TEST__` compiles the test bodies, and a binary built without it prints advice and
exits 1. If `check.sh --apply` reformats anything, re-run the `-test` build before trusting a run. Stage paths by name;
`git restore --staged docs/Core docs/OSAL` before every commit; never push.

## Step 8 — documentation, because a callback with no contract is a liability

`docs/TaskSystemGuide.md`: who may set a notice, when it fires, **which thread runs it** (the thread that dropped the
item — a worker mid-shift, so it must not block or take a task-stream lock), and that the engine cannot protect
`userData`'s lifetime. `docs/TaskSystemRedesign.md` §6.2 gets the sentence that abandonment now reaches the requestor that
asked, closing the gap this plan was written for. `TaskSystem.h`/`WorkItem.h` carry the contracts; `docs/Core` is the
concurrent agent's, so note the new public member for them instead of editing it.

## Free fact from the same read, for `PLAN_b3d_max_age.md`

`TaskStream.cpp:615` uses `std::chrono::steady_clock::now()` for its shutdown deadline — the engine's bound clock **is**
`steady_clock`. That resolves the two-minute clock-source check the max-age plan flagged: stamp `offerTime` from
`std::chrono::steady_clock::now().time_since_epoch()`, and the age book and the deadline book are the same clock.


## What landed, and what did not (written at implementation time)

Landed, gated green in Debug/Dev/Release: `FAbandonedNotice`, the two `WorkItem` fields, the `Task` fields, the constructor
stamp (which is why sub-slices inherit for free - `GenerateSubTask` passes `*this`, so no inheritance line was needed),
`TaskSystem::SetAbandonedNotice`, `TaskStream::FireAbandonedNotice` as the single decision point, and the call at the
released-task site. Sizes moved as the guards predicted: `WorkItem` 56 -> 72, `Task` 176 -> 192 absorbed by shrinking
`TaskRegistry::reservedToCacheLine` 48 -> 32 so the record stayed priced at 256.

**Not landed, in the order that matters:**
1. **The firing branch has no test.** The 59 green collections prove only that the `nullptr` default path is untouched -
   which is what makes this safe to land, not what makes it correct. Without Step 6's control case, deleting the
   `if (item.abandonedNotice != nullptr)` call would break nothing, which is precisely how `Engine::Run`'s loop header
   disappeared unnoticed. Do Step 6 before adding anything else here.
2. **The shutdown close sites are still count-only.** `CloseDrivenStream`/`CloseOtherStream` call `CountPendingItems()`
   and never examine an item, so work abandoned at teardown notifies nobody. Their log lines therefore remain true, and
   Step 5's rewording is deliberately not applied - it would be a lie in the other direction. This is the plan's named
   fallback, and it means the notice today covers released-task drops only.
3. Steps 7-8: the named mutants, and the documentation.

The reason for landing rather than continuing is disclosed rather than hidden: the implementing session ran out of context,
and half-written test bodies in a library source are how this project lost work three times. A committed, gated, opt-in
feature with its gap written down is recoverable; a broken tree is not.

### Why Step 6 was not written in that session, and the two designs that would work

The obvious end-to-end test - enqueue, release, drive, expect the notice - is **not safe to write casually**, and a flaky test
is worse than none because it teaches everyone to ignore the red one. Two obstacles, both from contracts in the tree:

* `TaskStream::Update` allows **one driver, and that driver is the stream's owner thread**. So a test body cannot simply call
  `Update()` on a stream it does not own; and a worker stream *is* driven by its own thread, which would race the release and
  sometimes run the item before it is dropped. That is the flake.
* `WorkItem`'s constructor is private with `Task` and `TaskStream` as its only friends, and `FireAbandonedNotice` is private to
  `TaskStream`, so a test cannot fabricate the drop the way it would like to.

**Design A - deterministic, in-process, kills the two mechanisms most likely to rot.** In the task-system test file, declare a
provider subclass to reach `MakeWholeItem` (C2 made it `protected static` for exactly this kind of legitimate use):

```cpp
	struct NoticeProbeProvider final : TaskProvider
	{
		using TaskProvider::MakeWholeItem;
	};
```

Then: set a notice on a task, `MakeWholeItem(task, priority)` and assert `item.abandonedNotice` and
`item.abandonedUserData` came through - which is the constructor stamp, i.e. the same mechanism a split slice inherits - then
call the stream's fire helper and assert the probe recorded this task's ID and this requestor's `userData`, and that a second
item with no notice left the probe untouched. This needs one enabler to decide first: either `TaskSystemTest` (or
`TaskStreamTest`) is made a friend of `TaskStream`, or `FireAbandonedNotice` becomes public. **Prefer the friend**: publishing
an internal drop-site helper as engine API to make a test shorter is the trade that later gets used by accident.

**Design B - the real end-to-end, and the condition that makes it legal.** Drive the base stream from its own owner thread and
assert on the *wiring* at the released-task site. Legal only if the test body genuinely runs on the base thread, which it has
to be checked for, not assumed - this project already has a base-thread witness in the isolation tests, and the measurement
that `Engine::Run` pumps almost never means the base stream is not being driven from a loop in this binary, so the test must
drive it itself and from the right thread. Bound the driving in wall clock, never in passes.

**Recommended order: A first** - it is deterministic, it costs microseconds, and it kills both the "fire removed" mutant and the
"stamp removed" mutant, which are the two that break silently. B afterwards, and only once the base-thread question is settled,
because B is also the test that would have caught `Engine::Run`.

