# Commit messages before the 2026-10-10 regroup

The 47 messages that `master` carried above `origin/master` (`f0098c0`), kept verbatim
before they were regrouped into 12 commits and cut to five sentences each. Every one is
also reachable with `git log backup/pre-regroup-47`. The list is oldest first, the order
the commits were written in.

## `43924ed` 2026-10-06

~~~text
A free-list link is a block index or the sentinel, and a wrong bound handed a live block to a second owner

PoolAllocator::Deallocate wrote numberOfBlocks - 1 when the free list was empty, which is not a terminator but the index of the last block, so exhausting a pool, returning fewer than all blocks and allocating twice gave one address to two owners. WriteNextIndex asserted index < numberOfBlocks and so forbade the terminator the constructor itself lays down; the bound is now index <= numberOfBlocks, the one ReadNextIndex already applied. The guard is the testlet Reallocation After An Exhausted Free List: with the old branch restored it fails naming the colliding address in Release, while Debug and Dev abort one step earlier at Assert(numberOfFreeBlocks > 0) in AllocateBlock, which is why Release was the only configuration that could observe it silently.

Also carried here, because these changes live in the same three translation units and separating them would mean hand-splitting patches: the prerequisite 0 defect fixes F1 to F4, the four contention benchmark testlets M1 to M4 whose measurement chose the sharded redesign, and the twelve prose comments deleted from the three headers now that their contracts own reference pages.

Verified: Debug, Dev and Release all build, 60 collections SUCCESS, 389 testlets, exit 0. blank_lines and includes clean on all three headers; layout.py reports the same four pre-existing MEMBER-LAYOUT findings and no data member moved.

~~~

## `2c7a643` 2026-10-06

~~~text
Three allocators get pages and the locking redesign gets a home, so the headers can stop carrying prose

53 pages: PoolAllocator 23, MultiPoolAllocator 15, ThreadSafeMultiPoolAllocator 15, one page per declared method name, each Signature section verbatim against its header. The module catalogue links all three and its Coverage row says three of nine allocators are documented rather than none; the six that remain are named rather than glossed over. ThreadSafeMultiPoolLocking_Design records the five locking options the measurement ruled out and why the thread-sharded chunk design won, including that only the qualitative verdict is decision-grade because the contention ratios move up to 57 percent between runs.

The PoolAllocator pages were written against behaviour that was subsequently fixed, so their sentinel contract, the WriteNextIndex bound, the quoted Deallocate body and the class Coverage row all say what the code does now. Pages describe a defect they cannot prove, and one such open question turned out to be real; where a claim is not established the page says so instead of asserting a contract.

htmlcheck.py: 620 pages, 0 problems. docs_methods.py Memory: 0 methods without a page.

~~~

## `85d0f2c` 2026-10-06

~~~text
The journal records what the measurement said, including the claim that turned out to be false

ReportConfiguration cache doubling is confirmed; the std::sort move-assignment freeing a live bank is refuted, zero releases measured, and it is written down as refuted so nobody re-files it. Plus the five-wave migration plan, the benchmark spec, the prerequisite 0 record, and the proposed quoted-block checker with its impact, cost and recommendation.

~~~

## `9cea149` 2026-10-07

~~~text
The quoted-block checker proves a page's copied code still matches the source it copied from

A `<pre><code>` block captioned with a file and a symbol claims to be that symbol's text, and nothing
re-checks that claim once the header moves. The checker reads every caption under a path, finds the run
of lines inside the symbol, and reports each block verbatim, stale with a diff, or unresolvable, so a
page cannot quietly keep quoting a body the engine no longer contains.

~~~

## `391b0fb` 2026-10-07

~~~text
A monotonic allocator hands its backing buffer back to the parent that supplied it, not the scope that happens to be open

The destructor recorded its owner in parentID at construction but released through the ambient
MemoryManager::Deallocate(void*, size_t), so destroying an allocator while its own AllocatorScope was
open routed the release into that allocator's own inert Deallocate, which ignores individual releases,
and the buffer was never returned. The two new testlets measure allocated against freed bytes across
the destruction, one per allocator, and both fail on the old call.

~~~

## `3883875` 2026-10-07

~~~text
AllocatorScope declares its two ids above its functions, so the data layer reads as one block

The private members sat behind the destructor instead of ahead of the constructors. Their order relative
to each other is untouched, so nothing about construction or destruction changes.

~~~

## `e0b5b02` 2026-10-07

~~~text
The last seven Memory classes get reference pages, so every header in the module has somewhere to point

120 new pages under docs/Memory, with the module index and the MemoryManager internals design revised to
match, and the plan and journal entries that recorded the migration kept with the rest of the project
process documents.

~~~

## `c41486c` 2026-10-07

~~~text
Seven Memory headers trade their prose for the pointer line, and the blank lines the deletions disturbed are repaired by the table

Deleting a comment before its replacement page exists is the one change that cannot be undone, so this
sits on the documentation branch rather than on main. Each header keeps exactly one
`/// API reference:` line addressing the page that now owns the text, and the deleted comments left
blanks behind that the size table, not `clang-format`, had to settle.

~~~

## `598b359` 2026-10-07

~~~text
Memory and Core name the real source-location type, so a profiling build compiles for the first time

~~~

## `3bcbf5c` 2026-10-07

~~~text
Quoted bodies say what they omit, so a page cannot thin the source in silence

Twenty-six quotations had narrowed a function body without marking the gap, which is indistinguishable from a quotation that has gone stale. Each now carries an elision marker on its own line, in the form the corpus already uses, and quoted_blocks.py reports 0 stale over docs/Memory where it reported 26. Twenty captions still name a parameter list that does not resolve to one definition, so the checker exits 1 on those and they are tracked separately: an unresolved caption is a page that has not said which overload it quotes, which is a defect of the same kind.

Measured with redirects: quoted_blocks.py 445 blocks, 191 verbatim, 0 stale, 20 unresolved; htmlcheck.py 740 pages 0 problems; docs_methods.py Memory 0 methods without a page.

~~~

## `f8d56c5` 2026-10-07

~~~text
The comment gate refuses a directory instead of counting it as one violation

Passing a directory examined no files and still reported a violation, and --strip on a directory reported a comment stripped from zero files while exiting 0, which is a gate that certifies nothing. The module totals that never reconciled with the per-file sums came from this: 120 against 109 was not a mystery in the sources, it was a phantom finding per directory argument. The exemption list now applies identically in both modes, so a module run and the sum of its file runs agree. Measured after the fix: Engine/Memory is 55 violations across 39 files and Engine/Core is 0 across 46, and a directory argument now fails by name.

~~~

## `aba2063` 2026-10-07

~~~text
Removed unnecessary union variable

Just simplification

~~~

## `c41f36a` 2026-10-07

~~~text
Add a temporary work folder to .gitignore

~~~

## `283d243` 2026-10-07

~~~text
Wave 4's infrastructure headers get pages, including the identity type every other signature is written against

AllocatorID, PoolConfig, PoolConfigUtil, MultiPoolAllocatorConfig, MultiPoolConfigCache, Optional, AllocatorProxy, AllocatorScope, ScopedAllocator, SystemAllocator, AllocStats and Memory.h. AllocatorID's page carries the 256 allocator ceiling and the invalid-id sentinel, because every other entry point in this module is written in terms of that identity and its failure modes.

~~~

## `cfcc911` 2026-10-07

~~~text
The infrastructure headers say nothing in their own files that the reference does not say better

Twenty-five counted comments removed across twelve headers, each one checked against its page first. Most were @brief restatements of the type name; the ones that carried a real claim, such as AllocatorScope saving and restoring the current id, were confirmed present on the page before deletion. AllocatorProxy keeps five in-parameter markers inside its two function-pointer aliases: they name an otherwise blank argument position, and the aliases' pages quote those lines verbatim, so removing them would cost readability and break signature equality. That remainder is reported debt, not an oversight.

~~~

## `359182a` 2026-10-07

~~~text
The Memory module page stops claiming headers are undocumented that now have folders

Its Coverage section said seven headers had no entry and that SystemAllocator was the last allocator without a page. Both were stale. Every header in the module now has a folder, so the honest gap is the seventeen .cpp files whose implementation prose needs design documents and cannot be migrated by deletion. This also gives the twelve wave 4 folders their only inbound links, and records that Memory.h is the one folder holding a single page for several names.

~~~

## `bdd1960` 2026-10-07

~~~text
An assert that fires must say what it found, in words the logger can actually print

MultiPoolAllocator::NewBankAllocate asserted false with no message at all after generating a bank it then could not locate, which is the one moment a reader needs the size, the index and the block count. PoolAllocator::ReadNextIndex passed printf conversions to this engine's logger, which concatenates values rather than substituting them, so the Dev abort printed the format string literally followed by unreadable digits. Both now state the quantities in the engine's own style.

~~~

## `c5d8318` 2026-10-07

~~~text
The sharded allocator's failure is recorded where the next reader will find it

Which measurement separated the redesign from the strip, what the failing asserts rule out, why an out-of-bounds free-list link with a garbage value points at an extent write rather than at off-by-one arithmetic, and that the branch does not yet fix the contention collapse it was designed for.

~~~

## `9948c54` 2026-10-07

~~~text
The narrowed diagnosis belongs on the main line, not only on the branch it came from

JOURNAL.md is the log a reader opens before choosing where to work, and the newest entry there was two sanitizer builds and an address-shaped free-list link. Both scratch build directories are now ignored on every branch.

~~~

## `5e4dbfa` 2026-10-07

~~~text
The last header pointer line, the authoring rules the waves were built on, and the suite's runtime artifact ignored

AllocatorProxy.h was the one header left owing its address because it kept five in-parameter markers, and its page exists now. The authoring rules file records what nine documentation agents learned, including that a class page carries no code at all and that verification cost must be tiered by what actually changed: a comment deletion is proven by one configuration, a changed allocation strategy by all three, and a page edit by no build whatsoever. .multiPoolConfig.dat is written by the configuration cache testlets on every run and was shadowing the tree.

~~~

## `0ed98be` 2026-10-07

~~~text
The sharded allocator's aliasing bug is named: extents are borrowed from the ambient scope

mmgr.Allocate with no id routes through GetScopedAllocatorID, so an extent is a loan from whichever allocator is ambient when the pool grows, and inside the benchmark that lender is a MultiPoolAllocator whose PoolAllocator banks still consider those bytes their own. The redesign then writes tagged 64-bit links into them, which is exactly the address-shaped free-list value the assert printed, and why both sanitizers stayed silent.

~~~

## `c6db17a` 2026-10-07

~~~text
Three quotations caught up with the source they describe, and one is now quoted whole

All three drifted because of fixes landed in this module rather than because a page was written wrong: the message-bearing FatalAssert in NewBankAllocate, the readable free-list assert in ReadNextIndex, and the destructor's own member name. new-bank-allocate.html now quotes the function body in full, because a 25-line body excerpted into runs had already drifted twice at its seams; the cost is that the page must be revisited when the two remaining comments inside that body migrate to a design document.

~~~

## `bf8c5dd` 2026-10-07

~~~text
A stack allocator returns its buffer to the parent that lent it, not to whoever is in scope

The constructor recorded parentID as the allocator that actually lent the backing buffer, and the destructor then released it through the two-argument call, which routes by the ambient scope. Construct one allocator inside a pool's scope and destroy it inside its own scope and the buffer is handed to an allocator that never owned it; measured before the fix the suite dies on StackAllocator's own cursor assert, which was message-less, so that assert now says how many bytes it was asked to release against how many are outstanding. The guard is StackAllocatorTest's Destroy In Own Scope, mirroring the one MonotonicAllocator already carried. Debug, Dev and Release each pass 60 of 60, and the destructor and deallocate pages are re-quoted, the latter now quoting its body whole because a seam-excerpted block broke the moment one assert inside it gained a message.

~~~

## `ea8e1f7` 2026-10-07

~~~text
MultiPoolAllocator narrates its own bank growth in the reference, not in the source

Four comments walked through what happens when no bank serves a request and when the selected bank has no blocks left. allocate.html already carries both claims, including the GetAvailableBlocks test, so the pages own them and the source says nothing. The body quotation on new-bank-allocate.html had to be re-taken in the same commit, which is the coupling that page now declares: a whole-body quote is exact until the source changes under it.

~~~

## `c2fdb1a` 2026-10-08

~~~text
Global allocation's reasoning moves to a design document, in its own words rather than a summary

Three block comments held why the accounting counters sit outside AllocatorScope, why they are constant-initialised atomics with no ready flag, why the backing store declines SystemAllocator's page-granular investigation configuration and stays on malloc, what exhaustion does and why bad_alloc is the wrong answer for an exception-free engine, why an unsized operator delete must ask the heap for the real block size, and why the replacement operators cannot carry the namespace. Paraphrasing that would have destroyed it, so the document carries it as written and the source says nothing. The document is linked from the Memory module page, and two blank-line findings left by the removals are repaired.

~~~

## `fd4f146` 2026-10-08

~~~text
The investigator self-tests get the run notes they never had, and lose three comments that were the whole documentation

Each self-test allocates one byte and then writes outside it on purpose, so the investigator posts a signal and the run stops on that line. The three comments said only to comment the write out to reach the next testlet, which is a procedure too short for its own page and too load-bearing to delete: a first run looks like a crash. The new document carries that, the switch table, and two shapes recorded as suspicions rather than design — the over-run testlet commits an under-run while standing in the branch where the under-run check is disabled, and the use-after-free testlet logs itself as Overrun Test. A build cannot exercise both buffer testlets because they are the if and else of one switch.

~~~

## `49053dd` 2026-10-08

~~~text
PoolAllocator's geometry and the reasoning behind its guards move to a design document

Twenty comments held five things: why a block smaller than the free-list link is a precondition rather than a clamp, why both operands of the capacity arithmetic are members so no unqualified name can pick up the unaligned request, why the guard probes block sizes 32, 24 and 100 — the defect was laying the free list out with the requested stride while walking it with the aligned one, which handed one block to two callers — why that guard walks the released blocks twice, and why it asserts on block addresses rather than on the stride. Every .cpp in the module is comment-free now, so the module page stops describing a debt that no longer exists and names the four documents that hold what the sources used to.

~~~

## `0f97c5d` 2026-10-08

~~~text
The manager reports its multi-pool configuration, spelled as the language spells it

ReportMultiPoolConfigutation was a slip in a public name, and the reference had been faithfully quoting it on the grounds that a page title a reader cannot grep is a page they will never find. That argument only holds while nobody fixes the name; it is fixed here in the header, the definition, both call sites, the page and the fifteen documents that linked it, and the sentence that justified keeping the typo is gone because a name that is right needs no apology. The method exists only under PROFILE_ENABLED, so the proof is a profiling-configuration build of the engine with zero errors, alongside Debug, Dev and Release each passing 60 of 60.

~~~

## `33bbc1b` 2026-10-08

~~~text
DeregisterAllocator releases its proxy slot on every path, and says what it found in the engine's own construct

Both detectors returned before the tail, and the tail is the only place the proxy goes back to the pool, so every leak report and every thread-id mismatch silently cost one registration slot; once the pool drained, RegisterAllocator handed out InvalidAllocatorID and every later profiling report tripped that guard, which is how the 256-slot exhaustion looked like a profiling bug. Neither report needs to interrupt control flow to be useful, so both are now Assert, which is what the engine uses for a claim that must hold and which costs nothing in a shipping build; the hand-written Log plus conditional debugBreak scaffolding is gone, and with it the second, unreachable leak branch that could never run because the first one always returned. Debug, Dev and Release each pass 60 of 60, and comments and blank-line gates are clean.

~~~

## `a6fafd7` 2026-10-08

~~~text
The inline pool's cached block reports both ways, and under profiling a foreign block is no longer swallowed

Statistics only exist when profiling is on, so the accounting no longer needed to stay off the immediate fast path: handing the cached block out and taking it back now report an allocation and a release, which makes allocCount, deallocCount and usage balance when the pool dies instead of leaving one block counted as outstanding forever. The ownership test also runs on the profiling side of the conditional and before the cache slot is filled, because a small block belonging to another allocator used to be captured into the cache and later handed to an unrelated caller; the shipping configurations are untouched, and Debug, Dev and Release each still pass 60 of 60. Honest limit: the profiling run still stops at the same assert in ReportAllocation against slot 234, so this was not the cause of that one, and the capture path remains open in a shipping build under #26.

~~~

## `7f83312` 2026-10-08

~~~text
__TEST__ is the only test macro the build defines

`__UNIT_TEST__` is removed and `__TEST__` carries the whole meaning. The two were defined
together in every configuration that defined either, and no source ever asked `__TEST__` a
question, so the pair named one question twice rather than asking two.

This layer is the definition itself. `.project.config` now emits one token where it emitted two,
so Debug and Dev carry `-D__TEST__` on every translation unit and
`precompileDefinitionsRelease` stays deliberately empty. `Engine/Test/.module.config` renames its
plain `precompileDefinitions` rather than re-scoping it, which preserves the asymmetry
docs/BuildSystem.md warns about: in Release `libTest.a` is compiled with the macro while the
engine modules are not, and that links only because `TestMain.cpp`'s `#else` branch never names
`Test::RegisterSuite`. `build.sh`, `build.bat` and the hand-written
`Engine/Renderer/Vulkan/CMakeLists.txt` lose the duplicate token; the Vulkan file is the only
CMake file edited by hand, because Renderer's `ignoreSubdirectories` keeps the generator away
from it.

The pair is replaced rather than one half renamed, so no duplicated `-D__TEST__` is emitted.

~~~

## `33cac23` 2026-10-08

~~~text
Regenerated: the 19 generated CMake files carry one test macro

Produced by `./generate_cmake_files.sh` from the committed `.project.config`, never by hand-edit,
because a hand-edit is erased by the next generation pass silently.

Fidelity proven before committing: the diff is 39 lines and every one is a definition line. 38 are
the `CMAKE_CXX_FLAGS_DEBUG` and `CMAKE_CXX_FLAGS_DEV` lines losing the `-D__UNIT_TEST__` token, and
the 39th is `Engine/Test/CMakeLists.txt`'s own `add_compile_definitions`, which its
`.module.config` drove in the previous commit. No target, source, include or link statement moved.

~~~

## `b6cc6cb` 2026-10-08

~~~text
All 127 test guards read __TEST__

Every `#ifdef __UNIT_TEST__` becomes `#ifdef __TEST__`, together with the 125 `#endif` structural
labels that name the closed construct and the two `TestMain.cpp` message strings that tell a user
which macro to define. Each file's existing label spacing is preserved: 104 sites keep
`//__TEST__` and 21 keep `// __TEST__`. The substitution touches the identifier and nothing else,
so the diff is exactly 254 lines changed in 127 files with no line added or removed, and no
directive changes form from `#ifdef` to `#if`.

The strings matter as much as the directives. A rename that stopped at the preprocessor would have
left the Release binary instructing the reader to define a macro that no longer exists.

check.sh reports 2 blank-line findings in Engine/Test/UnitTestCollection.cpp, at :119 (A4) and
:226 (A5). Both are pre-existing and are surfaced only because this commit touches the file: run
against the untouched original with the untouched checker they reproduce at the same two line
numbers, and the full finding set over all 127 files is byte-identical before and after the
rename. :226 is the trailing-region tension `docs/CodingStandards.md` already records for
TestMain.cpp — A14 wants a blank before a trailing `#ifdef __TEST__` while A5 refuses one after a
closing brace. Resolving it is a rule-table decision, not a side effect of a rename, so it is left
alone here and reported.

Suite parity held across the rename: 392 testlets in 60 collections, 0 failures, in Debug and Dev.

~~~

## `9f9153b` 2026-10-08

~~~text
The standards checkers recognise the guard by its new name

54 literals across blank_lines.py, check.sh, comments.py, docs_coverage.py, includes.py,
layout.py, prove_regroup.py and SKILL.md named `__UNIT_TEST__`. These are the dangerous sites,
because a regex that stops matching does not fail: prove_regroup.py's region guard would have
returned `[NONE]` and let a regrouping move test code unnoticed, includes.py would have probed a
guarded file without the macro and reported includes as unused — the exact failure JOURNAL.md
records as "11 names found before, 164 after" — and layout.py would have dropped the four wholly
guarded files from the member-layout check. All three now key on `__TEST__`, and
__pycache__ is deleted so no stale byte code can mask an edit.

Three sites needed a judgement rather than a substitution:

  includes.py: TEST_MACRO_FLAGS carried `-D__TEST__=1` twice after the substitution, so the
      duplicate is dropped. docs_coverage.py matched `__TEST__|__TEST__`, which is one alternation
      now. Both were already redundantly tolerant, since the dead macro's name was in them.
  layout.py said the rescue pass added a macro "which the Dev build does not define". That is now
      false, and the reason it still has to add it was never the configuration: it is that
      cmake-build-debug/compile_commands.json, the database the checkers read, is generated without
      the macro. The note and the docstring both say that instead of blaming Dev.

Each module imports cleanly after the edit.

~~~

## `8ec8967` 2026-10-08

~~~text
The pages that named two macros now name one, and the false ones are corrected

107 occurrences under docs/ plus AGENTS.md and README.md. Most are the substitution. The rest are
claims that the substitution would have left asserting something untrue, so they were rewritten
against measurement rather than renamed:

  docs/BuildSystem.md called the pair a single bullet and said
  `grep -c 'D__TEST__' ... impl-Release.ninja` reads 0. It reads 4, because Engine/Test's own
  `.module.config` defines the macro in Release; 0 was the figure for the dead macro. The bullet
  now names one macro, gives Debug 155 / Dev 155 / Release 4 by grep -c and 455 / 455 / 4 by
  grep -o, and keeps the warning that Release's four are Engine/Test's own sources linking only
  because TestMain.cpp's #else branch never names Test::RegisterSuite.
  docs/design/BuildMacros_Design.html offered "the engine's __TEST__ and __TEST__" as its example
  of a per-configuration definition, and its verification block still claimed Release read 0. Both
  corrected, and the history section gains the reason the pair was never two questions.
  README.md, AGENTS.md, docs/RunningTests.md and docs/HelperScript.md all said -D__TEST__ is what
  makes the unit-test sources compile. That was false while the dead macro existed — it compiled
  nothing — and the rename is what makes it true, so those lines needed no edit, only the record.

docs/Config/BuildConfig/index.html still says "the two macros", and correctly: those are
LOG_BREAK_IF_ERROR and LOG_FORCE_PRINT_IMMEDIATELY, unrelated to this change.

JOURNAL.md records why the merge is behaviour-free, why the tooling rather than the compiler was
the silent-breakage surface, and the profiling defect found beside the task.

~~~

## `1c08c1f` 2026-10-08

~~~text
The test macro is TEST_ENABLED, which docs/CodingStandards.md already required

`__TEST__` becomes `TEST_ENABLED`. Both halves of the old pair began with a double underscore, and
`[lex.name]` reserves every identifier containing one to the C++ implementation — which `docs/CodingStandards.md:27`
already states in prose and illustrates with `SCREAMING_SNAKE_CASE`. So the macro gating the entire suite violated the
rule written in the same document that names it two hundred lines further down. `61c6152` had already done this for
`__DEBUG__`, producing `DEBUG_BUILD` / `DEV_BUILD` / `RELEASE_BUILD`; this finishes that cleanup rather than starting
a new one.

`MEMORY_INVESTIGATOR_TEST_ENABLED` already exists, so the new name is a live suffix of a live one. It is safe, and
checked before editing rather than after: that macro contains `_TEST_`, not `__TEST__`, so the substitution cannot
reach it, and its two uses in `SystemAllocator.cpp` are unchanged. A future `grep TEST_ENABLED` will find both, which
is the trade for a name that is not reserved.

Third name, same counts, which is the point of measuring rather than asserting. `TEST_ENABLED` reads 455 / 455 / 4
tokens for Debug / Dev / Release, exactly what `__TEST__` read and what `__UNIT_TEST__` read before it. Debug and Dev
run 392 testlets in 60 collections with 0 failures; Release without `-test` prints `built WITHOUT TEST_ENABLED` and
exits 1. The style checkers report the same two pre-existing findings as before (`UnitTestCollection.cpp:119` A4,
`:226` A5) and 0 include findings, so the rename introduced none.

One commit rather than the five layers used last time, because a rename is atomic by nature: split across commits it
leaves every intermediate commit with guards reading a name no build defines, which is a tree that cannot compile.

Two defects the previous rename left behind are repaired here:

  scripts/hang.sh still named `-D__TEST__`. That pass selected its files by containing `__UNIT_TEST__`, so a file
  naming only the other macro was never in the set — a filter that silently drops members, reporting a complete
  sweep.
  docs/Memory/InlineMonotonicAllocator/index.html had listed two distinct macros, `__TEST__` and `__UNIT_TEST__`;
  after a blanket rename both halves became one word, and the page claimed the Debug and Dev flag sets add
  "`__TEST__` and `__TEST__`". Renaming prose that was naming two things makes the sentence fluent and wrong. It
  now names what those flag lines actually add.

The history paragraph in docs/design/BuildMacros_Design.html, which exists to record that `__TEST__` was the dead
macro, was itself rewritten by this rename and had to be repaired: it came out claiming the dead pair had been
`__UNIT_TEST__` and `TEST_ENABLED`. It now gives both renames in order and quotes the standard as the reason for the
second.

One earlier claim is corrected rather than repeated: the Renderer does not receive this macro from
`Engine/Renderer/Vulkan/CMakeLists.txt`. That file is never included — `Engine/Renderer/CMakeLists.txt` has no
`add_subdirectory` at all — so its `add_compile_definitions` is inert. The ninja counts said so all along. Renamed
there anyway so no stale name survives; no build behaviour changes.

~~~

## `be5b586` 2026-10-10

~~~text
A failed block claim is not an allocation, and the profiler no longer counts it as one

~~~

## `71565ea` 2026-10-10

~~~text
The pool buffer testlet returns its block before destroying the pool, so the leak check means what it says

~~~

## `1fc5212` 2026-10-10

~~~text
The profiling diagnosis is written down: four dead hypotheses, five fixes, and the one number still missing

~~~

## `a5611cb` 2026-10-10

~~~text
Malloc-backed memory gets one owner, and its release reports the bytes it really owned

SystemAllocator::DeallocateBytes tested `#ifndef MEMORY_INVESTIGATION_ENABLED`, which holds only when the macro is undefined, and BuildConfig.h always defines it -- so the release path reported the caller's requested size while the allocation reported OS::GetAllocSize. Every block whose malloc size rounded up therefore left a permanent residue: the allocator deregistered with usage=32884 against allocCount=deallocCount=829. Measured per pointer, 7 pointers account for a diff summing to exactly 32884 with zero outstanding, so nothing was leaked and the ledger was what lied. DefaultAllocator no longer calls malloc and free itself when its recorded id is the system allocator; it opens the scope and dispatches, which makes the id-0 registration own and report that traffic -- reported system allocations went 829 -> 1018, and the leak assert is gone. Dev, Debug and Release each run 60 of 60 collections.

~~~

## `e8f885f` 2026-10-10

~~~text
An inline pool can no longer keep a block it does not own, in any configuration

DeallocateBytes tested its one-slot cache before testing ownership, and the ownership test sat inside `#if PROFILE_ENABLED`, so a shipping build compiled it out: a small block belonging to another allocator was captured into immediateBlock and later handed to an unrelated caller -- one block, two owners. A new testlet releases an 8-byte block owned by a recorded parent into a pool with an empty cache slot; it fails in Dev, Debug and Release before this commit and passes after. The ownership test is now unconditional, and the second test, which could no longer run, is deleted. deallocate passes n * sizeof(T) where it passed the element count, so a parent is finally told the block's real size. Measured on the immediate route in a Dev build (4M allocate/deallocate pairs), the added ownership test moved the cost from 1.14 to 1.09 ns per op -- inside the run-to-run spread.

~~~

## `a228add` 2026-10-10

~~~text
InlineMonotonicAllocator reports under the macro that actually exists

Its Allocate reported under `__MEMOR_STATISTICS__`, a macro defined nowhere in the tree, so the allocator could never show usage and `DeregisterAllocator`'s leak check had nothing to read. Allocation now reports under `PROFILE_ENABLED`, passing the caller's size as `requested` and the aligned size actually taken as `allocated`. The teardown drain moves to the same macro, and it is what returns the arena in one debit and brings usage back to zero. The per-pointer `ReportDeallocation(..., 0)` is gone: a monotonic arena releases nothing on an individual release, and a count that cancels while the bytes stay credited is the exact signature this session spent itself removing. Proved live by halving the teardown drain, which makes the detector say `InlineMonotonicAllocator(189) leaks 8 of 2048 bytes` -- a sentence the dead macro made impossible to print.

~~~

## `1931cf6` 2026-10-10

~~~text
A multi-pool that generated a new bank now says so in its own statistics

NewBankAllocate credited only the engine-wide fallback counter, so AllocStats::fallbackCount read zero for the one allocator family whose fallbacks are bank generations, and both multi-pool testlets failed the claim they make in their own name. ReportFallback is public now, beside ReportAllocation and ReportDeallocation; it was private only because FallbackAllocate had been its sole caller, and a bank-generating pool serves its own fallback rather than delegating it. Each new-bank path reports the request it could not serve before it reports the block it ended up handing out. Both collections pass under profiling, and Dev, Debug, Release and the profiling tree each run 60 of 60.

~~~

## `fb8a983` 2026-10-10

~~~text
StackAllocator's own tests now test what their messages say

"Vector Allocation" and "Allocation (2)" failed on a working allocation: their condition read `GetUsage() > 0` beneath a message that demands usage be non-zero, and their siblings in the same file already read `== 0`. "Destroy In Own Scope" compared cumulative global new and delete bytes across its region and called any positive difference a leak; measured, its 8 bytes were a 16-byte StaticStringTable intern that is never meant to be freed, less the 8 bytes of malloc size-class padding an unsized `operator delete` records for a 56-byte object. It now asks the question its own message names -- did the parent get back the pointer and the byte count it handed out. Teeth proved by making the test's observer lie: a wrong pointer and a wrong size each print their own message, and the honest run prints nothing.

~~~

## `0efeed3` 2026-10-10

~~~text
A zero-count release through the default allocator is a no-op, not an abort

Once the system id stopped bypassing the manager, a request for zero elements came back null like every other id, and the release that followed it aborted on this class's own `Assert(ptr != nullptr)` -- caught by a scratch testlet before it could be shipped, Dev build, `[Assert] Engine/Memory/DefaultAllocator.h failed`. `MemoryManager::Deallocate` had always treated a null release with a zero count as nothing to do and asserted only when the count disagreed, so the adapter was the one place in the module that refused the call its own manager accepts. The guard now mirrors that rule, which keeps the real detector: a null pointer arriving with a non-zero count still aborts. Dev, Debug, Release and the profiling tree each run 60 of 60 with it.

~~~

## `9d00565` 2026-10-10

~~~text
The reference says what the allocators do now, and one stale quotation fewer

Pages that described the malloc bypass as a deliberate fast path were describing an accounting hole as if it were a feature, so DefaultAllocator's class page and its allocate, deallocate, constructors and GetID pages now tell the single-route story, including the zero-count rule and the fact that the registration owning a block is the one that reports it. InlineMonotonicAllocator's five pages lose the dead-macro explanation: `__MEMOR_STATISTICS__` was defined nowhere, so what those pages called dead code in every configuration is now `PROFILE_ENABLED` code, and the release report they documented no longer exists. InlinePool's release pages record the new order -- ownership first, in every configuration -- and the byte-count fix, with the one thing still unobserved named instead of papered over. ReportFallback is public with three call sites, and both multi-pool pages say which counters a bank generation moves. `quoted_blocks.py docs/Memory` is at 0 stale from 5, which includes the two that predated this work, and `htmlcheck` and `docs_methods` are clean.

~~~

## `278fa6a` 2026-10-10

~~~text
The journal and the plan record what the measurements said

Six commits of allocator and test work, and the plan's open-defect table now holds one design item and six findings rather than three live defects. The residue was malloc's size-class padding all along: 9012, 21620 and 32884 are all 4 mod 16, which is the arithmetic that ruled out a leak before any of the fixes. The labels the plan used as issue numbers -- 22, 25, 26 -- are not issues or PRs, so the file now says so and every item argues from source. Four findings newly recorded and deliberately not acted on: the inline pool's upper ownership bound, the monotonic arena's unreported overflow, the missing profiling-only assertion behind the byte-count fix, and a build gate that names an example target the root CMakeLists never adds.

~~~

