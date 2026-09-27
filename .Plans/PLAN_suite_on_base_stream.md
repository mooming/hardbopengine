# PLAN — run the suite on the base stream, driven by `Engine::Run` (owner's arrangement, 2026-09-28)

## Decision given

Owner's words: *"it'll be ideal if the engine test suite runs on the base stream while other engine
streams are working independently."*

This closes the item that had been blocked as **"the `Engine::Run` guardrail needs a headless-safe
target"**. No headless application is needed, because the arrangement makes the harness itself the
witness: **if the engine loop does not pump, no collection runs, and a zero-collection run is already a
hard refusal** (`runtest.sh` exit 98). The pass count stops being a number a dead loop can print
anyway and becomes proof that the loop ran.

`Applications/EngineTest/TestMain.cpp` already names this as the intended arrangement and records that
it is not the current one: today `RunTests()` **blocks and shuts the engine down before `Run()` is
entered**, so the loop sees an already-stopped system. Measured: the base stream recorded 8 driven
passes in the whole binary, at most 1 inside the loop.

```
today:    Initialize → RunTests() [blocks, pumps itself, shuts down] → Run() [0 passes]
target:   Initialize → post the suite to the base stream → Run() [pumps base; IO and worker
          streams run on their own threads] → suite's last collection requests shutdown →
          Run() returns → main reads the tallies and sets the exit status
```

## Feasibility, read from source rather than assumed

| Question | Answer | Where |
|---|---|---|
| Does shutdown requested from inside base work self-join? | **No.** `JoinAndClear()` never joins the base thread — it *is* that thread. It requests other streams closed, then **keeps pumping the base stream** under a wall-clock deadline while posted tasks and pending items remain, then reports abandoned work | `TaskSystem.cpp:206-231` |
| Can a test body wait for cross-stream conditions while running inside base work? | **Yes.** `TaskSystem::DriveUntil` detects the base-owner thread and takes the nested-pump branch (`SetNestedPumpAllowed(true)` + RAII guard, calling `Update()` per millisecond). Inside `Run()` the thread is still the base thread, so the same branch is taken — the nesting is one level deeper, not a different mode | `TaskSystem.h:297-340` |
| How is the suite posted? | `TaskStream::DispatchPostedTasks(MainThreadTaskQueue::TTaskFunc, void*)` — a callable run by the thread driving the stream. `HasPostedTasks()` is what `JoinAndClear` drains | `TaskStream.h:383-399` |
| Does anything else in the loop get exercised? | Yes, and that is the point: `Engine::Run`'s header, the frame-tick/pump ownership, and `JoinAndClear`'s bounded tail all become load-bearing for the suite's own result | `Engine.cpp:139-149` |

## Increments — each one green on its own, none of them half a harness

**A half-migrated harness invalidates every gate**, because all three configurations are gated on this
executable. So the sequence is built so that each commit leaves a runnable suite.

1. **Tally-vs-registered witness first, before touching the harness.** `RunTests()` knows how many
   collections are registered; assert that the number *executed* equals it, and fail out loud with the
   gap when it does not. This is the guardrail proper: it is what turns "the loop stopped pumping" into
   a named failure instead of a shorter green run. It must land while the harness still works the old
   way, so that a regression in it cannot be confused with the migration.
   *Mutant:* run one collection fewer than registered → the new check must fail by name.
2. **Move the shutdown request out of `RunTests()`** into a function the suite calls when its last
   collection completes, leaving today's inline pump path intact. Still green, still identical output.
   *Witness:* the "all N collections passed" line and exit code unchanged in all three configurations.
3. **Post the suite to the base stream and let `Run()` pump it.** `DispatchPostedTasks(&suiteEntry, nullptr)`,
   then `hengine.Run()`. Delete the inline pump from the old path in the same commit, or the suite runs
   twice. Expect the base stream's driven-pass count to jump from ~8 to hundreds — that number is the
   positive control that the loop is doing the work.
   *Mutant — the one this whole item exists for:* delete `Engine::Run`'s `while` header (the real
   regression, commit `a8946ea`) → must be caught by name, by increment 1's check and the zero-collection
   refusal, not by a hand-made witness.
4. **Independent streams must be proven independent, not asserted.** At least one collection that
   demonstrates a worker stream and the IO stream progressing while base work runs — IO already has its
   LogDriver thread. Bound every wait in wall clock via `DriveUntil`; never in passes.
5. **Documentation:** `TestMain.cpp`'s comment — which currently documents the *absence* of this
   arrangement — gets rewritten to describe it as fact; `docs/TaskSystemGuide.md` gains the harness
   section; `docs/Test/` pages if the module reference covers the runner.

## Risks to carry, and how each is settled

| Risk | Settlement |
|---|---|
| Nested pump depth: a test's `DriveUntil` re-entering `Update()` from inside a base item could re-enter the suite item itself | The suite item is popped before its body runs, so it cannot re-run itself; verify with an assertion counter in the suite entry that it executes exactly once |
| Shutdown requested from inside a work item | Settled by source above — `JoinAndClear` continues pumping base under its deadline, so late posted work still runs. If anything is left over, the existing abandoned-work report will name it |
| Tally read in `main` racing the suite | The suite's last action is the shutdown request, and `Run()` returns only after `JoinAndClear`, so the tallies are complete before `main` reads them — assert, do not assume: increment 1's check is what makes a truncated tally impossible |
| `Test::RunTests()`'s own signature is public API (`Engine/Test/UnitTestCollection.h`) | Keep the name, change what it does; a `Prepare()`-style split is not needed and would be a second way to do one thing |
| Flakiness from real concurrency where the old harness drove everything itself | This is a *feature* of the change — worker and IO timing stop being hidden. Expect the first runs to surface genuine races, which is what the D2/D3 guardrails exist to catch |

## Gate (unchanged, and now stronger because it is the subject)

```bash
./build.sh Applications/EngineTest -test -debug -dev -release
for C in Debug Dev Release; do .pi/skills/hb-standards/scripts/runtest.sh $C 280; done
bash .pi/skills/hb-standards/scripts/check.sh
```
Success is `runner exit=0  EngineTest: all <N> collections passed` three times with **N equal to the
registered count**, plus 0 mechanical violations and build gate 12/12. Commit before mutating, never
after; stage paths by name; never push.
