# Standards worker brief — read this completely before touching a file

You are one worker in a strictly sequential pass (one worker at a time) that brings one module of
HardBop Engine into strict conformance with its coding standard. You are not alone in this tree:
other modules are untouched, and an edit outside your assigned files is a defect even if the edit
is itself an improvement.

## Time discipline, because 13 modules follow you

The pass is sequential and the tree is large. Nothing below is a shortcut around a rule; each is a
job that belongs to someone else and that you would only duplicate.

| Do not | Because |
|---|---|
| Build anything, in any configuration | the orchestrator builds, and it builds the whole gate at checkpoints. Your compile is a duplicate of a step that already exists |
| Run a checker over the whole tree | your scope is your files. A tree-wide run is the orchestrator's measurement, and a worker that reruns it learns nothing new |
| Prove a non-behavioural edit is non-behavioural by diffing assembly | collapsing a blank line and naming a constant do not need an assembly proof. Do read what you changed and say why it cannot change behaviour |
| Measure something to put a number in a docs page unless the page needs that number | a page that says "used across the engine" is fine; one that says "26 files" must be counted, and then not re-counted |
| Re-read a file you have already read | you have it |
| Write a page twice for the same entry | the Coverage row and the class page are different things; decide which one an item belongs to and write it once |

Budget, if you want one: a module whose docs pages already exist should take you one pass over its
sources. A module with missing pages spends most of its time on those pages, and that is correct —
authoring is the long pole, not linting.

## What the standard is

`docs/CodingStandards.md` is the rule book, `AGENTS.md` the project summary,
`.Plans/AUTHORING_method_and_class_pages.md` the contract for HTML pages. Read the sections you
are about to apply — do not work from memory.

Engine facts that change decisions: C++23, exceptions are forbidden, `hbe` namespace, tabs not
spaces, Allman braces with no exemptions, 120 columns, `class` preferred over `struct` unless the
type is plain data.

## The three rules this pass exists for

### 1. Twelve-block member layout — the data layer is one visible block

All member variables before all member functions; inside each of those two layers,
`public` → `protected` → `private`, `static` first inside each section. Types are neither data nor
functions, so they head the class. `friend` declarations end it.

| Block | Contents |
|---|---|
| 0 | nested types, aliases, enumerators, `public` → `protected` → `private` |
| 1 / 2 | public static variables / public variables |
| 3 / 4 | protected static variables / protected variables |
| 5 / 6 | private static variables / private variables |
| 7 / 8 | public static functions / public functions — constructors, destructors, operators included |
| 9 / 10 | protected static functions / protected functions |
| 11 / 12 | private static functions / private functions |

**Safety invariant, non-negotiable**: the relative order of the data members with respect to each
other must not change. C++ initialises non-static data members in declaration order. Moving the
whole data block above the functions is provably semantics-preserving; re-sequencing two variables
against each other is a silent behaviour change that no compiler, lint or test necessarily reports.
If two variables appear to need swapping to satisfy the layout, the layout is already satisfied.

Move a member's doc comment, `#if` guard and `template <…>` header with it. A comment that ends up
describing a different member is worse than no comment.

Members a class-body `#include`d `.inl` contributes are part of the class and must be sorted with
it, but the lines live in the `.inl` — `Engine/Math/VectorCommonImpl.inl` and
`Engine/Math/MatrixCommonImpl.inl`. Fix the `.inl`, and never run clang-format on one.

### 2. No comments in `.h` or `.cpp`

Both halves of the source pair carry code only. Exemptions, exhaustive:

| Exemption | Note |
|---|---|
| `// Copyright (c) … Hansol Park` on line 1 | legal notice |
| Structural label on the line that closes its construct | `#endif // PROFILE_ENABLED`, `#else // !__DEBUG__`, `} // namespace hbe`. A label on the line **above** is a comment, not a label |
| `// hb-standards:ignore` | tool directive |
| `Engine/CodingStandards.cpp`, BAD EXAMPLE blocks of `Engine/CodingStandards.h` | teaching exemplar, breaks rules on purpose |

**Order of operations, and this is what keeps information alive**: prose leaves the file by being
written into `docs/` first, never by being deleted. If the page that should hold a comment does not
exist yet, write the page first. A comment deleted with nowhere to go is destroyed contract, and
`git log` is not a reference.

### 3. The judgement rules, which no script can check

Verify each, per file, and fix what is wrong:

| Rule | What to look for |
|---|---|
| Log before failing | every early `return`/`Assert` on an error path needs a descriptive `Error:`/`Warning:` log first |
| `[[nodiscard]]` | on getters and value-returning functions |
| `explicit` | on every single-argument constructor |
| `= default` | instead of `{}` for trivial special members |
| `out` / `inOut` / `in` | prefix write-only, read-write, and name-colliding parameters |
| Pointer validation | validate before dereference; prefer references over raw pointers |
| `static_assert` | use it wherever a condition is compile-time evaluable, in preference to a runtime `Assert()` |
| `constexpr` | no magic numbers; constants are PascalCase |
| Composition | prefer has-a; `final` on classes not meant as bases |
| `noexcept` | mark what is provably exception-free; drop it where allocation can throw |
| No `virtual` with `override` | `override` alone |
| No `m_` prefix, no Hungarian notation | `camelCase` members; renaming must follow every use |
| No explicit `inline` | a definition in a class body is already inline |
| No `std::move` on return of a local | it kills NRVO |
| Unit-test block last | a `#ifdef __UNIT_TEST__` region is the last thing in the file |
| Include layout | own header, then `<standard>`, then `"project"`, each alphabetical, one blank line before the first code body |

## What you must not do

| Forbidden | Why |
|---|---|
| Run `clang-format`, in any mode, on anything | the orchestrator owns the mechanical layer, so a formatting diff never mixes with an intent diff |
| Change behaviour | a standards fix that changes what the code does is a defect |
| Edit a file outside your assigned list | scope control; other modules get their own pass |
| Reorder data members relative to each other | see the safety invariant above |
| `git commit`, `git add`, `git push` | the orchestrator commits, per module, after verification |
| Delete a comment whose docs do not exist | that destroys the only copy of a contract |
| Add a comment to explain a fix | the ban applies to you too |

## Prove your own work before you report it

```bash
.pi/skills/hb-standards/scripts/comments.py <your files>          # must report 0
.pi/skills/hb-standards/scripts/layout.py --db cmake-build-debug/compile_commands.json <your files>
.pi/skills/hb-standards/scripts/docs_coverage.py check <MODULE>   # must report 0 missing
```

Those three, on your files only. Do not run `check.sh` — it reformats, lints the whole scope and
builds, and all three are the orchestrator's job.

`layout.py` reads the clang AST and is authoritative for ordering. If it reports something you
believe is wrong, say so in your report with the reason — do not work around it, and do not add a
`hb-standards:ignore` to silence a checker you have not proven wrong.

## Report format

Return a table, one row per file you were given: file, rules checked, findings, action taken,
and anything you deliberately left alone with the reason. Then a short prose note on any checker
output you dispute and why. Keep it under one screen: the orchestrator reads the diff, so a
narrative of what you did at length is a cost and not a signal. Your final message is data, not a
letter to a human.
