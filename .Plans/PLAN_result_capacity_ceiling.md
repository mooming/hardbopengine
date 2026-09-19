# Plan — the R21 result ceiling and the dormant dispatch guard

Owner-approved follow-on to `1f7e777`: R21 says the ceiling on declared results is a per-stream constant and
`0` means no ceiling. That makes the guard recorded under R14 — refuse at dispatch any task whose
`NumResults` could never be admitted — writable at last.

## Scope

In: the per-stream ceiling, the predicate that interprets it, a refusal channel on the paths that know which
stream a task is going to, and tests that can fail.

Out (each needs its own commit and none is in this one):
- Reserving room for `NumResults` in a container at admission (R13/R18 full admission).
- `Grow` refusing to cross the ceiling: no production code grows a container yet, so a check there could not
  be reached or proven. Lands with the growth call site.
- R15's fallback for refused work. Until it exists, a refused task does not run and its only signal is the
  error log. This is a consequence of landing R21 before R15 and is stated in the header contract.
- R14 lane closure, and so guard (b).

## Refusal channel, decided on measured blast radius

`TaskStream::EnqueueFifo` / `EnqueuePriority` and `TaskSystem::Enqueue(TIndex, const RangedTask&)` gain
`[[nodiscard]] bool`. `TaskSystem::Enqueue(const RangedTask&)` stays `void`: it pushes to the general queue,
where no stream is known yet, so there is no ceiling to test against; the guard fires when the task reaches a
stream. `[[nodiscard]]` rather than a silent drop because this engine has a documented history of dropped-task
hangs reaching a human, and every existing caller must say "I accept that this may not run" in the diff rather
than in a runtime log line. Six sites: `Logger.cpp:235`, `Test/UnitTestCollection.cpp:151`, four in
`TaskSystem.cpp`'s test section. `Task.cpp`'s four `Enqueue(rangedTask)` calls hit the general overload and do
not change.

## Checklist

Landed as `2e087a0`. 58 collections green in Debug, Dev and Release, runner exit 0 each,
`check.sh --staged --no-build` exit 0 with 0 violations and 0 advisories.

| # | Step | Verification | Done |
|---|---|---|---|
| 1 | `TaskStream`: `DefaultMaxResultCapacitySlots = 0`, member, get/set, `CanAdmitResults` | TC8 reads the default back | ☑ |
| 2 | `EnqueueFifo` / `EnqueuePriority` return `bool`, refuse before pushing, log name + declared + ceiling | refusal line present in the Debug and Release logs | ☑ |
| 3 | `TaskSystem::Enqueue(TIndex, ...)` propagates; 6 call sites made explicit | builds clean in all three configs | ☑ |
| 4 | Default ceiling admits a declaration above every container size | mutation: default 1024 reddened TC8 and nothing else | ☑ |
| 5 | Ceiling 2 admits 2, refuses 3 | mutation: `<=` to `<` reddened TC9 only | ☑ |
| 6 | Zero declaration admitted under a ceiling of 1 | mutation reddened TC10 only; had to be surgical, see the gap below | ☑ |
| 7 | Priority lane refuses too | mutation reddened TC11 only, FIFO stayed green | ☑ |
| 8 | Guard actually keeps refused work out of the lane | mutation: logging but still queueing reddened TC9 only | ☑ |
| 9 | `0` means unlimited, not a ceiling of zero | mutation reddened TC8 only | ☑ |
| 10 | Three-configuration gate, lint, commit, journal, handoff | this commit and the docs commit after it | ☑ |

**The gap, recorded because the plan asked for it in advance.** Step 6's mutation is artificial: it fires only
when the ceiling is exactly 1 and a task declares 0, which no production configuration reaches. The mutation
that would genuinely test the claim - refusing every zero declaration - starves the suite before TC10 runs,
because production tasks declare zero results and the base stream's own test dispatch would be refused. So
TC10's zero case is proven against a contrived predicate, not a natural one, and that is weaker than the
other three.

## Risks stated before writing

- A caller that ignores the return value at `TaskSystem` level drops work with only a log line. Prevented at
  the call site by `[[nodiscard]]`, not by the engine, until R15 lands.
- `maxResultCapacitySlots` is read on the enqueueing thread and written by `SetResultMaxCapacitySlots`. Safe
  when a stream's ceiling is set from the thread that dispatches to it, which is the documented use; not
  safe as a hot reconfiguration from another thread. Contract in `.h`, no atomic — a per-stream pool is not
  thread-safe either, so an atomic here would imply a safety the stream does not have.
- The tests use a worker stream and a deadline, so a refusal that never happens shows as a red assertion
  rather than a stalled suite.
