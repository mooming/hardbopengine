# Brief: Memory reference pages, wave 1

Shared instructions for the agents authoring `docs/Memory/` pages. Your prompt names the one class you own. Read
`.pi/skills/hb-docs/SKILL.md` and `.Plans/AUTHORING_method_and_class_pages.md` before writing anything.

## What you own and what you must not touch

| You own | You must not touch |
|---|---|
| `docs/Memory/<YourClass>/` and every page inside it | `docs/Memory/index.html` and `docs/index.html`. Module-page rows are a single integrator's job, because two agents editing one module page is how a row goes missing |
| The single `/// API reference: docs/Memory/<YourClass>/index.html` line above each of your documented entries, added **only** after your class passes every gate below | Every other header. Do not add or remove a pointer for a class you do not own |
| Reading anything | Any `.cpp` file, any `Engine/` code, any comment. You are authoring the destination, not performing the move. `hb-standards` owns the strip |

Never run a build. The skill says the docs author does not build; a build agent follows you.

## Content rules that the gate will actually enforce

| Rule | Consequence of ignoring it |
|---|---|
| Generate all chrome with `docs_page.py class` and `docs_page.py method`. Hand-write only the `--body` fragment | A hand-written sidebar or breadcrumb is a finding, and a class page that lists five of its seven methods is exactly the defect the checker exists for |
| Signature sections hold the **declaration**, byte-identical to the header, escaped where `static_cast<TIndex>(-1)` would otherwise read as a tag | `docs_pass.py signatures` refuses to write unless its own output then matches the header |
| Cite a file and a symbol, never a line number | A reader who cannot find the quoted line stops trusting the whole page |
| Every claim traceable to something you read. Thread-safety, lock order, ownership, identity reuse and use-after-free are where a confident guess damages someone else's code | Anything you cannot prove goes into that page's Coverage section as a named omission, not into a description |
| A partial page names what it omits | A half-written class must never read as a finished one |
| Column 1 of the properties table is the declaration as the header writes it, plus the correct badge. A `private` badge on a public member is a false statement about the interface | That table exists to get this right and nothing else |
| Methods table: the name is the link, one row per declared name in header order, every overload its own line in the signature cell with an overload-count badge | `docs_methods.py` reports the missing page |
| No invented acronyms or code names; write things in full | |

## Claims you must get right for this wave

The three allocators in this wave were reviewed on 2026-10-05 and fixed the same day. `JOURNAL.md` carries both entries,
and it is the best available source for the contract-level truths, so read it rather than re-deriving them:

| Truth a caller depends on | Where it came from |
|---|---|
| `ThreadSafeMultiPoolAllocator` synchronizes every bank operation behind one non-recursive mutex, and it is therefore safe to share between threads but not reentrant: a call must never allocate from the current scoped allocator while holding it | The 10:08 review finding, and the F3 fix that same day moved both diagnostics out of the locked region |
| A bank's backing memory is released through the allocator recorded at construction, not through the ambient scope | F2, the same day. Consequence for a caller: an allocator may be destroyed under any scope |
| The configuration cache reports the summed per-block-size peak, and `ReportConfiguration` asserts that the reported total equals it | F1, the same day |
| `PrintUsage` redirects to the system allocator before it touches a container, so calling it under its own scope is legal | F4, the same day |
| `Allocate` cannot serve a request needing alignment greater than `Config::DefaultAlign`, and out of memory is a fatal assert rather than a null return | Review findings 9 and 6, still open. State them as the contract they are, and say in Coverage that they are known-open rather than intended design |

## Gates before you report

Run all four, and filter the module-wide ones to your own class, since other classes in this module are unfinished by
design and their findings are not yours to fix.

| Check | Command | Clean for your class means |
|---|---|---|
| Signatures equal the header | `python3 .pi/skills/hb-docs/scripts/docs_pass.py signatures docs/Memory/<Class> Engine/Memory/<Class>.h <Class>` | `0 problem(s)` |
| No method page missing | `python3 .pi/skills/hb-standards/scripts/docs_methods.py Memory` | no line naming your class |
| Entry documented and addressed | `python3 .pi/skills/hb-standards/scripts/docs_coverage.py check-file Engine/Memory/<Class>.h` | `documented, addressed from the header, method pages complete` |
| HTML, links, CSS | `python3 .pi/skills/hb-docs/scripts/docs_page.py check Memory` | no problem points at your folder |

## Report back, in under 250 words

Pages created, grouped class page then method pages; the four gate results verbatim; every claim you could not prove,
listed as the Coverage omission you wrote for it; and any question about the class's contract that a reader would ask
and the header cannot answer, because that list is what the integrator and the design-document wave need.

---

## Outcome, and one follow-up that needs a decision before anyone builds it

Wave 1 landed: `PoolAllocator` (23 pages), `MultiPoolAllocator` (15) and `ThreadSafeMultiPoolAllocator` (15), the module
catalogue links all three, and the module Coverage row says three of nine allocators are documented instead of none.
`docs_methods.py Memory` reports zero missing pages, `htmlcheck.py` runs 620 pages with zero problems. The wave also produced
an unplanned result worth more than the pages: reading `PoolAllocator`'s Coverage section surfaced a real double-allocation
defect, now fixed and guarded — see `JOURNAL.md`, 2026-10-06.

**Proposed, not built, awaiting the owner's word:** a checker for quoted source blocks. Pages embed `<pre>` copies of `.cpp`
bodies captioned `From <file>, <symbol>`, and no gate compares them to the source. When one line changed in
`PoolAllocator.cpp`, three quoted copies in one page went stale and four more looked stale, found only by grepping for the
changed text. `docs_pass.py signatures` proves a Signature equals the header; nothing proves a quoted body equals the function
it claims.

| Impact | Cost | Recommendation |
|---|---|---|
| Removes the whole class of "the reference describes code that no longer exists", which the comment migration will now produce repeatedly — every stripped module re-editing `.cpp` bodies it documents | Roughly 100 lines over machinery both skills already have: parse the caption, extract the function body from the source, compare after normalising entities and leading indentation. No build needed. One day at most, and it pays for itself in wave 2 | Build it before wave 3. Every later wave strips comments out of files whose bodies change, so the drift gets worse the longer it waits |

Two known false-positive sources to design around, found by the sweep: unchanged code that legitimately contains the same
expression (`AllocateBlock`'s `if (index < numberOfBlocks)` is correct and is quoted on two pages), and prose that deliberately
quotes the old form to explain history (`Before that branch was corrected it wrote numberOfBlocks - 1`). The checker must
compare only blocks inside `<pre>` and ignore prose.
