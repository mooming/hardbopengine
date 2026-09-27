# Corrected again — why per-collection granularity cannot make the loop's iteration load-bearing

Follow-up to the "corrected after `54efe90`" section, written after trying to implement step 1.

## The blocking fact, read from source

`CPUBudget`: **an allowance of zero means unlimited**, and `CanTakeWork` reads it **once per pass**
(`CPUBudget.h:22,33,50,71`). The base stream runs with the default, which is zero. So a single
`TaskStream::Update()` keeps taking work **until the stream is empty** — a pass is not a bounded unit.

Consequence: posting the suite as 59 items instead of one changes nothing about whether one pass can
finish the run. **One pass can carry all 59**, exactly as today's single item carries all 59 (measured:
8 driven passes for the whole binary). Granularity was the wrong lever; the pass has no capacity.

## Why mutant M-A survives every witness so far

Traced in the `while`→`if` build, statement by statement:

```
Run() → taskSystem.Update() → base item taken → suite body runs all 59 collections
      → suite calls Engine::ShutDown() → JoinAndClear() runs INSIDE that Update()
      → Update() returns → loop header re-checks IsRunning() → false either way → Run() returns
```

The healthy build and the defective one differ only in a header test that is false in both. The
shut-down request comes from inside the suite's own single pass, so the loop never had a second pass to
make. **In the EngineTest target, as the suite is shaped now, the defect is not observable — not
imprudently un-witnessed, but genuinely unobservable.** That is the real reason three designs and two
mutants failed to catch it, and it is worth writing down before any further witness is built.

## The one lever that is not timing-fragile, and what it costs

Force the run to need more than one pass, then the existing rescue flag makes the loop's iteration
verifiable:

1. Give the base stream a **bounded per-pass allowance in the test target**, set from `TestMain` before
   `Initialize` — a small CPU-time duration (order 1 ms), not an item count, because the API has no item
   count. With the 59 collections costing far more than 1 ms in total, **one pass provably cannot finish
   the suite**, so a healthy run needs many passes and a dead loop yields exactly one. The threshold is
   then only wrong in the harmless direction: too small means more passes, never fewer.
2. Keep the existing per-item provenance check (`54efe90`) and add a harness assertion that the run
   spanned **at least two** engine-loop passes, reported by `Engine::Run` itself rather than inferred.
3. The rescue flag then does its job: if a shutdown drain finishes what the loop did not, that is a named
   failure rather than a green run.

Costs, honestly: the base stream is throttled for the whole test binary, so collections that rely on
base-stream throughput get slower (bounded in wall clock by `DriveUntil`, which already governs every
wait, so no test should break — but that must be measured, not assumed); the suite's wall time grows by
one `Platform::Sleep(1)` per pass; and the allowance value is a duration, so it is a tuning knob with a
comment rather than an exact quantity.

## Rejected, with the reason

* **Post collection items from a non-base thread** and hope the first pass misses them — the take loop
  drains whatever is queued when it looks, so this races the drain instead of bounding it.
* **Assert an exact pass count** — a `sleep` in the loop body satisfies any number. Assert a minimum of
  two, which is the property the defect actually violates.
* **Source-shape lint on the loop header** — pins a spelling; already tried and reverted as dishonest.
* **Leave it unverified and rely on review** — the position I held for five commits, and it is what let
  `a8946ea` ship.

## Open decision for the owner

Either (a) land the throttle + ≥2-pass assertion in the **test target** as above, accepting a throttled
base stream for every collection in that binary; or (b) keep the test target untuned and put the
iteration witness in a **smoke-run target** that has real frame work and no window (the headless target
question asked earlier), which costs a build target but leaves the suite's timing untouched.

Recommendation: **(a)**, because it keeps the witness inside the thing the gate already runs, and its
failure mode is confined to the verification binary.
