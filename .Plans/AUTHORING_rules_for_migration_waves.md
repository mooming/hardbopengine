# Rules for every documentation author in the Memory comment migration

Read `.pi/skills/hb-docs/SKILL.md` in full first. Section 2 generates page chrome with `docs_page.py`, section 4 carries the
citation and claim rules, section 6 the authoring boundary. Read `.Plans/AUTHORING_method_and_class_pages.md` if it exists.
Read `docs/Memory/PoolAllocator/` and `docs/Memory/MultiPoolAllocator/` as finished exemplars.

## Write to disk as you go

Land each page the moment it is drafted, one page per write. Five agents in this migration were killed by a network error after
47 to 75 tool calls each, and every one of them had written **nothing**. Their reading was lost too. A page on disk at call ten is
an asset; a page held in mind at call fifty is nothing.

## What you produce

`docs/Memory/<Class>/index.html` plus one page per declared method name, covering **every** entry the header declares: methods,
nested types, template parameters, constants. A page that does not cover everything names what it omits in its Coverage section —
a partial page must never read as a complete one.

The header's existing comments are your source material. Move their content faithfully into the pages. **Delete nothing from the
headers:** the strip is a separate verified step performed later by another agent, once your pages pass their gates.

## Binding boundaries

| Rule | Why |
|---|---|
| The only edit anywhere under `Engine/` is `/// API reference: docs/Memory/<Class>/index.html` on the line immediately above each class declaration you authored — one line per class, nothing else | `docs_coverage.py` refuses a page whose header carries no address, and refuses a pointer that names a different entry |
| Do not edit `docs/Memory/index.html` or `docs/index.html` | One integrator owns the module index. Concurrent edits there lose work |
| Do not create or edit another class's folder | Other authors are working in this tree right now |
| Generate chrome with `docs_page.py`; never hand-write it | Hand-written chrome drifts from every other page |
| Cite a file and a symbol, never a line number | Line numbers rot on the next edit |
| Never cite a path under `/tmp` | It is rebooted away. Wave 1 left three such citations and all three had to be scrubbed by hand |
| Every claim traceable to something you read. Anything you cannot establish — thread-safety, lock order, identity reuse, use-after-free, lifetime against the owning object — becomes a **named omission** in Coverage, never a confident guess | A wrong sentence about a live pointer is worse than a stated gap |
| A `Signature` section holds the declaration byte-identical to the header | The gate diffs it |
| Do not build, do not commit | Builds belong to the strip step, and nothing is committed in this checkout unless the owner asks |

## Gates before you report

Run and report each, filtered to your own folder where the script works module-wide:

```
python3 .pi/skills/hb-docs/scripts/docs_pass.py signatures docs/Memory/<Class> Engine/Memory/<Class>.h <Class>
python3 .pi/skills/hb-standards/scripts/docs_coverage.py check-file Engine/Memory/<Class>.h
python3 .pi/skills/hb-standards/scripts/docs_methods.py Memory        # filtered to your classes
python3 .pi/skills/hb-standards/scripts/htmlcheck.py "docs/Memory/<Class>/*"
```

Zero problems is the requirement, not the hope. If a gate cannot be satisfied, say exactly what it reports rather than working
around it.

## Report, in under 250 words

Page count per class, the gate outputs, and **every open question or named omission you recorded** — that list is the part the
integrator actually needs, because it is what this module still does not know about itself.

## Class page format, ruled by the owner

`docs/Core/TaskSystem/index.html` is the exemplar. **A class page carries its information in tables, rows and columns and embeds
no code snippet** — it holds zero `<pre>` blocks, and the class declaration facts reach the reader through the tables and the
class description instead. Its Coverage table keys rows by a column named `Piece`. Method pages are different: the exemplar's own
method pages do embed code blocks, so method pages keep theirs. Author new class pages this way from the start; wave 1's pages
were written with a declaration snippet and are being corrected.

## Verification tiers, adopted for time efficiency

Three-configuration builds are the only proof that matters for a source change, and they are also the most expensive thing an
agent in this migration can do. Documentation cannot change a binary, so most work in this migration does not need that proof.

| What you changed | Required proof | What you do **not** run |
|---|---|---|
| `docs/` only — pages, links, elision markers | `htmlcheck.py`, `docs_pass.py signatures` for touched classes, `docs_methods.py Memory` | Any build. The engine binary is byte-identical to one already proven |
| `.cpp` only, behaviour unchanged — comment deletion | One configuration build plus its suite run, and `comments.py` per file | Three configurations, unless the file is `ThreadSafeMultiPoolAllocator.cpp`, `PoolAllocator.cpp` or `MemoryManager.cpp` |
| `.h` declarations, or any of those three files above, or anything under `Engine/Core`, `Engine/OSAL` | All three configurations, all three suite runs | — |
| `.pi/skills/` scripts only | Run the script over two modules and compare | Any build |

Two standing rules that are not about builds, because both were learned by losing work: never more than two subagents in flight,
and never print a build log, compiler output or suite output — redirect to a file and report counts.
