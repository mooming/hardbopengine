# PLAN: bring Engine/Test into the HardBop Engine coding standards

Status: **paused before execution**. Measured at HEAD `25e972e` on 2026-10-01, owner decision was to wait
for the concurrent session to finish before any of this runs. No file under `Engine/` was modified.

## Why it is paused

A concurrent session owns this tree. Measured at `25e972e`, `Engine/Core/TaskSystem.h:65` carried an
uncommitted `std::atomic<std::x`size_t>` that made every `-fsyntax-only` and the whole layer 6 gate fail, and
`Applications/EngineTest/TestMain.cpp` carried an uncommitted blank line. **Both are gone as of `8fc0c16`** —
the header reads `std::atomic<std::size_t>` again and both preprocessor halves of `TestMain.cpp` now pass
`-fsyntax-only`. The blank line went with the `clang-format` run described at the end of this file.

That is the reason to pause rather than the reason to stop: the session is still live. During this one task
HEAD moved `bc3bfbf` → `25e972e` → `8fc0c16`, `layout.py`, `prove_format.py` and the new `autofix.py` were all
rewritten inside an hour, and `SKILL.md` plus `.pi/workflows/hb-fix-pairwise.js` are modified and untracked on
disk right now. Anything below that came from a script has to be re-measured with that script's current
version, because `layout.py` changed *after* the 32-finding count in the next table was taken.

## Re-measure before resuming — nothing here is a constant

```bash
git status --short && git log --oneline -3          # whose edits are in the tree, did HEAD move
python3 .pi/skills/hb-standards/scripts/layout.py Engine/Test/TestCollection.h
python3 .pi/skills/hb-standards/scripts/layout.py Engine/Test/TestEnv.h
python3 .pi/skills/hb-standards/scripts/layout.py Engine/Test/Testlet.h
python3 .pi/skills/hb-standards/scripts/docs_coverage.py check Test
python3 .pi/skills/hb-standards/scripts/docs_methods.py Test
clang-format --dry-run -Werror Engine/Test/*.h Engine/Test/*.cpp Applications/EngineTest/TestMain.cpp
```

## Layer state at 2026-10-01

| Layer | Result | Detail |
|---|---|---|
| 1 clang-format | clean | 0 violations in the 7 Test sources |
| 2 mechanical greps | clean | 0 findings tree-wide; the 4 `inline` advisories are in Core, Math, Renderer |
| 3 comment ban | **126 lines** | TestEnv.h 42, TestCollection.h 40, Testlet.h 17, UnitTestCollection.h 10, UnitTestCollection.cpp 9, TestCollection.cpp 5, TestEnv.cpp 3 |
| 4 member layout | **32 findings** | TestCollection.h 14, TestEnv.h 14, Testlet.h 4 |
| 5 reference | **1 class page + 21 method pages missing** | `docs_coverage.py check Test` and `docs_methods.py Test` both exit 1 |
| 6 build gate | never run | never attempted; the sweep stopped before commit 1 |

## Decisions already made — do not ask again

**Block 0 versus block 1 in `TestCollection`.** `using TTestlet = Testlet<MaxClosureBytes>;` cannot satisfy
"types head the class" because a `using` alias has no complete-class context, and `layout.py` has no
exemption mechanism. Owner decision: `using TTestlet = Testlet<>;` inside the class, and **delete
`MaxClosureBytes`**. `Testlet`'s template parameter already carries `= 48` at `Engine/Test/Testlet.h:63`,
so this leaves exactly one `48` in the tree; `Testlet<48>` would put a second independent copy of that
number in a different header. `MaxTestNameBytes = TTestlet::MaxNameBytes` stays — it depends on a type, so
block 1 is legal for it. Nothing outside `TestCollection.h` referenced `MaxClosureBytes`;
`docs/Test/TestCollection/index.html` quotes it in the properties table and must lose that row.

**Four extra fixes, all approved by the owner** — none is fixed by any of the six layers:

| Item | Site | Fix |
|---|---|---|
| Dead code | `TestCollection.cpp` `#if 0` block, ~20 unreachable lines of `operator<<` body | delete; the only comment the ban had left in it was `// Add prefix for the current test case.` |
| Log before failing | `TestCollection::RunTestAt` returns false on `testIndex >= tests.size()` silently; `GetTestName` returns `""` on the same index silently | add a descriptive error log on each, matching `TestEnv::RunTestlet` and `Testlet::Run`, which already log |
| Latent defect | `TestEnv::AddTestCollection` calls `std::forward(args)...` with no template argument, so it is ill-formed the moment anyone passes a constructor argument. It compiles today only because all 60 call sites are `AddTestCollection<XxxTest>()` with an empty pack | `std::forward<Types&&>(args)...`, proven by a compile probe that instantiates it with one argument — grep proves a name exists, only a compiler proves what it does |
| Formatting | `Applications/EngineTest/TestMain.cpp` stray blank line | done 2026-10-01 |

## The five commits, in this order

| Commit | Content | Proof required before committing |
|---|---|---|
| `Test layout and naming` | the 32 layout findings, the four fixes above, delete `MaxClosureBytes` | `layout.py` reports 0 for all three headers; `Test` and `EngineTest` compile in Dev |
| `Test docs` | 22 pages via `docs_page.py`, `renav`, class-page method tables, module index row for `TestletDispatch` | `docs_coverage.py check Test` 0, `docs_methods.py Test` 0, `htmlcheck.py` clean |
| `Test format` | `clang-format --apply` on the 7 sources only | `prove_format.py HEAD~1` exits 0 with every file classified `whitespace` |
| `Test comment ban` | `comments.py --strip` | snapshot first, `code_tokens` identical against it, `comments.py` then reports 0 |
| ledger and journal | `.Plans/DOCS_COVERAGE.md`, `JOURNAL.md` | numbers re-measured, not carried forward from this file |

Then the gate: `check.sh --test`, Dev, Debug and Release, plus `EngineTest` under a wall clock.

## The 22 pages `Test docs` owes

| Class | Pages |
|---|---|
| `TestletDispatch` | `index.html` — the namespace-scope POD of three function pointers, `invoke`, `destroy`, `relocate`, one `constexpr` instance per closure type |
| `Testlet` | `constructors.html`, `destructor.html`, `run.html`, `get-name.html` |
| `TestCollection` | `get-test-count.html`, `get-test-name.html`, `get-global-allocation-bytes.html`, `get-testlet-count-with-global-allocations.html`, `report-null-test-case.html`, `report.html` |
| `TestEnv` | `get-testlet-count.html`, `get-executed-testlet-count.html`, `get-testlet-label.html`, `get-global-allocation-bytes.html`, `get-global-allocation-count.html`, `get-testlet-count-with-global-allocations.html`, `get-collection-count.html`, `note-testlet-repeated.html`, `note-suite-driven-by-shutdown-pump.html`, `finalize-collection.html`, `report.html` |

`--strip` refuses to run while either doc check is non-zero, so the docs commit is a hard prerequisite of
the strip, not a nice-to-have.

## Already known stale, deliberately — not gaps to fix

`docs/Test/TestCollection/index.html` still states the old `Testlet` constructor signature, dropping
`std::size_t retainedByteCeiling`, and `get-name.html` still cites `Logger::Get(GetName())` through a
`Start` page it labels "(removed)". Both belong to the `Test docs` commit, not to a separate repair.

## One change already landed, and its proof

`Applications/EngineTest/TestMain.cpp`: the two multi-line `std::cerr` calls became 16 one-per-line
statements with `std::endl` replacing every `\n`. clang-format clean, longest line 117 columns, and the
bytes it prints are unchanged — reducing every literal body and every `std::endl` in both versions to the
text they stream gives 1145 chars on each side. `prove_format.py HEAD` correctly **refuses** this file: the
literals lost their `\n` and `std::endl` was added, so it is a code change and must never ride in a `format`
commit.

Compile-verified both ways once the Core header was repaired:

```bash
c++ -std=c++2b -fsyntax-only -I. -IEngine -Iinclude Applications/EngineTest/TestMain.cpp
c++ -std=c++2b -fsyntax-only -D__UNIT_TEST__ -D__DEBUG__ \
    -I. -IEngine -Iinclude -IEngine/Renderer -IExternal Applications/EngineTest/TestMain.cpp
```

The first is the one that matters for the fourteen new statements: without `__UNIT_TEST__` they are the active
branch, and with it they are preprocessed away, so a normal `-test` build never looks at them at all. The
second proves the split two-liner compiles in the configuration that actually runs.
