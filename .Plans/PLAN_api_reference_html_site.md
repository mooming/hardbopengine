# PLAN: Convert the API reference to an HTML site with per-module folders

## Goal
Replace the single-page `docs/EngineAPIGuide.md` / `docs/EngineAPIGuide.html` pair with an HTML
documentation site: `docs/index.html` as the Module Index start page, and one folder per Engine
module directory holding that module's API reference, as `AGENTS.md` requires.

## Confirmed decisions (user, this task)
| Decision | Choice |
|---|---|
| Layout | `docs/index.html` plus `docs/<Module>/index.html`, folders named after the 13 `Engine/` directories |
| Source of truth | HTML is canonical; `EngineAPIGuide.md` and the stale `EngineAPIGuide.html` are deleted |
| Existing guides | Stay where they are, linked from the owning module page |
| Style | One shared `docs/assets/hbe-docs.css` linked by every page |

## Evidence gathered before planning
| Observation | Consequence |
|---|---|
| The markdown `###` headings each cite `Engine/<Module>/<File>.h` | The module taxonomy is derived from code paths, not from the guide's 16 section numbers |
| Derived split: §1 sends `SourceLocation` and `Intrinsic` to OSAL; §13 sends eight blocks to Core and only the `Engine` class to Engine | Three guide sections are split, four are merged. Splitting the old HTML by section would have misfiled 10 class blocks |
| `EngineAPIGuide.html` lags the markdown: Containers, String Utilities, Configuration, Renderer, and the whole Coding Standards chapter | Re-authored from the markdown, not copied from the HTML |
| The markdown §16 build example still carries `-notest`, the flag removed from README.md on 2026-09-19 | The migration carries the corrected flag set |
| The markdown Coding Standards chapter asserts "All other identifiers: camelCase", which `JOURNAL.md` lines 640, 651, 764 already recorded as contradicting `docs/CodingStandards.md` | That chapter is dropped, not migrated; the site links the authority instead |

## Module page outline, fixed by AGENTS.md
Module description, Module classes, Module variables, Module functions — then per class:
Class description, Template parameters, Class properties, Class methods, Non-member helper functions.
A category the migrated content cannot answer is printed as an explicit coverage gap, never invented.

## Steps
| # | Step | Verification |
|---|---|---|
| 1 | Extract the 266-line stylesheet into `docs/assets/hbe-docs.css`, add cross-page navigation, module-card and coverage-gap styles | Page renders from a file:// open; no inline `<style>` left in a page |
| 2 | Build `docs/index.html` — Module Index: 13 module cards, class counts, design-document links, policy links | Every href resolves on disk |
| 3 | Build `docs/Core/index.html` as the exemplar and **stop for approval** | User approves structure and style before 12 pages are authored |
| 4 | Author the remaining 12 module pages from the markdown | Per page: class-block count matches the derived mapping |
| 5 | Delete `EngineAPIGuide.md` and `EngineAPIGuide.html`; rewire references | `grep` finds no live reference to either filename outside `JOURNAL.md` history |
| 6 | Update `README.md`, `AGENTS.md`, `docs/CodingStandards.md` where they point at the old pair; log `JOURNAL.md`; commit | Link sweep passes; commit lists only documentation paths |

## Out of scope (recorded)
- Filling Class properties and Module variables from the headers, where the migrated content is
  silent. Recorded per page as a coverage gap; a separate task if you want the headers read.
- Converting the nine existing design and policy markdown documents to HTML.
