## Running Tests

Engine tests are defined in `Engine/Test/UnitTestCollection.cpp`. Every test body in the engine sits
behind `#ifdef __UNIT_TEST__`, including the ones the test executable links from the library modules,
so building the suite and running it are two separate things that must both be verified.

### Build and run

```bash
./build.sh Applications/EngineTest -dev -debug -release -test
./build/Applications/EngineTest/<Config>/EngineTest    # <Config> = Debug|Dev|Release
```

`-test` reconfigures with `-D__TEST__ -D__UNIT_TEST__`, and it has to apply to the **whole** tree: the
test bodies live in the library sources `EngineTest` links, not only in `TestMain.cpp`. Building one
target with it and the rest without is how whole modules quietly stop being tested while nothing fails.

### A binary built without `-test` refuses to pretend

`TestMain.cpp` compiles to a `#else` branch under that configuration. It prints what is missing, how to
get it, and returns non-zero. This is deliberate: an empty `main` returning `0` reads exactly like a
passing suite, and that is precisely how the build gate once reported success over zero tests.

The guidance text is written as adjacent string literals, one per line, rather than a raw string, and it
is space-indented so it survives any tab width. The lint rule against space-indented source lines reads
the **file**, so the spaces belong inside the literals — where they are output — rather than at the start
of a source line, where they would be code.

### What the harness does, in order

| Step | Call | Why it is where it is |
|---|---|---|
| 1 | `hengine.Initialize(argc, argv)` | Builds the engine, including the task system the suite runs on |
| 2 | `GetStream(GetBaseTaskStreamIndex()).ConfigureBudget(duration(0.001))` | A bounded allowance on the base stream — see below |
| 3 | `Test::RegisterSuite()` | Registers every collection **before** the run starts, so the expected total belongs to the harness rather than being discovered by the suite |
| 4 | `Test::ScheduleSuiteOnBaseStream()` | Every testlet becomes a task on the base stream |
| 5 | `hengine.Run()` | Drives the suite to completion |
| 6 | executed vs registered check | The guardrail — see below |
| 7 | Heap report, then the verdict | The figures print before the verdict so a failing run carries them too |

Step 4 through step 5 are the only thing that can finish the suite: each testlet is a task that posts the
next one, so the run loop is what carries the suite to its end. The last testlet reports and requests
shutdown from inside the run, which is why `Run()` returns with the tallies already settled and no
separate shutdown call belongs in `main`.

Registering before the run matters for the failure case: a suite that never started cannot report a
total matching the nothing it ran, and the expected total has to come from outside the suite to make
that check meaningful.

### Why the base stream gets a bounded budget

The base stream is throttled, and the reason the extra passes are worth it is a future defect rather than
a present one.

Measured on the defect this harness exists to catch — `Engine::Run`'s `while` reduced to a single pass —
the shortfall check refuses it **with or without** this allowance, because each testlet only posts the
next one after it runs. The queue is therefore empty the moment an item is taken, and a pass that quits
early cannot reach far. The chain of testlets is the load; the budget allowance is the insurance.

`CPUBudget`'s zero means unlimited, and `CanTakeWork` reads the budget once per pass. So if the driver is
ever changed to re-scan the queue after every completed item, an unbounded pass would drain the whole
chain at once and make the guard vacuous again — silently, which is how this was missed for five commits.
A bounded allowance keeps that from being possible. Setting it too small costs extra passes and never
correctness, which is the harmless direction, and is why the figure is 1 ms rather than a guessed larger
one.

### The guardrail: registered against executed

```cpp
if (testEnv.GetExecutedTestletCount() != testEnv.GetTestletCount())
```

`registered` is known from before the run and `executed` is what the run achieved, so a loop that stopped
iterating leaves a gap that cannot be argued away. Measured against the same defect — `Engine::Run`'s
`while` reduced to a single pass — the suite reaches only a fraction of its testlets, and this is the
line that refuses it.

### Reading the report

```
EngineTest: global heap over testlet bodies <n> requests, <bytes> bytes, over <k> of <m> testlets;
            whole process <n> requests, <bytes> bytes
EngineTest: all <collections> collections passed (<testlets> testlets)
```

Two figures, deliberately. The first pair is what the **testlet bodies** asked global `operator new`
for — the traffic an `AllocatorScope` cannot redirect, because it is outside any scoped allocator. The
second pair is everything the process asked the same entry points for, engine and standard library
included; it is context for reading the first, not a gate. The `over k of m` clause names how many
testlets allocated anything at all.

The report is emitted before the pass/fail verdict so a failing run carries the allocation figures too.

### The per-testlet retained-memory ceiling

#### What the figure does not count

Two limits are known and deliberate. It counts the **global** door only: a testlet allocating through its
`AllocatorScope` draws on `MultiPoolAllocator` banks, which are not in this figure. And a release through
the unsized `operator delete` reports the block size the system heap chose, which can exceed what was
asked for, so retention can read slightly low.

A testlet can declare its own ceiling by using the three-argument form:

```cpp
AddTest("Name", ceilingInBytes, [] (auto& ls) { /* body */ });
```

`TestCollection` measures what the body asked for and what it released, and the retained figure is the
difference at the end of the body. The two-argument `AddTest` applies
`TestCollection::MaxRetainedGlobalBytes`, a default of **64 KiB**; the three-argument form is how a testlet
that legitimately needs more declares it.

A testlet that ends holding more than its ceiling is a **failure**, not a note: the breach is logged and
also pushed into the collection's error list, because whether a testlet passed is decided by that count,
and a guard which only prints is a comment with a number in it. The message says which ceiling was
breached — "a testlet may hold" for the default, "this testlet declared for itself" for a declared one —
and ends with the instruction that matters: free it, or raise the ceiling with a reason that survives
review.

### Failure modes this harness has actually caught

| Symptom | Cause |
|---|---|
| `built WITHOUT __UNIT_TEST__, so this binary contains no tests` | Built without `-test`; nothing was verified |
| `only N of M registered testlets ran` | The engine stopped driving the suite; see the guardrail above |
| `N test collection(s) FAILED` | A testlet assertion failed; each one prints its own message and level |
| `retained N bytes of the global heap at the end of its body, over the M byte ceiling` | The testlet held more than the
memory ceiling it declared for itself (or than any testlet may hold, where it declared none) |
