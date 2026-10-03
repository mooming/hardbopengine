# PLAN: run `hb-standards` on every file of Engine/Core

Owner decision that opened this: the earlier sweep deliberately stopped before `Engine/Core` while another
session was editing it; the owner ruled that session finished and asked for the **full skill on every tracked
file** of the module, not the rule-set-A blank-line pass alone. Plan approved in full, including the
authoring that the comment ban implies.

## Where the module stands

| Layer | Checker | State |
|---|---|---|
| clang-format | seam-aware `clang-format --dry-run -Werror` | clean (all 46 sources) |
| blank lines, rule set A | `blank_lines.py` | **0 findings** |
| include preamble, rule set B | `includes.py` | **0 findings** |
| comments | `comments.py` | **66 findings, all in 6 headers** — see B2 |
| member layout, twelve blocks | `layout.py` | 0 violations, 2 waived |
| member-init order | `layout.py --init-order` | 0 violations |
| paragraph seams | `blank_lines.py --paragraphs` | 0 files needing a reader |
| docs coverage / methods / HTML | three docs scripts | 0 missing pages, 0 method gaps, 0 problems |
| build and test | `./build.sh Applications/EngineTest -dev -debug -release -test` | links in 3 configs, 374 of 375 testlets |

`WindowTest::TC0` retains 101,552 bytes against `MaxRetainedGlobalBytes` = 65,536. Pre-existing, reproduced at
`HEAD` with all edits stashed, and unchanged in Dev, Debug and Release.

## What the cycle actually costs, learned here and recorded in SKILL.md

`docs_coverage.py check-file` gates a strip per file, and it draws the line AGENTS.md draws: an **API**
contract has a page, so its `///` block may go; an implementation note belongs in an HTML design document, so
the tool refuses without `--force`. The refusal sentence is identical for both cases while the material is
not, which is why the ban arrives in three groups rather than one sweep.

| Group | Files | Findings | What it was | Outcome |
|---|---|---|---|---|
| A — header whose class page owns the prose | 14 | 815 | caller contract, already on a page | stripped at `4e228d2`, code tokens proven identical |
| B1 — `.cpp` implementation notes | 8 | 93 | invariants, allocation strategy, locking, clock epoch | moved into two design documents, 88 deleted at `09250a4` |
| B2 — headers with no page kind | 6 | 66 | **user-facing** contract | closed in place on the module page, stripped without `--force` |

Two strips looked mechanical and were not. Stripping prose re-opens every paragraph it was grouping (owner
challenge, then the owner's own edit to `TaskRegistry.h` settling the constants block), so step 4b re-decides
seams; and `--strip` deleted exempt `} // namespace hbe` labels until `comments.py` learned that a newline is
a token boundary.

## B2 — the six headers, and why the gate cannot see them

| Header | Findings | Documented surface |
|---|---|---|
| `Time.h` | 35 | `namespace hbe::time`: 6 free functions + duration aliases; `TimeTest` in its `__UNIT_TEST__` region makes the file class-shaped, so the functions are billed nothing |
| `Debug.h` | 11 | logging macro machinery — the notes are design material, not macro reference |
| `TaskStreamIndex.h` | 11 | one alias, `TStreamIndex`, whose contract spans providers, produce contexts and successors |
| `Runnable.h` | 9 | one alias, `TRunnable`, whose return value drives re-invocation |
| `Types.h` | 1 | canonical aliases plus their legacy spellings |
| `CommonMacros.h` | 1 | `returnIf`, `returnValueIf`, `breakIf`, `continueIf`, `ONCE` |

`entries_in()` knows `class|struct|union|enum` and macro sets. `NAMESPACED_FUNCS` is defined and never used —
someone intended the free-function kind and did not finish it. Three gate defects, each measured:

1. `MACRO_SET` lacked `re.M`, so `^` anchored to the start of the file and **no macro set in the engine was
   ever demanded**. `Engine/Config/BuildConfig.h` already has the page the gate never asked for. Fixing it
   newly demands 3 pages, 2 missing (`CommonMacros`, `OSAbstractLayer`).
2. No alias entry kind. Across every engine header with no class entry, 20 declare aliases, free functions or
   macro sets; 11 have no page — 5 in Core, 6 in Math, Memory, OSAL, Renderer.
3. A file that owns both a class and namespace-scope free functions demands the class page only.

### Order of work — done, and the route changed once
1. `MACRO_SET` gains `re.M` — one line, and the page for `CommonMacros` becomes demanded rather than optional.
2. Alias and free-function kinds in `entries_in`, one `utility header` entry per header rather than one per alias.
3. **Route change, with the owner's ruling.** The plan said six new pages. `docs/Core/index.html` already documents
   `Types`, `Runnable` and the `Debug` and `Time` functions, and its Coverage section already declared the rest a
   gap — "Time functions not summarised … the header carries their contract". Writing the pages would have put the
   same claim in two places, so the gaps were closed on the index and both checkers learned the address
   `docs/<Module>/index.html#<Stem>`, with the id required to exist and to name the file carrying it.
4. `docs/design/AssertMacros_Design.html` received `Debug.h`'s implementation notes; its `noexcept` asymmetry
   against `FatalAssert` is recorded as an open question rather than given an invented rationale.
5. All six stripped **without `--force`** — `check-file` passed for each first. Code tokens proved identical against
   bytes taken from `HEAD`; clang-format, then the rule-set-A table (five files needed the mandated second blank at
   the preamble seam), then step 4b, which reported nothing undecided.

## Where this leaves the reference gate

`docs_coverage` now demands a documented surface for aliases, free functions and macro sets, and accepts either a
page or a module-page section as its address. Tree-wide that newly demands 11 pages; 5 were Core's and are closed,
6 belong to Math, Memory, OSAL and Renderer. Those six are backlog for their modules and were deliberately not
touched here.

## Explicitly out of scope

- Raising `MaxRetainedGlobalBytes` to make `WindowTest::TC0` green. The harness's own words are "Free it, or
  raise the ceiling with a reason that survives review", and that reason is an `Engine/OSAL` question.
- The 6 non-Core pages the new entry kinds expose (Math, Memory, OSAL, Renderer), and
  `Engine/Container/AtomicStackView.h:17` `ACCESS-EMPTY`.
