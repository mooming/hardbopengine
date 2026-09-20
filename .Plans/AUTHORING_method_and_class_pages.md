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

Badge vocabulary, so one page's row kind is the next page's row kind: `member type` for a `using`, `private` for
a data member, `private nested` for a nested class or struct, `alias template` for a member `template` alias, and
`deleted` for a special member that exists only to be removed. Anything else needs a sentence in the cell — the
badge is a label, not an explanation.

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

1. **Real call site first.** Grep `Engine/` and `Applications/` for a caller, quote it, and name the file:
   `<p class="meta">From Engine/Core/TaskSystem.cpp</p>` inside the example section.
2. **Otherwise an illustrative snippet, labelled.** Start the section with
   `<p class="note">Illustrative — no call site for this exists in the repository.</p>`. Use only types and
   functions that exist. Never invent a type name to make a snippet look real.
3. **Never** fabricate a call site or attribute code to a file that does not contain it.
4. **Declared with no definition is its own case.** Where the header declares something no translation unit
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
`<li>None — aliases only</li>` and keep the "Methods" heading, so the sidebar does not look half-written.

## 10. Footer

```html
<footer class="prevnext">
  <span><a href="index.html">← Array</a></span>
  <span><a href="index.html#methods">All Array methods</a></span>
</footer>
```
Class pages: `← <Module>` and `<Class> in the <Module> class list`.

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
    dead=[h for h in c.hrefs if not h.startswith(('http','#')) and not os.path.isfile(os.path.normpath(os.path.join(os.path.dirname(p), h.split('#')[0])))]
    anch=[h for h in c.hrefs if '#' in h and not h.startswith('http') and h.split('#')[0] and not f'id="{h.split(chr(35))[1]}"' in open(os.path.normpath(os.path.join(os.path.dirname(p), h.split('#')[0]))).read()]
    miss=[h for h in c.hrefs if h.startswith('#') and h[1:] not in c.ids]
    badcss=sorted(c.cls-css)
    if dead or c.err or c.st or anch or miss or badcss:
        bad+=1; print(p, 'dead',dead,'err',c.err[:3],'unclosed',c.st[:3],'xanchor',anch[:3],'localanchor',miss[:3],'css',badcss)
    if len(re.findall(r'<pre><code>',src))!=len(re.findall(r'</code></pre>',src)):
        bad+=1; print(p,'UNBALANCED code blocks')
print('ALL CLEAN' if bad==0 else f'{bad} pages with problems')
PY
```

Replace the glob with your module names. **Do not report done until it prints `ALL CLEAN`.**

## 13. Exemplar to copy from

- `docs/Container/Array/index.html` — class page
- `docs/Container/Array/resize.html` — contract-heavy method
- `docs/Container/Array/operator-index.html` — overload family
- `docs/Container/Array/size.html` — trivial accessor, and the proof that a short page is acceptable

Read all four before writing. Match their voice: declarative, no "this function does X", every sentence earning
its line.

## 14. Boundaries

- Touch only `docs/<your modules>/`. Never edit `Engine/`, `docs/assets/hbe-docs.css`, another module's pages,
  `README.md`, `AGENTS.md` or `JOURNAL.md`.
- Do not commit; the orchestrator validates and commits.
- Do not invent API. If a header has no such member, it does not appear. If a guide claims it and the header
  disagrees, the header wins and the disagreement goes in Coverage.
