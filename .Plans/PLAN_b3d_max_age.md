# PLAN — B3d: per-stream max age and abandonment (todo #6, N4)

Status: **not started, fully specified.** Written 2026-09-25 07:10 at `02ca08e` by the session that
fixed the `Engine::Run` loop-header regression. Every anchor below was read from the tree at that
commit; line numbers are given as landmarks, the quoted code is what to match. If an anchor does not
match, the tree moved — re-read that file rather than trusting the number.

## PRECONDITION — blocking owner decision, added 07:40 after the plan was written

Do not implement Step 3 until **standing decision 4** in `.Plans/TODO_task_system.md` is settled: today an abandoned
work item notifies **nobody**. `DispatchSuccessor` fires only when a join completes, so a dropped item stops the
requestor's chain silently, and `FindTask` returning `nullptr` cannot tell abandonment from a completed-and-released
task. A max-age drop is the third such site; the plan is valid either way, but its `ReportAgedOutWorkItem` must either
deliver the outcome the decision chooses or the decision must explicitly accept log-only for this stream class.

## CHANGED SINCE THIS PLAN WAS WRITTEN (13:20) - four facts to fold in before starting

1. **Both lanes are now reachable from the public API** (`TaskSystem::Enqueue(..., StreamDrainPolicy::ELane)`, and
   `EnqueueTask(..., priority, lane)`). An over-age check therefore has to cover **both** lanes, and the test has to be duplicated per
   lane rather than parameterised - that is how this project proved it, twice: a shared body let the priority loop go unwitnessed and a
   mutant survived it (`c07eecc` `56b70a3`).
2. **The abandonment vocabulary exists now.** `TaskStream::AbandonHeldWork` and `GetAbandonedWorkNoticeCount()` are the established way
   a stream reports work it will never run. Step 3's `ReportAgedOutWorkItem` should follow that shape and feed a readable counter, not
   invent a second vocabulary, and it must call `FireAbandonedNotice` so an aged-out item tells its requestor.
3. **`WorkItem` is 72 bytes, not 56**, since the abandonment notice arrived. So Step 1's arithmetic is **72 -> 80**, and the two decided
   figures to update are `decidedWorkItemBytes = 72` and the header note that says the item measures 72; `decidedTaskBytes` is 192 and
   `decidedRecordBytes` stays 256 because `reservedToCacheLine` was already shrunk 48 -> 32.
4. **Item `priority` is an insertion label, not a live value** (`073e746`). Relevant because age-based promotion in B3d must not be
   implemented by writing that byte on a queued item: the level the item sits in is what decides when it runs, so age has to be
   evaluated at the check site. That is the same derived-key choice as standing decision 6, and the two should be decided together.

## Scope decision (already made — do not reopen without the owner)

Implement the **per-stream max age**. Do **not** implement the per-task optional deadline in this
change: no caller in the tree asks for it, and it costs a field in `Task` plus a second comparison
path. The per-stream ceiling is what prevents the documented harm — work whose context went stale
being run because it happened to be queued. Rationale and the two rejected alternatives are in
`.Plans/TODO_task_system.md` under the 06:30 entry.

Rejected alternative worth remembering: reading age from a per-lane monotonic watermark. A lane that
stalls while other lanes advance gets the wrong age, which is exactly the case the feature exists for.

## Step 1 — stamp the offer time on `WorkItem` (+8 bytes, 56 → 64)

`Engine/Core/WorkItem.h`, after the `current` field and before `public:` (~line 49):

```cpp
	/// @brief When this work was offered to a stream, in engine steady-clock nanoseconds since the epoch.
	/// @details Stamped at construction, never re-stamped. A copy keeps the stamp: an item re-added to a lane after a
	///          partial run is the same work offered at the same moment, and re-stamping it would let a task dodge the
	///          stream's max age simply by not finishing in one call. A split inherits its parent's stamp for the same
	///          reason - see `Task::GenerateSubTask`.
	/// @note 0 means "never stamped", which no stream enforces. Only a stream with a max age configured reads it.
	std::chrono::nanoseconds offerTime{ 0 };
```

Add `#include <chrono>` to the header's includes.

`Engine/Core/WorkItem.cpp`, in the private constructor at line 35
(`WorkItem::WorkItem(Task& task, TIndex start, TIndex end, uint8_t priority)`): initialise `offerTime`
in the member-init list from the engine's steady clock. **Resolved 08:10, no check needed:** `TaskStream.cpp:615` uses `std::chrono::steady_clock::now()` for its shutdown deadline, so the engine's bound clock is `steady_clock` - stamp `offerTime` from `std::chrono::steady_clock::now().time_since_epoch()` and the age book and the deadline book share one clock. The original warning stands as history: use the same clock as
`StreamDrainPolicy` already measures lane time with, not a second clock — a mismatch between the age
book and the time-accounting book is a silent bug, and `StreamDrainPolicy.h` already returns
`std::chrono::nanoseconds` for `GetFifoUsed`/`GetPriorityUsed`, so the units match and only the clock
source needs confirming (`Engine/Core/Time.h`).

`Engine/Core/Task.cpp`, in `GenerateSubTask` — the new item is constructed with a fresh stamp, so
overwrite it with the parent's immediately after construction:

```cpp
	subItem.offerTime = offerTime;
```

One line, and the reason is the header note above. `Task::GenerateSubTask` is private and
`TaskProvider::MakeWholeItem` is the other construction path (C2 made them so); both go through the
same private constructor, so both are covered by the ctor stamp plus this one assignment.

## Step 2 — per-stream knob on `StreamDrainPolicy` (default: unlimited)

`Engine/Core/StreamDrainPolicy.h`, beside `SetCpuAllowance` (~line 45) and the
`std::chrono::nanoseconds GetFifoUsed() const noexcept` group (~lines 74-79):

```cpp
	/// @brief Set the oldest work this stream is still willing to run.
	/// @param maxAge Zero means unlimited: the stream runs whatever it is holding no matter how old it is.
	/// @note Default unlimited, so every stream behaves exactly as it does today until someone opts in.
	void SetMaxAge(std::chrono::nanoseconds maxAge) noexcept;

	/// @brief The oldest work this stream is still willing to run; zero means unlimited.
	[[nodiscard]] std::chrono::nanoseconds GetMaxAge() const noexcept;

	/// @brief Whether work offered at `offerTime` is too old to run on this stream.
	/// @details False for an unlimited stream and for an unstamped item, so the check cannot fire by accident.
	[[nodiscard]] bool IsOverAge(std::chrono::nanoseconds offerTime, std::chrono::nanoseconds now) const noexcept;
```

`StreamDrainPolicy.cpp`: store one `std::chrono::nanoseconds maxAge{ 0 };` field. `IsOverAge` is
`maxAge.count() > 0 && offerTime.count() > 0 && now - offerTime > maxAge` — all three terms matter,
and the test in Step 5 is what proves each of them.

## Step 3 — enforce it where the released-task check already lives

`Engine/Core/TaskStream.cpp`, immediately after this existing block (line 497-503) — copy its shape,
including the `restore()` and `return true`, because a pass that dropped something did do something:

```cpp
	auto* task = taskSys.FindTask(workItem->taskID);
	if (task == nullptr)
	{
		ReportReleasedTask(*workItem);
		restore();
		return true;
	}
```

Insert:

```cpp
	if (policy.IsOverAge(workItem->offerTime, <engine now in the same clock as Step 1>))
	{
		ReportAgedOutWorkItem(*workItem, *task);
		restore();
		return true;
	}
```

`policy` is a placeholder for whatever `TaskStream`'s member of type `StreamDrainPolicy` is called —
one grep in `TaskStream.h` for `StreamDrainPolicy` resolves it; there is exactly one.

Add `ReportAgedOutWorkItem` beside `ReportReleasedTask` (`TaskStream.cpp:186`, declare it in
`TaskStream.h` next to that declaration) and make it reuse the **shutdown abandonment vocabulary** from
`TaskStream.cpp:572` / `:627` ("... item(s) still held. They are abandoned, not requeued ..."), naming
the task ID, the item's range and its age. One vocabulary for abandoned work in this engine, not two —
that is the point of the exercise, not a nicety. Do not reach for the task's fields beyond the name/ID:
`ReportReleasedTask`'s own comment explains why the ID is the identity that means something.

Semantics, fixed: over-age work is **not run and not requeued**. Its `WorkItem` is destroyed like any
dropped item, and the task record is left to the registry's normal release path — the same treatment a
released task gets, which is what keeps RAII honest: an abandoned task is still destroyed.

## Step 4 — update the decided-size report

`Engine/Core/ResultPacket.cpp:35`: `constexpr std::size_t decidedWorkItemBytes = 56;` → `64`, and amend
the surrounding message (it explains that the width is paid per lane change, and that `RangedTask`'s
name copy came out when the item type was slimmed). The new 8 bytes are the offer stamp, and the message
should say what bought them, because that report exists to make a size change loud rather than silent.

## Step 5 — the test that has to prove it, with the mutant

New collection in `Engine/Test/UnitTestCollection.cpp`. It needs an **in-band sentinel**: a negative
assertion ("it did not run") passes vacuously unless the test also proves the item would have run
without the age rule. So run the same fixture twice:

1. Stream with **no** max age, work enqueued, driven → the sentinel counter reaches 1. This is the
   control, and without it the whole test is decoration.
2. Same fixture, stream with a ~20 ms ceiling, work enqueued, left **undriven** for ~60 ms (bound the
   wait in wall clock, never in iterations — an iteration-bound wait silently passes in Release and
   fails in Debug, which cost this project a phantom "configuration anomaly"), then driven.

Assert on the aged run: the sentinel **stays 0**, the abandonment report fired exactly once — read it
through a counter on `TaskStream` in the style of `GetLaneWorkRefusalCount`, not by scraping the log —
and the item is gone from the stream.

Named mutant, per the standing standard that a mutation is not accepted until a named test names the
gate: delete each of the three terms of `IsOverAge` in turn (`maxAge.count() > 0`, the unstamped guard,
the comparison). Each must be killed by the new collection's name. Also mutate the sub-slice stamp
inheritance in Step 1 — that one must be caught too, or write the test that catches it before making
the change.

**Commit before mutating, never after.** This rule exists because a `git checkout` used to undo a
mutant destroyed new work three times.

## Step 6 — documentation, or the change is not finished

* `docs/TaskSystemGuide.md`: a short "max age" subsection — what it protects, that it is per stream and
  off by default, and that aged-out work is dropped and reported, never run.
* `docs/TaskSystemRedesign.md`: the per-task deadline row keeps its design, marked **not implemented**
  with the reason (no caller in the tree), so the doc does not describe a feature that does not exist.
* `docs/Core/index.html` is **not yours** — a concurrent agent owns `docs/Core` and `docs/OSAL`. Note
  the needed `WorkItem` change there and let them apply it.

## Step 7 — the gate, which is the only thing that counts

```bash
./build.sh Applications/EngineTest -test -debug -dev -release
for C in Debug Dev Release; do .pi/skills/hb-standards/scripts/runtest.sh $C 280; done
bash .pi/skills/hb-standards/scripts/check.sh
```

Success is `runner exit=0  EngineTest: all <N> collections passed` three times and `0 mechanical
violations` with `build gate PASS 12/12`. `-test` is not optional: only `-D__TEST__` compiles the test
bodies, and a binary built without it prints advice and exits 1, so any verdict from it is vacuous. If
`check.sh --apply` reformatted anything, re-run `build.sh ... -test ...` before trusting a run — its own
rebuild drops `-test`. Stage paths **by name**, `git restore --staged docs/Core docs/OSAL` before every
commit, never push.

## Known interaction to leave alone

`TaskStream::Update`'s pass already returns whether it took, ran or dropped something; a drop from the
age check returns `true` for exactly the same reason `ReportReleasedTask` does. Do not "optimise" the
`FindTask` lookup or the age check into one branch — they answer different questions and the released
path is load-bearing for six named window tests.
