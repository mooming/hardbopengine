# Worker brief: authoring one class's API reference folder

For an agent asked to document one class of this engine. It states the grammar, the commands and the
boundaries. The contract itself is `.Plans/AUTHORING_method_and_class_pages.md`; this brief is the
short version plus the mechanics. Read both.

## What the reference is

Engine sources carry **no comments**. Every sentence that explains a contract lives in the HTML
reference under `docs/`, mirroring the source tree:

| Page | Path |
| ---- | ---- |
| Module index | `docs/index.html` |
| Module page | `docs/<Module>/index.html` |
| Class page | `docs/<Module>/<Class>/index.html` |
| Method page | `docs/<Module>/<Class>/<name>.html` |

Navigation is **module page → class page → method page**, and the class page's methods table is the only
navigation surface. No page carries a sidebar list of all methods — that was 41.3 % of one class's bytes.

`.pi/skills/hb-standards/scripts/docs_coverage.py check Core` and
`.pi/skills/hb-standards/scripts/docs_methods.py Core` are the gates: an entry without a page, or a declared
name without a page, blocks the module.

## Read before writing a word

1. `.Plans/AUTHORING_method_and_class_pages.md` — the contract: anatomy, grammar, naming, Coverage.
2. `docs/Core/TaskSystem/index.html` — exemplar class page in the current grammar.
3. `docs/Core/TaskSystem/update.html` and `docs/Core/TaskSystem/is-base-thread.html` — exemplar method
   pages, one of them carrying evidence snippets.
4. The class's own header **in full**, and its `.cpp` wherever a claim about behaviour depends on it. Plus
   the neighbouring headers the class talks to.

## Table grammar

Class page sections, in this order, ids exact: `description`, `template`, `properties`, `methods`,
`non-member`, `coverage`. Method page sections: `signature`, `description`, `parameters`, `return`,
`example`.

Properties — one row per alias **and** per data member, in header declaration order, three columns:

```html
<tr><th>Type + Name</th><th>Default Value</th><th>Description</th></tr>
<tr><td><code>std::atomic&lt;bool&gt; closeRequested{false};</code> <span class="badge">private</span></td>
    <td><code>false</code></td><td>One line: what the member means.</td></tr>
<tr><td><code>using TIndex = std::size_t;</code> <span class="badge">member type</span></td>
    <td><span class="muted">not applicable</span></td><td>One line.</td></tr>
```

- Column 1 is the declaration **as the header writes it**, type and name in one `<code>`, then the badge.
- Column 2 is the default **as the header writes it** (`false`, `nullptr`, `{0}`, `= 0`), or
  `<span class="muted">none in the header</span>` when the header gives none, or
  `<span class="muted">not applicable</span>` for a `using` alias or a nested type. A `static constexpr` shows its
  value, escaped — a default of `static_cast<TIndex>(-1)` written unescaped is a `<TIndex>` tag — and the trailing
  semicolon is the declaration's punctuation, not part of the value.
- Column 3 is **one line**. Three sentences mean the mechanism belongs to a method page or a design doc.
- Badges: `member type`, `alias`, `public`, `private`, `public nested`, `private nested`, `bit-field`,
  `alias template`, `deleted`, `enumerator`. A `private` badge on a public member is a false statement about
  the interface, which is the one thing this table exists to get right.

Methods — one row per declared **name**, header order, three columns, **no Link column**:

```html
<tr><th>Name</th><th>Signature</th><th>What a caller depends on</th></tr>
<tr><td><a href="run.html"><code>Run</code></a></td><td class="sig">bool Run(Task&amp; task) noexcept</td>
    <td>One clause a caller depends on.</td></tr>
<tr><td><a href="dequeue.html"><code>Dequeue</code></a><span class="badge">3 overloads</span></td>
    <td class="sig">bool Dequeue(WorkItem&amp; out)<br>bool Dequeue(WorkItem&amp; out, TStreamIndex)</td>
    <td>One clause.</td></tr>
<tr><td><code>operator=</code></td><td class="sig">WorkItem&amp; operator=(const WorkItem&amp;) = default</td>
    <td>Defaulted — no page; say what the default means.</td></tr>
```

The name cell is the link. A name with no page of its own (`= default`, `= delete`) is plain `<code>`.

## Page rules that are not negotiable

- **Signature = the header's declaration.** Never a function body.
- **Cite a file and a symbol, never a line number.** `Defined in Engine/Core/Task.cpp as Task::Create()`.
  Line numbers drift, and a reader who cannot find the line stops trusting the page.
- **When exact lines are the contract, print them**, verbatim from the source:

  ```html
  <pre><code>affinity.Unset(TaskSystem::GetBaseTaskStreamIndex());</code></pre>
  <p class="meta">From Engine/Core/WorkItem.cpp, WorkItem::WorkItem()</p>
  ```

  Quote **code only**. Sources are comment-free; never present a paraphrase as a quotation.
- **Every claim traceable to something you read.** Cannot prove it → do not write it. Add a row to that
  page's Coverage section naming what is not documented and why. A Coverage row admitting a hole beats a
  confident guess: thread-safety, lock order, ownership, identity reuse and use-after-free are exactly where
  a wrong sentence damages someone else's code.
- **Links** go to your own class's pages, or to folders that already exist. Anything else is not authored
  yet — name it in `<code>` with no link. The validator rejects a write carrying a dead cross-class link.
- No invented acronyms or code names in prose. Write things in full. UI/HTML/GUI are field vocabulary.

## Commands

Chrome (sidebar, breadcrumb, title, footer) is generated. Never hand-write it. Fragments go in
`/tmp/hbe-frag/<Module>/<Class>/`.

```bash
python3 .pi/skills/hb-standards/scripts/docs_page.py class  Core <Class> \
    --source Engine/Core/<Class>.h --summary "<one line>" [--tag "namespace hbe"] [--tag final] \
    --body /tmp/hbe-frag/Core/<Class>/index.body.html

python3 .pi/skills/hb-standards/scripts/docs_page.py method Core <Class> <page-stem> \
    --source Engine/Core/<Class>.h --summary "<one line>" --label "<Display Name>" \
    --body /tmp/hbe-frag/Core/<Class>/<page-stem>.body.html

# Prove every Signature block equals the header, character for character:
python3 .pi/skills/hb-standards/scripts/docs_pass.py signatures docs/Core/<Class> Engine/Core/<Class>.h <Class>

# Gates — both must be clean when you finish:
python3 .pi/skills/hb-standards/scripts/docs_page.py check Core     # 0 problems
python3 .pi/skills/hb-standards/scripts/docs_methods.py Core        # your class gone from the list
```

`--body` takes one HTML file holding every `<h2>` section in order; the sidebar's page-local list is read
back out of those headings. An `<h2>` heading must be plain text.

Method page file stems come from `docs_methods.py`, not from you. Overloads of one name share one page, all
declarations in header order. Constructors → `constructors.html`; destructor → `destructor.html`;
`operator<` → `operator-less.html`, `operator==` → `operator-equal.html`, `operator[]` →
`operator-index.html`, `operator<<` → `operator-left-shift.html`.

## Boundaries

- Do **not** edit anything under `Engine/`. Do **not** add `/// API reference:` pointer lines to headers —
  a pointer is a reviewer's claim that a page is complete.
- Do **not** edit `docs/Core/index.html`, `docs/index.html`, the stylesheet, or any class folder that is not
  yours. Shared files get edited after review, once, by the reviewer.
- Do **not** run `git add`, `git commit`, `git push`, `git checkout`, `git stash`. Read-only git is fine.
- If a claim in an already-published page contradicts the source, report it; do not silently rewrite the
  other class's page.

## Report format

Your final message is data for a reviewer, not prose. Include:

| Item | Content |
| ---- | ---- |
| Pages written | class page + each method page stem |
| Gates | final output line of `docs_page.py check Core` and of `docs_methods.py Core` |
| Grammar proof | the two `<tr><th>` header rows of your class page, quoted |
| Held back | each claim you refused to write and put in a Coverage row, and why |
| Defects | anything in the source that looks wrong, or where header and source disagree |
