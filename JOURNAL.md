# Journal

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
