# Journal

## Outcome delivery without a container (R30, lands on top of D1 `2ea4d72`)

I came to the owner with a designed completion list - mutex over a pre-allocated array, capacity from the registry,
overflow proof written out (R23e) - and one sentence killed it: *"there's not task completion container. Since task
itself has a data for its result."* Same stroke as R23 (result buffer) and `ac496e6` (`ResultContainer`). The header
I had written was deleted before it reached a build; nothing referenced it. What is left is one function,
`TaskSystem::DispatchSuccessor`, run by **the thread that closed the join**: copy the finished task's 128-byte packet
onto the successor's record, push one work item onto the stream the packet's destination byte names. Section 6.1 drew
that arrow producer-to-B-in-the-first-place; routing it through a buffer the base stream drains was my addition.

Four refusal paths, each logging the pair that caused it, because a half-filled routing field shows up as a task
that never runs and therefore as a hang far downstream: no stream named, stream not in this engine, successor no
longer tracked (released while its producer ran), successor reserved no subtask under R29. No successor recorded is
the common case (R16) and stays silent. Chains are not special - dispatch queues rather than runs, so A wakes B
wakes C for one push per link and no stack growth.

**Three measurement/ownership errors of mine, all found by the harness rather than by luck.**

1. The wall-clock runner reported **exit 0 when the child died by a signal** (`exit($? >> 8)` discards the signal
   number). Three mutations that killed the process - out-of-bounds stream index, released successor dereferenced,
   reservation check removed - were being read as green. Fixed to `128 + signal`, self-proved against a child that
   aborts itself, and every mutation verdict re-run. Any earlier conclusion of the form "exit 0 therefore alive"
   was only as good as this runner; the gate itself was safe because it also required the summary line.
2. The mutation harness restored sources but not the binary, so the "clean" run taken right after the matrix
   executed the last mutant's binary and appeared to crash on its own in a test that had passed. Diagnosis cost one
   rebuild: after `restore`, always rebuild before believing a clean result.
3. TC10 was written with the receiving role given to the wrong runnable (`RunStageTwo` counts, it does not read the
   packet) and the expected byte copied from the wrong producer (`0x5A` where stage one writes `0xA5`). It failed,
   and the failure message - kind 0 byte 0 rather than 165 - was the honest clue that nothing had been read at all.

**Mutation table (8, each with its own attribution).** Successor never queued / packet never copied / loop never
dispatches / routed to the base stream instead of the named one: each turns TC10 and TC11 red, naming its own
mechanism. Invents-a-stream-when-none-named: TC13 red. No stream bounds check: process died (trap, 133) inside
TC14, which is `Array::operator[]`'s own `FatalAssert` catching the wild index. Released successor guard removed:
**SIGSEGV (139)** inside TC15. Reservation check removed: trap (133) inside TC16, with R29's over-production assert
printing first - the guard exists to keep that assert unreachable. Clean tree afterwards: exit 0, all 59 collections
passed, twice.

Suite stays **59 collections** (7 delivery tests went into `TaskSystemTest`). R23c now owns only the budget window,
R23e is struck, and R23d's reason narrowed - an outcome no longer waits on the base thread, which is worth noting
because the unit suite blocks that thread for the better part of a minute and every delivery in this repository
would otherwise have been frozen behind the test run. Left open on purpose: the one work item the engine makes for a
successor carries the default priority, since making it is the engine's job and R18 bands are declared by whoever
queues - closing that needs a priority field the record has no room for (R28 priced the room).

## Successor, join counter, and a declare-first rule (D1, in progress of the three remaining items)

R28 priced the successor field, so this commit pays it: `Record` gains a `TaskID successor` and is padded to **256
bytes** (four cache lines, default table **1 MiB**). Routing is registry state, not task state - R9's sentence read
literally - and `Task` itself stayed at **176 bytes** because the counter R29 needed went into padding it already
had at offset 26. Accessors are `TaskRegistry::SetSuccessor` / `GetSuccessor`, wrapped by the task system.

**R29 is the part with teeth.** Handing out a work item used to *increment* the count, so a producer that queued
item 1 before generating item 2 could have a worker finish item 1 against `numSubTasks == 1`: the task reports
itself done on one item of three and anything waiting is woken before results exist. Now `ReserveSubTasks(n)`
declares the join before the first item is visible, `GenerateSubTask` counts nothing, and
`ReportFinishedSubTask` returns whether this call closed the join - on an **equality**, because a threshold makes
every finisher past the last one a winner and a successor would dispatch twice. Nine producer sites were updated
(7 in the suite, the logger's drain task, the suite's own task). `Task::Start` was deleted: dead, and it divided by
the member `numSubTasks` that the caller has not set yet, so a fresh task divided by zero.

**Two of my checks were weak, and mutation proving is what said so.** `SetSuccessor`'s refusal of an untracked task
was tested by recording a successor on a released ID and reading it back - green with the guard deleted, twice.
The first version was blind because the read path (`Find`) returns nullptr for a released task whatever the field
holds; the second was blind because `Create` clears the field when a record is reissued, which fixes the damage the
refusal was supposed to prevent. Only an attack that lands on a record somebody is *still using* separates the two
builds: an ID naming a live record at the wrong generation. That is the alias `TaskID` exists to catch, so the
final test is also the one that tests R7 rather than the accessor. Recorded in the test's own comment, because the
next person will otherwise write the same weak version.

**Mutation table:** successor never stored -> the read-back test red; `Create` not clearing -> the recycled-record
test red; live-record check removed -> the stale-generation test red (after two fixes); join rule back to `>=` ->
the join test red. The fifth mutation - deleting the generated-item counter - stayed **green**, and that is honest
rather than a problem: its only consumer is the over-production assert, which aborts and so is stated rather than
tested. One test also caught the price change on its own: `ResultPacketTest`'s layout test failed the moment the
record grew, which is exactly what a pinned price test is for.

Suite stays **59 collections**; Debug/Dev/Release green; the two stale `(void)` casts the guide audit named
(`Logger.cpp:246`, `UnitTestCollection.cpp:166`) are gone with this, since `Enqueue` cannot fail since `ac496e6`.

## The budget window landed, and it disproved a conclusion in our own design doc (2026-09-20)

**Two decisions and one correction.** `R25`: `RangedTask` is deleted in the commit that lands `ParallelFor`, not
before - it is not a caller convenience but the item type every queue stores (each stream's FIFO and priority
lanes, the shared general queue, and `TaskQueueItem`), so deleting it needs a replacement *and* a producer in the
same commit; measured blast radius 15 `GenerateSubTask` sites in 6 files plus 19 enqueue/dequeue sites, all in
nine production files, and doing it now means rewriting them twice. Price of waiting: 128 bytes per queue slot
against 40 for the `{TaskID, start, end, priority}` item R11 describes. `R26`/`R27`: the base-stream pass lands as
the **budget window only**, because delivery is defined as enqueuing a successor and no successor field exists - a
pass that drained completions with nothing lawful to enqueue would be a pass that finds work and discards it.
Window width is `time::GetBaseFramePeriod()` and no new tunable was invented: `ConfigureBudget` already states an
allowance per base frame period, so a second number could only drift from the first.

**The correction.** A census in `docs/TaskSystemRedesign.md` concluded an allowance "gates nothing today" because
`TaskStream::MayTakeNewWork` has zero production callers. Wrong, and I have the evidence: `StreamDrainPolicy::
ChooseLane` compares the same allowance against the round's accumulations and returns `None` when they reach it
(`StreamDrainPolicy.cpp:40-44`), and `EndRound()` - the only clearer - had no caller. So the truth was the opposite
of inert: **one call to `ConfigureBudget` and one spent allowance shut a stream's own lanes for the life of the
process**, with the work items still sitting on the lane. I found it by falling into it - a test of mine configured
1 ms on a worker, queued a task to that worker, and hung the process for 5 min 5 s until it was killed, because an
earlier test had already charged that round 219 ms. R2 as originally written was right; the round that produced
the census found the sibling defect from documentation two rounds earlier and missed this one by looking for
callers of the wrong predicate. `EndRound` now has one production caller, and removing it alone still makes a
queued task never run.

**What the mechanism refuses to be measured by.** Four designs failed before one held, each for a reason worth
keeping: reading the charged CPU gave **-222,014 us** (the baseline the test took was itself zeroed by a window the
test advanced); waiting for a charge instead gave **0 us** for a task that had just spent 222 ms (the charge lands
after the task reports itself done, and a pending window advance erases it as it lands); advancing windows while
looking for a refusal gave **0 refusals in 5 s** (advancing windows is precisely what un-shuts a stream, so the
shut state cannot exist while a test polls for it); and sleeping for the spent state hung, because a test runs
*inside* a work item on the base thread, which is the only thread that can advance a window. What holds is
behavioural: queue a task, advance windows **only until it is confirmed running**, then leave them alone; count
things a reopen cannot erase - tasks that ran, refusals recorded. Observed on one run: 3 windows advanced by the
base stream's own loop before the test ran, **14 refusals while shut, resumed on its own thread at window 5.**
That is a rule for anyone testing this subsystem again, and it is written in the design doc.

**My errors, five of them.** The hang above was mine and reached a working tree before I caught it. A trailing
cleanup loop I wrote as `while (now < now + 1s)` is an unbounded loop wearing a bound - fixed to a real deadline.
Two string literals lost their closing quote to my own edits, and one of them was only caught by the compiler.
`-Werror,-Wunused-private-field` caught a `refusalLogged` bool I had made redundant with the counter that already
answers "is this the first one", and `-Wunused-but-set` caught a busy-work variable; the loop is now `volatile`,
which is also what keeps a CPU-burning test honest in Release. The `-Wunused-function` gate has now caught three
of my dead helpers across this session - it is earning its keep.

**Practice adopted from the owner:** `EngineTest` is now always launched under a wall-clock limit
(`/tmp/hbe/runtest.sh <Config> [seconds]`, a perl fork+alarm since macOS ships no `timeout`), because a hang that
runs until something external sends SIGTERM returns exit 143 and reads like someone else's kill. Exit 255 plus a
"wall clock limit" line now means our own hang, which is a different diagnosis entirely.

**Suite stays 59 collections.** Debug, Dev, Release green. Five mutations, one red test each: no pass call in the
loop, no reopen block, no acquire gate, `EndRound` removed alone, no window counter. `Task::Start`'s divide-by-zero
is already recorded and stays unreachable - it has no caller, and R25's commit deletes the protocol that contains
it. Nothing pushed.

## Documentation only: the guide was rebuilt against HEAD, and it found two defects the code cannot see (2026-09-20)

The owner put the budget window on another agent and confined this session to documentation. Nothing in
`Engine/` was touched here. Two files changed: `docs/TaskSystemGuide.md` rewritten, and a new section appended
to `docs/TaskSystemRedesign.md`.

**R26 recorded: the pass is split by dependency, budget window first.** The owner chose it on a census, so the
census is in the document with its line numbers - `CPUBudget::Reset()` has no production caller,
`TaskStream::MayTakeNewWork()` is called only from `__UNIT_TEST__` (`TaskSystem.cpp:581`, `:622`, `:629`) and never
from `RunLoop`, `StreamDrainPolicy::EndRound`/`IsRoundExhausted` have no caller at all, and a successor task exists
only as a name in this document. Reading: an allowance gates nothing today, and the window has to exist before the
acquire gate can be wired at all, or wiring it produces R2 on the first spend. Triage and delivery move to the
successor round because delivery *is* "enqueue a successor that embeds the outcome" (section 6.1, R9) and there is
no successor to enqueue. The rejected alternative - reading a `{TRunnable, void*}` pair out of the payload to have
something to run - is recorded as rejected with its reason, which is R19's reason against a one-field descriptor.

**The mechanism I proposed under R26 is labelled as mine, not the owner's.** The pass signals a per-stream atomic
flag and the stream reopens its own budget and drain round on its own thread. The base-thread-walks-and-calls-`Reset`
shape is rejected in writing, because `Reset` writes `isMeasuring` and `taskStart`, which are plain members of a
class documented as single-owner: resetting from another thread between the owner's `BeginTask` and `EndTask` either
loses a charge or bills the thread's whole life to the budget - R2 introduced by the fix for R2. The accepted cost,
that a reopen lands on the owner's next iteration, is the overshoot rule rather than a lag.

**The guide was five months stale, and rebuilding it against source found two defects.** `docs/TaskSystemGuide.md`
was last touched `9cb5555` on 2026-04-23; it taught stack-`Task` + `Start` + `BusyWait` as the primary usage, which
against HEAD means a null `TaskID`, so every work item is dropped by every stream with a warning and no work runs.
Rewritten to 335 lines from 223, covering registry identity, the embedded packet and its three constants, the real
affinity polarity (raw `0` means set, one index spread across 8 bits per word, base and IO refused on first contact,
a `NonStreamIndex` thread taking nothing forever), the priority direction where `0` now means least urgent and the
default is 128, `Engine::Run` as a yield loop with no frame barrier, registry sizing parameters, and a "do not do
these things" table. Two findings written into the design document rather than fixed:

1. `Task::Start` divides by the task's **member** `numSubTasks` instead of its own `numberOfSubTasks` parameter, and
   `GenerateSubTask` - the only writer of that member - runs after the division. A fresh task divides by zero. It has
   **zero callers** in `Engine/` and `Applications/`; the three `.Start(` hits are `TaskStream`, `TestCollection` and
   `TestEnv`, different classes. So nobody has crashed, and the only thing that ever ran that arithmetic was a guide.
2. `ac496e6` claims all five `(void)` casts went "with the refusal they annotated". Three did. Two are at HEAD:
   `Engine/Log/Logger.cpp:246` and `Engine/Test/UnitTestCollection.cpp:166`, and `git log ac496e6..HEAD` on those
   files is empty - the message was wrong the moment it was written. Casting `void` to `void` harms nothing; asserting
   in a repo record that code was removed when it was not is the harm.

That makes **two verifiable claims false in one commit message**, the attribution of errors in `28a0cbc` and this
cast count. The lesson goes in the design document, not here: a subagent's prose is not evidence, and the only
record of what landed is what a grep returns from the tree it claims to have produced.


## The result moved into the task, and the container that held it was deleted (2026-09-19)

**What landed**, in order: `2b30818` one meaning for "base" - stream 0 is `Base`, the thread driving `Engine::Run`
is `EngineLoop`, `baseTaskThreadID` became `engineLoopThreadID`, and a thread that is not a stream reports
`NonStreamIndex` instead of claiming to be stream 0; `bc0342e` the result packet embedded in `Task` (R23's first
commit, no behaviour change); `ac496e6` the result container, its sizing chain and the refusal path deleted.
**Suite: 58 -> 60 -> 59 collections** (`TaskRegistryTest` and `ResultPacketTest` in, `ResultContainerTest` out).

**Four design rounds in one session, and the owner's instinct drove three of them.** Asked what "fold" meant, the
answer was: the design names it as a step and never says what it does - so the owner retired the word, then
rejected the refcounted janitor model as too complex, then settled the shape: **one 128-byte packet per task,
embedded in the record, destination stream in R5's 8-byte header, filled by the producing task**, and the base
stream triages and enqueues a deliver work item. Bigger results are the task's own problem. That retired **eleven
rows** - R3, R4, R11-R15, R17, R18, R20, R21 - and both guards I had been carrying. `R19`'s principle survived
vindicated: the `TaskDescriptor` was never invented.

**Measured, not estimated, at every pricing step.** `Task` 48 -> 176, record 64 -> **192** = three cache lines (so
every record is line-aligned), default table 256 KiB -> **768 KiB**. `RangedTask` measured 128 bytes. Peak RSS of
the test binary fell **12.3 MiB** on deletion - stated with its caveat: RSS counts touched pages, the 24 MiB pool
reserve was never fully written because there were zero producers, and the deleted test had its own allocations,
so the clean claim is the grep: nothing asks a pool for result storage any more. Layout is enforced by
`static_assert`: widening the payload or dropping the alignment fails the build and names the decision. Padding
the packet to its own cache line was measured and **rejected** - it makes the record 208, which is not a multiple
of 64, so records stop being aligned; the intuition was wrong and the measurement caught it.

**A live defect still unfixed, now the next commit.** `CPUBudget::Reset()` has no production caller: a stream that
spends its allowance **stops dequeuing permanently**. R1/R2 say the base-stream pass reopens every window; that
pass is the next commit, and the completion list and triage ride with it.

**Five errors surfaced in this round, all caught before or by the tools, none by luck - and four of them belong to the session that ran before this one, not to the run that deleted the container.** The attribution matters: `ac496e6` and this entry both claimed all five as their own, and `1608855`, `d6f6f1b` and `2b30818` already carry them. Evidence for the reassignment is in item 2.
1. *(earlier session, recorded in `2b30818` and `d6f6f1b`)* A test called `TaskSystem::Dequeue` to inspect the general queue - a queue shared with every other
   collection. It took `RHICapabilities`' work item and discarded it, and that collection aborted with "is not
   allocated by this allocator" one log line later. The failure looked like a memory bug in a subsystem the test
   never touched. Deleted, proven by removal, and recorded as a trap: that queue has no test seam.
2. *(earlier session)* The edit helper `open(path,"w").write(fn(open(path).read()))` truncates before reading - Python evaluates the
   truncating `open` first - which emptied `TaskSystem.cpp`, all 1068 lines. Caught by `git diff --stat`, restored
   from HEAD, helper rewritten to write a temp file and rename over it. **This is why items 1-4 are reassigned:** the file that got emptied was 1068 lines, and the state handed to the deletion run was already the restored file with six ceiling tests cut (20,525 bytes, ~630 lines); the pre-handoff backup `/tmp/hbe/orig2/TaskSystem.cpp.preC3c` is the 1068-line file. A run starting from 630 lines cannot empty a 1068-line file. Item 4's second half - a stream allocator taken by the same kind of cut - is the one detail with no counterpart in the earlier session, so it may be the deletion run's own.
3. *(earlier session, during `bc0342e`)* `clang-format` on `CMakeLists.txt`: it does not know CMake and joined line 1 into
   `cmake_minimum_required(VERSION 3.12) project(Core)`. Configure died and the build exited 1 with **zero**
   `error:` lines - a failed configure prints nothing clang-shaped, so grep `FAILED:` and read the tail. All 22
   tracked CMakeLists checked afterwards; only the edited file was damaged.
4. *(earlier session)* The cut of the container members deleted the two lane-enqueue definitions instead of converting them, and
   took the stream's own pool allocator with it - the compiler caught both.
5. *(this run - the only one of the five the deletion run incurred itself)* `git add -A` staged a concurrent agent's untracked docs; unstaged before committing. Related: three documents
   repeated "six `(void)` casts" where the measured count is five - a number counted once, wrongly, then copied
   (`1608855` corrected it).

**Two traps worth keeping.** `thread_local TIndex StreamIndex = 0` with `BaseStreamIndex == 0` made
`IsBaseThread()` true on every thread that never called `SetStreamIndex`, which made the assert in `BuildStreams`
incapable of failing and charged a non-stream thread's contact with a task to stream 0; `TaskStreamAffinity`
guards `bitIndex >= NumBits`, so the sentinel is dropped rather than shifting past the unit. And something else in
this checkout runs `EngineTest`: a Debug run took SIGTERM (exit 143) with a second instance alive - a `143` is not
a defect in the change under test, check for the other instance and rerun.

## The reference goes three levels deep: class pages and a page per method (2026-09-20)

**Decision.** `Array` became `docs/Container/Array/index.html` plus one page per method name, overload families
sharing a page, and `.Plans/AUTHORING_method_and_class_pages.md` is the contract every page follows. The user
chose the shape (one page per class, one per method, overloads together) and then chose **hand-authoring over a
generator** when the measured cost was put in front of them: 435 documented signatures, 381 distinct method
names, ~490 files. That is a deliberate trade — future style changes cost hundreds of edits instead of one — so
it is recorded here rather than rediscovered later.

**Two rules fixed before volume, because they are cheap to fix now and ruinous to retrofit.** Examples come from
real call sites found by grep, with the source file named, and an illustrative snippet is labelled as such;
fabricating a caller is how a reference starts lying. And a page about a getter is allowed to be six lines — the
alternative is 130 accessor pages padded into noise, which is the same stub problem that made the old guide
untrustworthy.

**Reading the header instead of the guide paid for itself inside the first class.** `Array::Resize` with a
negative count is not diagnosed: the allocation-failure branch is guarded by `newSize > 0`, so a negative value
walks past it, destroys every element, returns the buffer, and stores the negative length — corrupt but not a
crash, which is what makes it slow to find. And `operator[]` guards with `FatalAssert`, active in every
configuration, while `Resize`'s failed allocation uses `Assert`, Debug and Dev only, with its early return
outside the macro so a Release build stays safe and goes silent. My first draft asserted the opposite about
`FatalAssert`; the site's own Core page caught it. `DefaultAllocator::allocate` taking `std::size_t` is what
makes the negative-`Resize` case a landmine rather than a rejection.

**Also decided:** `TIndex = int` is documented as load-bearing rather than a typo — `IsValidIndex` tests
`>= 0`, so the signedness is the point, and the cost (a `size_t` loop counter comparing signed against unsigned)
is stated next to it.

**Method of work.** 490 files cannot be typed by one hand in one sitting, so authoring fans out to subagents per
module against the contract file, piloted on Config and Resource before the remaining ten modules are released —
a bad pattern should cost two modules, not twelve. The orchestrator validates and commits; agents write only
inside their own module directory.

**Pilot result (Resource): the contract, not the output, needed fixing.** 20 pages, validated clean by an
independent parse, and the voice matches the exemplar. What it actually produced that matters is a set of
corrections found by reading `ResourceManager.cpp` instead of the previous page: `RequestLoad` and `Load` are
declared in `ResourceManager.h` with **no definition anywhere in the repository**, so a call is a link error — I
greped the whole tree to confirm it before believing it. `PostUpdate` and `RequestTasks` have empty bodies and
zero callers, `referenceCount` is initialised and never read, and `Buffer(genFunc)` calls the generator in its
constructor despite the comment claiming lazy initialisation. Five claims the old page asserted as behaviour were
not behaviour. A verbatim-shingle test reported 10 of 14 old sentences missing from the new tree; reading them
showed 5 deliberate corrections of false claims and 5 paraphrases — which is why the check that matters is
whether the *concept* survived, not the wording.

**Four contract faults surfaced, all real, all fixed:** no rule for a class whose members are aliases only, no
row kind for a private nested class or a member alias template, no case for API that is declared but never
defined, and the footer example linked a per-class anchor a rewritten index would no longer carry. The last one
became a standing rule — **a module page rewrite keeps `id="<class>"` on the classes-table row**, because other
modules link to `../OSAL/index.html#window` and a rewrite that drops it turns their working links into 404s that
no local build notices. Inbound fragments get grepped before a rewrite, not after.

**Pilot result (Config) — 34 pages clean, and it corrected the pilot's own corrections.** It independently found
that the old page claimed `__DEBUG__` gates `FatalAssert`, the same error I had made in `Array`'s first draft and
for the same reason: the guide, not the header. Verified against the code: `ConfigParam::lock` is a `std::mutex`
declared once and never locked; `EngineConfig::GetMaxSystemMemoryTarget` exists only as a declaration, the third
API in two modules found callable-in-name-only; and since every config parameter in the repository is `uint8_t`,
`size_t` or `float`, `GetBool`/`GetInt` can never match a registered parameter. It also found something nobody had
claimed before: `StaticString()` interns to `"None"` rather than null, so with `ENGINE_PARAM_DESC_ENABLED 0`
parameters collide on the key `None` instead of keying on nothing.

**It reported a defect in my exemplar that does not exist** — `resize.html`'s markup parses clean. Its sentence
was genuinely contorted and got rewritten, but a fault report is a claim like any other: 4 of its 5 contract
complaints were right, the 5th was wrong, and had I acted on all 5 I would have "fixed" a file that was fine.
The same lesson ran the other way twice in one day: my own checker also flagged `RendererDesign.html` for
classes it never claimed to use, and `README.md#documentation-policy` for a fragment GitHub generates and no
string search can see.

**HSTL landed, and it corrected HSTL's own module page on four points.** A `std::hash<hbe::HString>` specialization
sits at `HString.h:24` while the module page said the module has no functions at all. "You get `std::vector`'s
guarantees with the engine's accounting" is wrong for the common configuration: `DefaultAllocator` short-circuits
to `malloc` when its allocator id is `MemoryManager::SystemAllocatorID`, so nothing is accounted in that case —
every HSTL page now says the pool is stamped at construction and unaccounted under the system allocator.
`MaxPathLength = 512` in `EngineConfig.h` resolved a figure the old page deferred as a guess, and
`HInlinePathString` has zero call sites anywhere.

**Three of my instruments were wrong, each found by something other than me.** The validator I shipped to every
agent opened each link target before checking it existed, so it crashed with `FileNotFoundError` on precisely the
dead link it was written to detect — a validator that fails by exception is worse than none, because `ALL CLEAN`
stops meaning anything; it now reports and moves on, self-tested against `docs/Container`, where four dead links
are genuinely pending mid-flight. My HSTL brief contradicted contract §9, the agent followed §9 and reported the
brief, and the contract now states that a brief does not override it — the brief is the artifact written last and
read once. And my site-wide check flagged a module page for missing a sidebar heading that only class pages
carry: the third false positive today, all three the same error, applying a rule past the scope it was written
for. Scope the check, then run it.

**Test landed — 21 pages, and it corrected its own module page on ten points.** `LogFlush` is not the RAII guard the
page described: `TestCollection.h:24` makes it a public class with five public fields and `~LogFlush() = default`,
so nothing flushes at destruction — it is a routing token consumed by an `operator<<`. `RunTests` returns before a
single test runs; it enqueues a ranged subtask, and that task is what calls `Engine::Get().ShutDown()`. The
collections live in `namespace hbe`, not `hbe::Test`. `ExecuteTest` and `Report` are private. `GetPassCount` is
never reset and `Start` clears two of five message lists, so "counts" are per-collection and cumulative. And
**my brief was wrong**: I asserted the whole surface is compiled under `__UNIT_TEST__`, when only
`UnitTestCollection.h` and `.cpp` carry the guard — the agent checked, wrote a conditional-build section, and my
instruction would have made the page lie. Brief #2 today that contradicted reality; briefs are the weak link, not
the agents.

**A claim about what compiles was wrong, and a three-line program settled it.** `TestEnv.h:36` reads
`std::make_unique<T>(std::forward(args)...)`, missing the `<Types>`. The page explained this as lvalue arguments
being silently consumed — a plausible-sounding mechanism that is not what happens. `std::forward`'s parameter type
is `remove_reference_t<T>&`, a non-deduced context, so `T` is never inferred: `c++ -std=c++2b -fsyntax-only` says
*no matching function for call to 'forward' / couldn't infer template argument '_Tp'*. The argument-taking form
does not compile at all, and an empty pack expands to no call, which is the only reason sixty call sites — every
one `AddTestCollection<SomeTest>()`, every collection default-constructible — have never tripped it. The defect is
latent, not silent, and the page now says so with the diagnostic quoted. The error was wrong in the reassuring
direction: "silently consumed" invites a workaround, "does not compile" tells the truth.

**So the contract grew section 16: a claim about what compiles needs a compiler.** Grep proves a name exists;
only a compiler proves what it does. Also added: public and private property badges are not interchangeable — a
`private` badge on a public nested guard misstates the interface, the one job a property table has; a module
function carrying a real contract gets a page inside the folder of the entry declaring it, because "see the module
functions" points at a table and the table cannot hold "this returns before any test runs"; and §12 gained an
inbound-link scan, since the outbound check is structurally blind to the one failure §8 warns about. The inbound
scan is negative-controlled before shipping: deleting one `id="buildconfig"` in a copy of the tree produced twelve
`BROKEN` lines across Renderer, Memory, OSAL, Math, Log and Engine. A scan that reports "all resolve" without
proving it can report otherwise is not a check, and I nearly shipped one.

**Renderer landed — 27 pages — and every one of its corrections was aimed at me.** Four sentences in the module
page I wrote the previous session were wrong, and I re-verified all four myself before repeating them.
`GetCapabilities()` returns `RenderCapabilities` **by value** (`VulkanRenderer.h:75`, `.cpp:1100`), not the
`const RenderCapabilities&` I published — and a caller who wrote `&GetCapabilities()` on my signature takes the
address of a temporary. `PushConstants` is `float model[16]; float viewProj[16];` — two matrices, 128 bytes, with
`projMat * viewMat` folded on the CPU in `RecordFrame` — not the "three matrices inline" I copied from the old
guide. `Render(float /*deltaTime*/)` names no parameter: the delta is accepted and discarded, so my prose about it
"matching `SystemStatistics::GetDeltaTime`" described an argument that is thrown away. The worst one was
operational, not descriptive: I wrote that a missing SDK means the renderer "compiles out rather than failing the
build", and `Engine/Renderer/CMakeLists.txt:81,89` says `message (FATAL_ERROR ...)` twice — configure stops.
Someone trusting my sentence would expect a soft failure and get a broken build. All four now say what the source
says, with the absence named as an absence.

**Which is the same failure that deleted `EngineAPIGuide`, one round later, in my own prose.** I built this site to
stop the guide lying, then rewrote the guide's Renderer claims into the new format without re-deriving them from
headers — the exact mistake the last journal entry was written to prevent. Transcribing a claim is inheriting its
staleness; the format changed and the trust did not. The exemplar rule "the header is the only source of truth"
was written by me and applied to everyone except me.

**Renderer's four contract complaints, all valid:** no badge for a `: 1` member, so a capabilities descriptor of
thirty bit-fields had no honest row kind — added `bit-field`; no sanctioned phrasing for an `enum class` or a
plain struct writing "no methods", so `DeviceType`, `Mesh` and `PushConstants` would each invent one — four
phrasings are now the whole vocabulary; the exemplars disagreed about the class-page footer (`#array` versus
`#classes`) — now `#classes`, because per-class row ids exist for *other modules'* inbound links, not for
footers; and the validator's `open()`-without-existence bug, fixed and negative-controlled earlier.

**Engine landed — 34 pages — and its README finding is worse than the one I had reported.** `README.md:223-236`
does not merely call `engine.WaitForEnd()`, a member `Engine.h` does not declare; it also never calls
`ShutDown()`, so the example tears the engine down through a destructor path the README never mentions. I read
the block myself to confirm both halves. The pages name `WaitForEnd` three times and only as an absence —
searching the site for it finds the correction, not a phantom API. It corrected the previous page's claims that the
accessors are `[[nodiscard]]` (none of them are), that `PostInitialize` and `PreShutdown` are public frame-loop
hooks (private, and `MemoryManager::PostEngineInit()` behind one has an empty body), and that `Run` is the frame
loop — it is a wait that pumps main-thread tasks, and neither shipped application calls it. Its example correction
stings most: the old page asserted its snippet was "taken from `Applications/WindowExample/Main.cpp` … rather than a
sketch" while quoting a window title and loop shape the file does not contain. A cited example that is not the
cited file is worse than no example, because the citation is what stops a reader checking.

**Its fourth complaint was wrong, and I rejected it after checking** — the second verified rejection today. It
read `docs/Resource/ResourceManager/constructors.html` as claiming the constructor accepts an `Engine&`; the
sentence says the class accepts one **in `PostUpdate`**, and `ResourceManager.h:26` declares
`void PostUpdate(Engine& engine) noexcept`. The page stands. A report about someone else's page is a claim to
test, not a work queue — the same discipline that caught my own `Renderer` errors runs both directions.

**Contract grew an `enumerator` badge and a "This entry" sidebar label**, because an `enum class` is neither a
class nor a namespace and had no honest row kind or heading; its free functions already follow §15. All 34 inbound
links into the rewritten Engine index resolve — `#engine` from Log, Test, Resource, Renderer and Config,
`#einitlevel` from Memory, OSAL, Log and Resource.

**Log landed — 36 pages, ten corrections, and one bug I confirmed line by line.** `Logger.cpp:230` stores
`isRunning = true` *before* `CreateTask`; when `FindTask` then returns `nullptr` the function logs "log lines are
flushed inline by whoever produces them" and returns — leaving `isRunning` true with no drain task that will ever
run. So the message that is supposed to reassure is the symptom, and every later `Flush` waits the full
`MaxFlushWaitMs` of 1000 ms for a task that does not exist. A log message describing a degraded mode is not
evidence of the mode; the flag is.

Three other corrections changed what the module page told a caller. It claimed logging "keeps working in a process
that never constructed an Engine" and that the logger does not reach `Engine::Get()` — `Logger.cpp:327,576` and
`LogLine.cpp:55,72` all call `Engine::Get()`; only `LogUtil` and the null-instance `EmergencyLog` path are
engine-free, which is the opposite of the safety the page advertised. `ReportMemoryConfiguration` is wrapped in
`#if PROFILE_ENABLED` in header and source, so in a default build it is not a member and calling it is a compile
error. And `PrintArgs` ends the line on the **last** argument, not the first — its base case is
`std::cout << arg << std::endl` — declared in an unnamed namespace at *global* scope, outside `hbe`.

Worth a note: `PrintArgs.h:19` writes `std::forward<TTypes>(args)...` correctly. Two headers in this engine call
the same function; one spells the template argument and one does not, and the one that does not is in the test
framework I documented an hour earlier. The correct spelling was available both times.

**Contract reconciliations from its report:** §9 and §15 contradicted each other — a namespace entry with
function pages was told to write "None — see the module functions", which points a reader at a table linking back
to the page they are on; §8 had no case for a sibling class in the same module, which `Logger` and `SimpleLogger`
need on nearly every page. Its two other complaints (validator crash, no badge for a public data member) had
already been fixed while it ran — stale reports are not defects, and it says so itself.

**Container landed — 49 pages — and wave A closed with the whole site at 231 API pages, zero dead links.** Its
corrections are the ones a container user actually steps on: `LinkedList` has no `Size` and no count member at all,
and `Remove` takes `TType&` and locates by address, not by value; `Vector` has no `Insert` or `Erase` despite the
page implying insertion paths; `RingQueue` neither overwrites nor refuses politely — `Push` runs
`FatalAssert(!IsFull())`, so a full queue aborts; `BoundedPriorityQueue`'s "bounded" bounds *priority levels*, not
item count, and its priority is used as an unchecked array index; `TPredicate` is a function pointer, so capture
lambdas do not compile; a `Deque`'s iterators are unmasked pointers that die on wrap; and no container's allocator
is exchanged by any move, which means a moved container still frees into the pool it was born in.

**Its most useful complaint was about my validator, and it was right in a way I could not see on my own.** Twelve
of its pages carried `class=\"kw\"` backslash contamination — markup an HTML parser accepts silently, because the
escaped quotes just become part of an attribute value — and all twelve passed `ALL CLEAN`. My checks test
structure, links and class names; none looks for escaped-quote debris, so a page can render literal `class="kw"`
inside a signature while every gate stays green. §12 now carries a contamination check, negative-controlled
before I trusted it: one injected `class=\"fn\"` in a copy of the tree, flagged, 1 of 49. The agent had already
repaired the twelve itself.

**My checker produced its fourth false positive of the day**, and the most instructive: `get-index.html`,
`is-valid-index.html` and `operator-index.html` are method pages, but they *end in* `index.html`, so my class-page
test — an `endswith("index.html")` suffix match — bucketed them as class pages and reported five missing sections.
The collision is mine, created when I named `operator[]`'s page `operator-index.html`. Match path shape with a
pattern anchored on `/index.html`, not a suffix.

**It also proved the exemplars underdetermined the three hardest page types**, since all four Array exemplars are
about a non-growing contiguous type: no invalidation, no allocator ownership across a move, no concurrency.
`Deque/begin` and `AtomicStackView/pop` are now exemplars for exactly those. And its §14 lesson — *derive* the file
list from links and declarations rather than copying a brief's enumeration — came from my own list being one file
short of what the site required.

## Tasks become things the registry owns, addressed by identity (2026-09-19)

**What landed**, as `3be27c3`: R7 with R22's sizing. `Engine/Core/TaskID.h` (`{index, generation}`) and
`Engine/Core/TaskRegistry.h/.cpp` are new; `Task` carries the identity it was issued; `RangedTask` holds a `TaskID`
plus the declared result count instead of `std::reference_wrapper<Task>`; a stream resolves the task at dispatch and
drops the work item with a warning when the generation does not match; `Logger`'s drain task, the suite's task and
the 11 test tasks in `TaskSystem.cpp` are created and released through the registry instead of living on the stack.
Test count **58 -> 59** with the new `TaskRegistryTest`. Gate green: Debug/Dev/Release build 0 errors, runner exit 0,
`all 59 collections passed`, `check.sh --staged` 0 violations.

**Two shapes measured out before the one that was built.** `Array<Record>` cannot work: `Array::Resize` memmoves
when its allocator answers `AllocateAligned`, `std::atomic_is_always_lock_free` is true here, and `Record` embeds a
`Task` that embeds three `std::atomic` - it would compile, run, and dangle every reference handed out before a
growth. `MultiPoolAllocator` for the banks reserves **4 MiB for a 256 KiB table** (the 2 MiB bank floor, plus a
second block to split a 256 KiB request) and would free through `MultiPoolAllocator::Deallocate` blocks taken from
`AllocateBlock`. Banks from the engine allocator give grow-by-exactly-N with neither flaw.

**Three of my own errors, each found by the suite rather than by reading.**
1. `Create` computed the next generation and never stored it, so every `Find` failed - the suite died at the
   `FatalAssert` on its own task. Diagnosed with temporary unbuffered probes at each step (init, create, find),
   because buffered log lines die with an aborted process.
2. The three sizing figures were `TConfigParam`, and `ConfigParam::Get`'s cross-thread assert fired the moment a
   test read them: `initialize()` runs on Main, tests run on a task. They are `TAtomicConfigParam` now - and these
   three were the **only** non-atomic parameters in the tree, which is the answer to "which is the house pattern".
3. My own assertion in the double-release test was wrong: I asserted a further creation must be refused when three
   free records remained. Replaced with the invariant that actually matters - create until refusal and require
   exactly 3 admissions, so a record pushed on the free list twice shows up as a fifth admission.

**An estimate of mine that measurement corrected.** R22 first quoted a 40-byte record and a 160 KiB table, written
before the class existed. The record as built is **64 bytes** and the table **256 KiB**, because identity added a
16-byte `TaskID` to `Task` - the width of the thing R7 exists to add. `TaskRegistry::RecordSizeBytes` publishes it
and the sizing test prints it, so the next reader gets a measured figure instead of my arithmetic.

**Eight mutations, each attributed to the test that catches it.** Generation not stored -> engine aborts at the
suite's own task (identity is load-bearing: with it broken the suite cannot start). `Find` ignores the generation ->
the recycling test. `Release` keeps the record in use -> resolution + free-list integrity. `Create` grows implicitly
-> refusal-at-full + free-list integrity. Exclusive ceiling -> the ceiling test. `Find` bounded to one bank ->
resolution, recycling, bank-stability. Bank sized at double the grow-by -> the precondition assert fires naming the
registry, the requested records and the bank size. Losing a subtask's identity cannot be reddened while the suite
*is* that subtask - it hangs with no output, since dropped work is dropped effectively; a surgical variant that
drops it for one task reddens exactly the identity test.

**Also.** `RangedTask.h/.cpp` carried a banner whose line 1 was an empty comment; file hygiene requires the
copyright there, so the copyright moved to line 1 and the `Created by mooming` attribution line stayed. 8 other
files still carry the old banner and are untouched. The three `m_`-prefix advisories are `using TIndex =
std::size_t;` in three headers - the same false positive `Array.h` already reports.

**A process observation worth keeping.** A concurrent agent reformatted `docs/TaskSystemRedesign.md` between my
edit and my commit, which silently dropped a correction I had made to R7's row (its blast-radius measurement). My
R22 section survived in condensed form; I re-applied the R7 correction against the new text and verified the diff
before committing. Edits to shared docs must be re-read immediately before committing, not just after writing.

## The API reference becomes an HTML site with one folder per module (2026-09-19)

**What landed.** `docs/index.html` as the Module Index start page, `docs/<Module>/index.html` for all 13
`Engine/` module directories, and one shared `docs/assets/hbe-docs.css` extracted from the stylesheet that was
inlined in the old single page. 14 pages, 291,519 bytes. `docs/EngineAPIGuide.md` and
`docs/EngineAPIGuide.html` are deleted; **the HTML is now the source of truth**, with no markdown twin and no
generator between them — the practice that had already let the old HTML rot. `AGENTS.md` and `README.md` now
name `docs/index.html` and the per-module paths. Plan: `.Plans/PLAN_api_reference_html_site.md`.

**The taxonomy came from the code, not from the guide's numbering.** Every `###` heading in the markdown cited
its header (`Engine/Memory/MemoryManager.h`), so the mapping was derived mechanically, and it moved work that
splitting by section would have misfiled: `SourceLocation` and `Intrinsic` from "Core Types" to **OSAL**, and
eight blocks from "Engine Core" to **Core** while only the `Engine` class stayed in **Engine**. Three guide
sections split, four merged, and the docs tree now agrees with `Engine/`.

**I did not reuse the old HTML, and that was the right call.** Measured against the markdown it was missing
Containers (~2,000 chars), String Utilities, Configuration, 64% of Renderer, and the entire Coding Standards
chapter. Re-authoring from the markdown also forced header checks that turned up claims in the reference that
the code contradicts — each corrected on the page, usually with the wrong spelling named so a reader searching
for it finds the right one:

| Claim in the old reference | What the header says |
|---|---|
| `OS::IApplication`, `OS::IWindow` interfaces | `OS::Application`, `OS::Window` facades; no interface exists |
| `IRenderer`, `RendererFactory::CreateWithFallback`, DX12 and Metal backends, `APIType` | none of them exist; one `VulkanRenderer`, backend chosen by macro |
| `PoolConfig` is a `struct` with `numBlocks` | `class` with `numberOfBlocks` and `operator<` by block size |
| `Array` is "fixed-size, no resizing" | `Array::Resize` exists, with a documented move-every-element contract; `Array::TIndex` is `int`, not the engine's `size_t` |
| `LogUtil::GetLogLevelName`, `GetCurrentTimeString` | `GetLogLevelString`, `GetTimeStampString`, `GetStartTime`, `ResetStartTime` |
| `BufferTypes::TGenerateBuffer = std::function<TBufferData()>` | `std::function<void(TSize&, TBufferData&)>`, and `TResizeBuffer` was missing entirely |
| Naming chapter: "All other identifiers: camelCase" | dropped, not migrated — `JOURNAL.md:640/651/764` already recorded it contradicting `docs/CodingStandards.md` |
| `build.sh ... [-notest]` | the flag removed from README.md the same day; `-test` is real |

Two of these were traps rather than typos. `String::ToLowerCase()` mutates and `GetLowerCase()` copies, and
both compile at the call site. `RHICapabilities::GetCapabilities()` creates and discards a Vulkan instance, and
`isDeviceQueried == false` means nobody asked — never "not supported". Both are on the page as warnings, and the
`<Task>`-shaped template arguments (101 of them in the source) are escaped, because a converter that missed them
would silently delete every signature in the document.

**Verification.** No pandoc, no Python `markdown` module and no Python precedent in this repo, so the 14 pages
were authored, not generated — and the check is mechanical: tag balance via `html.parser`, every `href`
resolved on disk including cross-page `#fragment` targets, every CSS class used found in the stylesheet, no
markdown leftovers outside code blocks, `<pre><code>` counts balanced against their closers.
Result: 14 pages, dead links 0, structural errors 0, bad anchors 0, markdown leftovers 0, unbalanced code
blocks 0. Four dead links and one bad anchor were my own path errors (`../README.md` from a module page is
`../../README.md`) and the check caught all five. What this does **not** prove: that the pages look right — I
cannot render them, so that gate is `open docs/index.html` on your machine.

**Gaps are printed, not hidden.** Every page ends with a Coverage section naming what it does not document —
6 undocumented Core headers (`CPUBudget.h`, `TaskProvider.h`, `StreamDrainPolicy.h`, `ResultContainer.h`,
`TaskStreamIndex.h`, `CommonMacros.h`), 8 in Memory, the four opaque types `OSInputOutput.h` only
forward-declares, and every constructor list I declined to invent. Examples are written only where a real one
exists — the `Engine` page's is transcribed from `Applications/WindowExample/Main.cpp`.

**Defects found outside this task's scope, recorded not fixed.** README's configuration table still prints five
retired macro names (`__MEMORY_VERIFICATION__`, `__MEMORY_LOGGING__`, `__FORCE_USE_SYSTEM_MALLOC__`,
`__MEMORY_DANGLING_POINTER_CHECK__`, `__RIGHT_HANDED__`) where `BuildConfig.h` spells them
`MEMORY_VERIFICATION_ENABLED` … `RIGHT_HANDED_COORDINATE`; README's "Basic Engine Initialization" example calls
`engine.WaitForEnd()`, a member that exists nowhere in `Engine/` or `Applications/`; and `OS::Application` holds
`m_platformHandle`, the `m_` prefix the standard forbids.

## The R21 ceiling, and a guard that can finally be written (2026-09-19)

**What landed.** `2e087a0`, after R21 turned the ceiling question from an open decision into a constant.
`TaskStream` gets `DefaultMaxResultCapacitySlots = 0` with `Get`/`SetResultMaxCapacity` and `CanAdmitResults`,
which is the only place the ceiling is interpreted; `EnqueueFifo`, `EnqueuePriority` and
`TaskSystem::Enqueue(streamIndex, task)` return `[[nodiscard]] bool` and refuse before a queue is touched,
logging the task name, its declaration and the ceiling. 58 collections green in Debug, Dev and Release, runner
exit 0 in each, `check.sh --staged` exit 0 with 0 violations and 0 advisories. The refusal behaves identically
in Release - verified from that run's log, not assumed, since `Assert` is a no-op there and a guard built on
assertions would have vanished.

**The design fork worth recording is the refusal channel, not the ceiling.** A guard with nowhere to report is
decoration, and every enqueue today returns `void`, so the only way to refuse was to widen a signature. Measured
blast radius before choosing: 3 call sites reach the stream methods through `TaskSystem::Enqueue(TIndex, ...)`,
9 reach `TaskSystem::Enqueue` in total, and 6 needed to say something explicit. `[[nodiscard]]` was chosen over
a plain `bool` because a caller that ignores this drops work, and this engine has a documented history of
dropped-task work surfacing as a hang a human watched. The honest cost is in the diff: six `(void)` casts -
`Logger.cpp`'s IO enqueue, the suite's base-stream dispatch, four in `TaskSystem.cpp`'s test section - are the
present shape of "no fallback exists yet", and the header says a refused task is not queued, not retried and not
run here. The general-queue overload still returns `void`, and deliberately: no stream is chosen at that point,
so there is no ceiling to test against.

**Seven mutations, each attributed to the test that caught it, each restored byte-identically.** Default ceiling
1024 instead of 0 -> TC8 only. `<=` to `<` -> TC9 only. `CanAdmitResults` always true -> TC9 and TC11. Priority
lane ignoring the declaration -> TC11 with FIFO green, which is what proves the two lanes are each guarded
rather than one. Refusal logged but the task still queued -> TC9 only. `0` treated as a real ceiling of zero ->
TC8 only. A ceiling of 1 refusing a zero declaration -> TC10 only. My first grep for red tests printed nothing
and I nearly reported "all mutations produced a red suite" as the finding; the format is
`# TCn.Name Result [FAIL] #` and without it there is no attribution, which is the whole point of mutating.

**Where that proof is weaker than it looks.** TC10's mutation is contrived - it fires only when the ceiling is
exactly 1 and a task declares 0, a state no production config reaches. The mutation that would really test the
claim is refusing every zero declaration, and that starves the suite before TC10 runs: production tasks declare
zero results, so the base stream's own dispatch gets refused and the evidence is a hang, not a red assertion.
Recorded in the plan file rather than smoothed over.

**Not implemented, and not pretending otherwise.** `Grow` does not refuse to cross the ceiling: nothing in
production grows a container yet, so a check there could be neither reached nor proven red; it lands with the
growth call site. Room is still not reserved at admission (R13/R18 in full), and R15's fallback - and with it
guard (b) - is still absent.

**A process error of my own, of the accusing-my-own-work kind.** After writing a Doxygen block I announced that
I had put stray non-English text into it and moved to fix it. The file was clean ASCII; the scan proved it, and
the correction I attempted found nothing because nothing was there. The false report was the defect. Announcing
a defect before reading the file costs the reader the same trust as hiding one, which is the standard this
project holds itself to for the opposite mistake.


## NumResults, and result containers built on Array instead of a new container (2026-09-19)

**What landed.** `1f7e777` after a proven-code-identical `2dca103`. `Task` gains `NumResults`, one
integer defaulting to zero (R18); `TaskStream` owns two `ResultContainer`s with an initial capacity of
1024 slots and `growBy` of 1024 (R17, R20); a slot is 128 bytes (R5), `Append` is `slots[count++]`,
`Rewind` is `count = 0` plus an epoch advance (R11). Suite **57 → 58 collections**, green in Debug, Dev
and Release with exit status 0 and zero `error:` in every build log. Nothing consumes any of it: no
capacity admission, no registry, no swap/fold pass, and `Wait` and `HasDone` are untouched.

**The container is not a new container.** Three shapes were measured before choosing, and two of them
were mine. `HVector` is `std::vector<T, DefaultAllocator<T>>` (HSTL/HVector.h:15): `push_back` doubles
and reallocates, so a slot already handed to a running task becomes a dangling pointer precisely when a
capacity bug appears, and `DefaultAllocator` captures `GetCurrentAllocatorID()` at construction
(DefaultAllocator.h:33) - a `TaskStream` member built in `BuildStreams` would therefore take the base
thread's allocator, not the stream's. `RingQueue` already refuses to grow on `Push`
(RingQueue.h:82) and has no production users, but it masks every access, rounds capacity to a power of
two, and has no grow path. A general `SlotBatch` template was actually written before the owner asked
the better question: `Array` with `Resize(newSize)` and a constructor taking an allocator instance
covers the requirements, and the reusable remainder is two integers - the used count and the epoch.
Naming it `HSlotBatch` would also have been wrong on the facts: `Engine/HSTL/` contains nothing but
aliases over `std::` types, so the prefix claims an STL stand-in.

**Two additions to a shared container, both additive.** `Array::Resize` is the growth API it never had -
the missing API is what forced B2's bounded attachment set - and it allocates exactly, which is the
property that makes an eighth-of-a-megabyte container arithmetic meaningful. `Array(const TAllocator&,
TIndex)` states an owner instead of inheriting a scope. `NamedPoolAllocator<T>` is the 12-line adapter
that lets any engine container draw from a named `MultiPoolAllocator`, which nothing could express
before. Blast radius measured: 10 files reference `Array<` - the figure of 6 I first wrote came from a grep that
missed half of them - and no existing behaviour changed, which the three-configuration run confirms.

**A consequence of R20 the owner should see.** A 131,072-byte request to a `MultiPoolAllocator` does not
reserve 131,072 bytes. `CalculateBlockSize` returns 131072, `CalculateNumberOfBlocks` takes
ceil(1 MB / 131072) = 8 and floors it at `MinNumberOfBlocks` = 16, so the first container triggers a
2 MiB bank per stream. This machine builds 12 streams, so roughly 24 MiB is reserved at startup for
3 MiB of slots; the other 14 blocks absorb later same-size allocations, so growth is free until the
seventeenth block. Computed from `MultiPoolAllocator.h:25,26` and `MultiPoolAllocator.cpp:397-422` (the floor is the
`std::max` at line 414), not
observed - `PROFILE_ENABLED` is 0, so no allocator statistics reach the run logs.

**A decorative check of my own, caught by mutating the implementation.** My first per-stream test
compared each stream against `TaskStream`'s own constants, so changing the decided 1024 to 512 left every
assertion green - the same false-green class this subsystem has now hit four times. The test now states
the decided figures as literals with `static_asserts` tying them to the engine's, which splits the
coverage in two: changing the decision breaks the build and names the decision, building a stream
differently turns the test red. Both halves were proven by mutation, and the second one produced red
output for all twelve streams. Other mutations, each watched going red: dropping the epoch advance from
`Rewind` (3 assertions), an `Append` that stops advancing the cursor (4), `NumResults` defaulting to 1
(2), and `Resize` rounding up to a power of two (1, and only in Release - in Debug the assert inside
`Grow` fires first, so that check is shadowed in a debug build).

**Errors, including one that made a measurement lie.** My first `Array` constructor delegated to
`Array(size)` and assigned the allocator afterwards, allocating from the ambient pool and freeing with
the named one - the exact cross-pool free this change exists to prevent; caught reviewing my own edit
before it was compiled. My first preservation test read a slot through a pointer kept across a `Grow`,
which is a freed buffer. A python slice truncated `ResultContainer.cpp` by the closing
`} // namespace hbe`, so the test block nested `hbe::hbe`, and two builds went to the symptom before the
cause was read - untracked files have no `git diff` to catch a truncation. Far worse: I asked whether the
touched files were already non-conformant by pointing clang-format at copies in `/tmp`, where no
`.clang-format` exists, so LLVM defaults gave 213/85/107/763/147 differing lines against a real
243/45/7/0/0. check.sh's header documents that exact fallback and I still fell for it. Corrected, and
the legacy reformat went into `2dca103` with a per-file digest taken after stripping comments, `#include`
lines and whitespace - identical in all three files - plus a separate multiset check on the includes so a
reordered one could not hide inside the first.

**Still unproven, stated rather than glossed.** Nothing checks that a container's buffer really came from
the pool named in its constructor: `MultiPoolAllocator` exposes no usage or address-range query to assert
against, so that regression surfaces as the pool's own "is not allocated by this allocator" fatal log
rather than as a red test. Instrumenting it needs `PROFILE_ENABLED` or a new accessor - a separate
decision, not something to slip into a feature commit.

**The question I asked, the answer, and what the answer cost.** Refusing at dispatch any task whose
`NumResults` can never be admitted needs a maximum reachable capacity, and R20 gives an initial capacity and a
`growBy` - both 1024 - but no ceiling, so growth is unbounded, no capacity is unreachable, and the guard had
nothing it could refuse. I left it unimplemented rather than inventing a threshold to make it fire, and asked
where the ceiling should come from. The owner answered: a per-stream constant, **default `0` meaning no
ceiling**, recorded as R21. The default is the inert value, which is the honest choice - the guard exists, is
opt-in per stream, and a default-configured stream still permits unbounded growth. Measured cost of opting in,
which is what the recommendation understated: reserved memory is `blockSize * max(16, ceil(1 MB / blockSize))`,
so a ceiling of 1024 slots reserves a 2 MiB bank and 4096 reserves 8 MiB. A ceiling is not a free knob.

**That measurement also contradicts a figure already in the design record.** R20 says a 1 MB bank fits 8192
slots, making an initial container one eighth of a bank. No bank of this block size is 1 MB: the 16-block floor
makes a 131,072-byte block produce a 2 MiB bank of 16,384 slots, so the initial container is one sixteenth and
the per-stream reservation is 2 MiB - 24 MiB across the 12 streams here, not the 512 KiB R20 implies. Corrected
in `docs/TaskSystemRedesign.md` alongside R21. The second guard - log a capacity-closed lane with the need and
the room remaining - still has nothing to log, because no lane closes on capacity yet.

**Next.** Handoff item 1: `TaskRegistry` with index plus generation, and `Task` stops being a stack
object - 6 construction sites, five of them test code.


## The README now states the documenting policy that only AGENTS.md carried (2026-09-19)

**What landed.** `README.md` § Documentation opens with a new **Documentation Policy** subsection: the
self-documented rule (no comments in `.cpp`, brief comments in headers only), where each kind of prose
goes — caller contract to the paired `.h`, implementation or system design to a document under
`docs/`, API surface to an API reference under `docs/` — the three exemptions (line-1 copyright notice,
structural labels, `Engine/CodingStandards.cpp`), and the fixed Module / Class / Function outlines an
API reference document must follow. Source of truth is `AGENTS.md` § Self Documented Code and API
Reference Documents, which since `37ba98e` bound agents only; a human reading the README was never
told. Plan: `.Plans/PLAN_readme_documentation_policy.md`.

**Two stale references repaired under explicit user approval, both inside the same section.**
`### Code Standard` linked `docs/CodeStandard.md`, a file that has never existed, and now links
`docs/CodingStandards.md`. `### Convenient Build Script` advertised `[-notest]` and an example ending
`-clean -notest`; `build.sh` has no such flag — its usage line and case block carry `-test`, which
reconfigures with `-D__TEST__ -D__UNIT_TEST__`. The README was teaching a flag whose whole absence is
why `EngineTest` can build green while testing nothing.

**Verified, not assumed.** `check.sh` lints C++ sources, so the three-configuration build gate cannot
cover a Markdown change; the gate here is that `git diff --stat` names `README.md` alone, that both new
relative links resolve on disk, and that the assertion "the standards lint requires the copyright
notice" was read at its source — `check.sh:277` fails any file whose line 1 is not
`Copyright (c) ... Hansol Park`.

**Recorded, deliberately not fixed** (user picked the narrow scope): `### Running Tests` still says to
run `./Applications/EngineTest/EngineTest` from inside `build/`, a path a Ninja Multi-Config tree does
not emit — `AGENTS.md` gives the per-configuration path; `### Platform-Specific`, `### Code Quality`
and `### Unfinished Features` sit under `## Documentation` but are code notes, not documentation links;
the file ends with a stray duplicate `## Build` configure command.

## B1b, B2, B3a: budget primitive, provider model, stream accounting — and a 20-minute hang that taught more than the feature (2026-09-18)

**What landed.** Three commits, each gated: `f751a8f` adds `OS::GetThreadCPUTime` on all three platforms plus `hbe::CPUBudget` and the
engine-wide frame-period yardstick (`time::Set/GetBaseFrameRate`, `GetBaseFramePeriod`); `e5fb359` adds `TaskProvider`, `TaskHandle`,
`TaskProduceContext` and moves `TStreamIndex` into its own header; `65944f1` wires a budget into `TaskStream` so each stream charges the CPU time
of tasks it runs and answers `MayTakeNewWork`. Suite grew 54 → 55 → 56 collections, 56/56 in Debug, Dev and Release throughout, `check.sh
--staged` at violations 0 on every commit. B1 is complete; B2 is complete; B3 is split into B3a (done), B3b (FIFO conversion), B3c (provider drain).

**The measured numbers that justify the design.** 20 ms of busy work charged **19987 µs** of thread CPU; **80 ms of sleep charged 20 µs**. That
second figure is G1's whole argument for CPU time over a wall clock, and it is now asserted by a test rather than argued in a document. Mutation-
checked both ways: substituting `steady_clock` for `clock_gettime` turns two tests red ("waiting is being billed as work"); deleting the provider
duplicate-attachment scan turns one red ("Attaching stream 2 twice left 3 attachments").

**A tool false-green, caused by my own shell.** My chain was `clang-format -i f && clang-format f | diff f - | grep -c '^[<>]' && git add f`.
`grep -c` prints `0` **and exits 1** when nothing matches, so the chain stopped, `git add` never ran, the index kept the unformatted file, and
`check.sh --staged` reported violations 0 — because every check reads the **worktree** while a `--staged` commit takes bytes from the **index**. The
epoch commit shipped non-conformant under a green lint, and its hash moved on amend (`57336b2`, not the `0154db0` reported mid-flight). `b629166`
makes index/worktree disagreement a `[FAIL]` naming the files, reproduced before and after. Two lessons: a counting grep is not a safe link in a
chain, and a lint whose input differs from the artifact under review proves nothing. Also fixed a rule that punished correct code — the
joined-empty-record pattern matched `struct timespec timeValue{};`, a declaration, because `[^;]*` spans a second identifier; narrowed and verified
in both directions (`3696baf`). One advisory left unfixed and recorded: "no m_ member prefix" cannot distinguish a member declaration from reading
POSIX's own `sched_priority` field.

**Two design deviations the codebase forced, recorded rather than absorbed.** The provider sketch held an `Array<TStreamIndex>`; the engine's
`Array` has **no growth API** — fixed at construction, non-copyable — so the sketch was unimplementable as drawn, and attachments became a bounded
inline set (cap 8) which also means a provider needs no allocator to exist. And `Stop()` detaching immediately would edit a container another
thread iterates — the D10 defect class — so it is an atomic request the owning stream applies. Both are in `docs/TaskSystemRedesign.md` under "B2
as built".

**The hang, and what it cost to guess.** First version of the stream-accounting test charged 0 µs, because `TaskSystem::Enqueue(task)` is a **general
queue that whichever stream asks first claims** — a worker had run my task. Pointing it at the base stream instead produced a 20-minute hang, and
`sample` showed why: **the entire EngineTest suite executes inside a task on the base stream** (`UnitTestCollection.cpp:147` → `TestEnv::Start`),
so the base stream is occupied for the whole suite and will never run a second task. My test premise was wrong twice over, and a third flaw made it
silent: the busy loop's only real bound was a CPU guard with a 400M-iteration cap and a clock read per iteration, so it ran far longer than the
guard implied. Both the general-queue routing and the suite-host fact are now recorded in the plan's new "Measured facts" table, together with
`HasDone` requiring `numSubTasks > 0` against a `BusyWait` that is a bare spin — the recipe for a silent suite hang. Also fixed my own stale log
text: it still said "Base stream charged" after the test moved to a worker.

**One honesty fix worth stating plainly.** `GetAccumulatedCPUTime` was written to be read from a diagnosing thread while the stream thread wrote
it, which is a data race with a doc comment on it — the same shape as D10. The counter is now atomic; the `BeginTask`/`EndTask` pairing stays
single-thread-owned because measuring across two threads does not measure a task. A stream with no allowance does not measure at all: two clock
reads per task for a number nothing can read is a tax with no beneficiary, and that clarification of G1 is written into the header.

**Next.** B3b — convert the stream queue to FIFO, now known to break no ordering promise since nothing sets a priority and the current queue has no
tie-break — then B3c's provider drain gated on `MayTakeNewWork`, then B4's named streams.

## G5 closed, B1 epoch landed, and a green lint that meant nothing (2026-09-18)

**G5 is decided (`97e6561`), which closes the last open gate.** Range splitting is not retired as a capability; `TaskSystem` gains
`ParallelFor` — a task that waits asynchronously for all its subtasks, collates their results, and yields one final result. The call sites were
counted before choosing, which is what the gate required, and they split the question in two: every range-splitting call is inside
`#ifdef __UNIT_TEST__` (`TaskSystem.cpp:318,364,422`), so production splits nothing, while `RangedTask` is load-bearing as a *carrier* —
`Logger.cpp:234` enqueues `GenerateSubTask(0, 1, 0)`, a one-unit range holding the drain runnable. Retiring the type would break the logger;
retiring splitting would not. Consequences written down instead of left to be found: the join is a completion count plus one enqueue, not the
`BusyWait` the current tests use, because parking a stream thread on its own children is the failure the model exists to avoid; an abandoned
child must still count as completed or a single deadline miss hangs the parent forever; collation goes into indexed slots because children
finish in stream order, not issue order. It also corrects the step table — **B7 is blocked by B5**, since "collate their results" needs children
to carry results, and results are B5's to define.

**B1's epoch half is in (`57336b2`).** One `steady_clock` epoch for the engine — `GetEngineEpoch`, `ResetEngineEpoch`,
`ElapsedSinceEngineEpoch` on `hbe::time`, contract in `Time.h`, storage one atomic nanosecond count because it is read per deadline check and
written once. It lives in `hbe::time` rather than on `Engine` because G2 requires it to be readable without `Engine::Get`, which asserts an
instance exists — the same reason A2 exists. `Engine`'s constructor sets it. Nothing consumes it yet, so behaviour is unchanged; `LogUtil` keeps
its own baseline until the D8 consolidation. `TimeTest` joins the suite, so the count is **54 collections, not 53**; all 54 pass in Debug, Dev and
Release. The test has teeth: neutralising `ResetEngineEpoch` turns TC2 red with "Elapsed was not reset: 36 ms measured immediately after
resetting the epoch" and exit 1.

**Two of my own errors, recorded because both were silent.** (1) I committed a message quoting a hash — `7f2b0e1a94` — that I had never
measured, with a self-correction left in the prose. Amended on the spot (`34abff3`); the real values were `801bf64a39` and `b00f83381b`. A hash in
a message is a claim about a run, and inventing one is indistinguishable from lying about one. (2) Far more serious: my chain was
`clang-format -i f && clang-format f | diff f - | grep -c '^[<>]' && git add f`. **`grep -c` prints 0 and exits 1 when nothing matches**, the chain
stopped dead, `git add` never ran, and the epoch commit captured the *unformatted* file while `check.sh --staged` reported violations 0. The
commit was non-conformant under a green lint, and it is why the epoch commit's hash moved on amend. The script already guards that grep with
`|| true` in its own loop; the lesson had not left the file. Fixed as a tool rule, not a resolution (`b629166`): under `--staged`, a disagreement
between index and worktree is now a `[FAIL]` naming the files, reproduced before and after — the same tree state that reported violations 0 now
reports violations 1, and re-staging silences it. One guard covers every check, and it also catches a half-saved edit or an editor reformatting
after staging.

**The proof standard got stronger, because a weaker one was caught out.** The whitespace-only hash for `Time.h` *failed*, and the cause was good:
clang-format also corrected a stale label, `} // namespace Time` is really `} // namespace hbe::time`. Comment text is not whitespace, so the hash
was right to move. From here the comparison strips comments as well as whitespace and includes, which separates "the code moved" from "a comment
was wrong". Three files were reformatted and proven code-identical before their semantic edits: `Time.h` (`0504444`), `Time.cpp` (`34abff3`), and
`UnitTestCollection.cpp` (`7420dd4`), which alone was 148 lines behind. `Time.cpp` needed `NamespaceIndentation: None` respected — namespace bodies
are not indented in this house style, which my new code got wrong until the formatter said so.

**One test design error worth keeping.** The first advance test spun on a `volatile` counter and would not compile: incrementing a volatile object
is deprecated in C++20 and the tree is `-Werror`. The instrument was wrong before the qualifier was — the epoch measures steady wall time, and the
CPU-time probe below shows a spin and a sleep differ entirely in CPU time while being identical here. A 5 ms sleep is deterministic and survives
`-O2`, so the same test means the same thing in Release.

**Measured for B1's second half, not assumed.** `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` works on this platform (arm64 macOS, `rc=0`, ns-valued),
and after a 50 ms sleep the value moved 13 µs — blocking does not accumulate, which is exactly G1's requirement. `thread_info(THREAD_BASIC_INFO)`
also works but only reports microseconds. So the per-stream budget is realisable as thread CPU time; the "cycles" in G1's wording will be recorded
as a deviation with this evidence, since a portable user-space cycle counter does not exist across the targets and the budget rule compares against
a *frame period*, which is a duration.

**Next.** B1's budget primitive on `TaskStream` (accumulator + configured budget + a dequeue gate that stops taking work rather than preempting,
per guardrail 5), then B2's `TaskProvider`/`TaskHandle` per G3/G3b.

## A4 and A3: levels, one application, and two verification errors that nearly lied (2026-09-16)

**What was done.** `EInitLevel` is wired (`7e62dcf`): `Engine::Initialize` takes a level set defaulted to `All`, so one code path keeps every
caller's behaviour while a headless context can pass `TaskSystem|Logger` and never touch the window server; `Logger` implies `TaskSystem`,
enforced in `Initialize` rather than in prose. `Run`, `ShutDown` and the signal handler now respect what actually started, and `Run` asserts
with a message naming the level it needs, which retires a message-less `FatalAssert` a legal no-Application level would have tripped. D3 is
closed (`ba42dd4`): both example mains read `Engine::GetApplication()` and a grep for `CreateApplication` under `Applications/` is empty.
Two style commits precede them (`673f00b`, `6913d8c`), one of them only moving include lines.

**A4 shipped a hang and the plan's own verification row caught it.** The row said "SIGINT at each level", so I ran it; at level `None` the
handler blocked forever, `SignalHandler -> Logger::StopTask -> Task::Wait -> __semwait_signal`, captured by sampling the process. Root cause:
the guard asked `Engine::IsLoggerReady`, and **`Logger`'s constructor calls `engine.SetLoggerReady()`** (`Logger.cpp:186`). That flag therefore
reports that the Logger object was *constructed* — true before any `Initialize`, true with no drain task — so `StopTask` waited for a task that
had never been started. The plan's premise that the ready flags "already say what started" is false for this flag and my code inherited it.
`isTaskSystemReady` is genuinely truthful (nothing sets it outside `Initialize`), so `Run` and `ShutDown` were right as written. The guard now
asks the object that owns the state, `Logger::IsDrainTaskRunning`. This is not a reversal of the A1 decision, where `isRunning` was rejected as
a proxy: A1 asked *"may this stream be woken"*, which goes false during shutdown while the task still has to be stopped; this asks *"is there a
task to stop and wait for"*, for which it is exactly the fact. The distinction is written into `Logger.h` so the two guards do not read as an
inconsistency. `isLoggerReady` is left alone — public surface someone else chose — but it now has no readers and is recorded below.

**Two verification errors, both of which nearly produced a false conclusion.** (1) I reported that SIGINT hung `VulkanExample`. It did not:
the test backgrounded `cd dir && ./app`, so `$!` was the subshell and the signal went to **bash** — the giveaway was a sample whose every frame
was in bash. The nasty part is that my regression check was valid and my headline result was not: the pre-`A4` build in a throwaway worktree
backgrounded the bare binary, so it correctly exited 130, while the current-tree test did not, and I nearly reported "I broke shutdown" on the
strength of an asymmetry in my own harness. Re-measured with the binary's real pid: exit 130 in about a second, identical to pre-`A4`. (2) A
scripted edit asserted each half of its pattern *separately*, so a replacement that never matched still asserted, ran, and printed success — the
file kept the old include order and only reading it back showed that. Both habits are the same one already logged: a check that cannot fail is
not a check.

**Two findings recorded, deliberately not fixed.** **D8:** `Engine::Log` formats its own timestamp inline against `statistics.GetStartTime()`
(`Engine.cpp`, ~line 178) while every other path goes through `LogUtil::GetTimeStampString`. One rule, two implementations; they agree today
only because A2's baseline and `SystemStatistics`' are the same instant, and nothing keeps them agreeing. **D9:** `Engine::IsLoggerReady` reports
construction, not readiness, and after A4 has no readers — rename it or redefine it when lifecycle next moves, before someone trusts the name.

**State.** 53/53 in Debug, Dev and Release after each commit; gate 12/12 and 0 violations each time; `WindowExample` runs its full loop and
exits 0; `VulkanExample` presents a frame and comes down either through its own shutdown path or on SIGINT with 130, depending on whether the
window survives in this session — both recorded rather than the tidy one alone. Task A is complete. Task B cannot start until gates G1–G5 are
decided; those are owner decisions, not engineering choices I can make on someone's behalf.

## Plan split, D6 closed, D7 found — and three errors of my own (2026-09-16)

**What was asked.** Review and correct `PLAN_single_executable_app_registry.md` with the smallest edits
that stop it overshooting, split the work into a bug-fixing task and a task-system refactor task, and
write the task-system redesign as a design doc: a provider/stream job graph with `Engine::Run()` as the
tick pump for the major systems. Planning went to `ec50284` and `f505165`, the design to
`docs/TaskSystemRedesign.md`; the split produced `.Plans/PLAN_defect_board_partial_init.md` (Task A) and
`.Plans/PLAN_task_system_refactor.md` (Task B). Execution then went commit-by-commit with a gate on each.

**Five corrections to the plan, all from reading the tree rather than trusting the summary.** `Engine::Run()`
does not return between frames — it spins on `taskSystem.IsRunning()` (`TaskSystem.cpp:271`, cleared at
`:101`), which is what makes an applet with its own loop deadlock, and the plan described it as a pump that
returns. `IsBaseThread()` already exists, so a step that was going to add it was redundant. The
`std::vector` rationale was inverted: `DefaultAllocator` fast-paths to `malloc`, so an `HVector` there buys
nothing and only loses the `std` interop `MacOSApplication` needs. The dependency list omitted `HEngine`
and `External`. `EngineTest`'s entry is `TestMain.cpp`, not `Main.cpp`. The first two were the ones that
would have caused real overshoot.

**D6 closed (`d75e7bc`).** `Logger::AddLog` woke the IO stream by index without asking whether that stream
exists; in a process that never initialised the task system the index is out of range and
`GetTaskStream` asserts. Guarded on `HasStream`, the precondition of index access, and stated in the `.h`
that an index without that check is unsafe. Repro went exit 133 to exit 0; suite 53/53 in Debug, Dev and
Release. The guard is on stream *existence*, not on the `isRunning` proxy I first reached for: `streams.Clear()`
runs in `JoinAndClear` and `Logger::StopTask` is only called from `SignalHandler`, so "drain task still
alive" misses the shutdown window entirely.

**D7 found and closed as A0 (`ccc05ec`, with `176e8f1` behind it).** The `-test` configuration **did not
compile at HEAD**: `Engine/OSAL/Window.cpp` raises 12 `-Wunused-result` errors under `-Werror` because six
`windowFuture.get()` calls in the `WindowTest` bodies discard a `[[nodiscard]]` result. Reproduced on a
clean HEAD tree with my work reverted, so it is pre-existing. Fixed with the discard convention the tree
already uses (`TaskSystem.cpp:164`, `StaticStringTable.cpp:210`). **Why it hid:** the default gate builds without `__UNIT_TEST__`, so it never compiles the test bodies. Said like that it is incomplete,
and the correction matters: `check.sh --test` already exists, builds `EngineTest` with `-test`, and **would have failed on this**. The
capability was present and the routine simply never passes the flag, so the gap is the default gate and the documented workflow — not a
missing feature. That is a smaller and less interesting claim than the one I first wrote here, and it is the true one. Corollary, and a correction to my own earlier claim: the
"53/53 ×3" I reported after A1 was a genuine run of a real suite, but that build had reused an
`OSAL/Window.cpp.o` compiled *without* `__UNIT_TEST__`, so `WindowTest`'s bodies had not been compiled, let
alone executed. They are now, and `TC0.Create Window` runs.

**The include-layout comparator was locale-dependent, which made two lint layers demand opposite orders of
the same block.** `awk`'s `L[i] < last` followed the ambient collation, where case folds first and
`Logger.h` precedes `LogLevel.h`; clang-format sorts case-sensitively and wants the reverse. That is why
`Engine.h` had no order that could pass both. **My premise for the fix was wrong and measurement caught
it:** I claimed pinning the comparison to byte order would make the layers agree, and `LC_ALL=C` alone
removed 2 findings while **adding 14**, because comparing across the `<...>`/`"..."` boundary compares
`chrono` against `LogLevel.h` — a merged block is a missing blank line, which is layer 1's finding, not an
ordering one. Byte order *plus* comparing only within a category is the actual standard. Whole tree: 54
flagged files → 18, zero added, `Engine.h` clean, and a negative test that puts the pair back shows both
layers rejecting it together. The check was not weakened.

**Three errors, recorded because that is what this log is for.** (1) `git checkout -- <path>` restores from
the **index**, not HEAD, so the "revert" I ran before reformatting `Window.cpp` was a no-op and the
behavioural `(void)` fix rode inside a commit that claimed to be formatting only — which is also why the
whitespace proof said FAIL and the commit still happened: I had chained it with `&&` after printing the
proof instead of gating on it. (2) That commit's message asserted a hash, `67a2f06608`, **that I never
computed**. It was destroyed by `git reset --mixed HEAD~1` and rebuilt as `176e8f1` with measured values
(`6b2743da92` body, `fbd4b093c8` include set, equal on both sides); nothing was pushed and no other commit
depends on it, but a fabricated verification number is the worst kind of thing to put in a record whose
whole purpose is verification. (3) The amended commit initially carried `(void)windowFuture.get();`, the
form clang-format rejects, because `--apply` had written the worktree while the index kept the old bytes —
amended, and re-checked in `HEAD` scope so the number quoted covers the committed bytes rather than an
empty index. The tool now re-stages what `--apply` rewrites (`4e562a5`), so the trap is gone for the next caller. It was reproduced
before being fixed: a malformed line staged into `EngineInitLevel.h`, every check reporting PASS, and `git show :file` still holding the
rejected bytes. After the fix the same probe leaves the index matching the worktree. `--all` and `<rev>` scopes still leave the index
alone on purpose — staging there would sweep unrelated work into the commit, which is what the script exists to prevent. The habit that caused all three is the same: running the check and the action in one chain,
so the check cannot stop the action.

**Open.** Task A's A2 (D5, log zero point), A3 (D3, one OS application per process) and A4 (`EInitLevel`
wiring; the header itself is committed as `8f10d80`, but nothing references it yet) are not started; making `--test` part of the documented
default gate is recorded but not done — `check.sh --test` exists and would have caught D7, so that is a change to the routine in
`SKILL.md`, not new machinery; Task B's gates
G1–G5 are undecided. Nothing pushed.

## Three recorded defects, closed by running them (2026-09-13)

**What was asked.** "Fix recorded defects" — the ones the entry below listed as found and not yet
fixed. Re-verified against the current tree first, because the tree had moved (`fb791cb`, and the
renderer/application refactor rewrote the example mains): **D3 was already gone** — `CreateNewApplication`
exists nowhere in `Applications/` or `Engine/`, `Engine.cpp:96` creates the one `OS::Application`, and
`WindowExample` only creates a window. The entry below claims it is unfixed; that claim is now wrong and
this paragraph is the correction.

> **Superseded 2026-09-16 — the paragraph above is the error, and this is the restoration.** D3 was never gone. The search was for
> `CreateNewApplication`, a name that has never existed anywhere in this tree; the function is `OS::CreateApplication`, and both
> `Applications/WindowExample/Main.cpp` and `Applications/VulkanExample/Main.cpp` called it *after* `Engine::Initialize` had created one, and
> called `Initialize()` on the result. The duplicate was live rather than shadowed, because `CreateApplication` returns
> `std::make_unique<Application>` on every call and is not a singleton. The parent plan's claim was right and I overturned it on a
> name-match, which should have been the reason to keep looking rather than the reason to close the item. Fixed in `ba42dd4`: both mains
> read `Engine::GetApplication()` now, and the engine is the only creator.

**Two severity claims of mine were also wrong, and measurement corrected them.** D2 was recorded as
"assert in Debug/Dev, null deref in Release"; the repro died of **SIGSEGV with no diagnostic** — the
`Assert` inside `Engine::Get()` produced nothing, so the crash looked like an unrelated fault. D1's
reachability was understated: the everyday trigger is not a hypothetical partial-init tool but
`StopTask`, which sets `isRunning = false` and resets `threadID`, then **logs afterwards** — and
`FatalAssert` itself calls `FlushLogs()` (`Debug.h:87`), so a fatal report became a hang.

**Both demonstrated before being fixed**, on a watchdog: D1's process printed "entering Flush" and was
killed at 25 s against HEAD; after `95053e5` it exits 0 with the queued line written by the caller. D2
died with signal 11 against HEAD; after `5677fce` it prints `[Repro][Info] logged with no Engine in
existence`. Two assumptions were also wrong in the *fix's* favour: `TLogStream` is
`InlineStringBuilder<Config::LogOutputBuffer>` — a fixed stack buffer — so the fallback needs no
allocator at all, and `TLogBuffer`/`TTextBuffer` are allocator-aware `HVector`s, so an inline drain does
not misroute frees. `ImmediateLog`, the helper that exists for "print now", opens
`AllocatorScope(SystemAllocatorID)`, and `MemoryManager::GetInstance()` FatalAsserts — so the same trap
as a `StaticString` built before the manager. The emergency write uses C stdio for that reason.

**Two findings from the process, both latent, neither fixed.** **D5:** `LogUtil::GetStartTime()` returns
a reference to a function-local static that nobody assigns (`LogUtil.cpp:13`, and `LogUtil.h:19` returns
it `const`, so it cannot be assigned through), while `Logger.cpp:103` calls it and drops the result — so
every log timestamp is measured from the clock epoch, not engine start: `1215:12:38` on a one-second run.
`GetTimeStampString` also pads nothing, so it would print `1:2:3.45`. **D6:** `Logger::AddLog` ends with
`taskSystem.GetIOTaskStream().WakeUp()` (`Logger.cpp:382`), and that indexing of an empty array
FatalAsserts — logging before `Engine::Initialize` aborts. This is the concrete reason `EInitLevel`
must enforce "Logger implies TaskSystem" rather than document it, and it belongs in plan step 1.

**Not covered by execution:** the 1000 ms give-up branch of `WaitForFlush`. Reaching it needs a live
drain task that stops making progress, i.e. blocking the IO stream — the tests cover the no-drain branch
and the two-waiter race instead (15 runs, queued entry always ordered before both waiters returned).

**Method worth keeping.** Style separated from semantics, and *proved* separated: strip every whitespace
byte and hash — `e855c0de` both sides for `Logger.cpp`, `85e6a516` for `Logger.h`, so no behaviour can
hide in the format commits. Also worth keeping: `check.sh` judges only *staged* files, so staging
`Logger.h` surfaced a pre-existing include-layout failure (standard and project blocks merged, no blank
between) that had never been seen because the file had never been staged — expect the same on other
long-unstaged headers. Owner extracted `WaitForFlush()` before this change, and that seam is where the
fix landed.

**D4, and its honest size.** `Engine/Core` and `Engine/Resource` carried `dependencies.txt` — plural —
which MakeBuild never reads: the config key is `dependency`, the legacy file is `dependency.txt`. The
modules do call OSAL (`TaskStream.cpp:77`, `Buffer.cpp:205`), so the build files understated the graph;
nothing could break, because `Engine` is a global include directory and every application links every
module. Fixed in `18f05e0`; the duplicate-library link warning now names `libOSAL.a`, which is the
dependency taking effect.

**State.** EngineTest **53/53 in all three configurations** and `check.sh` clean at every commit
(`a499508` `5677fce` `bc16916` `95053e5` `18f05e0`). Plan step 2 is done; step 1 (`EInitLevel`, now also
carrying D6) is next. Standalone repros live in `/tmp/spv_repro/` and are rebuilt from
`repro_d1.cpp`, `repro_d1b.cpp`, `repro_d2.cpp` against `lib/Debug/*.a` — throwaway, deliberately not
committed, because the defects they gate are fixed and the guard belongs in the suite once a lifecycle
that can host them exists.

## One executable, many applications: why registration is a call and not an initialiser (2026-09-13)

**What was asked, and what it turned out to be.** Port `Engine/Renderer/Vulkan/gen_spv_header.py` to a
C++ engine module. Asked twice — "is it worth it?" and "what do I lose?" — and answered with
measurements rather than opinion, which is what surfaced the larger finding. The port is still owed;
what landed first is the architecture it forced. `486649a` carries the build-system half.

**The landmine under the port.** `Applications/VulkanExample/CMakeLists.txt:98` ran
`add_custom_target(GenerateShadersSpv DEPENDS VulkanShaderCompile)` — a hand-written line inside a file
headered `GENERATED FILE - DO NOT EDIT`, with no `find_package(Vulkan)` and no `VulkanShaderCompile`
target generated anywhere in the tree. So regeneration deleted the target that produced the committed
`ShadersSpv.h`, and nothing was broken until someone re-ran `generate_cmake_files.sh`. Fixed by moving
both hand-written blocks into `Engine/customCMake.txt`, the sanctioned channel — and the migration
itself misfired twice: `customCMake.txt` is appended *after* `add_subdirectory`, so it cannot set an
inherited variable (build failure 3), and `Module::ParseList` de-duplicates identical lines, so four
bare `#` separators collapsed silently and the comment separator printed as a CMake command (build
failure 4). Verified as the fix's own gate: regenerate twice, `git diff --exit-code` clean, and
`-D__DEBUG__` present on 146 compile lines in Debug and Dev and 0 in Release.

**The measurement that decided the architecture.** The tool as an engine application needs applications
registered inside one binary. Static-archive self-registration was built and run, not argued: an
application behind an archive member yielded registry count **0 with no diagnostic**, because the linker
drops unreferenced members and `--force-load` only makes them *available*. `[basic.start.dynamic]` says
running a dynamic initialiser before `main` is implementation-defined or **deferred**, and deferral
fires on an odr-use in the same TU — which a registrar TU does not have. So registration is an
explicit call chain, guaranteed by `[expr.call]`. Costs came out second-order and are in the plan:
100 apps static **1.83 ms** launch vs eager-plugin **50.4 ms**; descriptors as **data** 1424 KB vs as
**code** 1408 KB RSS, so startup was made to execute no application code; and static linking does not
commit dead application code — 850 KB of the 100-app image stayed non-resident until run.

**Decided with the owner, in the plan.** One `hbengine` executable; `EInitLevel` so a tool skips
`Engine::Initialize`'s unconditional window-server touch; lifecycle gains `Initialize`/`Shutdown`,
states and `Pause`/`Resume` as requests (the host owns requests, the application owns state — two
atomics, deliberately); `ERunMode` per application; a generated catalogue from one `application =`
key, bare names so the compiler checks them; and per-application arenas under a **containment
contract** rather than owner-tagged allocations, which the owner declined. That last one leaves a
named hole: frees route by *current* scope (`MemoryManager::Deallocate` uses `GetScopedAllocatorID()`),
so an arena pointer freed under a wider scope reaches `free()`. `PoolAllocator.cpp:193` forwards
foreign pointers to its parent and `MultiPoolAllocator` reports Fatal and declines to free, so the
remaining direction is closed by construction — the host holds the scope across the instance's
destruction — plus a Debug tripwire on live blocks at arena teardown.

**Found, and not yet fixed.** `Logger::Flush()` spins on a flag only its IO task clears, so a
`FatalError` with no logger task hangs forever; `FallbackLog()` calls `Engine::Get()`, which asserts a
non-null instance — reachable exactly when no `Engine` exists; both example applications create a
second `OS::Application` although `Engine::Initialize` already creates one **[superseded same day: that had
already been fixed upstream by the renderer/application refactor; the entry above records the
correction]**; `Engine/dependencies.txt`
is the filename MakeBuild reads, so the `Engine/Core` and `Engine/Resource` `dependencies.txt` are
inert and neither currently links `Log` that way; `Component` is a poor base (mandatory `Update`,
engine-allocated `String` name, public non-atomic `SetState`) though its state vocabulary is reused.

**Harness lesson worth keeping:** capturing `EngineTest` output with `2>/dev/null` produced blank runs
that looked like hangs until a control run proved the harness. The test binary prints results to
stderr.

**Where it stands.** Nothing of the restructure is implemented. `.Plans/PLAN_single_executable_app_registry.md`
holds the final plan with a verification matrix and a nine-step commit sequence; the tool port is
re-scoped into it as step 9. Baseline on the current tree remains 53/53 in all three configurations.

## LinkedList's undefined `ContainsElement`, and the templates nothing ever instantiated (2026-09-13)

**What was asked, and what it turned out to be.** Implement the undefined function `LinkedList` calls.
The name is undefined since before this repo's first engine layout: `744ec75` ("Refactoring; Source ->
Engine") is a pure file move with **0 changed lines**, and the name was already missing there — so it
has never once compiled in the project's history. It survives because a template's member bodies are
compiled only when instantiated, and no test anywhere called `Remove`, `AddNext` or `AddPrevious`.
Confirmed by grep: zero call sites instantiate them.

**Deciding the semantics from the code, not from the name.** The two call sites assert
`ContainsElement(element)` where `GetNodeOf` then does `reinterpret_cast<Node*>(&element)`. That cast
only means anything if the element is a payload this list allocated — and `LinkedListNode`'s **first
member is `TType value`**, so the payload address is the node address. The precondition is therefore
*identity*, not value equality, and the class already had exactly that test: `Contains(const TType*)`
walks the list comparing `&element == ptr`. `ContainsElement` is thus a named precondition helper that
delegates to that overload — one implementation, no second walk. `FindAndRemove` corroborates it: its
`Remove(*found)` passes a reference obtained from `Find()`, i.e. a list-owned element.

**Instantiating the bodies found two more defects that could never have compiled.** (1) `Remove(const
TType& element)` passed a const reference to `GetNodeOf(TType&)`. Fixed by taking `TType&`, which does
more than compile: it makes the identity contract mechanical, since a temporary or a foreign object
can no longer bind, so the assert guards what is left. (2) `AddNext(TType&, TType&&)` called bare
`std::forward(value)`, but `remove_reference_t<T>` is a **non-deduced context**, so `T` can never be
deduced and the call is ill-formed in every instantiation — its sibling `AddPrevious` had it right as
`std::forward<TType&&>(value)`, and `AddNext` now matches.

**A fix that is not instantiated is not a fix.** Added test case `Element-Reference Mutation`, which
finds an element, splices with `AddNext`/`AddPrevious` using list-owned references, removes by
reference, then re-checks reachability, ordering and count. It selects the rvalue overloads
deliberately (passing `35` prefers `TType&&` over `const TType&`), which is what exposed defect (2).

**Verification.** Whole-tree gate detached: build **12/12** across Debug/Dev/Release, unit tests
`fail=0` in all three configurations, new case logs `[TC4.Element-Reference Mutation] Result [PASS]`,
standards debt unchanged at 80 mechanical / 27 advisory.

**Limit, stated.** The negative path — a foreign element failing the assert — is deliberately
untested, because `Assert` aborts rather than returning, so a test would crash the suite instead of
reporting. The non-const parameter is what guards that case now.

**Found, deliberately not fixed:** two test cases in `LinkedList.cpp` share the name "Growth and
Iteration" (confirmed by `uniq -d`), so one can mask the other in suite output. Renaming someone
else's test title is their call, not a formatting task's.

**Generalisable lesson.** A template API is untested code until something instantiates it. Grepping
for a symbol is not evidence it works — the only evidence is a translation unit that names it.

## A subagent review invented 36 defects, so findings are now machine-gated (2026-09-08)

**What happened.** The whole-tree judgement sweep came back with 36 findings, 140 files "reviewed",
and a confident tone. Every headline finding was invented. `ConfigFile.cpp:763` was cited as a
double-write bug in a file that is **121 lines** long; `ConfigSystem.cpp:678` in a 507-line file;
`ConfigSystem.h:124` and `:130` in a 90-line file; `Engine/OSAL/PlatformDefines.h` as a macro
redefinition in a file that **does not exist**; and `GetParam`/`SetEngineName` as APIs in files where
`grep` finds no such symbol at all. The agent also contradicted itself in its own summary — it was
handed 20 files, reported 10 as never opened, and counted 140. Its batch label said "batch 2 of 20,
covering files 233 through 298 of 1126", which is 66 files against a 1126/20 = 56 split: the batch
ranges had been computed against a pathspec-less `git ls-files` that swept in `External/` and the
docs, so the ranges did not describe the file universe the prompt promised.

**The lesson, stated plainly.** A model asked to audit files it did not open will manufacture
defects rather than report none, and the fabrication is invisible in prose but nearly free to
detect: the cited line is past the end of the file, or the file is absent, or the quoted text is not
there. Never accept an unaudited subagent report; make the citations resolve or the batch fails.

**The fix is mechanical, so it is enforced rather than requested.**
`scripts/verify-findings.py` takes a batch's findings JSON and exits non-zero unless every finding's
file exists, its line is inside the file, and its `evidence` appears verbatim on or beside that line
(whitespace-normalised, so reformatting cannot invalidate a true citation). It runs as a workflow
`gate:` after each reviewer, so a fabricating worker fails a batch instead of reaching the owner.
It was tested against the actual fabricated report before use: 5/5 rejected with
`line outside file (file has 121 lines)` and `file does not exist`, and 2/5 true citations accepted.
Reviewers now write findings to `.Plans/review/batchNN.json`, receive deterministic ranges computed
from `git ls-files '*.h' '*.cpp' '*.inl' '*.mm'`, and are told that zero findings is a correct answer
and that the only file they may write is their own findings file.

**Concurrency, per the owner.** Review runs happen off the main session, at **3** concurrent,
and the cap is not politeness — the build tree is single-occupancy. The owner also required that the
main session not absorb review work; when the fabrication broke my trust in the workers I started
running the greps myself, which was the wrong remedy: the defect was in the harness, so the fix went
there. The sweep script is persisted at `.pi/workflows/hb-review-citation-gated.js` so it is
relaunchable by name, and the stale temp journals were removed.

**Cost, recorded honestly.** Three sweeps were launched and killed and **zero batches completed**,
so the whole-tree judgement review had still not produced a single verified finding at the time of
writing. What is genuinely known in the meantime comes from deterministic checks only: exceptions
and RTTI are absent from all of `Engine/` and `Applications/`; `check.sh` reports 80 mechanical
violations, 27 advisory, 117 `[DEBT]`; and a per-file verdict table for all 276 sources is at
`.Plans/STANDARDS_PER_FILE.md`. My own `explicit` grep screen also over-reported — 6 of its 8
pointer/ref hits are move constructors, where omitting `explicit` is correct — which is the same
failure class one level down.

## hb-standards runs off the main thread now; concurrency capped at 4 (2026-09-08)

**Why.** The full gate takes minutes, so running it inline spends the caller's entire turn to learn
something a background process could have reported. `scripts/gate.sh` was added with
`spawn | status | wait | list | release`: it launches a detached `pi -p` that loads the skill, runs
`check.sh` with the arguments it was handed, and reports per the skill's own Reporting section.
Jobs live under `.pi/logs/gate/` (gitignored) holding `cmd`, `pi.log`, `done`.

**The verdict is `GATE_EXIT=<n>` inside `pi.log`, not the process exit code.** `done` records
whether that assistant survived; conflating the two reports a pass that never happened. `wait`
re-exits with `GATE_EXIT` so callers treat it exactly like `check.sh`.

**Owner policy, recorded so it survives the session:** at most **4** concurrent subagents or
detached gates, and *ask the owner before exceeding that*. Enforced by 4 `mkdir` slots (macOS ships
no `flock`), overridable with `PI_GATE_SLOTS`. The cap is not courtesy — the build tree is
single-occupancy, and two gates racing on `cmake-build-*` each report a pass the other invalidated.

**Two measured findings about headless `pi`, each costing a failed job to learn.** A detached `pi`
with no pinned provider/model dies at startup with `401 Invalid bearer token`; `--offline` alone
does not fix it — the fix is `--provider`/`--model`, inherited from `PI_PROVIDER`/`PI_MODEL`.
And `pi auth check --provider anthropic --json` answered `{"status":"ready"}` the whole time that
token was dead, so it is not a usable preflight; `gate.sh` instead spends one `--no-tools` "reply
OK" roundtrip, which fails in a second rather than burying a 401 in a log nobody opens until the end.

**Verification of the mechanism, not just the happy path.** With 4 synthetic slots held, the fifth
`spawn` was refused with exit 3; `release` kept the three live slots and freed the one whose job had
finished. A real detached run then reproduced an independent whole-tree lint exactly — 80
mechanical, 27 advisory, 117 `[DEBT]` — released its slot, and declined to claim the build gate
under `--no-build`, which is the skill's own trap list functioning.

**Three defects found while testing my own script, each fixed:** `${APPLY:+…}` expanded even when
`APPLY=0` because 0 is a non-empty string in bash, so the `--apply` permission note appeared on
every run; `resolve_job`'s `exit 3` died inside a command-substitution subshell, so `status` with no
jobs printed a fabricated `RUNNING` row and returned success; and `release` freed slots whose jobs
were still *running* while keeping stale ones — exactly inverted. All three were caught by running
the thing, not by reading it.

**Reported, not fixed:** `batch_ai_prompt.sh` uses `local` outside a function and increments its
counters inside a piped `while` subshell, so its closing tally always prints
`0 files processed, 0 failures`. Left alone as out of scope.

## Function naming returns to PascalCase — the camelCase sweep is reverted (2026-09-08)

This supersedes the entry below, which records the sweep being applied. Read both: the reasoning
below still explains what the macOS build cannot see.

**Why it went back.** The owner found the case pair `IsRunning()` / `isRunning` more intuitive than
any member-side alternative, and asked for PascalCase functions throughout. That is the coherent
choice rather than a reversal: PascalCase functions keep two registers available, so the question
and the storage can share a word and still be told apart. Under camelCase functions that free
distinction disappears and every collision needs a new noun — which is how `running`,
`isRunningFlag`, `runningState` and an `isRunningBit : 1` bit field all came to be proposed for one
`std::atomic<bool>`, three of them unusable.

**How it was undone: an exact inverse diff, not a reverse map.** `git diff 4e9e373 481f3d2 -- Engine
Applications | git apply` restored 222 files with symmetric 4,164/4,164 line counts, and the source
files are now byte-identical to `481f3d2` (`git diff 481f3d2 -- '*.h' '*.cpp' '*.inl' '*.mm'` is
empty) — the exact state that built 12/12 and passed 53 collections. A textual reverse map
(`allocate` -> `Allocate`) would have been far worse than the forward direction: 567 of the names
introduced are camelCase, and 12 of them (`get`, `set`, `size`, `data`, `count`, `find`, `clear`,
`insert`, `pop`, `push`, `release`, `reserve`, `reset`, `resize`, `store`) are also spellings that
std and third-party code use as member names, so `.get()` and `.count()` would be rewritten and the
compiler would only catch it by breaking loudly. It also would have silently mangled placement `new`
and `operator delete`. Reverting a diff has no such problem: it inverts the change, not the spelling.

**Consequences that fixed themselves.** The `"Prepare()"` -> `"prepare()"` string-literal change and
the OS-API calls the rename damaged inside Windows/Linux-gated regions (`OS::VirtualAlloc`,
`SetThreadPriority`) are restored by the same diff — which is the practical argument for the exact
inverse: gated code is repaired without ever being compiled here. The three header comments naming
renamed compounds also needed no correction, because the names they cite are correct again.

**Audit for functions that were camelCase before the sweep, so none survive.** The compiler is the
only honest oracle here. `clang -Xclang -ast-dump -ast-dump-filter=^[a-z]` over all 129 compile-DB
translation units reports **zero** lowercase-named functions declared in engine or application
sources, and a textual pass restricted to `PLATFORM_WINDOWS` / `PLATFORM_LINUX` /
`PROFILE_ENABLED` regions reports zero as well. A regex sweep had flagged 11 candidates; 10 were
`std::abort`, `std::copy`, `std::sort`, `signal`, `memcpy` and friends — call sites, not
declarations. That is why the AST was worth the minutes.

**Two exceptions that must stay lowercase, by requirement rather than taste.** `main()` is
mandated by the language. `Allocator::allocate` / `Allocator::deallocate` in
`Engine/String/StaticStringTable.h:41` are the C++ allocator protocol — renaming them to
`Allocate`/`Deallocate` breaks any standard container instantiated with that allocator. They are
exemptions, not leftovers.

**Lesson worth keeping.** `HasDone` and `HasPendingTasks` had been kept PascalCase because a
compiler diagnostic on the same line was attributed to them; no `hasDone`/`hasPendingTasks`
identifier existed anywhere, so the deferral was collateral, not evidence. A deferral justified by a
compiler message is only as good as that message's attribution — harvest offenders by the
identifier the message names, not by the line it appeared on.

**Verification.** Build gate 12/12 (EngineTest, VulkanExample, WindowExample, CodingStandards ×
Debug/Dev/Release). Unit tests `# Fail = 0`, `all 53 collections passed`, exit 0 in all three
configurations. Standards debt unchanged in kind and slightly better in count: 82 -> 80 mechanical,
27 advisory, same five categories.

**Documentation state after the reversal.** `docs/CodingStandards.md` was never edited for the
camelCase rule, so its "Classes, Functions, and Types: PascalCase" bullet is correct again. The
stale one is now `docs/EngineAPIGuide.md`, which documents camelCase — it needs the same
reversal, left undone pending the owner's word because the standing instruction was not to revise
`.md` files. `Applications/EngineTest/TestMain.cpp:52` prints a hint instead of running when
`__UNIT_TEST__` is undefined, so `check.sh --all` reconfiguring without `-D__TEST__` silently turns
`EngineTest` into a stub; rebuild with `./build.sh Applications/EngineTest -test` before trusting a
test run.

## Functions become camelCase — 567 renames, and what the macOS build could not see (2026-09-08)

The owner ruled that functions use camelCase, and that the **code** changes while the
Markdown stays as it was. `docs/CodingStandards.md` therefore still tells readers that
functions are `PascalCase`, and `docs/EngineAPIGuide.md` — which always said camelCase —
is now the only document that agrees with the tree. That contradiction is the instructed
outcome, not an oversight, and it is the first thing to settle when the docs are opened.

567 identifiers moved across 222 files, ~4518 occurrences, all of `Engine/` and
`Applications/`. `Examples/MacOSApp` was left alone deliberately: it includes no engine
header at all, so it is a standalone sample that no build target compiles, and renaming
inside it would have been unverifiable churn.

Nothing was trusted to the rename script alone. Five structural checks run against the
committed blobs: string and char literals unchanged, declared type names unchanged,
macro names unchanged, no un-renamed occurrence left, and total word-token count equal
(91187 before and after). Then 12/12 builds and 53/53 test collections in Debug, Dev and
Release. One literal is a declared exception to the literal check, and the check *proves*
it rather than skipping the file: `StringUtil.cpp` compares `__PRETTY_FUNCTION__` against
a hardcoded expectation, so `"Prepare()"` had to follow `Prepare` → `prepare` or the name
conversion tests would fail for the right reason.

The hold-backs are the substance of this change, because each one is a place where a
textual rename is simply not sound. **31 accessors cannot become camelCase as long as
their members are camelCase too**: `IsRunning()` returns `isRunning`, `NumberOfFreeBlocks()`
returns `numberOfFreeBlocks`, `Length()`, `Size()`, `Count()`, `Capacity()`, `IsValid()`,
`Value()`, `Swap()`, `Column()` and twenty more — renaming one collides with the field it
reads, and the standard forbids the `m_` prefix that would separate them. Resolving these
needs a member-naming decision, not a script. Then **`New`, `Delete`, `Register`, `Yield`**
are C++ keywords and **`Assert`, `Min`, `Max`** are standard macros, so their camel forms
cannot exist. Then the OSAL layer names functions exactly like the operating system does:
`OS::VirtualAlloc` is a real engine symbol, and so is Win32 `VirtualAlloc`, in the same
argument lists — `VirtualAlloc`, `VirtualFree`, `SetThreadPriority`, `SetThreadAffinityMask`,
`GetSystemInfo` and friends are held back because no textual rule can tell them apart.
Same for the POSIX set: `Open`, `Read`, `Write`, `Close`, `Truncate`, `Sleep`, `Pow`, `Abs`,
`Remove`, `Unlink`. Finally, no name was renamed on the strength of code this machine
cannot compile: declarations under `#ifdef PLATFORM_WINDOWS`, `PLATFORM_LINUX` or
`#if PROFILE_ENABLED` (which is `0` in every configuration) never entered the map.

Three defects in the tooling were caught by checks rather than seen in review, and one was
caught only by a subagent. The first version rewrote `#include "Log/Logger.h"` to
`"log/Logger.h"` — its include guard had inverted logic and *unprotected* those lines; the
macOS filesystem is case-insensitive, so the build stayed green while Linux would have
died. A lookbehind that excluded `.` skipped every member access `obj.GetID()`, and the
residue check used the same pattern, so both were blind together — the classic way a green
verification means nothing. The third was the expensive one: a shape test accepted
statement-level calls such as `XStoreName(display, window, title);` as declarations, which
put Win32 and X11 APIs into the map and renamed 29 real call sites inside
`Win32Window.cpp`, `LinuxWindow.cpp`, `WindowsMemory.cpp`, `WindowsThread.cpp`,
`WindowsAbstractLayer.cpp` and `WindowsDebug.cpp`. Every one of those files is excluded
from the macOS build, so **zero** local signal existed: builds passed 12/12 and all tests
passed the whole time it was broken. It is the reviewer's report that named them, and the
map is now built so those symbols cannot enter it.

Unfixed, and worth knowing before anyone trusts a Windows build: `Win32Window.cpp` cannot
compile for reasons the rename did not cause — `Window::windowProc` is defined and called
but declared in no header, and `GWLP_DLPTR` is not a Win32 constant (`GWLP_USERDATA` is).
Log strings and `addTest("…")` titles still name functions that no longer exist, because
literals were protected by policy. Both are follow-ups, not regressions.

## Rule adopted: no comments in `.cpp` files — and the sweep that has not run yet (2026-09-08)

`AGENTS.md` and `docs/CodingStandards.md` now forbid comments in `.cpp` files: implementation
files are self-documented, and explanation moves to where a reader forms intent — engine-user
material to the paired `.h`, implementation and system-design material to a design document
under `docs/`. Three exemptions, each argued rather than assumed. The line-1 copyright notice
stays: it is a legal notice, not documentation, and `check.sh:246` requires it on line 1, so
forbidding it would fail every file in the tree. `Engine/CodingStandards.cpp` stays commented:
it is the rule's own teaching exemplar and carries the BAD EXAMPLE commentary that keeps its
anti-patterns recognisable, which is the same reasoning `check.sh:205` already uses to exempt
it from behavioural checks. And structural labels stay — `#endif // PROFILE_ENABLED`,
`} // namespace hbe`, `}} // namespace hbe::StringUtil`, `#else // !__DEBUG__` — because a bare
`#endif` is not self-documenting, it cannot say which `#if` it closes, so the label serves the
rule instead of evading it. The permission is deliberately narrow: the comment must name only
the closed construct, so `} // namespace hbe  // TODO: rename` is deleted, and so are the 94
`// 'A' (65)` glyph labels in `Framebuffer.cpp`, whose index position already encodes.

Measured scope before any edit: 753 comment lines across 122 of 132 tracked `.cpp` files, of
which 344 are closing labels and 94 glyph indices. With those two exempt, the actual sweep is
**40 files and 607 comment spans** — 82 files turned out to hold nothing but a copyright line
and labels. It has **not** been applied. It will not be applied until the content worth keeping
has landed in headers and `docs/`, because deleting first is the one ordering that cannot be
undone.

The removal itself is a character state machine, not a regex — `//` and `/*` occur inside
string literals, and rewriting one byte of program output under the banner of a style change is
a behavioural edit. Its neutrality was proved rather than asserted: the tree was copied to two
mirrors, one swept, and every file preprocessed inside its own tree with the real flags from
`cmake-build-debug/compile_commands.json`, then compared with whitespace stripped. The
preprocessor deletes comments, so identical token streams means no code changed. **131 of 131
files identical, 0 changed, 0 unverified.** Three traps had to be fixed before that number meant
anything: sharing one include environment between the two copies sends identical files down
different header chains (a false 33-file failure), leaving `argv[0]` in a compile-DB command
makes clang read its own path as an input, and raw `-E -P` output differs by whitespace so the
comparison must be normalised.

Auditing what would be lost inverted one premise and found live documentation lies. The highest
value allocator knowledge is **not** commented at all: free-list links are block indices stored
inside the free blocks with `numberOfBlocks` as the end sentinel rather than null, the
`ThreadSafeMultiPoolAllocator` deliberately reports statistics *outside* its lock to avoid an
ordering inversion with `statsLock`, and allocator IDs are captured at construction not at
allocation — all of it carried today only by identifier choice and brace nesting. Meanwhile
`StackAllocator.h` claimed deallocations are unsupported while `StackAllocator.cpp:91` implements
them as strict LIFO (fixed in this change); `MonotonicAllocator.h` claims individual
deallocations are ignored while `MonotonicAllocator.cpp:93` forwards foreign pointers to the
parent; and `VulkanCapabilities.h:20` says a null handle leaves the descriptor untouched while
`VulkanCapabilities.cpp:82` resets it before the null check. Those last two are verified and
still unfixed.

Reviewing `docs/` before writing into it changed the plan. `docs/RendererDesign.md` and its
`.html` describe HLSL-single-source, bindless and render-graph architecture that was never
built — zero mentions of `VulkanRenderer`, `RenderCapabilities` or `PushConstants` — and carry no
banner saying so, while the md (881 lines) and html (430) have different section titles, so they
are parallel rewrites rather than a rendered pair. `docs/design/LightweightRenderer_Design.md`
handles this correctly with a §0 status table reconciling each unshipped section, which makes it
the pattern to copy; its weakness is that the shipped renderer's only home is a status note
inside a superseded proposal, so the renderer that exists still has no design document.
`docs/EngineAPIGuide.md:1806` tells readers "All other identifiers: camelCase", contradicting
both `docs/CodingStandards.md` and the code (`Allocate`, `FallbackAllocate`,
`DeregisterSystemAllocator`), and its §Coding Standards is a fourth copy of the standard that
already omits the rule added today. Design documents are to be maintained as coincident
`.md` + `.html` pairs; there is no generator, the HTML is hand-authored, and that is why these
pairs drift.

Still outstanding: the documentation restructure, roughly 19 header blocks and 36 design blocks
to place, then the 40-file sweep and the Dev/Debug/Release gate. The classification inventory
lived only in analysis output and is not in the repo, so expect to re-derive it. Unrelated bugs
surfaced by the same reading are logged separately in `ReviewNote` form rather than fixed here:
a first-seen block size dereferences `end()` in `MultiPoolAllocator.cpp:283-291`,
`ThreadSafeMultiPoolAllocator`'s destructor reads `banks` before taking its lock, and
`Task.cpp:52` may divide by the `numSubTasks` member rather than the parameter.

## `EngineTest` now says so when it was built without `-test` (2026-09-07)

`Applications/EngineTest/TestMain.cpp` compiled to an empty `main()` when `__UNIT_TEST__`
was off: it exited 0 having verified nothing, which is indistinguishable from a passing
suite and is precisely what let the standards gate once report success over zero tests.
Added an `#else` branch that prints how to get the macro on (`build.sh ... -test`, the
binary paths, and `check.sh --test`) and returns 1. Verified both halves: built without
`-test` the guide prints and `rc=1`; built with it, 53/53 collections pass and `rc=0`.
Gate green: 12/12 builds, 53/53 in Dev/Debug/Release. Also dropped the stray second blank
line after the includes, which clang-format had been rewriting on every run.

The `#else` branch cannot alter the tested path, and that was checked rather than assumed:
the two versions of the file produce **byte-identical objects** under `-D__UNIT_TEST__`.
That mattered, because a `Fail=2` then `Fail=1` result appeared immediately after the edit.
Same binary, two different tallies, so at least one test is nondeterministic - `BufferTest`
TC3 (File Buffer) is the one that came and went without a rebuild in between. 25 consecutive
runs at current HEAD are clean, so it is rare rather than constant, but a suite that can
report differently from the same binary is a suite that cannot be gated on yet.
`PoolAllocatorTest` TC2 also moved between builds and runs and has since been worked on
separately in `9bcecab`.

One operational lesson: the gate builds whatever the shared `build/` tree holds at that
moment, so a run that overlaps someone else's editing can compile code that was never
committed. Two of the results above changed for exactly that reason.

## `Assert` made config-independent, pool blocks aligned to 16, one LinkedList size (2026-09-06)

Owner picked three of the four open items; `Renderer::Vertex` stays as dead code by decision.

### 1. `Engine/Core/Debug.h` — both branches now declare the same thing

The release branch had `Assert(bool, const char*, Types&&...)`; debug had `Assert(bool, Types&&...)`.
Verified on the old header rather than assumed:

```cpp
hbe::Assert(value > limit, value, " exceeds ", limit);   // message starts with an int
```

compiles under `__DEBUG__`, and on the previous header gives **`error: no matching function for call
to 'Assert'`** in Release. Both branches now declare the plain variadic pair, and both say
`noexcept`: a `noexcept` difference alone is enough to flip `std::is_nothrow_*` traits between
configurations, which is a second, subtler way for Release to disagree.

Two honest qualifications:

- **Scope correction.** I had described the old trap as "a message built from other types". Too
  broad — the old overload demanded a `const char*` only as the *first* message argument, so
  `Assert(c, "value ", value, " bad")` was always fine. Only messages *beginning* with a
  non-string broke.
- **Preventive, not a bug fix.** Release built clean at `HEAD`, so no existing call site trips
  it. This closes a trap that was armed by `__DEBUG__` becoming the default experience.
- Arguments with side effects are still evaluated in Debug and dropped in Release. Inherent to
  any assert; noted in the header rather than papered over.

### 2. `PoolAllocator` now aligns blocks to `Config::DefaultAlign` (16)

`OS::GetAligned(std::max(inBlockSize, sizeof(TSize)), sizeof(TSize))` became
`..., Config::DefaultAlign)`, matching `InlineMonotonicAllocator` and the engine-wide assumption.

Aligning the *stride* is not the same as aligning the *blocks*, so the new test case checks the
address with `OS::CheckAligned` over 384 blocks. Proven to have teeth: reverting just that one
argument to 8 reports `blockSize 24: block 1 is not aligned to Config::DefaultAlign`, with the
rounding expectations adjusted to match align-8 truth so the address check is the thing that
fires. So the change moves real addresses, and the guarantee is now pinned by a test.

Side effect: `LinkedList` nodes are 24 bytes and now occupy 32-byte blocks, so its pools are
~33% larger — and, for the first time, correctly aligned.

### 3. `LinkedListTest` runs the same size everywhere

The `#ifdef __DEBUG__ CountBase * 2 #else * 16 #endif` is gone; one `CountBase * 16`. The smaller
development build was testing an eighth of what the shipped build tested.

I first wrote that this was the last place where `__DEBUG__` changed *what* got tested. It was
not — `ComponentSystem.cpp:17` does the same thing more aggressively, 1024 components and 60
updates under `__DEBUG__` against 20480 and 600 otherwise, a 20x gap. Left alone because the
instruction was about the `* 16`, but with `__DEBUG__` now the default that test runs a twentieth
of its work in every development build. Flagged, not fixed.

Measured cost of running 8x more work under live asserts: Dev 11.0s, Release 10.7s, Debug 22.9s.

### 4. Verification

| Config | `-D__DEBUG__` targets | Suite |
|---|---|---|
| Debug | 146 | 286 PASS / 0 FAIL / exit 0 |
| Dev | 146 | 286 PASS / 0 FAIL / exit 0 |
| Release | 0 | 286 PASS / 0 FAIL / exit 0 |

Also caught mid-verification: an `echo "... exit=$? ..."` that reported my own `grep -c` status
rather than the test binary's, briefly making Release look like it failed. Measured properly, all
three exit 0 with 53 collections green.

### 5. Process failure during this commit, and the repair

`git status` showed the other worker's `Applications/EngineTest/TestMain.cpp` guard with `M` in
the **first** column, which means *staged by them*. Plain `git commit` commits the whole index,
not just the files named in the message, so their work landed in my commit — while my commit
message asserted it was not included. Nothing was pushed, so: `git reset --soft HEAD~1`, then
`git commit -- <four paths>` to commit only my paths, which leaves the rest of the index
untouched. Their file is staged again exactly as it was, for them to commit themselves.

The trap is that staging looks like someone else's business right up until your own commit acts
on it. Pathspec on the commit is the fix, not care in `git add`.

## `__DEBUG__` on by default, clamp-path coverage, and a correction to my own numbers (2026-09-06)

### 1. `__DEBUG__` is now defined for Debug and Dev

One line in the root `CMakeLists.txt`:

```cmake
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -D__DEBUG__")
```

That is the whole change, and the reason it is one line is worth knowing: these `set()` calls
are copy-pasted into **26** `CMakeLists.txt` files, but every one of them *appends to the value
it inherits* rather than replacing it, so a define added at the root reaches every target. The
`DEV` flags are derived from `DEBUG` on the next line, so Dev inherits the define; `RELEASE` is
a separate variable and stays without it. Checked against real compile commands, not the
CMakeLists: 146 targets carry `-D__DEBUG__` in Debug and Dev, **0** in Release.

`PROFILE_ENABLED` is a hardcoded `0` in `BuildConfig.h`, so this does *not* drag the profiling
machinery along with it - that was the main risk and it is not one.

Cost, accepted knowingly: `LinkedList.cpp` sizes its case at `CountBase * 2` under `__DEBUG__`
vs `* 16` otherwise, so those tests now exercise an **8x smaller** list.

### 2. Clamp path gained real coverage, which is how my earlier numbers died

New `PoolAllocatorTest` case "Allocation With A Clamped Block Size": builds pools at requested
sizes 24, 26 and 100, checks the block size the pool settled on, then allocates the whole pool
and fails if any two allocations share an address, deallocates, and does it again. A second
round is the point - a mis-linked free list often survives the first pass.

It passed with expectations **24 -> 24, 26 -> 32, 100 -> 104**, and those values came from the
real allocator, not from me.

### 3. Correction: my two previous commits got the alignment wrong

I wrote that `blockSize` is rounded to `Config::DefaultAlign` (16) and that
`sizeof(LinkedListNode<int>) == 24` therefore sat in the broken class, rounding to 32. **Wrong.**
The constructor is:

```cpp
OS::GetAligned(std::max(inBlockSize, sizeof(TSize)), sizeof(TSize))   // align = 8, not DefaultAlign
```

`OS::GetAligned(size, alignBytes)` rounds up to `alignBytes`, and the argument passed is
`sizeof(TSize)` = **8**. So 24 is already aligned, stays 24, and **`LinkedList` was never
affected**. The real broken set is any size that is not a multiple of 8: 9 -> 16, 26 -> 32,
100 -> 104. Measured with the transcription re-run at align 8, 100 allocations from a 100-block
pool: 26 and 100 gave **2** distinct addresses before the fix, 100 after.

The fix itself stands unchanged - it made the body use the member whatever the member is - but
the diagnosis that justified it, and the `24 -> 32` figure in commit `9fd2cf7` and in my earlier
journal entry here, were wrong. Not rewritten: `47ac54e` landed between those commits and this
one, so rebasing would rewrite someone else's history to fix my prose.

### 4. New finding, deliberately not touched

`PoolAllocator` aligns blocks to `sizeof(TSize)` = 8, while `InlineMonotonicAllocator` aligns to
`Config::DefaultAlign` = 16. Pool blocks can therefore start on an 8-byte boundary while the
rest of the engine assumes 16. Whether that is a real hazard depends on what gets placed in
pools; it is an owner call, not something to change as a side effect.

### 5. Verification

| Config | `-D__DEBUG__` targets | Suite |
|---|---|---|
| Debug | 146 | 286 PASS / 0 FAIL |
| Dev | 146 | 286 PASS / 0 FAIL |
| Release | **0** (correct) | 286 PASS / 0 FAIL |

`VulkanExample` still links at 140/140 with asserts live. TC1's failure string said "100
expected" while testing against 4096; corrected.

**Does the new test have teeth?** Tested, and the honest answer is partly. Re-injecting the old
raw-stride bug makes the run trap at case 25 inside `PoolAllocatorTest TC0` - the destructor
frees `aligned * n` for a `raw * n` allocation - which is fatal long before TC2 executes, so I
could **not** observe TC2 itself firing. TC2's real job is pinning the documented rounding
values and giving the clamp path allocate/deallocate coverage; the loud failure is covered by
TC0 plus live asserts.

## `hb-standards`: four holes in the helper script, and one rule I had wrong (2026-09-07)

Four defects in `.pi/skills/hb-standards/scripts/check.sh` were reported by the owner as
measured behaviour. All four are now fixed, and chasing them exposed a fifth of the same
shape plus a rule that I had previously documented incorrectly.

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | `EngineTest` reported `[FAIL]` although the binary was fine | `timeout 120` called unconditionally; **neither `timeout` nor `gtimeout` exists on this machine**, so the call exited 127 | probe `timeout` then `gtimeout`, run uncapped if neither exists. A missing helper is an environment gap, never a code failure |
| 2 | Test gate passed with `PASS=0` | built without `-test`, so `#ifdef __UNIT_TEST__` left `main()` **empty**: exit 0 having run nothing. And `grep -c PASS` counted log lines, not tests | build with `-test`, parse the `TestEnv::Report()` tallies, and treat 0 tests, a missing report block, and a timeout as failures. Reconfigure without the defines afterwards |
| 3 | `Engine/Config/BuildConfig.h`: "block 2: not alphabetical" while having no includes at all | awk matched `/^#(include\|define)/`, so `HB_PROJECT_*` macros became fake include blocks | match `#include` only |
| 4 | ~50 guarded files reported "found 0" empty lines | any non-include line set `bodyline`, which froze the blank counter; and `blank` measured the gap *before the last include*, not after the region | conditionals made transparent; preamble closes exactly once, at the first body line |

### Why hole 4 was deeper than it looked

The dominant idiom in this tree puts further `#include` directives inside a
`#ifdef __UNIT_TEST__` test block far below the first code body, so treating the whole
file as one include region was simply the wrong model. A conditional that *hugs* the
includes is part of the preamble; a conditional preceded by a blank line opens a new
region. That distinction is what made `ImportanceSampling.cpp`, `DefaultAllocator.cpp`
and `WindowsDebug.cpp` stop being false positives.

Measured over 256 files of `Engine/` + `Applications/`: findings **100 -> 55**, and the
awk now contradicts clang-format **nowhere** (0 "not alphabetical" findings on files the
formatter leaves untouched, which is the expected correlation under `IncludeBlocks:
Preserve`). Eleven synthetic probes cover the exempt and failing shapes.

### The rule I had documented wrong

I had written into `.clang-format` and `docs/CodingStandards.md` that clang-format
collapses *any* blank-line count after the includes to exactly one. That measurement was
buggy. Re-measured with `MaxBlankLines` at its default:

| Body starts with | survives | collapses |
|---|---|---|
| `using namespace` | 1 **or 2** | 3+ -> 2 |
| `namespace` declaration | 1 only | 2+ -> 1 |
| a function | 1 only | 2+ -> 1 |
| a comment | 1 only | 2+ -> 1 |

So two blanks is legal in exactly one position. Both documents now say that, and the
blank-line rule is left to clang-format entirely rather than being copied badly in awk.

### Hole 5, same shape as hole 2

`HEAD` at the time touched no C++, so "0 violations" meant the lint had examined nothing
- a vacuous pass, indistinguishable from a real one. An empty scope now says so on its
own line, including on the `--no-build` early exit, which was silently skipping the notice.

### Decision: `__UNIT_TEST__` stays driven by `-test`

`MakeBuild` already emits `.project.config`'s `precompileDefinitions` into **every**
generated `CMakeLists.txt`, so a `precompileGlobalDefinitions` key would have duplicated
an existing mechanism. The owner declined it anyway for the right reason: `.project.config`
is project-wide and would push the macro into `VulkanExample` and `WindowExample` too.
Module-scoped injection from `EngineTest/.module.config` cannot work either, because the
test bodies live in the *library* sources that `EngineTest` links. `-test`, which turns the
macro on for the whole build tree, is the arrangement that keeps `__UNIT_TEST__`-dependent
code from ever being linked across a mismatch. `__TEST__` is referenced nowhere in
`Engine/` or `Applications/` and is dead weight in that flag.

Two things this exposed and did not fix, both deferred: only `Engine/Test` and
`Engine/Renderer/Vulkan` declare `__UNIT_TEST__`, so **108 modules compile with their test
bodies silently removed** unless `-test` is passed; and `Engine/CMakeLists.txt` is generated
by `MakeBuild`, so the hand-added `CodingStandards` target from `1e6a46e` will not survive
the next `generate_cmake_files.sh`.

### Controls

`566e139` -> exit 1 (still catches `Main.cpp` placing the `<...>` block after the project
block), `1e6a46e` -> 0, `ea0f157` -> 0, `bash -n` clean. `HEAD` (`PoolAllocator`) -> exit 1
on two genuine violations, which is a correct verdict rather than a script fault.

## `PoolAllocator`: `in`-prefixed parameters, which also repaired a live defect (2026-09-06)

Owner asked for `in`-prefixed parameters on `PoolAllocator` to stop member/parameter
shadowing. Doing it exposed that the shadowing was not cosmetic: it was corrupting the
free list. `Engine/Memory/PoolAllocator.{h,cpp}`.

### The mechanism

The ctor took `blockSize`/`numberOfBlocks`, shadowing the members of the same name. A
ctor parameter is scoped inside the class, so the initialiser used the parameter while
the body used the parameter too - and the *member* was the only thing ever clamped.

| Site | Before | After |
|---|---|---|
| member init (line 21) | `align_up(max(raw, 8), 16)` | unchanged |
| buffer size + free-list link stride (ctor body) | **raw parameter** | **member (aligned)** |
| `AllocateBlock` / `Deallocate` / `GetCapacity` | member (aligned) | member (aligned) |

So the pool wrote its links with one stride and walked them with another. Only a
`blockSize` already a multiple of `DefaultAlign` (16) escaped, because then raw == member.

### Measured, by transcription of the real code paths

100 allocations from a 100-block pool, counting *distinct* addresses returned:

| `blockSize` | member | distinct before | distinct after |
|---|---|---|---|
| 16, 32 (aligned) | equal | 100 | 100 |
| 24 = `sizeof(LinkedListNode<int>)` | 32 | **2** | 100 |
| 100 | 112 | **2** | 100 |

`sizeof(LinkedListNode<int>)` measured at 24, so `LinkedList.cpp`'s pools were in the
broken class. The failure mode is silent aliasing - two live objects sharing one block -
not an overflow.

### Two of my own earlier conclusions, corrected

- I first predicted a heap write past the buffer. It does not happen: fresh `mmap` pages
  read back as zero, so the walk re-serves the same two blocks instead. Superseded.
- Earlier I framed line 28 as "either the clamp is dead or the assert is wrong". Neither:
  **the assert is correct and load-bearing** (a free block must hold the `TSize` link),
  the clamp was the defect. `Assert(inBlockSize >= sizeof(TSize))` is now labelled a
  precondition so nobody clamps it away again.

### Verification

Build clean, 285 PASS / 0 FAIL, 5/5 runs. The gate's 3 findings on these files are all
pre-existing at `HEAD` (459 + 147 non-conformant lines already; line 88's joined empty
body is verbatim at `HEAD`) - the indented-namespace-body debt the gate itself marks
`[DEBT] ... not gated on purpose`. Not reformatted, per standing instruction.

### Decision: a sub-`TSize` block is a caller error (option A)

Presented with three options - reject (fix the test), accept (drop the assert), accept
observably (clamp + warning). Owner chose **reject**, so the precondition stays and the
test stopped probing illegal inputs: `for (size_t blockSize = sizeof(size_t); blockSize < 100; ++blockSize)`.
Named via `size_t` because `TSize` is declared above `private:` inside a `class`, so it is
private and `PoolAllocatorTest` cannot reach it.

Sizing the choice first: with **only** that assert removed and `-D__DEBUG__` live, the suite
ran **285 PASS / 0 FAIL** - it was the sole assert in the engine that failed, hence the last
blocker to enabling `__DEBUG__`. With the loop corrected instead, `-D__DEBUG__` also gives
**285 PASS / 0 FAIL / exit 0**, verified with `-D__DEBUG__` confirmed present in
`CMAKE_CXX_FLAGS`. The suite is clean under live asserts for the first time.

### Still open

- No test allocates from a pool whose `blockSize` needed clamping. TC1 uses 4096, already
  16-aligned, so the clamp path - the one that was broken - has zero engine coverage. My
  transcription covered it (100/100 distinct at 24 and 100) but the suite does not.
- TC1's failure string is stale: it tests `size != 4096` but prints "but 100 expected".
- `__DEBUG__` is now safe to turn on for Debug/Dev but is still not defined by default -
  a build-configuration decision, not a code one.
- One SIGSEGV observed at `WindowTest TC3.Visibility` (52 PASS), unreproducible in the 5
  runs after it, and again unreproducible in the runs since. Unrelated to `Memory/`; kept
  as a flake to watch, not a finding. Note my first "control" for it was one run each way
  and so proved nothing - the crash was noise, not a regression.

## Docs corrected, `NamespaceIndentation: None` decided, exemplars compiled (2026-09-06)

Three owner instructions executed: fix the wrong things in the docs, confirm
`NamespaceIndentation: None`, and make the engine build `CodingStandards.*`.

### 1. Documentation corrections

| File | Was wrong | Now |
|---|---|---|
| `docs/CodingStandards.md` | "K&R variant — opening brace **on the same line**", example `type FunctionName(args) {` | Allman with a real example, plus the explicit "not K&R / not BSD" distinction and the no-exemption empty-body rule with its `AllowShortFunctionsOnASingleLine: None` coupling |
| same | "remaining includes sorted alphabetically" + "two empty lines" | 3 blocks (own header / `<standard>` / `"project"`), **exactly one** blank line, with the reason stated |
| same | namespace rule stated as a bare preference | marked a confirmed owner decision |
| `docs/HelperScript.md` + `AGENTS.md` | `-notest` "skip running Engine tests after build" | **`-notest` does not exist.** `build.sh` only builds and never runs tests; the real flag is `-test`, and it does the opposite of what the doc implied — it adds `-D__TEST__ -D__UNIT_TEST__` so the test sources compile at all |

The `-test` discovery matters for build integrity: without it `TestMain.cpp`
compiles to an empty `main`, so **`EngineTest` builds green while testing nothing.**

While auditing I also found the exemplar header itself asserted "Macros (e.g.
Assert)" — `Assert()` and `FatalAssert()` are ordinary inline functions in
`Core/Debug.h`, not macros. Corrected.

### 2. `NamespaceIndentation: None` confirmed

Not a default — a decision, now recorded in `.clang-format`, `docs/CodingStandards.md`
and SKILL.md. ~218 indented engine files are legacy debt awaiting a sweep rather than
an open question. Kept as `[DEBT]` and not gated: failing every commit that happens to
touch one of those files would block unrelated work. `--apply` de-indenting such a body
is the rule working, not collateral damage.

### 3. `CodingStandards.{h,cpp}` is now a real target — the root cause is closed

It belonged to **no** CMake target, which is exactly why it could drift into
4-space indentation in a tabs-only tree, a backwards brace rule, and an include
layout its own formatter rejects. `Engine/CMakeLists.txt` now defines a
`CodingStandards` STATIC target, compiled with the project's own `-Wall -Werror`.
Verified in the default `all` build (steps 128/141 and 141/145 of a ninja dry-run),
so a plain engine build compiles it. Not linked into HEngine and not installed into
`lib/` — it is a documentation artefact and must not reach a shipping binary.

Adding it to a `-Werror` build immediately caught a latent defect: `Processor::extraData`
was declared, never initialised and never used → `-Wunused-private-field`. Fixed by
giving it a purpose, which incidentally made the composition exemplar demonstrate two
rules it previously only described: a member initializer list and an `in`-prefixed
colliding parameter (`inExtraData`). Clean under `-Wall -Werror` and `-Wextra`.

### Verification

Gate extended to 12 checks (9 application builds + `CodingStandards` × 3 configs).
The hardcoded verdict said "9/9" and was made dynamic.

`[PASS]` all 12 · lint controls: positive exit 0, negative exit 1 reporting the
originally reported include inversion.

Note the gate went from FAIL to PASS unaided — the earlier failure was the owner's
in-flight device-capability refactor, correctly left untouched.

### Still open

107-file / 650-line sweep for the no-exemption brace rule (owner deferred it while
finishing concurrent work) and the `NamespaceIndentation` legacy sweep.

## `hb-standards` skill + the two-blank-line convention is unsatisfiable (2026-09-06)

Added `.pi/skills/hb-standards/` (project-local, per owner instruction). It lints
the files **a commit touched** in two independent layers, then gates on builds.

**The two layers are both necessary, and this is now proven rather than assumed:**
the standard's own exemplar files were clang-format-clean but rule-non-clean, so
formatter-green is not standards-green.

**Convention retired — `Engine/CodingStandards.h` now says ONE empty line after the
include block, not two.** This is not a taste change: clang-format collapses any
count of blank lines after the include block to exactly one — measured for 1, 2, 3
and 4 — and this build exposes no `BreakAfterIncludes` to opt out. Keeping "two"
would mean every formatted file is rewritten on every run. I had actually hand-edited
the exemplars to two blank lines to satisfy my own new check before noticing the
conflict; the check was wrong, not the files. `docs/CodingStandards.md` still says
two and remains stale.

**Validated with controls, because an unvalidated linter is worse than none.**
- Positive control, HEAD → exit 0, 0 violations.
- Negative control, the commit that shipped `Main.cpp` → exit 1, and it reports
  the *original* complaint: "standard <...> block must precede the project block".
- A third commit surfaced real defects in unrelated in-flight files.

Controls caught four of my own bugs: `set -u` aborting on `VIOL` before
initialisation; blank lines never opening a new include block, which hid the very
violation the skill exists to catch; a greedy `sub(/.*[<"]/,...)` that extracted an
**empty** path for every quoted include, silently disabling both the alphabetical
check and own-header detection; and a brace pattern that false-matched
`, extent{}` value-initialisers as empty bodies.

**Exclusions, each with measured evidence, in the skill header and SKILL.md:**
`.mm`/`.m` (clang-format calls them Objective-C, aborts with exit 1 and writes
nothing — and from a directory where the config is not found it silently rewrites
them with LLVM defaults, 2249 → 2400 bytes, exit 0); `.inl` (`MatrixCommonImpl.inl`
is `#include`d inside a class body inside a namespace, so standalone formatting
de-indents it); generated headers such as `ShadersSpv.h`.

**Gate behaviour verified:** the gate `touch`es touched files so "ninja: no work to
do" cannot fake a pass. Running it end to end it correctly reported FAIL — the
failure is the owner's uncommitted device-capability refactor (`RenderCapabilities.{h,cpp}`
and `Vertex.{h,cpp}` are new, `RendererCommon.h` on disk has dropped `apiType`,
`maxTextureSize`, `maxVertexAttribs` which `RHICapabilities.cpp` still references;
all three are still present at HEAD). Left untouched. Added an "Attributing a gate
failure" section so this misread is not repeated.

**Also self-inflicted and caught by verification:** while editing `.clang-format`
comments I accidentally commented out `IncludeBlocks: Preserve`. The
resolved-config diff against HEAD exposed it immediately; repaired and re-verified
to an empty delta, i.e. behaviour-identical.

**Still open:** `docs/CodingStandards.md` (brace style + blank lines + `-notest`);
the 107-file / 650-line sweep for the no-exemption brace rule; the
`NamespaceIndentation` owner decision.

## Empty bodies now break their braces onto separate lines (2026-09-06)

Owner preference: no brace exemptions at all. `void Foo() noexcept\n{\n}` and
`struct Empty\n{\n};`, never joined forms.

**The non-obvious part, and the reason a naive fix fails:** `SplitEmptyFunction`
is NOT sufficient on its own. With `AllowShortFunctionsOnASingleLine: Empty`
inherited from LLVM, setting `SplitEmptyFunction: true` changes **nothing** —
`Empty` collapses `void Foo() {}` back onto the declaration line and wins. Both
options must move together. Measured matrix:

| SplitEmptyFunction | AllowShortFunctions... | Result |
|---|---|---|
| true | Empty | `void Foo() noexcept {}` — rule silently lost |
| true | **None** | `void Foo() noexcept` + `{` + `}` — correct |
| false | None | `void Foo() noexcept` + `{}` |

Set: `SplitEmpty{Function,Record,Namespace}: true` +
`AllowShortFunctionsOnASingleLine: None`.

**Consequence recorded in `.clang-format`:** with all three `SplitEmpty*` true,
the style is now *pure* Allman, and `BreakBeforeBraces: Allman` was verified
byte-identical to the explicit `BraceWrapping` table (40 real sources + a probe
covering every brace-bearing construct). `Custom` is still kept, but the reason
changed: it is now version portability (an unrecognised enum value makes the
whole file refuse to load, and CLion bundles its own older clang-format), not an
inexpressible exemption. The reverse hazard is documented where it will be hit:
a `BraceWrapping` override under a *named* style is silently ignored.

**Changes:** `.clang-format`, and the rule text plus bodies in
`Engine/CodingStandards.{h,cpp}` — which had to be rewritten because they
documented the now-reversed exemption. Verified: resolved-config delta is exactly
the 4 intended keys and nothing else; both exemplars clang-format clean and
`-fsyntax-only` clean; all 9 build configurations pass
(EngineTest/VulkanExample/WindowExample x Dev/Debug/Release) with a `touch`
control proving the builds really recompile.

**Not done — needs approval:** this reformats **107 files / 650 lines** across
`Engine`, `Examples`, `Applications` (measured as candidate-vs-current config
output, so it isolates this change from the separate `NamespaceIndentation` debt).
Largest: `Engine/Math/Vector4.h`, `Vector3.h`, `OSAL/Window.cpp`,
`Math/Quaternion.h`.

## Allman Established as the Declared Brace Style (2026-09-06)

**Trigger:** `Applications/VulkanExample/Main.cpp` flagged as off-standard; scope
redirected to the standard itself. See `.Plans/PLAN_allman_style_baseline.md`.

**Decision:** the codebase brace family is **Allman** — break the line before
every opening brace. Ruled out with measurements, not opinion: against
`Engine/CodingStandards.{h,cpp}`, `BreakBeforeBraces: Allman` misses 1
construct, `Linux`/BSD 28, `Stroustrup` 35, `WebKit` 36, `GNU` 41, `Attach`/K&R 45.
BSD/KNF is specifically wrong because it keeps `if (x) {` and `namespace x {`
attached. `BasedOnStyle: LLVM` is optimal (0 residual deviation; Microsoft ties,
Chromium 12, Google 16, WebKit 22, Mozilla 110, GNU 291).

**Non-obvious constraint recorded in `.clang-format`:** `BraceWrapping` is only
consulted when `BreakBeforeBraces` is `Custom` — an override under a named style
is silently ignored (proven: `SplitEmptyRecord: true` and `false` produce
byte-identical output). So `BreakBeforeBraces: Allman` cannot express the
empty-body exemption, and `Custom` carrying the Allman flag set is mandatory.

**Changes:**
1. `.clang-format` — names the style Allman, documents why `Custom` is required,
   groups options by concern, records the 3-block include layout. Proven
   behaviour-neutral: `clang-format --dump-config` is byte-identical to the
   previous revision. Legacy scalar spellings kept on purpose — several options
   became mappings/enums in clang-format 19-22 and modern spellings would make
   CLion's bundled formatter refuse to load the file.
2. `Engine/CodingStandards.h` — the rule text claimed "K&R variant" and gave the
   self-contradicting example `type FunctionName(args) {`. Corrected to Allman
   with a real example, the BSD/K&R distinction, and the empty-body exemption.
   Include-order note made explicit (own header / standard / project).
3. `Engine/CodingStandards.{h,cpp}` — converted from 4-space to tab indentation.
   These were the only 2 of 251 files using spaces, contradicting their own
   header comment; they belong to no CMakeLists target, so nothing ever
   formatted them.

**Verification:** `clang-format --dry-run -Werror` clean on both exemplars;
resolved config byte-identical; whitespace-stripped token diff shows only 6
empty-body merges and 2 line joins (zero semantic change);
`clang++ -std=c++23 -fsyntax-only -Wall -Wextra` exits 0.

**Open issue, deliberately not decided:** `NamespaceIndentation` is contradicted
repo-wide and is not a brace-style question. `None` (current; matches the rule
text, both exemplars and `Main.cpp`) leaves 218 files / 32 163 lines dirty;
`All` (what the engine body is actually written as) leaves 219 / 18 271. Kept at
`None`; needs an explicit owner decision before 14 000 lines are churned.

**Still pending:** `docs/CodingStandards.md` says "K&R variant — opening brace on
the same line" and now contradicts everything above; `Main.cpp` fixes await
approval.

## Summary of Macro Application Task

Applied the new macros from `Engine/Core/CommonMacros.h` across the codebase:
- `returnIf(condition)` for `if (condition) return;`
- `returnValueIf(value, condition)` for `if (condition) return value;`
- `breakIf(condition)` for `if (condition) break;`
- `continueIf(condition)` for `if (condition) continue;`
- `ONCE()` macro kept in CommonMacros.h (removed duplicate from CommonUtil.h)

### Changes Made

1. **Engine/Core/CommonMacros.h**: Added `returnValueIf` macro.
2. **Engine/Core/CommonUtil.h**: Removed duplicate `ONCE` macro definition.
3. Applied macros to 34 files across Container, Math, OSAL, Resource, String, Logger, ComponentSystem, TaskSystem, etc.
4. Added `#include "Core/CommonMacros.h"` to each modified file.
5. Verified that all 51 unit tests pass.

### Lines Saved Estimate

- Approximately 103 macro applications replaced 2-line patterns with 1-line equivalents, saving ~103 lines.
- Added 34 include lines (one per modified file).
- Removed 1 duplicate macro line from CommonUtil.h.
- **Net lines saved**: 103 - 34 + 1 = **70 lines**.

### Verification

- Built and ran EngineTest in Debug configuration: all tests pass.
- No regressions introduced.

## RingQueue Performance Optimization & Quaternion Test Fix

### Changes Made

1. **Engine/Container/RingQueue.h**:
   - Added `#include <bit>` for `std::bit_ceil()`
   - Modified constructor to round capacity to next power of 2 using `std::bit_ceil()`
   - Changed `WrapIndex` from modulo (`% cap`) to bitmask (`& (cap - 1)`)
3. **Engine/Math/Quaternion.cpp**:
   - Previous expectation was mathematically invalid (rotating vector parallel to axis of rotation)

## EngineTest Performance Analysis and Improvement Solutions

### Summary
Built and ran Applications/EngineTest in Debug configuration with tests enabled. Identified performance warnings where custom implementations underperform STL equivalents.

### Key Findings
- InlinePoolAllocator: 1.27x slower than system malloc for variable-sized allocations (due to fallback overhead)
- MultiPoolAllocator: 7.7x slower than std::malloc
- ThreadSafeMultiPoolAllocator: 8.0x slower than std::malloc
- LinkedList: 2.1x slower than std::list
- String: 14x slower than std::string (due to lack of Small String Optimization)

### Recommended Solutions
1. **InlinePoolAllocator**: Document as fixed-size only; use segregated allocators for variable sizes.
2. **MultiPoolAllocator**: Optimize block management (power-of-two sizes, per-CPU caching); reduce lock contention.
3. **LinkedList**: Integrate node pooling; cache size; use sentinel node.
4. **String**: Implement Small String Optimization (SSO); exponential growth; optimize operations.

### Next Steps
- Start with String SSO for highest impact.
- Proceed to LinkedList node pooling.
- Address allocators last due to complexity.
- Create microbenchmarks to validate improvements.

Full analysis available in PerformanceAnalysis.md.
| 2026-09-18 19:10 | 246794a | feat(container): priority queue drains most-urgent-first and oldest-first within a tie | (B3b.) Inverted so the highest number is most urgent, per the owner's explicit choice; buckets became hbe::Deque so a tie drains oldest-first (Pop took bucket.back(), so ties ran newest-first); buckets created on first push and released when empty. Measured: MainThreadTaskQueue's BoundedPriorityQueue<TaskItem,256,1024> reserved 256 x 1024 x 24 = 6,291,456 bytes while empty, across 256 allocations; construction now allocates nothing. DispatchToMainThread's default priority moved 0 -> 128: the two headers already disagreed (the queue documented 128, the signature passed 0), so under the old rule every silent caller was enqueueing at top urgency, and leaving 0 after the inversion would have silently demoted the same callers to the bottom. | ERROR, and it invalidates an earlier claim: BoundedPriorityQueueTest had 27 assertions that logged a message and returned WITHOUT lferr, so the whole collection could not fail - "56/56" was decorative for it. Found only because a deliberate mutation (Pop back to back()/PopBack) appeared to pass. ERROR in my verification command: I used grep -cE " error ", which cannot match clang's "error:" form, so it reported 0 through builds that had actually failed; a failed build leaves the previous binary on disk, and that stale binary is what made the first mutation look harmless. Build output is now grepped for "error:" and the exit status checked separately. ERROR: converting assertions to lferr needed [this] captures - these test lambdas were capture-less and lferr is a TestCollection member - and my first pass ignored that. With those fixed the mutation is caught: "Equal priorities drained tag 3 before tag 1 - newest is winning the tie". The same mutation also fails TaskSystemTest's two Bagel tests, so the old newest-first tie-break was not cosmetic. 56 collections pass in Debug, Dev and Release. |
| 2026-09-18 19:40 | 937244f | feat(core): drain policy primitive for a stream's two lanes | (B3c, first piece.) StreamDrainPolicy decides which lane a stream acquires from next and when it must stop, per the owner's answers: the FIFO:priority rate is a share of the stream's CPU allowance rather than a task count, borrowing is free while the stream total is inside budget, and a round ends at allowance exhaustion. With no allowance there is no CPU to divide, so the rate falls back to a weighted rotation over signed credits replenished by the weights; a zero weight means one, not "never serve", because a lane that can never be served would strand its queue. Landed detached rather than wired in, because the TaskStream migration has to move the finished-task sweep and the two re-add buffers together. The header states the two consequences the owner accepted: an idle lane does not hold its share hostage, and the long-run ratio is therefore NOT preserved when both lanes are permanently backlogged. Measured: 3:1 gives exactly 6:2 over 8 takes, 1:3 gives 2:6, unset weights give 2:2; 3:1 over 2 ms gives 1500/500 us shares; 0.6 ms of a 1 ms half-share is still chosen when it is the only lane with work; 1.2 ms of 1 ms exhausts the round and EndRound reopens it. ERROR disclosed by the mutation, not hidden: disabling credit replenishment turns the ratio test red, but "A light lane is never starved by a heavy one" stayed green - it passes incidentally and does not discriminate, so it is a regression guard, not evidence. Also removed a speculative bool field before building; clang errors on unused private fields. 57 collections pass in Debug, Dev and Release. |

| 2026-09-18 21:30 | 4ecd68d | feat(core): TaskStream gets a FIFO lane and a priority lane | (B3c wired.) A stream now owns `fifoQueue` (hbe::Deque, arrival order) and `priorityQueue`, with StreamDrainPolicy choosing which to acquire from; `Enqueue` split into `EnqueueFifo`/`EnqueuePriority` and the single existing caller (`TaskSystem::Enqueue(streamIndex, task)`) names FIFO explicitly, which is faithful because no caller in the tree sets a non-zero priority. The sweep is per lane and unfinished tasks return to their own lane. Two things deliberately NOT wired: the policy does not gate dequeuing (see the error below), and `TaskStream::Dequeue` keeps its serve-whenever-work-exists behaviour because its callers were not audited - gating an unaudited refill is how you get a stall. Also fixed by reasoning rather than by any test: rotation credit was spent in the CHARGE path, so an unlimited stream - which never charges - would never replenish and would starve its lighter lane forever; credit is now spent on the take via `CommitTake`. | ERROR, the most serious of the session: the first build deleted the line that pushes `readdingFifo` back into its lane, so unfinished FIFO tasks - the Logger drain task among them - went into a buffer nobody drains and a parent's `HasDone` never became true. That is a hang, and a human watching the close test stall reported it; nothing I had run would have caught it, which is the same dropped-task class as D10/D11. ERROR, and the same lesson a third time: inserting `CommitTake` into the test's `DriveTakes` silently did nothing TWICE because I guessed the indentation (three tabs where clang-format had made it four) and did not assert the replacement count - "8:0 instead of 6:2" is what exposed it. A replacement I do not assert is a replacement I did not make. DESIGN FAULT of my own: I closed a round at allowance exhaustion and reset it instantly in the charge path, which reopens the gate the same instant it shuts; TaskSystemTest's budget test failed correctly ("A stream that has spent its allowance still reported that it may take more work") so the gate went back to `CPUBudget::CanTakeWork` and the policy selects lanes only. What reopens a round is now an explicit open question for the owner, not something I invented. PLAN ERROR corrected and the commit amended to match: I had written that hbe::Deque supports `Erase` and that I had compiled it - it does not, neither does hbe::Queue, which is a ring buffer with no iteration; the real sweep rotates the lane. Two python edits also aborted on their own asserts before writing (a `,s1` typo, a wrap-sensitive anchor), so no partial file was produced. 57 collections pass in Debug, Dev and Release; check.sh 5 formattable files, 0 violations. COVERAGE GAP declared, not hidden: nothing in the tree calls `EnqueuePriority`, so the dual-lane wiring has no test - and the hang above is exactly what such a test would have caught. |

| 2026-09-18 22:10 | 2d9b5d3 | test(core): both stream lanes are shown to drain | Added TaskSystemTest "Both lanes serve their tasks": three subtasks on the FIFO lane and one on the priority lane of an idle worker stream, then a bounded poll - "Worker1 served 4 of 4 tasks across its two lanes", about a millisecond. It polls against a 5 s deadline rather than waiting on the task, because the defect in this area was a hang and a test that hangs reports nothing. | SCOPE CHECKED, AND IT FAILED: I re-introduced the dropped-`readdingFifo` bug to see whether this test catches it, and the test never ran - the suite hung BEFORE TC4, because the unfinished work being dropped is the Logger's task and the suite's own subtask. So the drop bug is caught only by the suite's liveness, which is not a test failing, and the coverage gap I named in the previous commit is still open. Closing it needs a task that is provably unfinished after one `Run`, which needs `RangedTask` range semantics pinned down by reading, not guessing - I have already written down one false claim about a container API this session by trusting a recollection of compiling something. 57 collections pass in Debug; only a test body moved, so Dev and Release are unaffected apart from relinking. |

| 2026-09-18 23:30 | (docs) | docs(design): budget window reopen, packet delivery, task registry, continuation-only completion | Ten owner decisions taken one at a time and written into docs/TaskSystemRedesign.md. R1 the base stream calls into each stream to reset budgets, which forces isMeasuring atomic and amends the single-owner note. R2 the reason: there was NO reopen rule at all - CanTakeWork is accumulated < allowance and Reset() had no caller, so a configured stream latched itself off permanently; a pre-existing defect I inherited, and the test that seemed to cover it only proved the latch never opens. R3 two containers per stream with a swap-and-drain-unlocked delivery. R4 fixed 128-byte packets from a thread-safe pool, filled on workers, released on the base stream, valid one frame; measured that MultiPoolAllocator has no mutex, atomic or thread_local anywhere, so it cannot take this, and the free list is a mutex over pre-allocated banks rather than a Treiber stack because of ABA. R5 header inside the 128 - 8 bytes, payload 120, index arithmetic is a shift, 8192 packets per 1 MB bank. R6 kind split 0-63 engine, 64-255 application. R7 index plus generation in a task registry and Tasks stop being stack objects, measured blast radius 6 sites. R8 Wait, BusyWait and public HasDone removed, continuation only - which is illegal on the base thread the suite already runs on, so five test call sites need rewriting. R9 pipelines are the caller's business and TaskSystem only knows dispatch, successor, join counter, which is what let the header stay 8 bytes. R10 ParallelFor spreads across requested streams and ships as several functions. | CORRECTION to my own earlier wording: I told the owner the budget had a "per-frame latch". It does not - there is no reopen anywhere. I also had the header as uint16 payloadSize until the owner pointed out 0..128 fits in a byte. No code written this round, so nothing to build; the decisions are the deliverable and the open list is recorded rather than guessed at. |

| 2026-09-18 23:55 | (docs) | docs(design): result container is a stack allocator with declared capacity | R11-R14 from the owner, continuing the same session. R11 the container is a bump allocator over fixed 128-byte slots and the base stream only swaps the two containers; workers never deallocate to reuse, they rewind the count - so allocation is index=count++ and release is count=0, no free list, no atomics, no lock in the allocator. R12 Reset() may grow the buffer by a rate declared in the task's TaskDescriptor, which keeps allocation off the hot path entirely. R13 capacity is declared per task and the stream will not proceed without room, which makes overproduction an assert. R14 admission is checked at dequeue and an oversized head closes that lane until the next Reset rather than being overtaken, which is what preserves B3b's arrival order. | I pushed back on R14's stall risk in the question itself and the owner chose it knowing that cost, so three guards are recorded as MY addition, not their decision, and are marked as such in the doc: refuse at enqueue anything that can never reach its capacity, because R14 would otherwise close a lane permanently on a configuration value; log a closed lane with the need and the room, because that failure is otherwise indistinguishable from today's dropped-task hang; and put a container epoch in the handle, because a bump allocator cannot detect use-after-rewind and a stale index silently reads someone else's packet. No code written, nothing built. |

| 2026-09-18 23:59 | (docs) | docs(design): capacity fallback to the other lane, completion made optional | R15: a lane closed by capacity does not idle the stream - it takes from the other lane when that task needs no result slots. Arrival order survives because the other lane is a separate queue, not a queue jump. R16: completion is optional; fire-and-forget jobs report nothing and a caller needing a join brings its own collation, which is R9's ruling that pipelines are the caller's business applied to completion. The engine's whole contract is now dispatch, admit by declared capacity, swap, deliver. | Named the consequence rather than leaving it implicit: the B3d rule that an abandoned task still counts as completed for its join now applies only to tasks that opted into completion, so an expired fire-and-forget job simply disappears with nobody waiting. Also noted that R15 only has something to run because R16 makes zero-output tasks real - with a mandatory completion packet every task would declare at least one slot and the fallback could never fire, reading correctly and doing nothing. No code, nothing built. |

| 2026-09-19 00:10 | (docs) | docs(design): growBy is a stream property, NumResults replaces ResultCapacity | R17-R19. growBy belongs to the stream, not the task, so a stream's ceiling is decided where it is built and tasks inherit it; a task wanting more waits for repeated grows. There is no reportsCompletion field - NumResults decides it, so the contradiction of a zero-output task asking to report completion cannot be expressed, and R15's capacity-free population is exactly NumResults == 0. Start simple: first implementation is NumResults, capacity admission, two containers, swap, fold, rewind, with no TaskDescriptor object invented to carry a single integer. | R17 partially retracts R12, which I had recorded as growth coming from the task object; it comes from the stream. Corrected in the doc rather than left contradictory. No code written, so nothing built - the suite is still 57/57 from 2d9b5d3 and untested since, because nothing has compiled. |

| 2026-09-19 00:20 | (docs) | docs(design): initial container 1024 slots, growBy 1024 | R20 closes the capacity ceiling open item with one number for every stream rather than per-stream tuning, consistent with R17 putting growBy on the stream. Stated the arithmetic rather than leaving it to be discovered: slot is 128 bytes so a container starts at 131,072 bytes and each grow is the same size; two containers per stream is 256 KiB per stream before any growth; a 1 MB MultiPoolAllocator bank fits 8192 slots so an initial container is one eighth of a bank. | No code written and nothing built. Suite remains 57/57 from 2d9b5d3, untested since because nothing has compiled. |


---

## Refactor: Static multi-platform (Application + Window) — Steps A & B

### Summary
Replaced runtime polymorphism with static compile-time dispatch, per user directive.

- **Step A (`ea01c87`)** — Application: dropped `IApplication` interface + factory `#ifdef` dispatch. Single concrete `OS::Application` class; per-platform impl in `LinuxApplication.cpp` / `OSXApplication.mm` / `Win32Application.cpp` (selected via `#if defined(PLATFORM_*)`). `CreateApplication()` returns `unique_ptr<Application>`. Engine owns `unique_ptr<OS::Application>`.
- **Step B (`5e9eaed`)** — Window: dropped `IWindow` interface. Single concrete `OS::Window` class with compact header (no platform includes); per-platform impl in `LinuxWindow.cpp` / `OSXWindow.mm` / `Win32Window.cpp`. `CreateWindow()` returns `unique_ptr<Window>`. OSX member `nsWindow` generalized to `osHandle`. Removed obsolete per-platform Window/Application headers.

### Verification
- `EngineTest`: **52/52 PASS, 0 FAIL** (test-config build).
- `WindowTest`: all runtime cases PASS (Create/Title/Size/Visibility/Poll Events) — concrete `Window` works end-to-end.

### Known / pre-existing (NOT a regression)
- Building with the global `-D__TEST__ -D__UNIT_TEST__` flag (the `./build.sh -test` path) pulls the test framework into *every* app via `Window.cpp`'s `WindowTest`, creating a pre-existing `OSAL ↔ Test` static-lib circular dependency. With `Test` linked last, `EngineTest` links fine but thin apps like `WindowExample` fail to link (missing `Logger`/`Memory` symbols). This is a build-ordering property orthogonal to the refactor — `WindowExample` links cleanly in a normal non-test config. No build files were changed for it (per AGENTS.md, generated CMakeLists are not hand-edited).

---

### Questions Addressed

1. **What is String SSO?**
   Small String Optimization (SSO) stores small strings (typically ≤15-22 chars) directly in the string object instead of allocating heap memory. This avoids allocation overhead for common cases and improves cache locality. Standard `std::string` implement SSO.

2. **LinkedList Node Pooling Check**
   Yes, verified: LinkedList uses a template allocator parameter (`TAllocator = DefaultAllocator<LinkedListNode<TType>>`). Unit tests specifically configure it with PoolAllocator:
   ```cpp
   PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
   AllocatorScope allocScope(alloc);
   ```
   Despite node pooling, performance issues may stem from:
   - Pointer chasing causing poor cache locality
   - Algorithm inefficiencies beyond allocation
   - Test configuration or measurement overhead

3. **MultiPoolAllocator Locking**
   Correct: 
   - `MultiPoolAllocator` (single-threaded): No mutex locks
   - `ThreadSafeMultiPoolAllocator`: Contains `std::mutex lock` for thread safety

4. **HString and SSO**
   Verified: `HString = std::basic_string<char, ..., hbe::DefaultAllocator<char>>` 
   As a typedef of `std::basic_string`, it inherits the standard library's SSO implementation (when available on the platform).
   The poor-performing string in tests was the custom `String` class (Engine/String/String.h), not HString.

### Key Clarification
The StringTest warning referred to the custom `String` class (always heap-allocated via `Shareable<Vector<TChar>>`), not HString which does benefit from std::string's SSO.

## Follow-up: Clarifications on Performance Analysis

### Questions Addressed

1. **What is String SSO?**
   Small String Optimization (SSO) stores small strings (typically ≤15-22 chars) directly in the string object instead of allocating heap memory. This avoids allocation overhead for common cases and improves cache locality. Standard `std::string` implement SSO.

2. **LinkedList Node Pooling Check**
   Yes, verified: LinkedList uses a template allocator parameter (`TAllocator = DefaultAllocator<LinkedListNode<TType>>`). Unit tests specifically configure it with PoolAllocator:
   ```cpp
   PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
   AllocatorScope allocScope(alloc);
   ```
   Despite node pooling, performance issues may stem from:
   - Pointer chasing causing poor cache locality
   - Algorithm inefficiencies beyond allocation
   - Test configuration or measurement overhead

3. **MultiPoolAllocator Locking**
   Correct: 
   - `MultiPoolAllocator` (single-threaded): No mutex locks
   - `ThreadSafeMultiPoolAllocator`: Contains `std::mutex lock` for thread safety

4. **HString and SSO**
   Verified: `HString = std::basic_string<char, ..., hbe::DefaultAllocator<char>>` 
   As a typedef of `std::basic_string`, it inherits the standard library's SSO implementation (when available on the platform).
   The poor-performing string in tests was the custom `String` class (Engine/String/String.h), not HString.

### Key Clarification
The StringTest warning referred to the custom `String` class (always heap-allocated via `Shareable<Vector<TChar>>`), not HString which does benefit from std::string's SSO.

## Vulkan Renderer: Build Wiring + First Running Frame (2026-08-31)

### Problem
The Vulkan renderer written on 2026-08-30 had **never been compiled**. `libRenderer.a`
held only `RHICapabilities.cpp.o` + `RendererTest.cpp.o`, and `VulkanExample` had no
build target at all, so the plan's per-phase "it compiles / it runs" claims were
unverified rather than true.

### Root causes (build system)
| ID | Cause | Fix |
|----|-------|-----|
| B1 | `Engine/Renderer/.module.config: ignoreSubdirectories = DX12 Metal Vulkan`. MakeBuild collects sources only from a module's own directory (`Module::CollectFiles`) and skips ignored subdirs entirely (`ProjectBuilder::TraverseDirectoryTree`), so `Vulkan/*` could never reach the Renderer target | `customCMake.txt`: `target_sources (Renderer PRIVATE Vulkan/VulkanRenderer.cpp)` (+ `.mm` under `if(APPLE)`) |
| B2 | `Applications/VulkanExample/` had **no `.module.config`** → `buildType = None` → the generator's `switch` emitted no `add_executable` | added `Applications/VulkanExample/.module.config` (Executable, deps incl. Renderer) |
| B3 | No one linked the Vulkan loader; `.mm` needs `VK_USE_PLATFORM_METAL_EXT` or `vkCreateMetalSurfaceEXT` is `#ifdef`'d out of `vulkan_metal.h` | `find_path`/`find_library` with `FATAL_ERROR` guards + `target_compile_definitions` |

**Decision:** fix through `.module.config` / `customCMake.txt` instead of hand-editing
generated `CMakeLists.txt` files, so the generated files stay machine-owned.

### Decisions and why
- **Plain `target_link_libraries`** in `customCMake.txt`: MakeBuild emits the plain form
  for module dependencies (`target_link_libraries (Renderer Log)`), and CMake forbids
  mixing plain and keyword signatures on one target. Plain items still propagate, so
  consumers get `libvulkan` + the Apple frameworks transitively.
- **`Renderer` now declares `dependency = Log`** and reports every Vulkan failure through
  `Logger` with the `VkResult`. Per AGENTS.md, graphics code must log; a silent `return
  false` cost hours of guessing. (`VulkanRenderer` logs a single "first frame presented"
  milestone and errors otherwise - no per-frame chatter.)
- **Dead setters removed** (`SetModel`, `SetLightDir`): `MC.vert`'s push block is
  `{ mat4 view; mat4 proj; }` (exactly the 128-byte portable maximum), so model/light
  direction had no path to the GPU. Re-introduce them in the MC phase together with the
  shader + uniform layout, not as no-ops.
- **Linux/Windows surfaces not faked.** Linux cannot create a surface from the current
  OSAL API (`Window::GetNativeHandle()` returns only the X11 `Window`, no `Display*`);
  Win32 is wired behind `VK_USE_PLATFORM_WIN32_KHR` but is **compile-unverified** here.
- **Mesh memory is host-visible/host-coherent** and mapped directly. Device-local +
  staging upload is the right long-term shape; recorded as a follow-up rather than
  half-built now.

### Runtime defects found only after it finally compiled
| # | Defect | Symptom |
|---|--------|---------|
| 1 | `CreateMetalSurface()` was defined in **both** `.cpp` (unguarded stub) and `.mm` | `VulkanRenderer.cpp.o` satisfied the symbol first, so `VulkanRenderer.mm.o` was **never pulled from the archive** - the real Metal surface never ran. Stub now guarded `#if !defined(PLATFORM_OSX)` |
| 2 | `VK_KHR_swapchain` never enabled on the device | loader: "Driver's function pointer was NULL" |
| 3 | `attachmentCount = 2` with `pAttachments = &colorAttach` (two separate locals) | Vulkan read attachment #1 out of adjacent stack memory → MoltenVK SIGSEGV in `getMTLPixelFormat`. Now one contiguous `VkAttachmentDescription[2]` |
| 4 | AppKit ignores `+layerClass` and installs `NSViewBackingLayer` | `setDevice:` → `doesNotRecognizeSelector` NSException. The `CAMetalLayer` is now created explicitly and adopted by the layer-backed view (measured: `view.layer class = CAMetalLayer`) |
| 5 | Fences created unsigned, 1 shared command buffer, no dynamic viewport/scissor, `vkCmdBindIndexBuffer` missing | would have deadlocked frame 0 and drawn nothing; fixed with the rewrite |

### Verification
- `Renderer` + `VulkanExample` build clean under `-Wall -Werror`, Dev config, no warnings.
- `libRenderer.a` = `RHICapabilities`, `RendererTest`, `VulkanRenderer.cpp.o`, `VulkanRenderer.mm.o`.
- Run: `first frame presented (800x568, swapchain images=3, index count=0)`; loop stable, zero loader/validation errors. Extent is 800x568 (content rect of an 800x600 titled window) and is taken from `currentExtent`, not assumed.
- `EngineTest` builds and exits 0.

### Important caveat found (pre-existing, NOT fixed)
`Assert()` in `Engine/Core/Debug.h` is live only under `__DEBUG__`, and **`__DEBUG__` is
defined nowhere in the project**. In every configuration `Assert` compiles to a no-op, so
the suite's "280 tests PASS / Fail = 0" is vacuous. Consequently `RendererTest`'s stale
stub-era expectations (`Initialize(nullptr) == true`, `supportsComputeShader == true`)
pass while actually being false. Needs a decision: define `__DEBUG__` for Debug builds
(likely lights up pre-existing failures) and rework `RendererTest`.

### Follow-ups recorded
Swapchain recreation on resize · device-local + staging mesh upload · VK_EXT_debug_utils
messenger · `ShadersSpv.h`/`*.spv` are generated but not gitignored and `gen_spv_header.py`
is run by hand · orphaned `Engine/Renderer/Vulkan/CMakeLists.txt` (no `add_subdirectory`)
still claims to build `VulkanRenderer.mm` · Linux surface needs `Display*` from OSAL.

## VulkanExample: rotating quad made real (2026-08-31)

### Problem
The window was titled "Rotating Quad" but showed a flat navy field. `Main.cpp` never
called `SetMesh`, `SetView` or `SetProj`, so `indexCount == 0` and `RecordFrame()`
correctly drew nothing. The stub-era example expected the renderer to supply geometry.

### Decision: where to put the rotation (user chose option B)
`MC.vert`'s push block was `{ mat4 view; mat4 proj; }` = 128 bytes, already the
portability-guaranteed maximum, so a model matrix had nowhere to go.

| Option | Assessment |
|--------|------------|
| A. Rotate inside the view matrix | cheapest, but the world-space normal never moves, so Lambert shading cannot respond - the demo would look static in lighting |
| **B. `mat4 model` + CPU-combined `mat4 viewProj` (chosen)** | still exactly 128 bytes, restores a meaningful `SetModel`, and the quad's shading visibly pulses as its normal sweeps the light |
| C. Descriptor set + uniform buffer | the real fix for the 128-byte ceiling and for app-controlled lighting; deferred to the Marching Cubes phase |

### Changes
- `MC.vert`: push block is now `{ mat4 model; mat4 viewProj; }`; normal uses
  `mat3(model) * aNormal`, with a comment that this is valid only while `model` carries
  rotation/translation (switch to `transpose(inverse(model))` once scaling appears).
- `MC.frag`: removed dead code (`vWorldPos` varying and a `depth` value that was
  computed and never read); light/ambient/albedo named as constants. The light direction
  stays compile-time fixed because model+viewProj consume the whole push range - stated
  in the file rather than implied by a dead setter.
- `PushConstants { model, viewProj }`; `MultiplyColumnMajor()` in the renderer does the
  one `proj * view` multiply per frame; `SetModel()` restored.
- `GetExtent()` added: the drawable follows the window *content* rect (800x568 for an
  800x600 titled window), so examples must not assume the requested size for aspect ratio.
- SPIR-V regenerated with glslangValidator; `MC.spv` renamed to `MC.vert.spv` so vertex
  and fragment outputs are named consistently; regeneration commands now documented in
  both shader files.
- `Main.cpp`: quad mesh (XY plane, normal +Z) uploaded once, RH perspective with the
  Vulkan depth/Y convention, camera at z=-3, `model = rotationY(t)` each frame.

### Verification
`first frame presented (800x568, swapchain images=3, index count=6)` - the index count
proves geometry reached the pipeline - with zero loader or validation output, and a
warning-free `-Wall -Werror` build. Pixel-level confirmation is the user's, because
`screencapture` is blocked in this environment.

## OSAL: the window close button was never observed (2026-08-31)

### Symptom
Closing the window left `VulkanExample` running forever.

### Root cause (macOS)
`Window::PollEvents()` is the only code that ever sets `closedFlag` (via
`![window isVisible]`), and **nothing calls it** - applications pump
`OS::Application::PollEvents()`, which drains and dispatches the event queue but has no
path back to a `Window`. So `IsClosed()` stayed false and every example loop ran forever.
`applicationShouldTerminateAfterLastWindowClosed` also returns NO, and there is no
`NSApp run` loop to consult it.

The same gap exists on the other platforms (not fixed here, see below).

### Fix
An `HBWindowDelegate` implementing `windowShouldClose:` marks the owning `Window` closed
the moment the button is pressed, and returns YES so AppKit proceeds. This is event-driven
rather than "poll until the window looks hidden", so it no longer depends on somebody
remembering to call `Window::PollEvents()`. `Window` owns the delegate reference (new
per-platform `osDelegate` member) because `NSWindow.delegate` is unretained; it is
released in `Close()` next to the window's own release.

### Verification (with control)
An ad-hoc ObjC++ harness created a real `OS::Window` and invoked `performClose:` - the
same path the red button takes:

| Build | `IsClosed()` before | after | |
|-------|--------------------:|------:|---|
| Without `[window setDelegate:]` (control) | 0 | **0** | reproduces the reported bug |
| With the delegate | 0 | **1** | fixed |

A synthetic UI click was not possible here: `osascript`/System Events is denied
Accessibility permission (-1719), and `screencapture` is likewise blocked.

### Same defect, not addressed (unverifiable on this machine)
- **Win32**: `WM_DESTROY` sets `shouldCloseFlag`, but `IsClosed()` returns `closedFlag`, so
  the information is recorded and then dropped. One-line fix: return
  `closedFlag || shouldCloseFlag`.
- **Linux**: `Window::PollEvents()`'s `ClientMessage` branch is an empty placeholder, so
  `WM_DELETE_WINDOW` is never handled and `WM_PROTOCOLS` is never registered. Needs a real
  atom handler (~8 lines).

## Lighting reviewed and deliberately left as-is (2026-08-31)

The rotating quad looked under-lit, so the shader was audited before changing anything.
The model is a single Lambert term - `ambient 0.25 + max(dot(n, L), 0) * albedo 0.95` -
with `n = mat3(model) * aNormal`, i.e. rotation is correctly applied to the normal.

Solving it for this geometry (all four vertices share normal (0,0,1), so the world normal
is `(sin t, 0, cos t)` and `L = (0.337, 0.842, 0.421)`):

`dot(t) = 0.337 sin t + 0.421 cos t` -> amplitude **0.539**, peak at **t = 38.7°**, output
range **0.250 .. 0.762**, and pure ambient for **50%** of the turn.

That explains the flat look completely, and the four causes are:

1. One flat normal across the whole quad -> identical colour in every fragment; lighting
   reads through spatial variation, which a lone quad cannot express.
2. The light points 84% along +Y while the normal sweeps only the XZ plane, so at most
   0.539 of it is ever usable.
3. `cullMode = VK_CULL_MODE_NONE` with no `gl_FrontFacing` normal flip -> the back half is
   drawn with a normal facing away and clamps to ambient.
4. No colour management: `VK_FORMAT_B8G8R8A8_UNORM` is chosen and values are written raw,
   so darks read lifted relative to a gamma-managed render. (Switching to the `_SRGB`
   format requires `CAMetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB` to match,
   or MoltenVK rejects the mismatch.)

**Decision: leave it unchanged.** Every item above is either a property of the demo
geometry (1, 2) or a deliberate improvement better made together with the work that needs
it (3, 4). Marching Cubes terrain has per-vertex normals in every direction, so Lambert
will finally have something to express, and the same phase wants app-controlled lighting -
which is the descriptor-set/uniform-buffer change that also frees the 128-byte push budget
holding `kLightDir` in the shader. Deferred options are recorded in
.PlanS/PLAN_real_vulkan_renderer.md rather than silently dropped.

**Known-deferred backlog:** back-face normal flip · sRGB swapchain + matching CAMetalLayer
pixel format · descriptor set + UBO for light direction · swapchain recreation on resize ·
device-local + staging mesh upload · VK_EXT_debug_utils messenger · Win32 `IsClosed()`
ignores `shouldCloseFlag` · Linux `WM_DELETE_WINDOW` unhandled · generated `ShadersSpv.h`
and `*.spv` not gitignored · orphaned `Engine/Renderer/Vulkan/CMakeLists.txt` ·
`__DEBUG__` undefined so `Assert()` never fires.

## TriangleExample removed (2026-08-31)

### Why
It was superseded by `VulkanExample` + the real `VulkanRenderer`, and it was the one target
that could not link: its `customCMake.txt` carried

```
include_directories(.../External/VulkanSDK/include)          # path does not exist
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_LIBRARIES Test)   # wipes the link line
```

The second line replaces the whole link line with `libTest.a`, so every `MemoryManager` and
`TestCollection` symbol goes missing. This was not repairable inside the rules that keep
generated `CMakeLists.txt` files machine-owned - the hack exists precisely to fight the
generator - and both the Vulkan linking and the sample geometry it existed to demonstrate
now live in `VulkanExample`.

### Changes
- Deleted `Applications/TriangleExample/` (8 files, incl. `MinimalVulkanRenderer.*`,
  `TriangleExampleTest.cpp`, and the offending `customCMake.txt`).
- Regenerated all `CMakeLists.txt` via `generate_cmake_files.sh`; the
  `add_subdirectory (TriangleExample)` entry and the global
  `${CMAKE_SOURCE_DIR}/Applications/TriangleExample` include path disappeared with it, so
  no generated file was hand-edited.
- Repointed instructional references at a target that exists: `AGENTS.md`, `README.md`,
  `build.sh`, `build.bat`, `run.sh`, `run.bat`, `docs/HelperScript.md`,
  `docs/EngineAPIGuide.md`, `docs/EngineAPIGuide.html`.

### Deliberately left alone
- `.Plans/PLAN_ApplyCommonMacros.md`, `.Plans/PLAN_renderer_application_window_refactor.md`
  - dated records of past work; rewriting them would falsify history.
- `docs/design/LightweightRenderer_Design.{md,html}` - a superseded design document whose
  acceptance checklist still names the old target. Retiring that document is a separate
  call, so it is flagged rather than silently edited.

### Verification
`cmake --build build --config Dev` - the build-everything command the generator itself
prints - now exits **0** with no errors; it failed on this link error before. `EngineTest`
exits 0, and `VulkanExample` still logs
`first frame presented (800x568, swapchain images=3, index count=6)`.

## LightweightRenderer design document reconciled (2026-09-01)

`docs/design/LightweightRenderer_Design.{md,html}` (dated 2026-06-21, "Draft - Awaiting
Review") still described a renderer that was never built, and parts of it had been actively
reversed. Rather than delete it, the document was updated so it can no longer mislead:

- **Header** keeps the original author/date and adds an explicit *partially superseded*
  status plus an update line.
- **New §0 "Implementation Status (read first)"**: section-by-section verdict (not built /
  superseded / not adopted / partial), the file tree that actually exists, and a table of the
  shipped renderer's real properties (backend, uniform path, mesh memory, framing, culling,
  resize behaviour, example).
- **§5 RHI** banner: the abstraction was not merely thinned but removed (`e1b5efb`), and no
  `RHI/Vulkan` + `RHI/Metal` files exist; §5.3 kept only as notes for a hypothetical second
  backend, since macOS is served by MoltenVK.
- **§7 File structure** banner: `Core/ Graph/ Resources/ Commands/ Backends/` was not adopted;
  the platform split is by file extension (`.cpp` / `.mm`), not directory.
- **§9** phase-outcome table: 1,2,5,7 not built · 3 superseded · 4 replaced by MoltenVK ·
  6 partial.
- **§10** opens with the blocking caveat that `Assert()` is a no-op because `__DEBUG__` is
  defined nowhere, so a green suite proves nothing yet.
- **§10.3** replaced with commands that exist (`Applications/VulkanExample`, full-tree build)
  and a note that `Renderer/Vulkan`, `Renderer/Metal` and `Applications/TriangleExample` are
  not targets at all.
- **§11** marks the "thin abstraction" decision as reversed in practice; **§12** answers Q4
  (render passes, not dynamic rendering) and records Q3 as already constrained by the 128-byte
  push budget.

Both mirrors were edited (527-line Markdown, 996-line styled HTML) and the HTML re-checked for
tag balance; the one unbalanced `</span>` found predates this change (old line 587, inside the
architecture diagram) and was left alone.

**Every factual claim was verified against the tree**, including `sizeof(PushConstants) == 128`,
`MAX_FRAMES_IN_FLIGHT == 2` with one command buffer per frame, `VK_CULL_MODE_NONE`, host-visible
mesh memory, and the absence of `Core/ Graph/ Resources/ Commands/ Backends/`.

## Marching Cubes kickoff: decisions, noise, and a shutdown-path defect (2026-09-01)

**Decisions taken (`.Plans/PLAN_marching_cubes.md` §4):** D1 = A full cross-platform OSAL input
(written to documented APIs for Win32/X11, **compile-unverified** — this machine builds macOS
only); D2 = relative motion + cursor warp; D3 = 64³ field with full re-mesh per dig; D4 = noise in
`Engine/Math`; D5 = yes, fix the dead `Assert()`; D6 = module name `Engine/Voxel`.

**Recon corrections to the plan.** `OSInputOutput.h` is a file-I/O test collection, not an input
API — OSAL has **no** key or mouse surface at all, so Phase 7 is "input layer + example", not just
an example. `Math::Matrix4x4` is row-major (`m[row][column]`) while the push path wants
column-major, so the app-local column-major helpers stay. Unit tests fail through **Error-level
log lines** (`isSuccess = errorMessages.empty()`), not through `Assert()`.

**Perlin noise landed first (`Engine/Math/PerlinNoise.{h,cpp}`, 5 tests).** The permutation table is
shuffled by a hand-written Fisher-Yates driven by a golden-ratio counter plus the Murmur3 finalizer,
**not** `std::shuffle`: the standard leaves that algorithm unspecified, and a seeded voxel terrain
must be byte-identical across compilers. Measured, not assumed: 729/729 integer lattice points
exactly zero, peak |value| 0.739 (improved-Perlin bound is about 0.866), largest single-step jump
0.0376 at step 0.02 (C1 continuity holds), and all 256 samples differ between two seeds.
A negative control — injecting a deliberate failure — confirmed the suite reports FAIL, so the
PASS results mean something.

**That control exposed a far bigger problem, and its root cause was in the platform layer.** The
failing suite still exited with status 0, so the suite could never gate anything. `lldb` showed
why: `OS::Application::~Application()` called `[NSApp terminate:]`, which calls `exit(0)`.
`Engine::Run()` finishes with `application.reset()`, so the process died inside a destructor and
`main` never resumed — **every statement after `Run()` in every macOS application was dead code**,
including `TestMain`'s return value. Swapping the call for `[NSApp stop:]` fixes it: NSApplication
is a process-wide singleton that outlives the wrapper, and the engine owns its own shutdown, while
the close-button flow is untouched because it goes `windowShouldClose:` → `closedFlag` → loop exit.

Verification went further than a green build, because `exit()` had been *masking* whatever happens
after the frame loop: a temporary hook drove the real button path (`performClose:` is exactly what
the close button sends), and the run then went `Shutting down...` → static destructors → `main`
returns → **exit 0, no hang**. The hook was removed afterwards; `git diff` on that file is the
intended change only. Suite: 53 collections / 285 test cases, exit 0 green, exit 1 injected-failure,
full-tree build clean in Dev and Release (the one remaining warning is the pre-existing duplicate
`libLog.a` link line).

**Side finding worth acting on later:** `Window::PollEvents()` is the only code mapping
"window is not visible" to `closedFlag`, but applications pump `OS::Application::PollEvents()` —
my first hook, placed in `Window::PollEvents`, never ran, while the one in `Application::PollEvents`
fired immediately. So that visibility check is dead for real applications.

**Still open on D5:** `__DEBUG__` must be defined *globally and per-config*, because `ConfigParam`
has a member that exists only under `__DEBUG__` — a `Debug.h`-level definition would give different
translation units different class layouts (ODR violation). MakeBuild's `precompileDefinitions` is a
single config-blind string, so the options are the submodule (`CMakeLists.cpp:128-130`, 2 lines,
recommended) or a `build.sh` stopgap. Awaiting a decision, since the submodule is a separate repo.

## Render capabilities are queried, not guessed (2026-09-06)

**Trigger:** owner asked whether `RHICapabilities::GetCapabilities()` could reflect real
hardware, then extended it: use API-neutral property names for a future Vulkan/DX12/Metal
split, delete the dead `APIType`, delete `RendererCommon.h`, and do not omit useful fields
such as device name and API version. See `.Plans/PLAN_device_capability_query.md`.

**The hardcoded values were wrong, and this is measured, not argued.** A probe against the
loader (`VK_KHR_portability_enumeration` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`
are required, because the MoltenVK ICD json sets `is_portability_driver: true`) reports an
Apple M4 Pro at Vulkan 1.1.334 / driver 10401:

| field | old hardcode | real | |
|---|---|---|---|
| `maxTextureDimension2D` | 4096 | 16384 | 4x understated |
| `maxVertexAttributes` | 16 | 31 | wrong |
| `maxUniformBufferBindings` | 16 | 155 | ~10x understated |
| `supportsTessellation` | false | **true** | **inverted** |
| `supportsGeometryShader` | false | false | right by luck |
| `supportsComputeShader` | true in one path, false in the other | core-mandatory | self-contradictory |

Two of the old field names were not even Vulkan: `maxVertexAttribs` and `shaderCompute` are
OpenGL-era. Vulkan says `maxVertexInputAttributes`, and compute has no feature bit at all
because it is mandatory in core 1.0 — so "does this device support compute" is not a question
Vulkan answers; it is true by definition.

**Design.** `RenderCapabilities` is an API-neutral descriptor — identity, 20 feature bits,
18 limits — with the expected Vulkan/D3D12/Metal mapping documented per field in the header.
The translation lives in `Vulkan/VulkanCapabilities.{h,cpp}`, which is the only place that
knows Vulkan's spelling of anything; a DX12 or Metal backend adds its own translator writing
the same struct rather than extending this one. Selection is by macros at compile time in the
OSAL style (owner's direction), so no runtime API-kind enum is needed and `APIType` is gone —
it had one enumerator, no `switch` anywhere, and nine uses that were all `== Vulkan`, i.e.
assertions that could not fail. `Engine/Renderer/DX12` and `Engine/Renderer/Metal` do not
exist, though `.module.config` still names them in `ignoreSubdirectories`.

**The root cause was the type, not the numbers.** A default-constructed descriptor used to
carry 4096/16/16, indistinguishable from a real query. It now zero-initialises and
`isDeviceQueried == false` means "nobody asked", so an unqueried snapshot can never pose as
data again. Two query paths disagreed on `supportsComputeShader`; there is one adapter now.
The 21 flags are bit fields (`: 1`), measured at 236 -> 220 bytes — a modest win because
`deviceName[128]` and the limit fields dominate. Bit-field caveats are documented in the
header: no address-of, and never serialise or upload this struct.

**Two latent defects found while getting here.**

1. `Core/Debug.h` opens `namespace hbe {` at line 21 in its `__DEBUG__` branch and never
   closes it before `#else`. Any `__DEBUG__` build therefore swallows every later header into
   `namespace hbe` and cannot compile — and `__DEBUG__` is defined nowhere in the build system
   (`BuildConfig.h` line 41 still claims it tracks `NDEBUG`/`_DEBUG`/`DEBUG`, which is stale).
   So **every `Assert()` in the engine is a no-op in every configuration**, and the suite's
   "285 PASS" means "did not crash", not "held". That is exactly how
   `Assert(caps.supportsComputeShader)` — asserting `true` against a `false` default — survived
   review, and how `Assert(renderer.Initialize(nullptr), "should succeed")` survived even though
   `Initialize` returns false on line 149. I corrected that test to assert what the code does.
   Fixing `Debug.h` is a one-line close-brace but would light up assertions engine-wide, so it
   is left for the owner rather than smuggled in here.
2. `hb_standards.sh` include-layout cannot accept a `#ifdef`-guarded `#include`: `#endif`
   counts as the first body line, and any later `#include` resets its blank-line counter. Since
   50 engine headers declare their test class through exactly that idiom, `RHICapabilities.h`
   and the pre-existing conditional `<vulkan/vulkan_win32.h>` in `VulkanRenderer.cpp` stay red.
   Proven pre-existing by running the checker's own logic over the `HEAD` revision. My own files
   pass; I did not deviate from a 50-file convention to appease a linter blind spot.

**Verification.** Build `-Wall -Werror` clean; `EngineTest` 285 PASS / 0 FAIL across 53
collections; `VulkanExample` links. End-to-end through the engine's own code (not the throwaway
probe): descriptor reports "Apple M4 Pro", api 1.1, driver 10401, vendor 0x106b, 16384 2D
texels, 31 vertex attributes, 155 uniform bindings, tessellation true — matching the independent
probe field for field. Assertion liveness proven out-of-tree, since the suite cannot show it:
20 predicates pass with `__DEBUG__` active, and a negative control wearing the retired
4096/16/16 numbers does trip the regression assert. `RenderCapabilities` is asserted
`is_trivially_copyable`; the fresh-vs-queried split is unit-tested both ways.

## `__DEBUG__` made buildable; what `hb_standards.sh` does and does not prove (2026-09-06)

**Trigger:** owner asked to fix the `__DEBUG__` issue, and separately doubted that
`hb_standards.sh` can enforce the coding standards. `Renderer::Vertex` stays as it is.

**The fix is one closing brace.** `Core/Debug.h` opened `namespace hbe {` in its `__DEBUG__`
branch and never closed it before `#else`, so any `__DEBUG__` build swallowed every later
header into `hbe` — `hbe::hbe`, and libc++ failing inside `<sstream>`/`<mutex>`. The whole
engine now compiles with `-D__DEBUG__` (140/140; it failed to compile at all before). The
three standard headers moved out of the guard to file scope, which is also what makes the
file pass the include-layout check.

**Turning assertions on is deliberately NOT part of this commit.** With `__DEBUG__` defined,
`EngineTest` aborts after 25 of 285 cases, inside `PoolAllocatorTest` TC0 "Construction". The
cause is a pre-existing contradiction in `Memory/PoolAllocator.cpp`: line 28 asserts
`blockSize >= sizeof(TSize)` on the raw parameter while line 21 clamps that very parameter
with `std::max(blockSize, sizeof(TSize))` — either the clamp is dead code or the assert is
wrong — and the test constructs pools with `blockSize` 1..99, so `i < sizeof(TSize)` trips it.
`Assert()` is also divergent across branches: the debug overload is variadic on any type and
not `noexcept`, the release overload demands `const char*` second and is `noexcept`. Note
`FatalAssert()` has always been live, since it sits outside the guard. `BuildConfig.h`'s
"Debug Control" comment claimed `NDEBUG`/`_DEBUG`/`DEBUG` drove this; nothing did, so the
comment now states what is actually true and how to opt in.

**What `hb_standards.sh` actually runs**, in order: clang-format check-or-apply over the
in-scope files; 8 grep/awk rules (space indentation, joined empty bodies, joined empty
records, exceptions, `m_` prefix, `return std::move`, `virtual` + `override`, `inline`); file
hygiene (copyright line 1, trailing newline, trailing whitespace); an include-layout awk; and
a build gate over EngineTest/VulkanExample/WindowExample x Debug/Dev/Release plus the
CodingStandards target. It excludes `.mm`/`.m` and `.inl` on documented grounds. It checks
only staged/committed files, so the ~220 non-conforming files are never flagged unless touched.

**Four measured holes, which is why it cannot be treated as proof of conformance:**

1. `--test` cannot pass on macOS: it calls GNU `timeout`, which is absent here. All 12 builds
   passed and it still reported `build gate : FAIL`, exit 2.
2. Worse, `--test` is vacuous by construction: it builds via `build.sh <target> <cfg>` with no
   `-test` flag, so `__UNIT_TEST__` is undefined and the binary contains **zero** test
   collections — observed directly as `PASS=0` after the gate ran, versus 285 with `-test`.
   A fixed `timeout` would report `[PASS] ... 0 pass lines`. Its metric is
   `grep -c 'PASS'`, counting log lines, not outcomes.
3. The include-layout awk matches `/^#(include|define)/`, so it treats `#define` as an include:
   `Config/BuildConfig.h` — which contains no `#include` at all — fails "block 2: not
   alphabetical" because its configuration `#define`s are grouped by concern, not sorted. It
   cannot pass without shredding the file's structure.
4. The same awk cannot accept a guarded `#include`: `#endif` registers as the first body line
   and any later `#include` resets its blank-line counter, so "exactly one empty line before
   the first code body (found 0)" is unavoidable. 50 engine headers declare their test class
   through exactly this idiom. `Core/Debug.h` was made to pass by un-guarding its includes;
   `RHICapabilities.h` and `VulkanRenderer.cpp`'s conditional `<vulkan/vulkan_win32.h>` stay
   red, proven pre-existing by running the checker's own awk against `HEAD`.

Nothing in it detects dead assertions — the exact class of defect fixed today. Rules needing
judgement (single-arg `explicit`, `[[nodiscard]]` getters, log-before-return, `constexpr` over
magic numbers) are listed in `SKILL.md` for manual review, which is honest rather than faked.
Use the tool as a fast mechanical filter plus build gate; conformance of behaviour still needs
a reader.
