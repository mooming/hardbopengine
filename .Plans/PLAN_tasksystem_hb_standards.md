# PLAN: apply hb-standards to Engine/Core/TaskSystem.h and Engine/Core/TaskSystem.cpp

## Confirmed goal

Bring the pair into the standard end to end, and hand the result to the owner for review. Two rulings
were given before any edit:

| Ruling | Content |
|---|---|
| Baseline | The committed header-to-source body move (`b12c953`) and the `DriveUntil` to `TestHelper` move (`3686c82`) are the starting state, not something to undo |
| Comments | Read all 201 lines, keep only what is worth a document, abandon the rest. Blank lines that a deleted comment was holding apart go with it |

## Measured starting state

| Layer | TaskSystem.h | TaskSystem.cpp |
|---|---|---|
| 1 clang-format | clean | clean |
| 2 mechanical greps | 0 failures | 0 failures |
| 3 comment ban | 0 comments | 201 comment lines in 42 blocks |
| 4 member layout | 0 findings | 0 findings (4 `[PARTIAL]` notes) |
| 4b grouping | two seams left by the body move | blank lines inside three function bodies |
| 5 reference | class page and 47 method pages exist | the body move made citations and some claims stale |
| 6 build | not yet run on this state | |

## What the comment audit found

`/tmp/hbe_ts/clause_docs.py` splits each comment block into clauses and reports which survive verbatim
in the tag-stripped `docs/` corpus. The reference is already the keeper of most of this prose:
`docs/Core/TaskSystem/join-and-clear.html` reproduces the shutdown order as a step table plus two notes,
and `has-stream.html` quotes the guard comment as its example.

### Engine code, 39 lines in 15 blocks

| Lines | Claim | Where it lives now | Disposition |
|---|---|---|---|
| 135-136 | no streams means nothing to ask; the destructor reaches this twice | `request-other-streams-close.html`, `has-stream.html` | delete |
| 144-146 | the IO stream is spared with the base stream because log work runs on it | `request-other-streams-close.html` | delete |
| 200-201 | teardown is reachable without the ordered shutdown, so asking is idempotent | `join-and-clear.html` step 2 and the safe-to-call-twice paragraph | delete |
| 213-216 | the 2000 ms bound is a measured choice, found by sampling | `join-and-clear.html` bound note | delete |
| 219-220 | the stream is pumped by shutdown and recorded before the first pass | `join-and-clear.html` step 3 | delete |
| 235-238 | abandoned work is named, never dropped quietly; posted callables reported as present | `join-and-clear.html` step 4 | delete |
| 254-256 | flush while the logging stream still runs | `join-and-clear.html` step 5 | delete |
| 259-260 | the driver outlives the system, so withdraw it before the streams are freed | `join-and-clear.html` step 6 | delete |
| 265-267 | the base stream closes last because it is the last executor left | no page says this | add one clause to `join-and-clear.html`, then delete |
| 269-273 | both engine streams end inside the engine's shutdown rather than one ending when the logger is destroyed | no page says this | add one clause, then delete. Also detached from the statement it describes |
| 275-276 | the reporting is finished, so the two executors close last, in that order | `join-and-clear.html` step 7 | delete |
| 296-298 | nothing is destroyed that was not closed first | `join-and-clear.html` step 8 | delete |
| 777-778 | `BuildStreams` must run on the thread that will drive the engine | `build-streams.html` step 1: verify it states the requirement, not just that the assert is Debug-only | verify, add if short, then delete |
| 792 | "Pre-defined Engine Task Streams" | nowhere needed, the block emplaces `Base` and `IO` by name | delete |
| 824 | the IO stream's driver is the logger's own thread | `get-io-task-stream.html` | delete, after fixing that page's stale return-type claim |

### Test region, 162 lines in 27 blocks

None of it is engine API: anonymous-namespace helpers and `AddTest` bodies. The durable content is the
test-methodology rules and the invariants the tests exist to hold, so it goes to
`docs/TaskSystemGuide.md` section 13 (what is tested) and `docs/TaskSystemRedesign.md` section 9
(guardrails), which already carry the zeroed-charge measurement and the lane-rate promise.

| Rule worth keeping | Lines it came from |
|---|---|
| A witness must not be a quantity the mechanism erases, and must be seen moving in the control case | 845-853, 1051-1086, 2323-2339, 2353-2354, 3364-3370 |
| Bound a wait in wall clock, never in iterations or passes, because a burn is milliseconds apart across configurations | 1668-1669, 1809-1812, 2117-2130, 3401-3404 |
| An in-band sentinel barrier beats a sleep: sleeping 200 ms passed on a loaded machine for the wrong reason | 971-978 |
| Write the two lanes out separately: one body over both is how a lane went unwitnessed and a mutant survived | 1205-1213, 3364-3370, 3666-3669 |
| A test owns a stream and does not run in front of a timing-sensitive neighbour | 1787-1790, 1714-1716, 2247-2248 |
| Read a shared stream's setting before tightening it, and restore it on every exit path | 3364-3370, 3496-3505 |
| A throttle may delay work; it may not strand it | 1901 |
| The age clock lives on the task, not the item, and a recycled record is dated strictly newer | 3288-3295 |
| Restore the recording of who dispatched, or an isolation result is silence rather than evidence | 1948-1952 |

Everything else in the test region is narration that restates the code or the test name beside it -
including one paragraph pasted three times at 2323, 2328 and 2333 - and is abandoned.

## Sequence, with the proof each step produced

| Step | Work | Proof, as measured |
|---|---|---|
| 1 | Strip the 201 comment lines, delete the blank lines they were holding apart, format | `6dad493`. Token multiset of each file identical to `/tmp/hbe_ts/pre_TaskSystem.*`; `comments.py` 0 violations in both; `clang-format` clean. Source 3716 to 3500 lines, header 157 to 143 |
| 2 | Re-group the header into the order the class is used | `9d9eace`. Non-blank line multiset unchanged, `__UNIT_TEST__` region byte-identical, data member declaration order untouched, `layout.py` clean |
| 3 | Repair the reference: re-point every citation, rewrite the claims the body move falsified, carry the rows above | 53 citations re-pointed; 5 pages rewritten; `htmlcheck.py` 44 pages 0 problems; `docs_coverage.py check-file` reports one entry documented, addressed from the header, method pages complete |
| 3b | The owner's rename `AreOtherStreamsClosed` to `AreUserStreamsClosed` landed mid-pass | Page renamed to `are-user-streams-closed.html`, 44 pages updated, the DOC-METHOD blocker cleared, and the page now states that both names cover the same set of streams |
| 3c | Re-sequence the class page's method table and all 44 sidebars, which follow header order | Derived from the declarations rather than hand-mended: 43 rows and 43 links, one per page, `renav` propagated |
| 3d | Guide section 13 rewritten: 9 stale test rows became 36, plus the test-writing rules | A checker compares `AddTest` names against the section: 36 in the source, 36 named verbatim |
| 4 | Lint and build | `check.sh --staged --test`: grep rule failures 0, advisory 0, `build gate PASS 12/12`, EngineTest 59 collections passed in Dev, Debug and Release, `TaskSystemTest` 36 of 36 in all three |
| 5 | Commit per step, journal the rulings | Four commits: strip, regroup, rename, reference. Journal entry at the head of `JOURNAL.md` |

## Deliberately not done

* `comments.py --strip` needs `--force`: the refusal is module-scoped and Core still owes 15 class pages
  and 197 method pages. This pair's prose has a home or is abandoned, which is the case `--force` exists for.
* Member order inside the data block does not change: member order is initialisation order.
* No engine behaviour changes. A body move or a signature is not part of this task.
* Definition order in `TaskSystem.cpp` stays as it is. No rule requires it to mirror the header, and
  moving 3 500 lines to satisfy a preference nobody wrote down is churn.
* Two extra guardrail rows in `docs/TaskSystemRedesign.md` §9: the reopen mechanism is already R1, R2,
  R23c and the *signals versus reopens* table, and the age clock is guide §16. Giving one claim a second
  home is how the first one starts to rot.
* Per-method `/// API reference:` pointers. The pair carries the class pointer at `TaskSystem.h:16`,
  which is what `docs_coverage` checks, and most Core headers are in the same state.
