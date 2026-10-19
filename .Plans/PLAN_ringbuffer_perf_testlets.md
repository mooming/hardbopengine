# PLAN — Ring-buffer performance testlets: compact in EngineTest, full in Examples

Owner decisions that set this shape (2026-10-16):

| Question | Decision |
|---|---|
| Where do benchmark measurements run? | As **`Testlet`s**. The *only* difference between EngineTest and the benchmark app is **which testlets each registers** |
| Which set does each binary run | EngineTest = **Compact** (must stay cheap). `RingBufferBenchmark` = **Full** |
| Where does the benchmark app live | Moved out of `Applications/` into **`Examples/`** |
| How does the app get testlet code in Release | Build it with **`-release -test`** (perf testlets stay under the `__UNIT_TEST__` convention; a Release engine carries no bench code) |
| Expected verdicts | **MPSC: atomic beats mutex.** **MPMC at two producers: mutex beats atomic.** Both become reported verdicts |

## 1. Facts the design rests on (measured, this tree, Apple 12-CPU: 8P+4E)

| Cell (ns/item, min-of-rounds) | atomic | mutex | ratio |
|---|---|---|---|
| MPSC saturated, 1 producer | **15.5** | 108.5 | 7.0× atomic |
| MPSC saturated, 2 producers | **41.1** | 104.1 | 2.5× atomic |
| MPMC saturated, 2 producers, 2 consumers | 140.1 | **77.5** | 1.8× mutex |
| MPMC saturated, 4 producers | 205.2 | **160.8** | 1.3× mutex |
| MPMC saturated, 16 producers | **207.5** | 301.0 | 1.5× atomic ← **flips** |

Two consequences that are not optional:

- The MPMC expectation is **true only below about eight producers**. `MPMC: mutex faster` must therefore be asserted at
  a pinned shape (2 producers, 2 consumers, capacity 64, no arrival gap) and must never be stated as a general law. The
  flip at 16 producers is in the design record, and the Full sweep keeps the whole curve so a reader can see it.
- The machine's own disagreement between two *identical* cells was up to 6.4% (saturated) and 0.2% (sparse). A verdict
  margin smaller than that is not a verdict.

## 2. Target shape

```
Engine/Test/PerfHarness.h/.cpp        measurement only, no container names
  RotateArms / MinOfRounds / MedianOfRounds / ControlSpread / VerdictFor / ReportLine
        ^                            (compiled with __UNIT_TEST__ by Engine/Test/CMakeLists.txt already)
        |
Engine/Test/RingBufferPerf.h/.cpp     two TestCollection classes, ring names appear here
  RingBufferPerfTest              -> 2 compact testlets   (EngineTest)
  RingBufferPerfBenchmarkTest     -> 7 sweep testlets      (Examples app)
  RingBufferMPSCFetchAdd          -> file-local, still not engine API
        |
   registered by
   +----------------------------------+----------------------------------+
   | Engine/Test/UnitTestCollection   | Examples/RingBufferBenchmark/     |
   | RegisterSuite() adds the         | TestMain.cpp: RegisterPerf…();    |
   | compact collection               | for(i) RunTestlet(i); Finalize(); |
   +----------------------------------+----------------------------------+
```

Why the perf testlets live in `Engine/Test/` and not beside the containers: `Engine/Test/CMakeLists.txt` carries
`add_compile_definitions (__UNIT_TEST__)`, so that one target is where test-only code can exist without the engine
libs needing `-test`; and the ring buffers are **header-only templates**, so the perf source needs no symbol from the
Container library. `Engine/Container/*.cpp` keeps the *correctness* testlets exactly where they are.

Why the app drives testlets directly instead of through `hbe::Engine`: `TestEnv::RunTestlet` / `Finalize` are public,
so a nine-line loop replaces engine and task-system startup. A timed region must not include a task system whose
budget it is also testing — and the app then links `Test Container Config Log Core Memory …` only, not `Renderer`.

## 3. Checklist (each step verified before the next)

| # | Step | Verification |
|---|---|---|
| 1 | `Engine/Test/PerfHarness.{h,cpp}`: rotation over arms, min/median per arm, control spread, verdict with margin, one report line per arm | built in Debug/Dev/Release; a self-check cell compares two identical arms and demands the verdict be *inconclusive*, not *faster* |
| 2 | `Engine/Test/RingBufferPerf.{h,cpp}`: 2 compact + 7 full testlets; every measurement cell checks exactly-once inside itself (carried over from the app) | `EngineTest` compact cells PASS; app full sweep reproduces the §1 numbers within the control spread |
| 3 | Registration split: `RegisterSuite()` gains the compact collection; new `Test::RegisterPerfBenchmarkSuite()` registers only the full one | `EngineTest` reports +2 testlets and its wall time grows by less than 0.5 s; app runs only the sweep |
| 4 | Move the app: `Applications/RingBufferBenchmark` → `Examples/RingBufferBenchmark`; new `Examples/CMakeLists.txt`; root gains `add_subdirectory (Examples)`; `Applications/CMakeLists.txt` reverted; the old 581-line harness deleted once its measurement code has moved | `./build.sh RingBufferBenchmark -release -test` builds; plain `-release` prints the "no perf cells compiled" refusal like EngineTest does |
| 5 | Standards + docs: pages for the two new classes and every declared name, design-doc §bench/§harness rewritten to the new home, `docs/BuildSystem.md` records the move and the `-test` requirement, `docs/index.html` counts | `layout.py`, `blank_lines.py`, `includes.py`, `comments.py`, `htmlcheck.py`, `docs_coverage.py check-file` clean; `check.sh --staged` 0 grep failures |
| 6 | Three-configuration gate + `EngineTest` | Debug/Dev/Release build; `EngineTest` all collections pass; app sweep re-run once and its table pasted into the design doc |

## 4. Report policy (owner decision 2026-10-16: information, not Error and not Warning)

A performance cell **never** calls `ReportError` or `ReportWarning`, so its collection reports SUCCESS whatever the
numbers say. What a cell reports instead is a **grade**, best-to-worst, printed beside the evidence:

| Grade | Condition | Meaning for the reader |
|---|---|---|
| `GREEN` | the expected design won, **and** by at least the expected margin, **and** the two same-code controls agreed within that margin | this machine agrees with the guide |
| `AMBER` | the expected design won, but by less than the expected margin | the direction holds, the size does not — look at the numbers |
| `RED` | the expected design did **not** win on this machine | the guide is wrong here, or the shape being asserted is not this machine's shape |
| `NOISE` | the two same-code controls disagreed by more than the expected margin | no grade is supportable; the machine cannot answer this finely |

`NOISE` is a qualifier and not a substitute: a `NOISE` cell still prints the ratio it measured, because the reader needs
the measurement more than they need a colour the machine cannot justify.

| Expectation asserted | Margin | Measured here | Expected grade here |
|---|---|---|---|
| MPSC: atomic faster than mutex, 1 and 2 producers, capacity 64 | ≥ 2.0× | 7.0×, 2.5× | GREEN |
| MPMC: mutex faster than atomic, 2 producers 2 consumers, capacity 64 | ≥ 1.2× | 1.8× | GREEN |
| MPMC at 16 producers (Full sweep only, reported as the flip) | no expectation — reported | 1.5× the other way | printed with a note, never graded |

Statistic: **min-of-rounds**, not median — the question is "can this design be faster", and a scheduler hiccup must not
manufacture a better colour. Compact cells: 5 interleaved rounds, fresh queue per cell, arm order rotated each round.

Rendering: the grade is an `EPerfGrade` computed in the harness, so both binaries report the same thing from the same
code. `GREEN`/`AMBER`/`RED`/`NOISE` are always spelled as words; ANSI colour codes are emitted only when a colour mode is
switched on, which the Examples app does and EngineTest never does — EngineTest output has to stay grep-able and diff-able.
The engine has no colour or TTY facility today (`grep` for `033[`/`isatty` in `Engine/` finds nothing), so the terminal
check is five lines inside the app rather than a new OSAL API.

## 5. Scope guards

- No change to the four containers, their correctness testlets, or their API pages.
- The `fetch_add` arm stays benchmark-only, in `Engine/Test/RingBufferPerf.cpp` as a file-local type.
- No condition-variable arm and no spin-cap default change (still open items in the design doc).
- `WindowExample` missing from `check.sh`'s target list is pre-existing debt; untouched unless asked.

## 6. Risks

| Risk | Mitigation |
|---|---|
| Wall-clock results are machine-dependent | they are a report and not an assertion, so the risk is a reader trusting a colour: mitigated by printing ns/item, the ratio, the margin and the control spread on the same line, and by `NOISE` whenever the machine disagrees with itself |
| Compact cells inflate the regression suite | budget ≤ 0.5 s for the two cells, verified by timing the suite before/after (step 3) |
| Testlet retention ceiling (64 KiB) refused by thread-heavy full cells | pass an explicit ceiling at the registration site **only** if measured to need it, and record the number in the design doc |
| `-release` without `-test` builds an app that measures nothing | the app refuses loudly and prints the rebuild command, copying `EngineTest/TestMain.cpp`'s existing `#else` branch |
| Two report formats (compact/full) drift apart | one `ReportLine` in the harness; both collections print through it |
| A performance regression can no longer fail the suite (owner decision, information-only) | the grade leads every report line so Amber/Red/Noise are visible in a diff of a run, the Full sweep localises a regression, and the numbers stay in `docs/design/FixedRingBufferQueues_Design.html` so a grade moving across commits is reviewable material |

## 7. Result — closed 2026-10-17, with four divergences from this plan

| Step | Outcome |
|---|---|
| Harness (`Engine/Test/PerfHarness.{h,cpp}`) | shipped; rotation, `snprintf` report, control-arm noise floor, six-grade `EPerfGrade` |
| Compact cells in `EngineTest` | 3 cells, 0.75 s total, max cell 0.30 s — under the 0.5 s budget in §6 and far under the 3 s per-testlet ceiling |
| Full sweep in `RingBufferBenchmark` | 22 cells, max cell 2.33 s, 0 violations, all 5 graded claims `GREEN` |
| Suite | 66 collections / 409 testlets / 0 failures in Debug, Dev and Release |
| Gates | `check.sh --staged --no-build`: 0 grep failures, 0 advisories; 71 docs pages, `htmlcheck` 0 problems |

1. **The MPMC claim was retired, not met.** §1 predicted `mutex >= 1.20x atomic` at 2P/2C. Measured inside a running
   engine the lock-free ring wins at every producer count, 1.22×–1.86×, so no MPMC cell states a claim; it prints
   `claim numbers only`. The disagreement with the old standalone driver is written up in the design document rather than
   resolved by picking the convenient number.
2. **The app moved to `Examples/RingBufferBenchmark`** instead of staying under `Applications/`, which is where example
   programs already lived but had no `.module.config`, so `Examples/` was absent from the build graph. Adding the two
   config files also put `WindowExample` back in the build and silenced the three FAIL lines `check.sh` had printed on
   every run — §5 said that debt was untouched unless asked; it turned out to be one config file, so it was worth asking.
3. **Colour is not implemented anywhere**, including the app, contrary to §4. `EngineTest` output stays greppable, and the
   app inherits plain text until an OSAL TTY/colour facility exists; the grade is a word, so nothing is lost but prettiness.
4. **Grading is Release-only**, a rule this plan did not have. The same cells measured 4% control spread in Release and
   66% in Dev, so a `Dev` grade would have been a statement about `-O1`. A `Dev` or `Debug` run prints its numbers and
   the words `claim numbers only`.

Open, deliberately: the idle-machine experiment settling §1's disagreement; a condition-variable mutex baseline (the
sparse rows are measuring a busy-poll nobody asked for); the spin default, still 8; `IsSlotFree` as a public predicate,
which the naming plan approved but no caller needs — adding it now would be dead code with a documentation page.
