# PLAN — Regroup the 47 commits above origin/master

**Status:** done — the series is open as PRs #18 to #23, stacked, merge in that order
**Started:** 2026-10-10
**Branch:** `master` rewritten in place; old head saved as tag `backup/pre-regroup-47`

## Goal

`master` is 47 commits ahead of `origin/master`. Turn those into 12 deliverable-sized
commits, each with a message of five sentences at most, without changing a byte of
repository content (one new file excepted: the archive of the old messages).

## Decisions taken with the owner

| Question | Answer |
| --- | --- |
| Regrouping depth | Option C — one commit per deliverable, 8 to 12 commits |
| Where the rewrite lands | `master` in place, plus an annotated backup tag at the old head |
| Message style | Conventional prefixes: `type(scope): summary`, then at most four body sentences |
| Disposition of old detail | Verbatim archive in `.Plans/COMMIT_MESSAGES_before_regroup.md`, and the tag |
| Verification depth | Three configurations at the new head and at every commit that changes engine or build code |

## Why the content is safe to hold identical

The series is linear, one author, and no pull request points at `master`. Each commit
either changes engine/build files or documents. Every doc path in the series is created or
edited only by doc-intent commits, so the two macro commits (`8ec8967`, `1c08c1f`) can hand
their 13 `docs/` files that did not exist yet to the commit that creates them.

## Redundancy found

| Redundancy | Commits | Resolution |
| --- | --- | --- |
| Rename to `__TEST__`, then a second rename to `TEST_ENABLED` | `7f83312` `33cac23` `b6cc6cb` `9f9153b` `8ec8967` `1c08c1f` | One commit. Net effect: two reserved-name macros become `TEST_ENABLED`. `1c08c1f` itself argues for one commit |
| Three one-line `.gitignore` additions | `c41f36a` `9948c54` `5e4dbfa` | One commit |
| Pages authored, then the same headers stripped of prose | `2c7a643`+`c41486c`, `e0b5b02`+`c41486c`, `283d243`+`cfcc911`+`359182a` | Two commits: pages, then the strip |
| Four "prose moves to a design document" commits | `ea8e1f7` `c2fdb1a` `fd4f146` `49053dd` | Split by path: design documents into the pages commit, comment deletion into the strip commit |
| Two journal/plan commits naming the same two files | `1fc5212` `278fa6a` | One journal commit |
| Journal entries scattered over ten commits | `85d0f2c` `598b359` `c5d8318` `9948c54` `5e4dbfa` `0ed98be` `8ec8967` `1c08c1f` `1fc5212` `278fa6a` | Journal and `.Plans` hunks move to the last commit |

Message statistics before the change: 6 messages exceed five sentences, the worst at 23
sentences and 486 words (`1c08c1f`); bodies total about 5,300 words.

## Target series

| # | Title | Absorbs | Paths |
| --- | --- | --- | --- |
| 1 | `fix(memory): a pool's free-list terminator stops pointing at a live block` | `43924ed` | 6 engine |
| 2 | `fix(memory): an allocator returns a borrowed buffer to the allocator that lent it` | `391b0fb`, `bf8c5dd` engine | 3 engine |
| 3 | `fix(memory): the profiling build compiles and a fired assert names what it found` | `598b359` engine, `bdd1960` | 11 engine |
| 4 | `refactor(memory): member order, a dead union, and one misspelled name` | `3883875` `aba2063` `0f97c5d` | 7 engine, 13 docs renames |
| 5 | `fix(memory): DeregisterAllocator frees its proxy slot, and the inline pool reports its cached block` | `33bbc1b` `a6fafd7` | 2 engine |
| 6 | `build: TEST_ENABLED is the only test macro` | `7f83312` `33cac23` `b6cc6cb` `9f9153b` `8ec8967` `1c08c1f`, minus 26 created-later docs and their journal hunks | ~180 engine, ~110 docs, 9 skill |
| 7 | `fix(memory): allocation statistics count what the allocator actually did` | `be5b586` `71565ea` `a5611cb` `e8f885f` `a228add` `1931cf6` `fb8a983` `0efeed3` | 11 engine |
| 8 | `chore: ignore sanitizer builds, the work folder and generated runtime files` | `.gitignore` from `c41f36a` `9948c54` `5e4dbfa` | 1 |
| 9 | `feat(skills): a quoted-block checker and a comment gate that reads directories` | `9cea149` `f8d56c5` | 3 skill |
| 10 | `docs(memory): reference pages for every class the module declares` | `2c7a643` `e0b5b02` docs, `283d243` `359182a` `3bcbf5c` `c6db17a` `9d00565`, design docs from `ea8e1f7` `c2fdb1a` `fd4f146` `49053dd`, `bf8c5dd` docs, the 26 created-later docs of the macro commits | ~300 docs |
| 11 | `refactor(memory): the headers stop carrying prose` | `c41486c` `cfcc911`, engine of `ea8e1f7` `c2fdb1a` `fd4f146` `49053dd`, `AllocatorProxy.h` from `5e4dbfa` | ~21 engine |
| 12 | `docs: the journal and plans of the memory investigation` | journal and `.Plans` of `85d0f2c` `e0b5b02` `598b359` `c5d8318` `9948c54` `5e4dbfa` `0ed98be` `8ec8967` `1c08c1f` `1fc5212` `278fa6a`, plus the message archive and a new journal entry | ~30 process |

Total: 47 to 12.

## Pull requests

`origin/master` is linear with no merge commits, and PR #17 landed as its two commits with fresh hashes, so
this repository rebases: a split costs nothing of the commit messages. Each PR bases on the branch before it
so each shows only its own diff; after one merges, retarget the next at `master`.

| PR | Branch | Base | Commits | Diff |
| --- | --- | --- | --- | --- |
| #18 | `fix/memory-allocator-defects` | `master` | 6 | 21 files, allocator defects and the two skill gates |
| #19 | `build/single-test-macro` | #18 | 1 | 214 files, 485+/460-, the macro rename |
| #20 | `fix/memory-allocation-ledger` | #19 | 2 | 11 files, the ledger and the ignore rules |
| #21 | `docs/memory-api-reference-pages` | #20 | 1 | 243 files, the pages and four design documents |
| #22 | `refactor/memory-comment-strip` | #21 | 1 | 21 files, 21+/121-, the prose out |
| #23 | `docs/memory-investigation-records` | #22 | 1 | 30 files, journal, plans, the message archive |

Local `master` sits at `origin/master` while they are open. `backup/regroup-12-v2` is the pushed tip,
`backup/regroup-12-v1` the same series before the dead-link fix, `backup/pre-regroup-47` the 47 commits.

## Method

`git cherry-pick -n` onto a fresh series from `origin/master`, one `git commit` per row.
Conflicts resolve against ground truth in the old history: `git show <old commit>:<path>`,
or `git checkout <old commit> -- <path>` when the whole path's final state belongs here.
Engine files keep their per-hunk history; docs paths land once, in their final form.

## Verification

| Gate | Command | Pass when | Result |
| --- | --- | --- | --- |
| Content preserved | `git diff backup/pre-regroup-47 HEAD` | empty except the archive file and the new journal entry | pass — 3 files, all additions |
| Builds at head | `./build.sh Applications/EngineTest -test -debug -dev -release` | all three link | pass |
| Tests at head | `./build/Applications/EngineTest/<Config>/EngineTest` | 60 collections, 0 failures | pass — 393 testlets, exit 0 |
| Builds mid-series | same, at the eight engine or build commits | all three link | pass — 60 collections at each |
| Profiling | `BuildConfig.h:38` set to 1, Dev, then reverted | builds and runs | pass — 60 of 60 |
| Standards | `.pi/skills/hb-standards/scripts/check.sh` | no new findings | pass — findings identical, tree byte-identical |
| Reference | `docs_coverage.py`, `docs_methods.py`, `htmlcheck.py`, `quoted_blocks.py` | same counts as before | pass — 0 missing pages, 0 stale quotations; `htmlcheck` 0 problems at all 12 commits |
| Message hygiene | sentence and title-length check over the 12 messages | all at most 5 sentences | pass — 2 to 5 sentences, titles 42 to 76 chars |

## New identifiers

| New | Absorbs |
| --- | --- |
| `8bdfbb6` | `43924ed` |
| `a8b4226` | `9cea149` `f8d56c5` |
| `ca6ab76` | `391b0fb` `bf8c5dd` (engine) |
| `7763b02` | `598b359` (engine) `bdd1960` |
| `532c669` | `3883875` `aba2063` `0f97c5d` (engine) |
| `23a2d9c` | `33bbc1b` `a6fafd7` |
| `7e08cc9` | `7f83312` `33cac23` `b6cc6cb` `9f9153b` `8ec8967` `1c08c1f` |
| `f86f49e` | `be5b586` `71565ea` `a5611cb` `e8f885f` `a228add` `1931cf6` `fb8a983` `0efeed3` |
| `5c4affc` | `c41f36a` `9948c54` `5e4dbfa` (`.gitignore`) |
| `9d776ec` | the docs halves of `2c7a643` `e0b5b02` `3bcbf5c` `359182a` `c6db17a` `283d243` `ea8e1f7` `c2fdb1a` `fd4f146` `49053dd` `5e4dbfa` `9d00565` `bf8c5dd`, plus `.Plans/PLAN_lockfree-contention-benchmark.md` so the document that cites it has a target |
| `d5e6cf7` | `c41486c` `cfcc911` and the engine halves of `ea8e1f7` `c2fdb1a` `fd4f146` `49053dd` `5e4dbfa` |
| head | the `.Plans`/`JOURNAL.md` halves of ten commits, plus the archive and this plan; its own hash cannot be written in itself |

## Follow-up — the one dead link

`docs/design/ThreadSafeMultiPoolLocking_Design.html`, written by the pages commit, links
`.Plans/PLAN_lockfree-contention-benchmark.md`, which the first pass left for the process-document
commit. `htmlcheck.py` therefore found one dead link at the pages commit and one at the strip commit,
none before and none at the head. Fixed by moving that plan file into the pages commit, which is the
commit whose document cites it: `htmlcheck.py` now reports 0 problems at every one of the 12 commits,
566 pages before the pages commit and 805 after, and the content gate is unchanged. The pre-fix series
is tagged `backup/regroup-12-v1`.

## Reported, not fixed

* A page must own its entry before its comment is deleted, and the series holds that only at the
  endpoints: the twelve comments `43924ed` deleted leave in the first commit while their pages arrive in
  the tenth, and `DefaultAllocator.h` and `InlinePoolAllocator.h` lost their prose inside the ledger fix
  because the prose sat on code that commit deleted.
* `check.sh` at the strip commit still reports the pre-existing `ACCESS-REDUNDANT` and member-layout
  findings in the Memory headers. They are pre-existing because the tree at that commit is byte-identical
  to the old head for every file the gate reads.
* `.Plans/PLAN_remove_glfw_submodule.md`, `PLAN_remove_rapidjson_submodule.md`,
  `PLAN_simplify_pr17_messages.md` and `handover_base_stream_wip.md` were untracked before and stay
  untracked.

## Recovery

`git reset --hard backup/pre-regroup-47`, or the reflog of `master`.

## Risks

| Risk | Mitigation |
| --- | --- |
| A conflict resolves to the wrong state | The content gate is an empty diff, not a judgement call |
| An intermediate commit does not compile | Builds at every engine or build commit, not only the head |
| Detail lost from a trimmed message | Verbatim archive plus the tag |
| Force-push before review | Nothing is pushed; the owner decides |
