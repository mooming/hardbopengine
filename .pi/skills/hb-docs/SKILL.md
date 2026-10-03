---
name: hb-docs
description: >-
  Generate and revise HardBop Engine HTML API reference pages under docs/ — one folder per Engine/ module
  directory and per class, a class page whose tables link every method page, and one page per declared
  method name. Use when asked to write, revise, repair or reskin API reference pages, apply the reference
  template (properties as Type + Name / Default Value / Description, methods with the name itself as the
  link), rebuild a module or class page, prove that every Signature section equals the header, remove
  duplicated page chrome, move prose out of a header's comments and into the pages that own it, or when
  docs_coverage.py / docs_methods.py / htmlcheck.py report pages a module still owes. hb-standards owns the
  coding-standard gates; this skill owns the documents the comments move into.
---

# HardBop API reference authoring

Engine sources carry **no comments**. Every sentence that states a contract lives here, in `docs/`, so the
reference is not decoration: it is where the information goes when a comment is deleted, and
`hb-standards` refuses the deletion until the page that receives it exists.

| Concern | Owner |
| ------- | ----- |
| What a page must contain, its grammar, its citations | this skill, plus `.Plans/AUTHORING_method_and_class_pages.md` (the full contract) |
| Chrome: sidebar, breadcrumb, title, footer, `<main>` | `scripts/docs_page.py` |
| Signature text, page reskin, prose-preservation proofs | `scripts/docs_pass.py` |
| Whether pages exist, whether links and CSS classes resolve | `../hb-standards/scripts/docs_coverage.py`, `docs_methods.py`, `htmlcheck.py` |
| Deleting the comment once its page owns it | `hb-standards`, which gates the strip per file |

## 1. Layout

| Page | Path |
| ---- | ---- |
| Module index | `docs/index.html` |
| Module page | `docs/<Module>/index.html` |
| Class page | `docs/<Module>/<Class>/index.html` |
| Method page | `docs/<Module>/<Class>/<name>.html` |

The documentation tree mirrors `Engine/`. Navigation is **module page → class page → method page**, and the
class page's methods table is the only navigation surface: no page carries a sidebar list of all methods.
That list was once 41.3 % of one class's bytes — 43 links copied 44 times, each a bare name whose
description already sits beside the same target in the class table.

## 2. Generate, never hand-write chrome

```bash
python3 .pi/skills/hb-docs/scripts/docs_page.py class  Core TaskStream \
    --source Engine/Core/TaskStream.h --summary "<one line>" [--tag "namespace hbe"] [--tag final] \
    --body /tmp/hbe-frag/Core/TaskStream/index.body.html

python3 .pi/skills/hb-docs/scripts/docs_page.py method Core TaskStream update \
    --source Engine/Core/TaskStream.h --summary "<one line>" --label "Update" \
    --body /tmp/hbe-frag/Core/TaskStream/update.body.html

python3 .pi/skills/hb-docs/scripts/docs_pass.py signatures docs/Core/TaskStream Engine/Core/TaskStream.h TaskStream
python3 .pi/skills/hb-docs/scripts/docs_pass.py reskin     docs/Core/TaskStream
python3 .pi/skills/hb-docs/scripts/docs_page.py check Core        # htmlcheck over the module
```

`--body FILE` is one HTML fragment carrying every `<h2 id="…">` section in page order; the sidebar's
page-local list is read back out of those headings, so an authored file cannot disagree with the chrome
indexing it. Section ids are fixed: `description`, `template`, `properties`, `methods`, `non-member`,
`coverage` on a class page; `signature`, `description`, `parameters`, `return`, `example` on a method page.

A page is written to `<name>.html.new`, validated, and moved into place **only if `htmlcheck` accepts it**.
One finding is tolerated — a dead link to a sibling page of the same class that is not authored yet — and
the count is printed, so finishing the class stays a visible obligation.

## 3. Template grammar

Class properties — one row per alias **and** per data member, header declaration order:

```html
<tr><th>Type + Name</th><th>Default Value</th><th>Description</th></tr>
<tr><td><code>std::atomic&lt;bool&gt; closeRequested{false};</code> <span class="badge">private</span></td>
    <td><code>false</code></td><td>One line: what the member means.</td></tr>
```

- Column 1 is the declaration **as the header writes it**, type and name in one `<code>`, then the badge.
- Column 2 is the default as the header writes it, `none in the header` when it gives none,
  `not applicable` for a `using` alias or nested type. A `static constexpr` shows its value — escaped,
  because `static_cast<TIndex>(-1)` written unescaped is a `<TIndex>` tag — without its trailing semicolon,
  which is the declaration's punctuation rather than part of the value.
- Column 3 is **one line**. Three sentences mean the mechanism belongs to a method page or a design doc.
- Badges: `member type`, `alias`, `public`, `private`, `public nested`, `private nested`, `bit-field`,
  `alias template`, `deleted`, `enumerator`. A `private` badge on a public member is a false statement
  about the interface, which is the one thing this table exists to get right.

Class methods — one row per declared **name**, header order, no Link column:

```html
<tr><th>Name</th><th>Signature</th><th>What a caller depends on</th></tr>
<tr><td><a href="run.html"><code>Run</code></a></td><td class="sig">bool Run(Task&amp; task) noexcept</td>
    <td>One clause a caller depends on.</td></tr>
<tr><td><code>operator=</code></td><td class="sig">WorkItem&amp; operator=(const WorkItem&amp;) = default</td>
    <td>Defaulted — no page; say what the default means.</td></tr>
```

The **name is the link**. Overload count is a badge beside it and every overload gets its own line in the
signature cell. A name with no page of its own (`= default`, `= delete`) stays plain `<code>`.

Method pages: the Signature section holds the **declaration, never a body**. A page that pasted a
definition once reproduced a source comment inside the document that quotes it — and that comment is
scheduled for deletion.

## 4. Prose rules

- **Cite a file and a symbol, never a line number**: `Defined in Engine/Core/Task.cpp as Task::Create()`.
  Line numbers drift, and a reader who cannot find the line stops trusting the page.
- **Where exact lines are the contract, print them**, verbatim, and attribute them:

  ```html
  <pre><code>affinity.Unset(TaskSystem::GetBaseTaskStreamIndex());</code></pre>
  <p class="meta">From Engine/Core/WorkItem.cpp, WorkItem::WorkItem()</p>
  ```

  Quote **code only**. Sources are comment-free; never present a paraphrase as a quotation.
- **Every claim traceable to something read.** A claim that cannot be proved does not go in a description;
  it goes in that page's Coverage section as a named omission. Thread-safety, lock order, ownership,
  identity reuse and use-after-free are exactly where a confident guess damages someone else's code.
- **A partial page must say so.** Its Coverage section names what is not documented and why, so a
  half-written class never reads as a finished one.
- No invented acronyms or code names: write things in full. `UI`, `HTML`, `GUI` are field vocabulary.

## 5. Gate before a class is called done

| Check | Command | Clean means |
| ----- | ------- | ----------- |
| Signatures equal the header | `docs_pass.py signatures docs/<M>/<C> Engine/<M>/<C>.h <C>` | `0 problem(s)` — it refuses to write unless the page's text then matches the header byte for byte and nothing outside the Signature section moved |
| No method page missing | `../hb-standards/scripts/docs_methods.py <Module>` | no line for that class |
| Entry documented and addressed | `../hb-standards/scripts/docs_coverage.py check-file Engine/<M>/<C>.h` | "documented, addressed from the header, method pages complete" |
| HTML, links, CSS | `docs_page.py check <Module>` | `0 with problems` |

Only then add the header's one pointer line — `/// API reference: docs/<Module>/<Class>/index.html`
immediately above the declaration — and give that class its row in `docs/<Module>/index.html`. The pointer
is the claim that the page is complete; a checker rejects a documented entry whose header does not carry
it, and rejects a pointer that names a different entry or resolves nowhere.

`docs_pass.py reskin` is the mechanical re-skin: it re-emits a folder's method pages through
`docs_page.py` and **fails if any prose moved**. Run it twice and compare hashes — a second run that
changes bytes is chrome accumulating, which is exactly how one class came to list its own source file four
times.

## 6. Boundaries

- Generate chrome; never hand-edit it. Hand-written chrome drifts: a class page listing five of its seven
  methods, a method page linking `../index.html` from three levels up, a class the stylesheet never declared.
- Move a header's comments into pages faithfully; **delete nothing** — `hb-standards` owns the strip and
  proves the destination first, per file, with `docs_coverage.py check-file`.
- One class per author when several are at work, and never two module pages at once.
- Do not edit `Engine/` beyond the pointer line, do not build, and do not commit unless asked.

## 7. Authoring at scale

`.Plans/NOTE_api_authoring_worker_brief.md` is the one-page brief for a worker assigned one class: this
grammar, the commands, the citation rules, the boundaries, and the report format. Give a worker a class, not
a module; a finished class is worth more than three started ones.

Verify a worker's report against the tree before committing it. A report has previously claimed a gate
passed when the command could not run, and counted 190 pages where the site held 467: run
`htmlcheck.py`, `docs_methods.py` and `docs_pass.py signatures` yourself and paste their output lines.
