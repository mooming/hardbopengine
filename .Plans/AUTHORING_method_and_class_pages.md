# Authoring rules: class pages and method pages of the HardBop Engine API reference

The site is hand-authored HTML. There is no generator: every file is typed. This file is the contract every
page must satisfy, so 490 files written by different hands read as one reference.

Approved structure (user decision, 2026-09-19): **one page per class, one page per method name, overload
families share one page.**

## 1. Layout on disk

| Level | Path | Contains |
|---|---|---|
| Module Index | `docs/index.html` | module cards |
| Module page | `docs/<Module>/index.html` | module description, classes table linking to class pages, module variables, module functions, module Coverage |
| Class page | `docs/<Module>/<Class>/index.html` | class description, template parameters, class properties table, class methods table, non-member helpers, Coverage |
| Method page | `docs/<Module>/<Class>/<method>.html` | one method name, all its overloads |

Class directory and file names are the **source spelling**: `Array`, `TaskSystem`, `StaticStringID`.

**The unit is a documented entry of the module page, not strictly a class.** Some entries are a macro set
(`BuildConfig`) or a namespace of free functions (`EngineConfig`). They still get a folder and an `index.html`,
because a link target and a sidebar entry are worth more than a taxonomically pure tree. Keep the AGENTS.md
outline — for a macro set, Class description says what the entry actually is, Template parameters says *none*,
Class properties carries the macros, and Class methods says none.
Method file names are **lower case, words separated by `-`**, operators spelled out:

| Method | File |
|---|---|
| `Resize` | `resize.html` |
| `ToRawArray` | `to-raw-array.html` |
| `operator[]` | `operator-index.html` |
| `operator=` | `operator-assign.html` |
| `operator<` | `operator-less.html` |
| `operator==` | `operator-equal.html` |
| `operator<<` | `operator-left-shift.html` |
| constructors (all overloads) | `constructors.html` |
| destructor | `destructor.html` |
| conversion operator | `operator-cast.html` |

## 2. Content source of truth

The **header**, always. Read it before writing a single word. `docs/*.md` guides are stale in specific,
already-confirmed ways (`OS::IApplication`, `IRenderer`, `Array` "no resizing", `LogUtil::GetLogLevelName` do not
exist). Never copy a claim from a guide into an HTML page without seeing it in the header.

State contracts, ownership, thread-safety, preconditions, complexity, and what breaks. Do not restate the
signature in prose, and do not pad. A getter page may be six lines; that is correct, not lazy.

## 3. Page outline — fixed by AGENTS.md

**Class page:** Class description → Template parameters → Class properties → Class methods → Non-member helper
functions → Coverage.

**Method page:** Signature → Function description → Parameters → Return value → Examples.

Section headings are `<h2 id="…">` with these ids: class pages `description`, `template`, `properties`,
`methods`, `non-member`, `coverage`; method pages `signature`, `description`, `parameters`, `return`, `example`.
The sidebar's "This class" / "This function" list points at exactly those ids — no more, no fewer.

**An entry that is not a class heads that list "This entry"** — a macro set, a namespace of free functions, or an
`enum class`. "This class" above a list of enumerators is a label that contradicts the page under it, and the
sidebar is the one place a reader uses to orient before they have read anything.

## 4. Tables

Class properties — one row per alias **and** per data member:

```html
<table>
  <tr><th>Name</th><th>Declaration</th><th>What it means</th></tr>
  <tr><td><code>TIndex</code></td><td><code>using TIndex = int;</code></td>
      <td><span class="badge">member type</span> …</td></tr>
  <tr><td><code>length</code></td><td><code>TIndex length;</code></td>
      <td><span class="badge">private</span> …</td></tr>
</table>
```

Badge vocabulary, so one page's row kind is the next page's row kind: `member type` for a `using` **declared
inside a class**, `alias` for an alias at namespace scope, `public` for a data member in the interface, `private`
for one that is not, `public nested` and `private nested` for nested classes and structs, `bit-field` for a
`: 1`-style member, `alias template` for a member `template` alias, and `deleted` for a special member that exists
only to be removed. `bit-field` exists because a capabilities descriptor is thirty of them and `public` says
nothing about the storage that is the entire point of the row. `enumerator` is the row kind for an `enum class`
member — an enumerator is neither public nor private nor a data member, and forcing it into one of those labels
misstates the type. The public and private variants are not
interchangeable: a `private` badge on a public nested guard states a falsehood about the
interface, which is the one thing a property table exists to get right. `member type` on a namespace-scope alias
is a false statement about scope, which is why the two are separate. Anything else needs a sentence in the cell —
the badge is a label, not an explanation.

For an entry that is macros only, `Name | Declaration | What it means` states the same thing twice, so the
middle column becomes **where it is read**: `Name | Read by | What it controls`, with `Read by` naming the
translation unit or "nothing". Naming the consumer is the whole value of such a table — a define nobody reads is
a define that can rot, which is how `EngineAPIGuide` came to lie.

Class methods — one row per **method name**, the name linking to its page, the overload count as a badge:

```html
<tr>
  <td><a href="resize.html">Resize</a></td>
  <td class="sig">void Resize(TIndex newSize) noexcept</td>
  <td>… one or two clauses a caller depends on …</td>
</tr>
<tr>
  <td><a href="begin.html">begin</a><span class="badge">2 overloads</span></td>
  <td class="sig">Iterator begin()<br>ConstIterator begin() const</td>
  <td>…</td>
</tr>
```

Parameters — `Parameter | Type | Requirement`, one row per parameter, `<p>None.</p>` when there are none. For a
function template with no ordinary arguments, give one row for each template parameter with Type set to
`<em>template parameter</em>`, and a final row `<em>(function parameters)</em>` | `—` | what they are — that pair
is what makes the absence of an argument list an explicit fact rather than an oversight.
Return value — `<table>` `Type | What you get` when the type carries meaning, a `<p>` when it is `void`.

## 5. Examples — approved rule

**This rule is about method pages.** The fixed class-page outline has no Examples heading, and §3's outline is not
yours to extend: on a class page, cite a real call site inline where it belongs — in the Class description, or in
that method's row in the methods table — as `<code>Engine/Core/TaskSystem.cpp</code>`. Method pages get the
Examples section and the rules below.

1. **Real call site first.** Grep `Engine/` and `Applications/` for a caller, quote it, and name the file:
   `<p class="meta">From Engine/Core/TaskSystem.cpp</p>` inside the example section.
2. **Otherwise an illustrative snippet, labelled.** Start the section with
   `<p class="note">Illustrative — no call site for this exists in the repository.</p>`. Use only types and
   functions that exist. Never invent a type name to make a snippet look real.
3. **Never** fabricate a call site or attribute code to a file that does not contain it.
4. **When most of a module has no caller, say so once at module level.** Half the containers in this engine are
   never instantiated anywhere: writing "Illustrative" on thirty pages tells the reader nothing after the third.
   Put one table in the module page — **Usage in the engine** — naming which entries are instantiated, where, and
   which are unused, then let the per-page notes be short. Honesty about reach is information; a per-page apology
   is noise.
5. **Declared with no definition is its own case.** Where the header declares something no translation unit
   defines, the example section states that plainly and explains the consequence — a call does not link — instead
   of showing a snippet that could not build. This is not a gap in your work; it is a finding about the module,
   and it belongs on the page, in Coverage, and in your report.

## 6. Callout boxes

`<div class="note">` for an asymmetry, a convention, or something worth knowing.
`<div class="gap">` for a hazard, a contract that bites, or a documented absence. Bold the first clause.
Do not use them for trivia; two or three per page maximum is the norm, more means the prose is doing no work.

## 7. Escaping — this is the failure that deletes content

Inside `<code>` and `<pre>`: `&` → `&amp;`, `<` → `&lt;`, `>` → `&gt;`. A raw `template <typename T>` or
`Vector3<float>` becomes an unknown HTML tag and **silently erases the signature**. Syntax spans:
`<span class="kw">` keyword, `<span class="ty">` type, `<span class="fn">` function, `<span class="cm">`
comment, `<span class="nu">` number, `<span class="st">` string. Spans are optional; escaping is not.

## 8. Links

| From | To | Write |
|---|---|---|
| class page | stylesheet, Module Index, own module | `../../assets/hbe-docs.css`, `../../index.html`, `../index.html` |
| class page | another module's **page** | `../../Memory/index.html` or `../../Memory/index.html#allocators` (the anchor must exist) |
| class page | its own method pages | `resize.html` |
| class page or method page | a **sibling class in the same module** | `../SimpleLogger/index.html`, or `../SimpleLogger/flush.html` for one of its methods — a module is not a namespace boundary, and `Logger` and `SimpleLogger` reference each other on almost every page |
| method page | its class page | `index.html` |

- **Never link to a class page or method page that is not in your assigned batch.** Cross-module references point
  at module pages, which all exist.
- **A module page rewrite must preserve every anchor that survives inbound links.** Other modules link to
  `../<Module>/index.html#window`, `#allocators`, `#buildconfig` and friends. When the deep per-class section
  moves to a class page, put `id="<class>"` on that class's row in the classes table, so the old address keeps
  resolving instead of becoming a silent 404 in someone else's page. Find the inbound fragments before you
  rewrite: `grep -rn "<Module>/index.html#" docs --include=*.html | grep -v "^docs/<Module>/"`.
- `../README.md#some-heading` is not checkable by string search: GitHub generates that fragment from the heading
  text. It reads as broken to the validator and is not.
- CSS classes available: `sidebar`, `main`, `breadcrumb`, `subtitle`, `tag`, `badge`, `note`, `gap`, `prevnext`,
  `module-grid`, `module-card`, `sig`, `methods`, and the syntax spans. Nothing else — `check` with the validator.

## 9. Sidebar

Copy verbatim from the exemplar and change only the `current` marker and the "Methods" list. The Modules list is
identical on every page of a module, and a method page marks itself current in the Methods list. The Modules list
on a class page marks **no** entry current; on a method page it also marks none.

A class whose members are aliases only has no method pages: replace the list with
`<li>None — aliases only</li>` and keep the "Methods" heading, so the sidebar does not look half-written. Same
for a macro set (`None — macros only`) and a namespace entry whose functions need no page
(`None — see the module functions`). An `enum class` writes `None — an enumeration declares no members`; a plain
data struct writes `None — data only`. **A namespace entry whose functions do carry a contract per §15 lists those
pages** — `Config/EngineConfig` lists `get-max-system-memory-target.html`, and telling it to write "see the module
functions" would point a reader at a table that links straight back to the page they are on. Those four phrasings
are the whole vocabulary — inventing a fifth per module is how thirty pages end up saying
the same absence thirty different ways. **Every class
page carries the heading**, even when the honest content is a two-word absence: a missing heading cannot be told
apart from an unfinished page. **An orchestrator brief does not override this file** — one brief told an alias
family to write `None — see the module functions`, which §9 reserves for namespaces and which pointed at a
section reading "None." If a brief and this file disagree, this file wins and the brief is the bug.

## 10. Footer

```html
<footer class="prevnext">
  <span><a href="index.html">← Array</a></span>
  <span><a href="index.html#methods">All Array methods</a></span>
</footer>
```
Class pages: `← <Module>` and `<Class> in the <Module> class list`, the second pointing at
**`../index.html#classes`** — the classes table. Not `../index.html#<class>`: the per-class row ids exist so that
*other modules'* inbound links keep resolving after a rewrite, and a footer pointing at one works but teaches the
wrong reason for it. Method pages: `← <Class>` and `All <Class> methods` at `index.html#methods`.

## 11. Coverage section

Every class page ends with `<h2 id="coverage">Coverage</h2>` naming what the page does **not** document —
undocumented members, contracts that live in another module, test-only code. A reader must never mistake a
partial page for a complete one. A page that genuinely covers everything states so in one row.

## 12. Validate before you report done

```bash
cd /Users/anav/atelier/hardbopengine && python3 - <<'PY'
import re, os, glob
from html.parser import HTMLParser
VOID={'area','base','br','col','embed','hr','img','input','link','meta','source','track','wbr'}
class C(HTMLParser):
    def __init__(s): super().__init__(); s.st=[]; s.err=[]; s.ids=set(); s.hrefs=[]; s.cls=set()
    def handle_starttag(s,t,a):
        d=dict(a)
        if 'id' in d: s.ids.add(d['id'])
        if 'href' in d: s.hrefs.append(d['href'])
        for c in d.get('class','').split(): s.cls.add(c)
        if t not in VOID: s.st.append((t,s.getpos()[0]))
    def handle_endtag(s,t):
        if t in VOID: return
        if not s.st or s.st[-1][0]!=t: s.err.append((t,s.getpos()[0]))
        else: s.st.pop()
css=set(re.findall(r'\.([A-Za-z][\w-]*)', open('docs/assets/hbe-docs.css').read()))
bad=0
for p in glob.glob('docs/<your modules here>/**/*.html', recursive=True):
    src=open(p).read(); c=C(); c.feed(src); c.close()
    dead=[]; xanchor=[]
    for h in c.hrefs:
        if h.startswith(('http','#')): continue
        f,frag=(h.split('#')+[None])[:2]
        t=os.path.normpath(os.path.join(os.path.dirname(p), f))
        if not os.path.isfile(t):
            dead.append(h); continue                     # report it, never open it
        if frag and not f.endswith('.md') and f'id="{frag}"' not in open(t).read():
            xanchor.append(h)
    local=[h for h in c.hrefs if h.startswith('#') and h[1:] not in c.ids]
    badcss=sorted(c.cls-css)
    pre=len(re.findall(r'<pre><code>',src))!=len(re.findall(r'</code></pre>',src))
    contam=re.findall(r'<[a-zA-Z/][^>]*\\|\\"', src)
    if dead or c.err or c.st or xanchor or local or badcss or pre or contam:
        bad+=1; print(p,'dead',dead,'err',c.err[:3],'unclosed',c.st[:3],'xanchor',xanchor,'localanchor',local,'css',badcss,'pre' if pre else '','CONTAMINATED' if contam else '')
print('ALL CLEAN' if bad==0 else f'{bad} pages with problems')
PY
```

Replace the glob with your module names. **Do not report done until it prints `ALL CLEAN`.** A missing file is a
finding to print, not an exception to throw — an earlier revision opened every link target before checking it, so
the script crashed with `FileNotFoundError` on exactly the dead link it existed to catch, which is the worst
possible behaviour for a validator: it fails silently-by-crash mid-batch and teaches the reader to distrust
`ALL CLEAN`.

If you run this over the whole site rather than your module: the standalone design documents
(`docs/RendererDesign.html`, `docs/design/**`) carry their own inlined styles and are not part of this
stylesheet, so their classes are not ours to check. A link into a `.md` file is likewise not checkable — GitHub
generates heading fragments that no string search can find. Both read as failures and are not.

### The outbound check cannot see the failure mode §8 warns about

Rewriting a module page can delete an anchor that six other modules link to, and the script above only inspects
links leaving your pages. Run this inbound scan too — it is the one that catches the mistake that hurts:

```bash
cd /Users/anav/atelier/hardbopengine && python3 - <<'PY'
import re, os, glob
mod='<your module>'
broken=[]
for p in glob.glob('docs/**/*.html', recursive=True):
    if p.startswith(f'docs/{mod}'): continue
    for h in sorted(set(re.findall(r'href="([^"]*index\.html#[^"]+)"', open(p).read()))):
        f,frag=h.split('#',1)
        t=os.path.normpath(os.path.join(os.path.dirname(p), f))
        if f'docs/{mod}/index.html' not in t.replace('./','docs/') and f'/{mod}/index.html' not in t: continue
        if not (os.path.isfile(t) and f'id="{frag}"' in open(t).read()):
            broken.append((p, h)); print('BROKEN', p, '->', h)
print(f'inbound links into docs/{mod}: {"all resolve" if not broken else str(len(broken))+" BROKEN"}')
PY
```

## 13. Exemplar to copy from

- `docs/Container/Array/index.html` — class page
- `docs/Container/Array/resize.html` — contract-heavy method
- `docs/Container/Array/operator-index.html` — overload family
- `docs/Container/Array/size.html` — trivial accessor, and the proof that a short page is acceptable
- `docs/Container/Deque/index.html` and `Deque/begin.html` — **iterator and reference invalidation**, the page
  type Array cannot demonstrate because `Array` never grows in place; `Deque`'s iterators go unmasked and die on
  wrap
- `docs/Container/AtomicStackView/pop.html` — **concurrency**: memory ordering, who may call what, and what a
  returned `nullptr` does and does not prove

Read all six before writing. Match their voice: declarative, no "this function does X", every sentence earning
its line. **All four Array exemplars are pages about a non-growing contiguous type**, so on the three hardest page
types — invalidation, allocator ownership across a move, concurrency — "match the exemplars" underdetermines the
answer; the last two exemplars exist to close that gap, and a page of a kind none of them covers is a page where
you write the section from the header rather than from the shape of the example.

## 14. Write discipline — learned from an agent that lost a whole module

One response must carry **one file, at most three**. A sibling agent composing the Test module emitted roughly
230,000 output tokens in a single response; the API rejected the entire response as over the context ceiling and
every page in it was lost. It had written nothing, which is the only reason the cost was zero.

So: write each file as you finish thinking about it rather than assembling a batch to emit at once, and run
section 12 every few pages so a failure costs pages instead of a module. Rewrite the module page in its own
response — it is the longest single file in a job and the worst place to run out of room.

**Derive your file list; do not copy one from a brief.** An orchestrator's enumerated list is a snapshot of what it
thought of, and the site requires more files than the list: Array's pages referenced eleven siblings, the brief
named ten, and the eleventh existed only as a link in files you are forbidden to edit. Derive the set instead —
every name a page links to, plus every member the header declares — and the list will not be short by one.

## 15. A module-level function with a real contract still gets a page

"None — see the module functions" points a reader at a table, which is right for a one-line helper and wrong for
the function carrying the module's most surprising contract — "this returns before any test runs", for instance.
Where a module function's behaviour cannot be told in its table cell, give it a page **inside the folder of the
entry whose header declares it**, named by the file rule: `docs/Test/UnitTestCollection/run-tests.html`. Keep the
module page's functions table as it is, and let the row link to the page. Do not invent a function page for a
getter-shaped free function that needs none.

## 16. A claim about what compiles needs a compiler

Pages state build consequences: "not active in Release", "a link error", "does not compile". Those are the most
expensive claims on the site, because a reader acts on them by changing code. **Grep proves a name exists; only a
compiler proves what it does.** If you assert a compile outcome, run the smallest program that isolates it —
`c++ -std=c++2b -fsyntax-only probe.cpp` — and quote the diagnostic on the page. Two claims in this project were
wrong in exactly this way and one was corrected by a three-line probe: a variadic `std::forward(args)...` was
described as silently moving lvalue arguments, when `remove_reference_t<T>&` is a non-deduced context and the
argument-taking form does not compile at all. Wrong in the reassuring direction, which is the direction that
matters: "silently consumed" invites a workaround, "does not compile" tells the truth.

Say "declared, never defined — a call does not link" only when you greped every translation unit. Say "this does
not compile" only when you have seen the error.

## 17. Boundaries

- Touch only `docs/<your modules>/`. Never edit `Engine/`, `docs/assets/hbe-docs.css`, another module's pages,
  `README.md`, `AGENTS.md` or `JOURNAL.md`.
- Do not commit; the orchestrator validates and commits.
- Do not invent API. If a header has no such member, it does not appear. If a guide claims it and the header
  disagrees, the header wins and the disagreement goes in Coverage.
