# Plan — Core API reference revision, TaskSystem family first, then the comment strip

Status: **awaiting approval.** Nothing is edited until the owner says go.

Owner rulings already given:

| Ruling | Effect |
|---|---|
| Full scope: revise the 45 existing TaskSystem pages **and** author the 156 missing family method pages + 11 family class pages | Stages 1–5 run to completion, not a sample |
| Properties table: `Name \| Declaration \| One-line description` — **no Link column** | Methods get 4 columns, properties get 3 |
| Methods table: `Name \| Signature \| Link \| One-line description` | Every class page's method table is rebuilt to this shape |
| Strip comments from Core sources and headers while documenting | Stage 6, gated on the module being fully documented |
| Drop the all-methods sidebar from **method** pages | `AUTHORING` §9 amended, `docs_page.py` chrome changed, 44 existing pages rewritten |
| Method-page *Signature* shows the **declaration only** — no function bodies | 15 existing pages lose their pasted body; algorithm prose stays in *Function description* with a `file:line` citation, for every page without exception |
| `docs/Core/index.html` does not lead to the class pages, so no method page is reachable by clicking | Stage 5 gains: delete the embedded per-class sections, every classes-table row links `<Class>/index.html` |

## 1. Measured starting state

| Measurement | Value | Source |
|---|---|---|
| Core API entries | 24 detected + `ResultPacket` invisible to the gate = **25** | `docs_coverage.py`, `Engine/Core/ResultPacket.h:28` |
| Core entries with a class page | 9 | `.Plans/DOCS_COVERAGE.md` |
| Core method pages missing | 197 — 156 in the TaskSystem family, 41 outside it | `docs_methods.py Core` |
| Comment lines left in `Engine/Core` | 1 004 over 29 files | `comments.py` per file |
| TaskSystem reference bytes | 427 KB over 45 pages | `wc -c` |
| …of which sidebar chrome | 176 KB = **41.3 %** | `<nav class="sidebar">` extraction |
| TaskSystem method pages showing a whole function body in *Signature* | 15 of 44 | `<h2 id="signature">` line count > 6 |

Family work list, in the order the headers depend on each other:

| Entry | Header | Comment lines | Method pages owed | Class page |
|---|---|---|---|---|
| `WorkItem` | WorkItem.h | 60 | 2 | exists — revise |
| `TaskID` | TaskID.h | 18 | 1 | new |
| `Task` | Task.h | 84 | 15 | new |
| `ResultPacket` | ResultPacket.h | 48 | — | new, invisible to the gate today |
| `TaskStreamAffinityBase` | TaskStreamAffinity.h | 16 | 6 | new |
| `StreamDrainPolicy` | StreamDrainPolicy.h + .cpp | 77 | 19 | new |
| `CPUBudget` | CPUBudget.h + .cpp | 51 | 9 | new |
| `MainThreadTaskQueue` | MainThreadTaskQueue.h | 26 | 7 | new |
| `TaskRegistry` | TaskRegistry.h + .cpp | 122 | 20 | new |
| `TaskProvider`, `TaskHandle`, `TaskProduceContext` | TaskProvider.h + .cpp | 151 | 24 | 3 new |
| `TaskStream` | TaskStream.h + .cpp | 299 | 53 | new |
| `TaskSystem` | TaskSystem.h + .cpp | 0 | 0 | exists — revise, 44 method pages |

Outside the family, required before any Core strip is allowed: `Component` 13,
`SystemStatistics` 16, `ComponentSystem` 7, `CommandLineArguments` 5 method pages and 4 class pages.

## 2. Why the strip cannot happen before the pages

`comments.py --strip` runs the two doc checks the gate runs and **refuses** while the module owes class or
method pages. The refusal is module-scoped by design: prose deleted before it has a page is gone. So the
strip is the last stage, and it needs all 197 method pages, not the family's 156.

One hole defeats the safety property entirely: `docs_coverage.py` does not see
`class alignas(std::uint64_t) ResultPacket final`, so its 48 comment lines can be deleted with no page ever
demanded. Stage 0 closes it in the checker rather than remembering it by hand.

## 3. Table shapes

Class page, properties — three columns, badge for the row kind kept inside the description cell:

| Name | Declaration | What it means |
|---|---|---|
| `NonStreamIndex` | `static constexpr TIndex NonStreamIndex = -1` | public static — the index of a thread that owns no stream; not zero, because stream 0 is real |

Class page, methods — four columns, one row per method name:

| Name | Signature | Link | What a caller depends on |
|---|---|---|---|
| `Enqueue` | `void Enqueue(const WorkItem&) noexcept` …3 overloads | enqueue.html | Three destinations: the general queue, or one stream's FIFO or priority lane |

Module page adopts the same shape, so one table grammar runs from module index to method page.

## 3b. The navigation defect that started this

Measured, not inferred:

| Click a reader tries | What happens |
|---|---|
| `docs/index.html` → Core | lands on `docs/Core/index.html` |
| Classes table → **TaskSystem** | `href="#tasksystem"` scrolls inside the module page to a hand-written copy of the class doc; never navigates |
| That copy → a method name | plain text in a table, no link |

| Measurement | Value |
|---|---|
| Core entries whose class page is linked from the module page | 6 of 25 |
| Files in all of `docs/` linking any TaskSystem method page | 1 — `docs/Test/TestHelper/drive-until.html` → `update.html` |
| Members the module page's embedded `Task` section claims that `Engine/Core/Task.h` does not declare | 2 — `Start`, `Wait` |
| Members `Task.h` declares that the same section omits | `ReserveSubTasks`, `SetRunnable`, `LoadIntoRecord` |

The embedded sections are a second home for claims the class page owns, and the second home rotted first.
They are deleted; the classes table becomes the index.

## 4. Items removed

| Item | Where | Evidence | Action |
|---|---|---|---|
| 44-link `Methods` sidebar list on every method page | 44 pages | 176 KB / 41.3 % of the class's bytes; the class page's method table already carries the same links with descriptions beside them | **Remove.** Method pages keep the Modules list, the *This function* list, and the footer's `← TaskSystem` / `All TaskSystem methods`. Needs an `AUTHORING_method_and_class_pages.md` §9 amendment and a `docs_page.py` change — owner call |
| Sentences about the documentation project rather than the API | class page, Coverage | "a member-layout deviation the layout pass still owes", "their pages are outstanding", "docs/Core/TaskStream/ does not exist yet" | **Remove.** Findings about the code stay; findings about the queue do not |
| Rename/move history repeated across pages | class page + 5 method pages | "left the class on 2026-10-02", "now documented as" | **Remove** from method pages; one row on the class page's Coverage keeps the pointer a reader needs |
| `Template parameters: None` / `Non-member helpers: None` bodies | class page | §9 requires the heading to exist | **Keep heading, one line.** A heading cannot be dropped — an absent heading is indistinguishable from an unfinished page |
| Whole function bodies pasted into *Signature* | 15 method pages | The header's declaration is the signature; the body is the `.cpp`'s | **Remove** — owner ruling: declaration only, no exception for "the body is the guarantee" |
| `Parameter | Type | Requirement` rows restating the type already in the signature | every method page | Type appears twice per row | **Collapse** to `Parameter | Requirement` when the type is in the signature line |

Kept deliberately: the `.note` / `.gap` callouts (46 + 33 across the class — they carry the preconditions
and hazards a caller cannot re-derive), the `Coverage` section on every class page, real call-site examples
with `file:line`, and every memory-ordering and thread-safety claim.

## 5. Stages

| Stage | Work | Verification before it is called done |
|---|---|---|
| 0 | Repair `docs_coverage.py` entry detection for attributes before the class name, so `ResultPacket` is demanded; amend `AUTHORING` §9–§10 for the trimmed method-page sidebar; teach `docs_page.py` to emit the trimmed method-page chrome | Checker reports `ResultPacket` MISSING; a probe page built by `docs_page.py` passes `htmlcheck` |
| 1 | Two template exemplars on `TaskSystem`: one class page, one method page, in the new tables and trimmed | Owner approves the rendered pair before any mass production |
| 2 | Revise the remaining 43 TaskSystem method pages + class page table | `htmlcheck` clean; every header line quoted is byte-compared against `Engine/Core/TaskSystem.h` |
| 3 | Family pages in dependency order (table above), each entry: read header → grep call sites → class page → method pages → add the entry's single `/// API reference:` pointer line | `docs_coverage.py check Core` and `docs_methods.py Core` counts fall by exactly the batch; `htmlcheck` clean per batch |
| 4 | Non-family Core entries: Component, ComponentSystem, SystemStatistics, CommandLineArguments | Both doc gates at 0 for Core |
| 5 | `docs/Core/index.html`: drop the duplicated `ComponentState` row, add rows for the 12 entries it never listed, adopt the shared table grammar, add the Usage-in-the-engine table | Inbound-anchor scan over all of `docs/` reports every surviving fragment still resolves |
| 6 | Strip Core comments: snapshot → `comments.py --strip` → `clang-format` → `code_tokens` proof → re-check grouping | Per file: token multiset identical to the pre-strip snapshot, `comments.py` reports 0, `check.sh` format layer clean |
| 7 | Gate | `gate.sh spawn --all --test` → `GATE_EXIT=0`, EngineTest in Debug/Dev/Release |

Batching follows `AUTHORING` §14: one to three files per response, `htmlcheck` every few pages, module page
alone. The owner can stop at any batch boundary.

## 6. What this plan does not touch

| Left alone | Because |
|---|---|
| The 3 modified files in the working tree (`TestMain.cpp`, `TaskSystem.cpp`, `docs/CodingStandards.md`) and the untracked `blank_lines.py` / `includes.py` | They belong to `.Plans/PLAN_blank_line_and_include_passes.md`, status *awaiting approval*; two changes in one commit cannot both be described |
| `Engine/` behaviour | Doc work edits headers only to add one `/// API reference:` pointer per documented entry |
| Non-Core modules | Math 16, Memory 19, OSAL 9 entries still owe pages; out of the stated scope |
| Definition order in `.cpp` files | No rule requires it; churn, not conformance |
