# PLAN: Memory module comment migration

## Goal

Bring `Engine/Memory` to the rule in `AGENTS.md`: no comments in `.h` or `.cpp`. The module reference tree is the
destination for that prose, and `docs/Memory/` currently holds exactly one file, `index.html`, so the destination has
to be built before anything can be deleted.

Definition of done for the module, measured by the gates and not by impression:

| Gate | Command | Clean means |
|---|---|---|
| No source comments left | `.pi/skills/hb-standards/scripts/comments.py Engine/Memory/*.h Engine/Memory/*.cpp` | `0 violation(s)`. The module run reports 120 today; summing per-file runs gives 109, and that gap must be reconciled before any wave is called finished |
| Every documented entry owns a page and is addressed from the header | `docs_coverage.py check-file` per header | `documented, addressed from the header, method pages complete` |
| No method page missing | `docs_methods.py Memory` | no line for any class |
| HTML, links, CSS classes, module index reaches every page | `docs_page.py check Memory`, `htmlcheck.py` | `0 with problems` |
| Engine still builds and tests pass | `./build.sh Applications/EngineTest -dev -debug -release -test` then run all three | Debug, Dev, Release green, 60 collections `SUCCESS` |

## Which artifact receives which comment

| Source | Content kind | Destination | Consequence |
|---|---|---|---|
| `Engine/Memory/*.h` | contract, preconditions, ownership, lifetime, thread-safety, complexity a caller depends on | `docs/Memory/<Class>/index.html` plus one page per declared method name, per `.Plans/AUTHORING_method_and_class_pages.md` | 79 violations across 19 headers |
| `Engine/Memory/*.cpp` | invariants, algorithms, allocation strategy, locking protocol, platform quirks | an HTML **design document** under `docs/design/`, not a reference page | 30 violations across 4 sources. `AGENTS.md` routes implementation material to design documents, so a reference page is the wrong home and would be rejected as such |

Treating a `.cpp` comment as reference material is the failure mode to avoid: it puts an implementation claim in a page
a caller trusts for contracts.

## Waves

One class per authoring agent, as the `hb-docs` skill mandates, and never two agents editing `docs/Memory/index.html`.
Within a wave the authors touch only their own new folder; a single integration pass adds the module-index rows. The
strip and the three-configuration build are one agent per wave, after that wave's pages pass their gates.

| Wave | Classes and sources | Violations | Why here |
|---|---|---|---|
| 1 | `ThreadSafeMultiPoolAllocator`, `MultiPoolAllocator`, `PoolAllocator` | 12 in headers | Blocked behind the running `fix-prercq0` task, whose fixes change what these pages must say. Also the classes under active design work, so their pages get used soonest |
| 2 | `MemoryManager` | 19 | Largest single header. `docs/design/MemoryManagerInternals_Design.html` already owns three of its behaviours and states in its own Coverage section that they belong on method pages once authored, so that document must be revised in the same wave rather than left duplicating them |
| 3 | `StackAllocator`, `InlinePoolAllocator`, `DefaultAllocator`, `Shareable`, `MonotonicAllocator`, `InlineMonotonicAllocator` | 30 | Self-contained allocator family, no dependencies on later waves |
| 4 | `AllocatorProxy`, `AllocatorID`, `AllocatorScope`, `ScopedAllocator`, `PoolConfig`, `PoolConfigUtil`, `MultiPoolAllocatorConfig`, `MultiPoolConfigCache`, `Optional`, `SystemAllocator`, `Memory.h`, `AllocStats` | 23 | Small headers, several with one or two entries, several with none. Good candidates for a single agent to cover several of them, which the one-class rule permits because that rule governs concurrent authors rather than one author's workload |
| 5 | `PoolAllocator.cpp`, `GlobalAllocation.cpp`, `SystemAllocator.cpp`, `MultiPoolAllocator.cpp` | 30 | Design documents, not pages. Last, because the content is best written once the allocator family's contracts are already on paper |

## Rules every agent in this migration must follow

| Rule | Source |
|---|---|
| Move prose faithfully. Delete nothing during authoring; the strip is a separate verified step | `hb-docs` boundary: "`hb-standards` owns the strip and proves the destination first" |
| Do not edit `Engine/` beyond the single `/// API reference:` pointer line, added only after the class passes its gates | `hb-docs` section 5 |
| Generate chrome with `docs_page.py`; never hand-write it | `hb-docs` section 2 and 6 |
| Cite a file and a symbol, never a line number | `hb-docs` section 4 |
| Every claim traceable to something read. Thread-safety, lock order, identity reuse and use-after-free go in the page's Coverage section as a named omission rather than as a confident guess | `hb-docs` section 4 |
| A partial page must name what it omits | `hb-docs` section 4 |
| The docs author does not build. Builds happen in the strip agent | `hb-docs` section 6 |
| Do not reorder data members while touching a header | `AGENTS.md` |
| Nothing is committed or pushed without the owner asking | `AGENTS.md` work policy 9 |

## Known open items this migration inherits

| Item | Status |
|---|---|
| `docs/design/MemoryManagerInternals_Design.html` is not reachable from the Memory module card in `docs/index.html`, so it sits on disk invisible to a reader | Unfixed. One link, deliberately left for the wave 2 owner, who revises that document anyway |
| Two `MEMBER-LAYOUT` findings in each of `ThreadSafeMultiPoolAllocator.h:27` and `:70` and the same two in `MultiPoolAllocator.h` | Unfixed. Neither involves data-member sequencing so both are mechanically fixable, but they belong to the standards-migration task and not to a documentation wave |
| The `Memory` module's reference tree does not exist, so **no comment in it is currently strippable** | This plan is the response |

## Concurrency limit, learned the expensive way

The LLM backend sustains **two** concurrent subagents. Five were launched at once and all five died of connection errors with
nothing written to disk, losing roughly ten million tokens of reading; they are not resumable after that kind of death, because
the context is collected along with the run. Two rules follow, and both are binding on every later wave:

| Rule | Reason |
|---|---|
| Never more than two subagents in flight | Three or more die. A wave that finishes late is worth infinitely more than a wave that dies at call 60 |
| Every author writes each page to disk the moment it is drafted, and writes a handover file when stopped | The failure mode is losing everything read, not losing the run. A handover under `.Plans/` converts a death into a fifteen-minute restart |
