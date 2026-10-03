# PLAN: run `hb-standards` on every file of Engine/Core

Owner's instruction, 2026-10-03: "Run hb-standards on every file of Engine/Core." The rule-set-A pass was
already done (`8504000`..`5e0988b`); this plan covers the layers that were not, and states plainly what each
one costs, because the last of them is authoring work, not a sweep.

## Measured state of the module, per layer

| Layer | Check | State at `92405e0` | Work |
|---|---|---|---|
| 1 clang-format | seam-aware diff | **1 of 46** files: `Task.cpp` | sort its include block |
| 2 mechanical fixes | `autofix.py --dry-run` | 0 fixes | none |
| 2b rule set A | `blank_lines.py` | **0** | done already |
| 2b rule set B | `includes.py` | 4 findings: B5 = 3, B2 = 1 | root-relative paths, sort block |
| 3 comment ban | `comments.py` | **975 findings** | see the two-phase split below |
| 4 member layout | `layout.py` | 46 clean, 0 violations, 2 waived | re-check after strip |
| 5 docs | coverage / methods / validity | 0 missing pages, 0 methods without a page, 285 pages 0 problems | already complete |
| 6 gate | build + suite | builds in 3 configs; `WindowTest::TC0` fails **pre-existing** | re-run each phase |

## Why the comment ban is two phases, not one

`docs_coverage.py check-file` decides per file, and it draws the line the AGENTS.md rule draws: an **API**
contract has a page, so its `///` block can go; a `.cpp` comment is an **implementation note**, which belongs
in an HTML design document — "a migration no tool can verify". It refuses those without `--force`.

| Group | Files | Findings | What must happen first |
|---|---|---|---|
| **A — headers whose class page owns the prose** | 21 | ~760 | nothing: `check-file` passes, pages exist, class pointer present |
| **B — refused, implementation notes** | 25 | ~215 | author the design document that receives each note |

Phase A is a tool strip with a code-token proof. Phase B is authoring: read the file, decide what the note
means for the system, write it into `docs/Core/`, then strip. `--force` exists, and using it to skip the
migration would destroy the only copy of knowledge the repo holds — it is not the option.

## Where it actually stands, after steps 1-5 ran

Steps 1 to 5 are done and committed: `a4a7918` (rule set B), `0d5177b` (the checker's
attribute-blind name reader), `4e228d2` (the 14-header strip, 815 lines deleted). Core is at 0 findings for
clang-format, rule set A, rule set B, layout and member-init order; 374 of 375 testlets pass, the one failure
the pre-existing `WindowTest` heap ceiling.

Step 6 turned out to be two different jobs, because the refusal reason is the same sentence for both while the
material is not.

| Kind | Files | Findings | What the comments actually are | What must be authored |
|---|---|---|---|---|
| **B1 — `.cpp` implementation notes** | 8 | 93 | invariants, allocation strategy, locking protocol | a design document that receives them |
| **B2 — headers with no page kind** | 6 | 68 | **user-facing** contract: `Time.h`'s epoch semantics and thread-safety, `Debug.h`, `Runnable.h`, `TaskStreamIndex.h`, `Types.h`, `CommonMacros.h` | the reference has no entry kind for a header of aliases and free functions, so nothing owns this prose yet |

`Time.h` proves the gap: it declares type aliases and free functions (`Sleep`, `ResetEngineEpoch`,
`SetBaseFrameRate`) and one `__UNIT_TEST__`-only class, so `docs_coverage.py` finds "no documented entry of
its own" and demands no page — yet 35 comment lines state contract a caller depends on. The coverage model
requires pages for types and macro sets, so a utilities header can never become strippable. Stripping it
would delete the only copy; leaving it breaks the ban. This needs a ruling, not a sweep.

## Execution order

| Step | Action | Proof |
|---|---|---|
| 1 | Fix rule set B (4 findings) and `Task.cpp` formatting | `includes.py` 0, seam-aware format diff 0, three-config build |
| 2 | Strip group A headers, file by file | `strip_file` refuses if any code token moves; `comments.py` then 0 for that file |
| 3 | Re-run clang-format, then the rule-set-A table (format first, table second) | seam-aware diff 0, `blank_lines.py` 0 |
| 4 | Re-check layout and member-init order; re-check docs coverage | `layout.py` 0 violations, `docs_coverage.py`/`docs_methods.py` still 0 missing |
| 5 | Gate: build Dev/Debug/Release, run `EngineTest` | links in all three; the one pre-existing failure stays pre-existing and is reported, not silenced |
| 6 | Group B: per file, move implementation notes into a design document, then strip | design document named by the file it describes; `htmlcheck.py` 0 problems |
| 7 | Regenerate the ledger, journal, commit per step | `file_ledger.py --write` |

## What is explicitly out of scope

- Raising `MaxRetainedGlobalBytes` to make `WindowTest::TC0` green. The harness's own words are "Free it, or
  raise the ceiling with a reason that survives review", and the reason is an `Engine/OSAL` question.
- `--force` on any file before its notes have a home.
