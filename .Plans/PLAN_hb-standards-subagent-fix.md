# PLAN — hb-standards: deterministic fixes as a sub-tool, subagents as the primary fixer

## 1. Confirmed decisions

| # | Decision | Source |
|---|---|---|
| 1 | Layer 1 (`clang-format --apply`) is always included in a fix run | owner, 2026-10-01 |
| 2 | Layers 2, 3, 4, 5 must change from report-only to actually changing the tree | owner, 2026-10-01 |
| 3 | Layer 4 moves types, functions and data members, **and** re-sequences constructor initialization lists to match the new declaration order | owner, answer to the layer-4 question |
| 4 | Deterministic scripts fix **only a few safe items** and exist for efficiency; **AI subagent review and correction is the primary tool** | owner, answer to the layer-2 question |
| 5 | Layer 5 updates or creates the `docs/` API reference pages from the corrected code | owner, 2026-10-01 |

## 2. Shape of the change

Three roles, and the split is the whole design.

| Role | Does what | Never does |
|---|---|---|
| Deterministic script (`autofix.py`) | Mechanical rewrites that are correct without understanding intent, and declares every token it removes in a manifest | Rename across files, delete error handling, move members, write prose |
| Subagent (primary) | Everything needing judgement: member reorder, init-list re-sequencing, renames, exception-path removal, comment-to-page prose, fixing what the scripts refuse | Skip a proof, push, or edit a file another agent owns |
| Gates (`prove_format.py`, `layout.py`, `comments.py`, `docs_*`, build) | Independently re-derive what changed and refuse anything the fixer did not declare | Pass on "no work to do", or check nothing and say so |

Report-only stays the **fallback**, not the default: whenever a fix cannot be proven, the layer reports
instead of guessing. That preserves the one property the current skill is right about.

## 3. Layer by layer

| Layer | Now | After | Who fixes |
|---|---|---|---|
| 1 format | report, `--apply` optional | **always** applied first | `clang-format` |
| 2 mechanical | report only | safe subset applied, rest reported | script (subset) + subagent (rest) |
| 3 comment ban | `--strip` on demand | strip only after the prose is proven to live in `docs/` | script (strip) + subagent (prose first) |
| 4 member layout | report only | reorder applied per file, data block internally ordered, init lists re-sequenced | subagent, verified by `layout.py` + new init-order check |
| 5 docs | check only, `docs_page.py` writes when asked | pages authored/refreshed from the corrected signatures before any strip | subagent through `docs_page.py`, verified by the three existing checkers |
| 6 build | Dev, Debug, Release | unchanged, still mandatory and last | `check.sh` |

### 3.1 The safe list for `autofix.py`

| Fix | Why it is safe | Proof |
|---|---|---|
| include layout: sort within group, drop duplicates in the same group | the rule requires it; set equality is checkable | include set identical |
| joined empty bodies `{}` split to block form | layout only | token stream identical |
| `virtual` removed where `override` is present | `override` already implies `virtual` | removed-token declared, build proves vtable unchanged |
| trailing whitespace, missing final newline | no tokens involved | token stream identical |
| missing line-1 copyright notice | additive comment, exempt by rule | comment count only |
| `return std::move(x);` → `return x;` where `x` is the local return object | restores NRVO; a documented, single-shape edit | removed-token declared, build gate |

`autofix.py` writes `.Plans/fix-manifest.json`: per file, the fix names and the token counts each removed.
`prove_format.py` gains `--manifest` and refuses any difference the manifest does not declare, so a fixer
cannot grade its own homework.

### 3.2 What only a subagent may do, and its proof

| Work | Why not a script | Proof before commit |
|---|---|---|
| member reorder across the twelve blocks, data members included | needs to preserve intra-block order and re-open access at the anchor — the recorded failure is a file that compiled and failed three files away | `layout.py` 0 findings for the file |
| constructor init-list re-sequencing | must follow the new declaration order, per constructor | new `layout.py --init-order` reports `[FAIL]` on any mismatch; `-Wreorder` grep over the build log |
| `m_` / snake_case renames | cross-file, and grep hits string literals and macro bodies | subagent edits every referent; 3-configuration build |
| exception removal, log-before-failing | deletes or redirects control flow | subagent + `EngineTest` |
| comment → page prose | "a tool cannot know what a function was doing wrong" | `docs_coverage.py check <mod>` 0, `docs_methods.py <mod>` 0, `htmlcheck.py` clean |

### 3.3 Orchestration

| Property | Choice |
|---|---|
| Driver | `.pi/workflows/hb-fix-pairwise.js`, same shape as the proven `.pi/workflows/hb-review-pairwise.js` |
| Unit of work | one header plus its `.cpp`; disjoint file sets per agent so no two agents touch one file |
| Cross-file renames | run alone, after the per-file passes, in one agent for the whole module |
| Concurrency | 3 by default, hard cap 4, and ask the owner before exceeding it (`gate.sh` slots still enforced) |
| Isolation | no git worktrees — the worktree copy cannot see uncommitted siblings, and merges would be manual |
| Each agent | fixes, self-runs the layer check, then `gate.sh spawn` for the build, and writes a JSON verdict; edits confined to its own files |

## 4. Files touched by this plan

| File | Change |
|---|---|
| `scripts/autofix.py` | new: the six safe fixes + manifest output |
| `scripts/check.sh` | `--fix` (safe subset then full re-lint; layer 1 always), `--no-fix` keeps today's behaviour; `--fix --all` still refuses without owner scope |
| `scripts/prove_format.py` | `--manifest` support: declared vs observed token differences |
| `scripts/layout.py` | `--init-order` mode comparing mem-init lists with declaration order |
| `SKILL.md` | rewrite of the six-layer table, the "layers 3 to 5 never rewrite" paragraph, the module cycle, and a new "fix roles" section |
| `.pi/workflows/hb-fix-pairwise.js` | new: subagent fix driver |
| `JOURNAL.md` | why the report-only invariant was removed and what replaced it |

## 5. Invariants deliberately kept

1. Snapshot before strip, and `code_tokens` identical against it — the arithmetic proof stays.
2. A mover never invents an access specifier: insert a labelled block, then re-open the anchor's access.
3. No gate may pass on `ninja: no work to do`; touched files are `touch`ed.
4. `docs_coverage.py` must be called with the module **name**, and the plan adds a fail-loud guard so the
   path form `Engine/Core` (which silently checks nothing and prints "0 missing") cannot lie again.
5. Never push without explicit permission.

## 6. Checklist

| # | Step | Verification |
|---|---|---|
| 1 | `autofix.py` with the six fixes + manifest | unit-check on a scratch copy of `Engine/Core/ScopedLock.h`; token diff equals the manifest |
| 2 | `prove_format.py --manifest` | passes on a real `--fix` diff, fails when a token removal is undeclared |
| 3 | `layout.py --init-order` | fires on an intentionally mis-sequenced init list; silent on the current tree |
| 4 | `check.sh --fix` wiring, `--no-fix` unchanged | `--no-fix` output byte-identical to today on the same input |
| 5 | `docs_coverage.py` fail-loud on path-form argument | `check Engine/Core` now errors instead of printing "0 missing" |
| 6 | `hb-fix-pairwise.js` driver | dry run on one pair (`ScopedLock`), 3-configuration build green |
| 7 | `SKILL.md` rewrite | every claim in the new tables re-measured, not carried forward |
| 8 | Journal entry + commits per step | one commit per step, no push |

## 7. Open risks

| Risk | Mitigation |
|---|---|
| Init-list re-sequencing silently changes behaviour where a base class is initialized between members | `--init-order` reports `[FAIL]`, and the subagent must not reorder data members of a class whose constructors it cannot fully re-sequence — report instead |
| Data member reorder inside a class with `#ifdef` variants whose order differs per configuration | detect conditional-member blocks and report rather than move |
| `--fix` making a dirty tree dirty in ways the operator did not ask for | `--fix` refuses to run on a tree with unstaged changes outside the file list, and prints them |
| Subagents over-reaching into files another agent owns | disjoint ownership enforced in the driver prompt; `git status` diff-of-touched-files compared against the assigned list after each wave |

## Decision recorded after approval: no dependency on an agent framework

The owner ruled that the project must not depend on `@tintinweb/pi-subagents`. Two consequences, both
verified rather than assumed:

- `SubagentWorkflow` runs JavaScript in a Node realm supplied by that extension, so no Python driver can
  substitute for it inside the same mechanism. The alternative was the skill's own instructions plus the
  Python gates, which is what step 6 became.
- `pi -p --skill/--approve/--offline/--tools` are core flags; `--subagents-workflow-file` is not. The
  detached build gate therefore never needed the extension, and stays.

Open: whether the two `.pi/workflows/hb-review-*.js` review drivers remain in the tree.
