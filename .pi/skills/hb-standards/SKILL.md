---
name: hb-standards
description: >-
  Bring HardBop Engine C++ sources into the project coding standards end to end: format, apply the safe
  mechanical fixes, reorder members and their initializer lists, author the HTML reference the comments
  must move into, delete the comments, and prove the tree still builds in Debug, Dev and Release. Use when
  asked to apply clang-format, check or fix coding standards, run check.sh --fix, run the module fix cycle
  pair by pair, prepare or amend a commit, review a commit for style conformance,
  sweep or strip comments from a module, write or repair API reference pages under docs/, or when Main.cpp
  / engine sources need the Allman brace style, tab indentation, include ordering and the
  Engine/CodingStandards.h conventions enforced. Also use before declaring any engine change done, because
  the skill ends with a three-configuration build gate.
---

# hb-standards

Six layers, then a build gate. All are required: on this tree the standard's own exemplar files
were **clang-format-clean but rule-non-clean** (include layout), so no single layer is sufficient.

| Layer | Checks | How | Who fixes |
|---|---|---|---|
| 1 | Allman braces, tabs, 120 columns, include order, blank lines | clang-format | script, always, in every fix |
| 2 | joined empty bodies, no exceptions, `m_` prefix, explicit `inline`, hygiene, include layout | greps; `scripts/autofix.py` applies the safe subset | script for five fixes, reviewer for the rest |
| 3 | comment ban in `.h` and `.cpp` | `scripts/comments.py`, a lexer; `--strip` deletes | reviewer writes the prose into `docs/`, script deletes, gate proves |
| 4 | twelve-block member layout | `scripts/layout.py`, clang AST; `--init-order` for initializer lists | reviewer, gate-proved |
| 4b | grouping *inside* a block: one concern per group, one blank line at each seam | `scripts/prove_regroup.py` proves the edit moved nothing but order | reviewer decides, script proves |
| 5 | every entry owns a page, every method owns a page, every page is valid HTML | `scripts/docs_coverage.py`, `scripts/docs_methods.py`, `scripts/htmlcheck.py`; `scripts/docs_page.py` writes them | reviewer authors, `docs_page.py` builds chrome, gate proves |
| 6 | Dev, Debug, Release compile, `EngineTest` on request | cmake + ninja, `scripts/runtest.sh` | nobody fixes it; it decides whether the fix is real |

Three roles, and the split is the design.

| Role | Does | Never does |
|---|---|---|
| Deterministic script (`autofix.py`, clang-format) | rewrites correct without knowing what the code was for, and declares every token it removes in `.Plans/fix-manifest.json` | rename across files, delete error handling, move members, write prose |
| Reviewer — the assistant, in session | member reorder, initializer re-sequencing, renames, exception paths, the prose that moves into `docs/`, and every fix the scripts refuse | skip a proof, push, or edit a file outside the pair being fixed |
| Gate (`prove_format.py`, `layout.py`, `comments.py`, `docs_*`, the build) | independently re-derive what changed and refuse what the fixer did not declare | pass on `no work to do`, or check nothing and call that clean |

Report-only is the **fallback**, not the default: a layer that cannot prove its own edit reports
instead of guessing. That preserves the property the previous invariant was really protecting — no tool
may move a data member past another on a guess, because member order is initialisation order — while
stopping "a human will do it all" from being the reason nothing gets done.

## Run it

```bash
.pi/skills/hb-standards/scripts/check.sh                 # files in HEAD
.pi/skills/hb-standards/scripts/check.sh --staged        # files about to be committed
.pi/skills/hb-standards/scripts/check.sh <rev>           # files in a given commit
.pi/skills/hb-standards/scripts/check.sh --staged --apply  # rewrite, then lint
.pi/skills/hb-standards/scripts/check.sh --staged --fix  # clang-format, autofix.py, clang-format, re-lint
.pi/skills/hb-standards/scripts/check.sh --all --no-build  # whole tree, no compile
.pi/skills/hb-standards/scripts/check.sh --test          # also run EngineTest
```

`--fix` always runs layer 1: a fix that left layout to a formatter afterwards would re-run every grep
against bytes about to change. It refuses while a C++ file the run does not own is dirty, because a fix
that sweeps someone's half-finished refactor into its own commit cannot be explained by
`prove_format.py` afterwards. Script-level edits stop at the safe list in layer 2; everything else a fix
needs is reviewer work, run in session pair by pair — see *The fix cycle, in session* below.

Exit status: `0` clean, `1` violations, `2` build failed, `3` usage error.

Default scope is the **files a commit touched**, not the whole tree. That is deliberate: a
bulk sweep is a separate owner decision, not something to fold into a feature commit.
Use `--all` when the whole tree is the intended subject.

Always run it **after** `--apply` and **before** committing. If `--apply` changed
files, re-run the lint from scratch rather than trusting the formatter's exit code.

## Running it off the main thread

The full gate takes minutes. Running it inline costs the caller its whole turn and
buys nothing, so run it as a separate process and read the verdict afterwards.

```bash
# one line, no wrapper — the model MUST be pinned (see the trap below)
.pi/skills/hb-standards/scripts/gate.sh spawn --all --test
.pi/skills/hb-standards/scripts/gate.sh wait                      # blocks, exits with the gate's code
```

`gate.sh spawn` launches `pi -p` detached, which loads this skill, runs `check.sh`
with the arguments it was given, and reports per the Reporting section. Under
`.pi/logs/gate/<job>/` you get `cmd`, `pi.log`, and `done`; `gate.sh status [job]`
reads the verdict, `wait` blocks on it, `list` shows jobs and slot occupancy,
`release` frees slots left behind by a killed job. Logs live under `.pi/logs/`,
which is gitignored.

**The verdict is the `GATE_EXIT=<n>` line inside `pi.log`, not the process exit code.**
`done` holds whether that assistant survived; `GATE_EXIT` holds whether the tree is
clean. Confusing the two reports a pass that never happened. `wait` re-exits with
`GATE_EXIT`, so scripts can treat it exactly like `check.sh`.

Raw form, if you want to see it without the wrapper:

```bash
pi -p --offline --no-session --approve --skill .pi/skills/hb-standards \
   --provider "$PI_PROVIDER" --model "$PI_MODEL" --tools read,bash,grep,find,ls \
   "Run .pi/skills/hb-standards/scripts/check.sh --all; echo GATE_EXIT=\$?; report per the skill's Reporting section; edit nothing."
```

Everything this skill requires is `clang-format`, `python3`, `cmake`, `ninja` and `git`. Fan-out to
concurrent agents is **not** one of them: it needs a subagent extension, this project depends on none, and
the sequential loop below reaches the same state. If such a tool happens to be available, the per-pair
sequence is unchanged and the pairs are simply independent until a rename crosses a file boundary — at
which point they were never independent, and the rename is done last, alone, for the whole module.

### Concurrency: 4, and ask first

**Never run more than 4 concurrent detached gates without asking the
owner.** `PI_GATE_SLOTS` overrides the cap for `gate.sh`. This is not politeness:
the build tree is single-occupancy, so two gates race on `cmake-build-*` and each
reports a pass the other invalidated. `gate.sh` enforces it with `mkdir` slots
(macOS ships no `flock`) and refuses the fifth job with exit 3.

### Traps already paid for here

- **A detached `pi` must have its provider and model pinned.** Without
  `--provider`/`--model` it picks its own default and dies at startup with
  `401 Invalid bearer token`. `--offline` alone does **not** fix this — measured.
  Inside a pi session, inherit `PI_PROVIDER` and `PI_MODEL`; `gate.sh` does, and
  fails fast with a one-call preflight instead of burying the 401 in a log.
- **`pi auth check` can report `ready` while the token is dead**, so it is not a
  usable preflight. Ask the model to reply `OK` with `--no-tools` instead.
- **`--tools` is the safety belt.** A lint-only run gets `read,bash,grep,find,ls`
  so it cannot edit anything; `edit,write` are added only when `--apply` was asked for.
- **Never let a detached job push.** It is stated in the prompt and must stay there.
- `batch_ai_prompt.sh` is the older convention and is broken in two quiet ways: `local`
  is used outside a function, and its counters increment inside a piped `while`
  subshell, so its closing tally always prints `0 files processed, 0 failures`.

## Layer 1 — clang-format

Enforces things that need real C++ parsing: Allman braces (break before every `{`, empty bodies
included), tabs, 120 columns, blank-line *limits*.

Two limits on what that sentence means. It caps how many blank lines may sit together, and inserts one
place only — `SeparateDefinitionBlocks: Always` puts a blank between definition blocks, so two inline
member bodies can never sit adjacent. Everywhere else it leaves placement alone, so a file can be
format-clean and still read as one dense slab: grouping is a reader's edit, layer 4's table of contents,
not the formatter's. And where the formatter and Allman genuinely conflict with no knob to mediate, the
formatter wins: clang-format 22 writes a `concept` body as `concept C = requires(T t) {`, has no
`BraceWrapping` key for it, and `CLockable` in `Engine/Core/ScopedLock.h` therefore keeps the attached
brace. Do not "fix" it back.

## Layer 2 — mechanical rule checks clang-format cannot do

Tab-vs-space indentation · joined empty bodies that survived the formatter ·
no exceptions · no `std::move` on return (kills NRVO) · no `virtual` with
`override` · no `m_` prefix · copyright header, trailing newline, trailing
whitespace · include layout (own header → `<standard>` → `"project"`, each
alphabetical, exactly one blank line before the first code body) · header
minimality.

### Header minimality: declared in the header, defined in the source

A non-template, non-`constexpr` function body belongs in the `.cpp`. The header is the API surface, and a
body in it is both a line the reader did not ask for and a private advantage handed to every includer.

The optimisation reasoning has to be stated rather than assumed, because it is the part people get wrong.
A body in a header can be inlined by any caller; a body in a `.cpp` can be inlined only inside its own
translation unit unless link-time optimization runs. This tree has **no `-flto` and no
`INTERPROCEDURAL_OPTIMIZATION`** (measured against `CMakeLists.txt`, where Release is `-O3`), so the
second case really does cost a call today. The rule accepts that. Where a profile names a hot accessor, the
fix is IPO on the Release configuration — not an exemption, and not a body creeping back into a header,
which is how a header ends up re-parsed by every translation unit that wanted to ask one question.

Technical exceptions, not stylistic ones: templates and members of class templates; `constexpr` and
`consteval` a caller evaluates at compile time; a `friend` operator defined in-class where
argument-dependent lookup is the reason; data and `static constexpr` values, since layout and constant
expressions are what the header must expose. `Engine/CodingStandards.{h,cpp}` demonstrates all of it, the
bad example included.

This is the one rule in the layer that a reader reports rather than a grep: deciding whether a body is
"genuinely necessary" needs the reason, and reasons are not greppable. It ships ungated on purpose — 145
bodies sit in the 39 engine headers that declare no templates, and a gate switched on over a backlog that
size is red from its first day, which is the failure mode this skill has spent a session removing. An AST
checker is cheap when the sweep is ready (a method record carrying a `body`), and it must enter as an
advisory count before it becomes a finding.

`Engine/CodingStandards.{h,cpp}` are exempt from the behavioural checks only:
they carry deliberate BAD EXAMPLE blocks. Formatting, naming, hygiene and include
checks still apply to them. To silence a specific line elsewhere, end it with
`// hb-standards:ignore` — and note what that directive is *not*: it is a waiver, not an eraser.
`layout.py` reads it, suppresses the finding, and re-prints the member as `MEMBER-WAIVED` with a
count in its summary line, because "not reported" and "clean" are different claims and a standing
exception is the former. Where a waiver needs a reason, the reason goes in a design document, since
engine sources carry no prose to hold it.

### What a script may fix, and what it hands off

`autofix.py` applies exactly these, and each is safe because its result is decided by the rule text
rather than by what the code was for:

| Fix | Why no judgement is needed |
|---|---|
| split a joined empty body or empty record | layout only; layer 1 re-flattens what it likes |
| drop `virtual` where `override` is present | `override` already implies `virtual`, so the keyword is dead text |
| `return std::move(x);` to `return x;` for a plain local | the rule's own stated reason is NRVO, and this is its one shape |
| trailing whitespace, missing final newline, missing line-1 copyright | hygiene, no tokens involved |

Generated headers are skipped exactly as `check.sh` and `docs_coverage.py` skip them — the first
tree-wide dry run found this script about to insert a copyright line into `ShadersSpv.h`, which the next
codegen run would delete. A line carrying a comment is left alone: that comment is scheduled for
deletion by layer 3, and rewriting code under text about to disappear is how a wrong edit gets made for
the right reason.

Everything else is counted and handed to a reviewer, because it needs the cross-file view (`m_`,
snake_case), the control flow (exceptions), or the AST (member layout). Include order is **not** in the
fixer's list: layer 1 is clang-format and `SortIncludes` is on, so a second sorter would only disagree
with the first.

The fixer declares its own edit and cannot grade it: `autofix.py` writes `.Plans/fix-manifest.json`
with the tokens each file removed, and `prove_format.py HEAD --manifest .Plans/fix-manifest.json`
explains a token loss only if the fixer declared exactly that loss. Verified in three directions: a
declared fix passes, the same diff with no manifest fails, and one extra undeclared keyword removal
under an otherwise valid manifest fails.

## Layer 3 — the comment ban (`comments.py`)

No comments in `.h` or `.cpp`. The engine's prose belongs to `docs/`; see
`docs/CodingStandards.md` for the rule and the exhaustive exemption list. A grep cannot enforce
this: `http://` inside a string literal is not a comment, so the script lexes the file — line
comments, block comments, string and character literals, and line continuations. Raw string
literals are absent from this tree (measured: 0 files), and the lexer fails loudly rather than
mis-lexing if one appears.

The one exemption that is neither legal notice nor structural label is the **API reference pointer**: one
`/// API reference: docs/<Module>/<Entry>/index.html` line directly above a documented entry. It is an
address, so it is checkable in a way a sentence never is — `comments.py` rejects a pointer whose path does not
resolve, whose module is not the file's own, whose entry the file does not declare, or which sits on a member
rather than above its declaration, and `docs_coverage.py` rejects a page whose header carries no pointer at
all. The form is exact for that reason: a pointer allowed to grow a sentence is a `@brief` comment that
escaped the ban, which is what it replaced.

Two properties of this script are easy to misread. It **exits 1 while any comment remains**, which is the
normal state of a module that still owes reference pages — a nonzero exit here is a count, not a failed
step, and a driver that treats it as a failure marks a correct run as broken. And its exemption for
structural labels (`#endif // PROFILE_ENABLED`, `} // namespace hbe`) depends on knowing which construct
each line closes, which is a stack discipline no compile can check: `--selftest` runs five fixtures over
it, including the nested-guard shape whose absence once deleted permitted labels from
`Engine/Core/ScopedLock.h`.

Deleting them is `--strip`, and the ordering rule and its token proof live in
*Deleting the comments, with proof* under Layer 5, because what gates the deletion is the reference.
The ledger that schedules the work is the same file either way:

```bash
.pi/skills/hb-standards/scripts/docs_coverage.py ledger     # rewrite .Plans/DOCS_COVERAGE.md
.pi/skills/hb-standards/scripts/docs_coverage.py check Core # pages Core still owes
```

Pass the module **name**, never the path: `check Core`, not `check Engine/Core`. The path form matched
no ledger row and used to print "0 missing page(s)" for a module that owed twenty-three. Both doc
gatekeepers now name the argument, suggest the module name and exit 3.

## Layer 4 — twelve-block member layout (`layout.py`)

Types, then all data, then all functions; each layer `public` → `protected` → `private`, `static`
first inside each. The block table is in `docs/CodingStandards.md`. The checker asks clang, because
C++ declarator syntax defeats patterns exactly here — see the traps below.

One exception is forced by the language, not by taste: **a type whose definition needs a class constant
follows that constant.** A nested type sized by `MaxProvidersPerLane`, or an alias bounded by
`MaxQueueSize`, cannot name a name the compiler has not yet seen, so the order the table wants does not
compile. Mark the type's own line `// hb-standards:ignore`, keep the constant directly above it, and put
the reason in the module's design document. `layout.py` then reports `MEMBER-WAIVED` and prints
`, N waived` — a file with a waiver is not a clean file, and must not print as one.

Needs a compile database for the project's own flags: `cmake-build-debug/compile_commands.json`.
Absent, the layer prints `[NONE]` and says so; a rule that could not run must never be readable as
a rule that passed. Regenerate one with
`cmake -S . -B cmake-build-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.

`layout.py --init-order` is the second half of a reorder. Moving data members is only honest once every
constructor initialises them the way they are now declared, and this is the compiler's own `-Wreorder`
diagnostic computed statically. Three properties of clang's JSON had to be measured to get it right: the
field lives at `anyInit.name` rather than on the initializer, this dump carries no `isWritten` flag, and
children arrive in **initialization** order — already sorted by Sema to match declaration order — so
comparing children order against declaration order compares the data with its own sort key and can never
disagree. Written order is rebuilt from each initializer's source position. The first version read
`name` off the initializer, skipped every entry, and reported clean on any input.

## Layer 4b — grouping the declarations inside a block (`prove_regroup.py`)

The twelve-block table says a class's state sits above its API. It says nothing about the order *within*
a block, and that is where a header earns or loses its readability: a reader should be able to skim the
declarations and get a table of contents out of the whitespace.

The convention is `docs/CodingStandards.md`'s: members of one concern sit adjacent, exactly one blank line
separates two concerns, and a block never opens or closes on a blank line. Beyond that the order is a
judgement about what the class is for, which is why this layer has no autofix. A header that reads as a
table of contents for a run loop looks like this, and the order below is the one `Engine/Core/TaskSystem.h`
carries:

```
types → constants → state            (identity · thread identity · streams and registry · queue · budget)
lifecycle → drive the frame → the queue it drains → handing work to a stream
→ creating and fanning out → join and abandonment wiring → introspection → private helpers
```

Three things this layer has to respect, each measured on this tree:

- **The formatter owns some of the seams.** `.clang-format` sets `SeparateDefinitionBlocks: Always`, which
  inserts a blank line between definition blocks. Two inline member bodies can never be made adjacent, so
  group around the declarations and let the formatter have the bodies.
- **A strip destroys grouping, silently.** `comments.py --strip` deletes the comment-only lines that used to
  sit between concerns. Re-group a header after stripping it, not only before — this is how `TaskSystem.h`
  ended up format-clean and unreadable after its 233 comments went.
- **Grouping is whitespace and order, so it must be provable as such.** Run the proof immediately after the
  edit, and use `--between REV_A REV_B` to certify the commit rather than a working tree that may hold
  someone else's uncommitted work.

```bash
python3 scripts/prove_regroup.py Engine/Module/Header.h --between <rev>^ <rev>
```

It proves four things: every non-blank line survives unchanged (so a member cannot be dropped or quietly
edited — `layout.py` cannot see a vanished member, because a class missing a member has no ordering
problem); non-static data members keep their declaration order, read from the clang AST, because C++
initialises them in that order and nothing else reports a change; the `__UNIT_TEST__` region is
byte-identical, since test-only surface is never part of a regrouping; and it reports how many lines moved,
so a commit can say what it regrouped. Exit 0 proven, 1 not proven, 3 could not run — and a run that could
not measure the data order says so rather than printing a pass.

A regrouping changes what the class's reference page should look like too: the page's member table follows
the header's groups, so the two are re-read together.

## Layer 5 — the reference under `docs/`

Three claims, three checkers, because "this module is documented" is not one question. Only the first
was wired into `check.sh` for a while, and the gap is exactly the failure it was meant to catch:
`docs/Log/Logger/` held 18 method pages while none of them was `SetIODriver`, `StopDriverThread` or
`DriverLoop`, so layer 5 read "documented" about a class whose entire IO-driver mechanism lived only in
`///` comments a sweep was about to delete.

| Check | Question it answers | Script | Fails the gate as |
|---|---|---|---|
| Coverage | does every namespace-scope entry own a page, and does the module index link it | `docs_coverage.py check <Module>` | `[DEBT]` — unauthored work |
| Method pages | does every declared method own a page of its own | `docs_methods.py <Module>` | `[DEBT]` — unauthored work |
| Validity | does every page parse, link something real, and use a class the CSS declares | `htmlcheck.py <page ...>` | `[FAIL]` — a defect in what exists |

A page that exists and is broken is a violation, not remaining work: an undeclared CSS class or a dead
anchor is a bug in what was just written, and the reader meets it before anyone reads the ledger.

`docs_coverage.py ledger` regenerates `.Plans/DOCS_COVERAGE.md`, the per-module worklist. Run it before
scheduling a module, not after — its first version counted only `.h` and `.hpp` for the comment column and
so understated `Core` by 295 lines, and a queue built on it sends the next worker to the wrong module.

### Method page naming, and why the checker asks clang

Names come from the clang AST through `layout.py`, because the question is "what can a caller call":
`operator bool` has no name a regex can find, `using TValue = std::function<void(int)>` reads as a call
and is not one, and members of `Engine/Math/VectorCommonImpl.inl` belong to the class that includes them.

File names follow `.Plans/AUTHORING_method_and_class_pages.md` section 2 — that table is the rule and
`docs_methods.py`'s `OPERATOR_PAGES` is its machine-readable copy, so the two are edited together. Both
sides of the comparison are `norm()`-ed, which is worth stating because `norm` deletes hyphens: comparing
the raw `operator-right-shift` against the normalised file list means **no operator page ever matches the
file that satisfies it**, and only single-word pages like `constructors` pass by the accident of having no
hyphen. Three things that bug hid, all found in one sitting:

- Operators were routed to a "check by hand" bucket before `expected_file()` was consulted, so a class
  whose entire API is one conversion — `hbe::EndLine`, which exists to become `"\n"` — required no page at
  all, and a reader could never reach its behaviour.
- clang names a class template's constructor `ConfigParam<T, IsAtomic>`, so taking the last whitespace
  token of the description yields `IsAtomic>` and reports a constructor as a method named `IsAtomic` that
  owns no page. The name is whatever follows the kind word `function`.
- The contract spells `operator[]` as `operator-index.html` and `operator<<` as
  `operator-left-shift.html`; a copy of the table said `subscript` and omitted `<<` entirely, and my own
  Resource pages went out as `operator-shift-left.html` — the third spelling in one tree.

`[[nodiscard]]` on getters, `= delete` and `= default` operators are exempt from needing a page and are
**counted in the summary line**, not dropped: what a class forbids or inherits by default is its ownership
story, which the class page carries, and an exemption nobody can see becomes a way to hide a gap.

### Writing the pages (`docs_page.py`)

`docs_page.py` owns the chrome — the sidebar module list, the `current` marker, the breadcrumb depth, the
prevnext footer, and the method list every page of a class must agree on. It does not own the prose: every
sentence comes from a fragment file you wrote after reading the header, because a tool cannot know what a
function was doing wrong.

```bash
docs_page.py class  String Letter --source Engine/String/Letter.h --summary "…" --sections spec.json
docs_page.py method String Letter is-lower-case --source Engine/String/Letter.h --summary "…" \
              --sections spec.json
docs_page.py renav  String Letter        # after adding a page, re-sync every page's method list
docs_page.py check  String               # htmlcheck over what was written
```

`spec.json` is a list of `{"anchor", "heading", "file"}` (or `"body"` for a short one) in page order, and
the anchors must be the contract's ids. Chrome is lifted from the module's own `index.html` and its
relative paths are deepened by one directory — the failure that proved this necessary was a lifted page
whose stylesheet resolved to `docs/String/assets/`, caught by the validator rather than by a reader.

A page is written to `<name>.html.new`, validated, and moved into place **only if `htmlcheck` accepts it**,
with one tolerated exception: a dead link pointing at a sibling page of the same class that has not been
authored yet, which is the normal state of the first page of a class. The count of tolerated links is
printed, so finishing the class stays a visible obligation.

### Deleting the comments, with proof

The ban is only safe after the prose has somewhere to live, and that ordering used to live in one sentence
of this file plus the operator's memory. It is now a precondition of the tool: `--strip` runs the same two
doc checks the gate runs and **refuses** if the module still owes class or method pages, naming the
command that lists them. `--force` overrides, for the case where prose is genuinely going somewhere other
than a class page — a guide, or nowhere.

```bash
git show HEAD:Engine/Module/Header.h > /tmp/pre_Header.h        # 1. snapshot, before anything
python3 scripts/comments.py Engine/Module/Header.h               # 2. read the findings
python3 scripts/comments.py --strip Engine/Module/Header.h       # 3. delete; refuses if code moved,
                                                                 #    or if an entry here lacks its pages
clang-format --style=file -i Engine/Module/Header.h              # 4. format: an argument comment leaves
                                                                 #    its space behind, `(void* )`
python3 scripts/comments.py code_tokens Engine/Module/Header.h /tmp/pre_Header.h   # 5. prove equality
clang-format --style=file --dry-run --Werror Engine/Module/Header.h                # 6. prove the space
```

Both proofs are needed because they see different things. `code_tokens` compares tokens, so it cannot
see whitespace and passed a header that had become `void (*)(void* )` after `/*userData*/` was deleted;
`check.sh`'s format layer was what actually caught it, and the header had already been committed and
gate-passed. A strip without a format pass afterwards leaves residue the formatter exists to remove.

Then re-read the grouping — layer 4b. The comments that were deleted are what used to separate one concern
from the next, so a header grouped before a strip is not grouped after it.

`code_tokens` is a real C++ tokenizer — comments blanked by the ban's own lexer, string and character
literals opaque, line continuations treated as whitespace, maximal munch — and step 4 compares token
multisets, so the proof is arithmetic rather than a reading of the diff. Two weak proofs came first: a
word-level comparison that claimed drift in 98 files that were byte-identical, and a whitespace-collapsed
one that still flagged 15 because it joined literals *with* their quote characters. The tokenizer's own
docstring example caught a bug in its first version: `++` and `--` were missing from the operator list, so
`a++b` and `a+ +b` tokenised identically.

The refusal is **module-scoped**, and that is a property of the question, not an obstacle: "does this
module's reference cover every entry" is answered per module, so one header/source pair can never
satisfy it alone. The cycle therefore documents the whole module first and deletes in one pass: every pair
reaches step 3 of *The fix cycle, in session*, and only then does a single module-wide pass strip every
file with its token proof. A pair whose module still owes pages leaves its comments in place and reports
the count — a comment that survives costs nothing, while prose deleted before its page exists is gone.

## Layer 6 — build gate (mandatory, last)

Builds `EngineTest`, `VulkanExample` and `WindowExample` across **Dev, Debug and
Release**, plus the `CodingStandards` target. Lint passing means nothing if the
reformat broke compilation.

`CodingStandards` is compiled as a real engine target (see `Engine/CMakeLists.txt`)
precisely so the standard cannot drift from itself again — it belonged to no target
and quietly rotted. `build.sh` cannot reach it (it derives the CMake target from the
basename of an application directory), so the gate invokes `cmake --build` directly.

The touched files are `touch`ed first so ninja genuinely recompiles: without this,
"ninja: no work to do" would report a pass that exercised nothing. Treat a
`no work to do` line as a **suspicious** result and confirm it with
`touch <file>`.

`Examples/MacOSApp` is not covered: `Examples/` is not in the root `CMakeLists.txt`,
so formatting there cannot be compile-verified.

## Proving a commit moved no code (`prove_format.py`)

`clang-format --apply` on 169 files is not reviewable by reading 169 diffs, and "the build passed" does
not prove the formatter only moved whitespace. Run it against a revision and it classifies every file by
token comparison:

```bash
python3 scripts/prove_format.py HEAD~1     # exit 0 when every difference is explained
```

Verdicts: `whitespace` (nothing but layout), `include-order` (a re-sorted include block), `literal-split`
(a long string the formatter broke across lines), `reorder` (declarations moved — legal only for a member
layout commit), `unexplained`, and `reorder` with a lost token is a hard failure. The invariant is stated
once in the tool: nothing may be **lost** from the token multiset, and the only tokens a reorder may add
are access labels, which crossing an access section requires. An earlier draft demanded an identical
multiset and so would have failed the very reorder it was written to verify, and its first permitted set
included `template`, which is not an access label and can carry a change of behaviour.

## The module cycle

One module — or one header inside a module too large to review at once, which is the owner's decision for
`String`, `Core` and `Container` — lands as separate commits, in this order, each with its own proof:

| Commit | Content | Proof required before committing |
|---|---|---|
| `<mod> judgement review` | the twelve rules above, two slicings, citations machine-checked, nothing edited | `verify-findings.py` passes every pass file; `review_merge.py` reports the unique count and the corroboration count — this list is what the next commit works from |
| `<mod> layout and naming` | twelve-block reorder, `m_` and snake_case removal, `[[nodiscard]]`, `explicit`, `= default`, `out`-prefixed write-only parameters, named constants | `layout.py` reports 0 for the files **and** `layout.py --init-order` reports 0, because data moved; the module target compiles; **if the docs already describe the code, do this first**, otherwise pages quote signatures that are about to change |
| `<mod> format` | `check.sh --apply`, or `--fix` for the mechanical subset as well | `prove_format.py HEAD~1 --manifest .Plans/fix-manifest.json` exits 0 with every file explained |
| `<mod> docs` | pages via `docs_page.py`, module index updated, each documented entry's header given its `/// API reference:` pointer, `.Plans/DOCS_COVERAGE.md` regenerated | `docs_coverage.py check <mod>` and `docs_methods.py <mod>` at 0, which includes 0 pages without a pointer; `htmlcheck.py` clean |
| `<mod> comment ban` | `comments.py --strip` | `code_tokens` identical against the pre-strip snapshot; `comments.py` then reports 0 |
| ledger and plan | `.Plans/DOCS_COVERAGE.md`, `JOURNAL.md` | the numbers in them re-measured, not carried forward |

### The fix cycle, in session

A module is fixed one header and its implementation at a time, because that is the largest unit whose
reorder can be checked by reading it. Work the pairs in order; nothing here needs an agent framework.

| Step | Command | What must be true before the next step |
|---|---|---|
| 0 snapshot | `git show HEAD:<file> > /tmp/pre_<name>.h` for each file | the snapshot exists — it is the only copy of the pre-deletion bytes |
| 1 mechanical | `clang-format --style=file -i <files>`; `python3 scripts/autofix.py --manifest .Plans/fix/manifest-<pair>.json <files>`; `clang-format --style=file -i <files>` | the manifest names every token removed, and nothing else was touched |
| 2 layout | `python3 scripts/layout.py <files>`, edit, run it again | 0 findings, **and** `python3 scripts/layout.py --init-order <files>` at 0, because data moved |
| 3 reference | `python3 scripts/docs_coverage.py check <mod>` and `scripts/docs_methods.py <mod>`, then `docs_page.py class` / `method` / `renav` / `check`; then replace the entry's doc block with its `/// API reference:` pointer line | both gates at 0 for the module, `htmlcheck` clean, and no `[NO POINTER]` row for an entry whose page now exists; step 4 is refused per file by `docs_coverage.py check-file <path>` until that file's own entries pass, which is what lets a finished header be cleaned while its neighbours are still owed pages |
| 4 delete | `python3 scripts/comments.py <files>` for the count, then `--strip`, then `comments.py code_tokens <file> /tmp/pre_<name>.h` | token multisets identical per file, then `comments.py` reports 0 |
| 5 build | `.pi/skills/hb-standards/scripts/gate.sh spawn --all --test`, read with `gate.sh wait` | `GATE_EXIT=0`; never a pass read off `ninja: no work to do`. The marker line is echoed by the reviewing agent, not captured by the harness, and `check.sh` exits 0 when its lint scope comes out empty — so a CLEAN verdict with nothing else quoted is not evidence. Re-run `check.sh` yourself and read `$?` before believing one |

Inside step 2, four rules hold: the data block moves as a unit and never re-orders internally; only
functions and types pass data, and only into their own block; a mover re-opens the access of the anchor it
inserted before; and a class whose data members are declared under different `#ifdef` configurations is
reported rather than reordered, because no single order is then provably right. In step 3, if a passage
cannot be placed honestly in a class page or a design document, the comment stays and the reason gets
reported. Step 4 is read by its output, not its exit code: `comments.py` exits 1 whenever a comment is
still present, so the pre-strip count is a nonzero exit by design, and the pass is judged on the token
proof and on the post-strip count.

Three rules the cycle exists to enforce, each learned by being broken:

- **Snapshot before stripping.** The proof of a comment deletion is a token comparison against the exact
  bytes before it, and there is no way to recover that snapshot after the fact — one strip had to be
  undone and redone to get a real baseline.
- **Data member order never changes.** Member order is initialisation order. Only functions and types may
  move past data, and only into their own block.
- **A mover must never decide an access specifier.** Insert a labelled block, then re-open the access of
  the anchor it was inserted before. A mover that inserted `private:` plus members without re-opening
  `public:` produced a file that compiled and then failed three files away with "field of type `Buffer` has
  private default constructor" — after two days of notes said exactly this.

Applications and Examples are not modules: they own no API pages, and `Examples/` is outside the root
`CMakeLists.txt` so **nothing compiles it** — a strip there is provable only by `code_tokens`, never by the
build gate. Their prose goes to the top-level guides (`docs/RunningTests.md`, `docs/VulkanExampleGuide.md`
and siblings), which is the convention the tree already uses for prose that is not API.

## Helper scripts

| Script | What it is for |
|---|---|
| `check.sh` | the six layers plus the build gate; `--staged`, `<rev>`, `--all`, `--apply`, `--fix`, `--test`, `--no-build` |
| `autofix.py` | the five mechanical fixes a script may make, and the manifest declaring every token it removed |
| `gate.sh` | run the whole gate detached, `spawn` / `wait` / `status` / `list` / `release`, with 4 slots |
| `runtest.sh` | build then run `EngineTest` under a wall clock, and report even when it says nothing |
| `hang.sh` | build, run, and if the binary stalls, sample **its** stacks and say where |
| `verify-findings.py` | reject review findings whose cited line does not exist or does not contain the quoted evidence |
| `review_merge.py` | fold two independent review passes into one deduplicated finding set and mark the corroborated findings |
| `prove_format.py` | prove a commit moved no code, per revision; `--manifest` holds a fixer to its declared tokens |
| `docs_page.py` | emit reference pages with correct chrome, validated before kept |

`verify-findings.py` is not decoration. A model asked to audit files it never opened invents defects rather
than reporting none, and the invention is invisible in prose but cheap to detect: the line it "quoted" is
past the end of the file, or the file does not exist. Feeding findings through it turns that observation
into an exit code, so a reviewer that fabricates fails a gate instead of reaching a human.

## Judgement checks a script cannot do

Grep these in the changed hunks and fix by hand:

| Rule | What to look for |
|---|---|
| Log before failing | every early `return`/`Assert` on an error path needs a descriptive `Error:`/`Warning:` log first |
| `[[nodiscard]]` | on getters and value-returning functions |
| `explicit` | on every single-argument constructor |
| `= default` | instead of `{}` for trivial special members |
| `out` / `inOut` / `in` | prefix write-only, read-write, and name-colliding parameters |
| Pointer validation | validate before dereference; prefer references over raw pointers |
| `static_assert` | use it wherever a condition is compile-time evaluable |
| `constexpr` | no magic numbers; constants are PascalCase |
| Composition | prefer has-a; `final` on classes not meant as bases |
| `inline` keyword | redundant on an in-class member definition and on a template; **load-bearing** on a function or operator defined at namespace scope in a header, where dropping it makes every including translation unit emit the symbol and the link fails. Only the AST separates the two, so the lint reports `inline` as advisory |
| No snake_case member | a member is the token before `;`, and a type sits in the same position — `size_t MaxNameLength = 127;` is a type then a PascalCase name, so no grep separates them. The `m_` prefix is checked mechanically; this half of the rule is not |
| `noexcept` | mark only what is provably exception-free; drop it where `new` is called |
| Unit-test guard placement | an `#ifdef __UNIT_TEST__` region sits at the **end** of the file, one region per file |

### Running a judgement review

When the ask is "review this module" rather than "fix this file", the review changes nothing: the fix
cycle is a different activity, and a review that edits files mid-pass loses the distinction between what
was found and what was assumed.

| Step | What it is | Why it is that way |
|---|---|---|
| Scope | `git ls-files 'Engine/<mod>/*.h' 'Engine/<mod>/*.cpp'` | never a bare `find`: the tree carries docs and vendored `External/` sources that are not the engine's to review |
| Slice twice, differently | once per header/source pair, once by line budget (~2,700 lines a single reader holds honestly) | the slices are not redundant. A pair-slice sees a getter's whole contract; a budget-slice sees a 1,500-line implementation no pair split hands to one reader |
| Read every file in full | open it, do not grep it | rules 1, 4, 6 and 9 are invisible in an excerpt, so a grep-only pass is not a review |
| Cite or drop | file, line, the verbatim text of that line, one sentence of why, the concrete edit, a confidence | `verify-findings.py <file.json>` rejects a finding whose file is missing, whose line is out of range, or whose evidence does not appear on or beside the cited line |
| Merge | `python3 scripts/review_merge.py <dir>` | folds free-text rule names onto rule numbers, deduplicates by (file, line, rule), keeps the highest-confidence wording, marks a finding corroborated when both slices cited it |

**Do not report**: anything clang-format fixes (braces, indentation, spacing, blank-line *counts*,
include order); comment presence in `.cpp` files, which a scheduled migration owns; namespace bodies
indented at column 1,
which is owner-confirmed legacy debt; speculative refactors, performance opinions, "consider renaming".
A run that fills itself with those displaces the findings that were the point of the run.

Blank lines carry one exception to that exclusion, because the formatter caps them but never places them:
a declaration block whose members are separated by *no* consistent grouping — one concern indistinguishable
from the next — is a readability finding worth reporting, and fixing it is whitespace only, so it is safe to
hand to a reader. The reverse is a real hazard: `comments.py --strip` deletes the comment-only lines that used
to sit between concerns, so a strip can flatten grouping that was there before it. Re-check grouping after
stripping a header, not only before.

**Exempt from the behavioural rules**: `Engine/CodingStandards.{h,cpp}`, whose job is to break them
legibly, and `Applications/EngineTest/TestMain.cpp`, whose `__UNIT_TEST__` guard is intentionally `main()`'s
body.

**Zero findings is a correct answer**, for a file and for a slice. This is stated rather than implied
because a reviewer told to produce findings will produce them, and the citation gate exists precisely
because a previous run invented defects in files it had not opened.

Independence, stated plainly: a session that reads its own earlier findings is not a second reviewer.
Write each pass to its own JSON and do not open the other pass's output until `review_merge.py` has
already folded them — otherwise the second pass is recalling the first, and the corroboration count is
measuring memory instead of agreement.

Measured result for Engine/Core, the run this procedure came out of: 46 files, 177 input findings folded
to 133 unique, 44 corroborated by both slices, confidence 67 high / 50 medium / 10 low / 6 field-omitted,
and 11 files clean — the last number as much a result as the first, because it says where a fix cycle may
start.

## Traps already paid for — do not relearn them

- **A ban on `inline` cannot be a grep.** Stage 0 raised this rule from advisory to FAIL on the strength
  of the standard's sentence "do not use the `inline` keyword explicitly". Applying it would have
  deleted the keyword from `Assert`, `FatalAssert`, `IdentityMatrix` and `operator<<` for `ComponentState`
  — namespace-scope definitions in headers, where `inline` is the only thing separating a working build
  from a duplicate-symbol link error. 16 of the 18 sites in the tree are that shape; exactly one, a
  constructor defined inside `Exception`, was noise. The standard's sentence is now qualified, and the
  layer is advisory again.

- **Member layout is not greppable.** All four of these were mis-classified by the first
  prototype of `layout.py`: `std::function<void(int)> cb;` is data containing parentheses;
  `using TLogFunc = std::function<void(std::ostream&)>;` is a type that reads as a call;
  `explicit operator bool() const` is a function with no name; and an unnamed `union` in a class
  body is a **data member written in place**, not a nested type — sorting it as block 0 tells you
  to hoist `Engine/Math/Vector3.h`'s union above its constants.
- **The AST dump needs a qualified filter.** `-ast-dump-filter=hbe::` against
  `Engine/Memory/MemoryManager.h` dumps 1 MB in 0.3 s; unfiltered it dumps **624 MB**. The filter
  matches the qualified name, so derive it from the file's own namespace.
- **clang's JSON emits one document per filtered declaration.** `json.loads` takes one document
  and raises `Extra data` at the second root; decode the stream root by root with `raw_decode`.
- **A templated class has no `file` on its definition node.** clang puts it on the enclosing
  `ClassTemplateDecl` only. Filtering on a node's own `loc.file` silently skips *every template in
  the engine* — measured on `Engine/Core/ScopedLock.h` and `Engine/Math/Vector3.h`, both reported
  "no class body". Inherit the file from the nearest ancestor that has one.
- **`#include`d `.inl` members belong to the class but not to the includer's line numbers.**
  `VectorCommonImpl.inl` and `MatrixCommonImpl.inl` are included *inside* a class body; clang
  marks their locs with `includedFrom`, and the line is relative to the `.inl`. Attributing them
  to the includer reports line 242 of a 151-line file. Resolve the real file by proving the
  declaration is on that line of a candidate include, and say so when you cannot prove it.
- **A `#ifdef __UNIT_TEST__` file compiles to nothing under Dev**, and clang then exits 0 having
  dumped zero declarations. That is not a pass. Re-run with `-D__UNIT_TEST__=1` and report which
  macros were active — `Engine/Renderer/RendererTest.h` and three Math sources need it.
- **The doc gate must check three things, not one.** `docs_coverage.py` asks whether a page exists; it
  cannot tell whether the page says anything about the method it names. Wiring only that one left `Log`
  passing with 18 method pages covering none of the driver-thread methods. Add `docs_methods.py` and
  `htmlcheck.py` to layer 5 and keep their exit codes distinct: unauthored pages are backlog, a broken page
  is a violation.

- **`htmlcheck` must count a page's own `<style>` block as a declaration.** Only reading the shared
  stylesheet reported seven styled blocks in `docs/RendererDesign.html` as undeclared classes, which
  teaches a reader to ignore the tool. A class consumed by a library rather than CSS — `mermaid` — is
  declared in the shared stylesheet with the reason written next to it, so the rule keeps meaning "nobody
  renders this".

- **A ledger column is a measurement, not a constant.** The comment column of `docs_coverage.py ledger`
  walked `.h`/`.hpp` only, which understated `Core` by 295 lines, and the same file still carried counts
  for two modules stripped weeks earlier. Re-measure before scheduling work from it; a plausible number is
  still a guess until something produces it.

- **Do not trust your own earlier claim that a check runs end to end.** `expected_file()` was verified with
  a unit test on its own inputs and reported as "the gate now requires a conversion page"; the caller
  routed every operator past it. Verify the whole path — generate the page, run the checker, read the
  verdict.

- **`docs_coverage.py` namespace detection needs the newline.** A pending-declaration buffer that
  drops newlines reads `...h"namespace hbe` and the pattern for a namespace brace can no longer
  see the keyword, so every class looked nested. Keep a separator when you strip whitespace.

- **`.mm` / `.m` are excluded.** clang-format classifies them as Objective-C, the
  repo config declares only `Language: Cpp`, so it aborts with exit 1 and writes
  nothing. Run where the config is *not* found, it silently rewrites them with
  LLVM defaults instead (measured 2249 → 2400 bytes, exit 0). Never add them to a
  `clang-format -i` batch without a `Language: ObjC` section in `.clang-format`.
- **`.inl` are excluded.** `MatrixCommonImpl.inl` / `VectorCommonImpl.inl` are
  `#include`d *inside a class body inside a namespace*; their one-tab indentation
  comes from the includer, so formatting them standalone de-indents everything.
- **Generated headers are skipped** when line 1-3 says auto-generated / do not edit
  (e.g. `Engine/Renderer/Vulkan/ShadersSpv.h`).
- **`AllowShortFunctionsOnASingleLine` must stay `None`.** With `Empty`, setting
  `SplitEmptyFunction: true` changes nothing at all — `Empty` wins and rejoins the
  braces.
- **`BraceWrapping` under a named `BreakBeforeBraces` is silently ignored.** Any new
  brace rule goes in the `Custom` table.
- **A `concept` body has no wrapping knob.** clang-format 22 wants `concept C = requires(T t) {`
  attached, and `BraceWrapping: {AfterConcept: true}` is rejected outright — `unknown key`. The one
  engine-wide exception to a broken opening brace, recorded in `docs/CodingStandards.md`.
- **Blank lines: the formatter is a ceiling, and one kind of floor.** `MaxEmptyLinesToKeep: 2` means
  three or more collapse to two — between data members, before a function, before a comment — and one or
  two both survive in every one of those positions. `SeparateDefinitionBlocks: Always` is the exception
  that inserts: a blank appears between definition blocks whether you wanted one or not. Grouping is
  otherwise invisible to it and a badly grouped header passes layer 1. An earlier version of this bullet
  claimed two never survives after includes and that `docs/CodingStandards.md` was stale for permitting
  two; both were wrong, and the standard now carries the measured behaviour.
- **Probe clang-format through stdin, never a file outside the tree.** The style comes from the
  directory of the file it is handed, so a probe in `/tmp` runs LLVM defaults while looking exactly like
  a verified result. Two wrong claims reached the standard through that route; `clang-format --style=file -`
  with the input on stdin uses the working tree's config and is the only form to trust.
- **`build.sh` takes `-test`, not `-notest`** as `AGENTS.md` and
  `docs/HelperScript.md` claim. Without `-test`, `__UNIT_TEST__` is undefined and
  `TestMain.cpp` compiles to an empty `main`, so the test sources are never
  compiled — pass `-test` when the gate is meant to cover them.

## Attributing a gate failure

The gate builds the whole tree, so it can fail on work that has nothing to do with
the commit being checked — including someone else's uncommitted edits in the same
working tree. Before "fixing" anything, decide whose breakage it is:

```bash
git status --short                                    # uncommitted edits?
git show HEAD:<file> | grep -c <missingSymbol>        # present at HEAD but not on disk?
grep -rln <missingSymbol> Engine/                     # who still references it
```

If the symbol exists at HEAD and is gone on disk, a refactor is mid-flight: report
it, do not repair it inside a formatting task. A style change confined to
`.clang-format` and `Engine/CodingStandards.*` cannot break compilation at all —
those files are referenced by no CMakeLists target, which is worth knowing before
spending time on a gate failure they caused.

## Known debt surfaced, not gated

`NamespaceIndentation` is **confirmed `None`** by the owner (2026-09-06) — this is
no longer an open question. ~218 engine files are written indented and are legacy debt awaiting a sweep. The script reports them as `[DEBT]` and does not fail on them: failing every commit that happens to touch one of those files would block
unrelated work. When you `--apply` to such a file, expect its namespace body to be de-indented to column 0 as part of bringing that file into conformance — that is the rule working, not collateral damage.

`docs_coverage.py check` returns **3** when no ledger exists, printing `[NONE]`: a check that could
not run is a refusal, not a pass, and `comments.py --strip` now depends on that answer. It used to
return 0, which is the same shape as "nothing is owed" — measured on this tree, `--strip` would have
read an unmeasured tree as fully documented. The module-name argument already refused the path form
for the same reason; this closes the other way the same lie could be told.

The owner scheduled the whole-tree sweep on 2026-09-29, module by module, each in its own commit
separate from any reordering commit. When the last module lands, flip this layer from `[DEBT]` to
`[FAIL]` — until then it stays advisory precisely so unrelated work can still be committed.

## Reporting

Report the count per layer — grep failures, advisories, sweep backlog — the
three-configuration build result, and anything left as `[DEBT]` or `[WARN]`. On a fix run, report what
was **refused** as well as what was changed: a refusal is a decision somebody made, and a diff cannot
show the files that were deliberately left alone. Never claim the gate passed on the strength
of a "no work to do" build. Never push without explicit permission.
