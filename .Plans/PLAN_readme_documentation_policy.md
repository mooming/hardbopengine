# PLAN: Add the default documenting policy to README.md

## Goal
Give README.md a section that states the project's default documenting policy, derived only
from what `AGENTS.md` already declares, so a human reader of the README is held to the same
rules an AI agent is.

## Confirmed decisions (user, this task)
| Decision | Choice |
|---|---|
| Placement | `### Documentation Policy` as the **first child** of the existing `## Documentation` section |
| Scope | Add the section **and** repair the two stale references inside the same documentation area |

## Source of truth read before planning
| File | What it settles |
|---|---|
| `AGENTS.md` § Self Documented Code and API Reference Documents | The policy itself; added by commit `37ba98e` |
| `AGENTS.md` § Coding Standards + `docs/CodingStandards.md` § Comments | The `.cpp` no-comment rule, its exemptions, the structural-label definition |
| `docs/HelperScript.md` | The real `build.sh` flag set: `-dev -debug -release -clean -test` |
| `docs/EngineAPIGuide.md` | Current consolidated API reference; a per-module Table of Contents, but no page named "Module Index" exists yet |

## Steps
| # | Step | Verification |
|---|---|---|
| 1 | Insert `### Documentation Policy` immediately after `## Documentation` (line 265) | Section renders; the three document-shape outlines match `AGENTS.md` verbatim in content |
| 2 | Fix `### Code Standard` link: `docs/CodeStandard.md` → `docs/CodingStandards.md` | Target file exists |
| 3 | Fix `### Convenient Build Script`: drop `-notest`, add `-test` with its real meaning | Flags match `build.sh` usage line and `docs/HelperScript.md` |
| 4 | Log the change in `JOURNAL.md`, commit `README.md` + plan + journal only | `git show --stat` lists exactly those three paths; the uncommitted `Engine/` work stays unstaged |

## Deliberately out of scope (reported, not fixed)
- `### Running Tests` (README line 68) tells the reader to run `./Applications/EngineTest/EngineTest`
  from inside `build/`; a Ninja Multi-Config tree has no such path. `AGENTS.md` gives the correct
  per-configuration path.
- `### Platform-Specific`, `### Code Quality`, `### Unfinished Features` are children of
  `## Documentation` but are notes about the code, not documentation links.
- The trailing `## Build` duplicate of the configure command at the end of the file.
- No "Module Index" page is created: the policy is stated as the convention `AGENTS.md` declares,
  without shipping a dead link to a page that does not exist.

## Risk
Single file of prose plus two reference repairs: no build, no test surface touched. The three-configuration
build gate applies to C++ changes; here the gate is that no `Engine/` file is staged and the tree is unchanged.
