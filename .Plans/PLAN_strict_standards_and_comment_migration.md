# Plan: strict coding-standards conformance, twelve-block member layout, comment ban with docs migration

Approved by the owner across five decisions on 2026-09-29. Nothing in this file is assumed.

## 1. Goal, as confirmed

Every tracked C++ source under `Engine/` and `Applications/` obeys `docs/CodingStandards.md`
strictly, under a standard that has been extended with two new rules:

1. **Twelve-block member layout** — data layer visible as one block, separated from the function layer.
2. **No comments in source files** — `.h` and `.cpp` alike. Prose moves to the HTML reference under `docs/`.

Scope: 270 tracked `Engine/` sources + 2 `Applications/` sources. `Examples/` is read-only advisory
(it is outside the root `CMakeLists.txt`, so nothing there can be compile-verified).

## 2. Confirmed decisions

| # | Decision | Chosen |
|---|---|---|
| 1 | Class layout | Twelve blocks: types, then all data, then all functions |
| 2 | Deliverable | Verify **and fix**, commit per module |
| 3 | File scope | `Engine/` + `Applications/` |
| 4 | Worker shape | One subagent per directory, files visited inside it, strictly one worker at a time |
| 5 | `NamespaceIndentation: None` on 109 legacy files | Sweep now, separate commit per module |
| 6 | `m_` prefix (24 files) and explicit `inline` (5 files) | Fix both fully |
| 7 | Comment ban vs 2514 lines of contract | Migrate docs per module, then strip that module |
| 8 | Enforcement | The `hb-standards` skill gains the checks, so the rules are enforced and not merely written down |

## 3. The canonical layout

Types are not data and not functions. They sit at the top so every later declaration can name them.

| Block | Contents | Example |
|---|---|---|
| 0 | Nested types, aliases, enumerators, `static_assert` on those types, ordered public → protected → private | `using TId = TAllocatorID;` |
| 1 | public static variables | `static constexpr TId SystemAllocatorID = 0;` |
| 2 | public variables | `uint32_t id;` |
| 3 | protected static variables | `static constexpr int MaxRetry;` |
| 4 | protected variables | `MemoryManager* owner;` |
| 5 | private static variables | `static const char* Tag;` |
| 6 | private variables | `AllocatorProxy allocators[MaxNumAllocators];` |
| 7 | public static functions | `static MemoryManager& GetInstance();` |
| 8 | public functions, including constructors, destructors, assignment and operators | `void PostEngineInit() noexcept;` |
| 9 | protected static functions | `static int Clamp(int v);` |
| 10 | protected functions | `void LogContext() const;` |
| 11 | private static functions | `static bool IsValid(TId id);` |
| 12 | private functions | `void RegisterSystemAllocator();` |
| end | `friend` declarations | `friend class AllocatorScope;` |

Applied to every `class` and `struct` body, at every nesting depth.

### Safety invariants that are not negotiable

| Invariant | Why |
|---|---|
| The relative order of data members with respect to each other never changes | C++ initialises non-static data members in declaration order. Preserving their subsequence order makes the move provably semantics-preserving. Moving them relative to each other is forbidden, and this is the only way a layout fix can silently change runtime behaviour |
| A nested type used by a data member or a signature moves to block 0 | Otherwise the declaration names an undeclared type and the build breaks |
| A doc comment, an `#if` guard and a `template <...>` header travel with the member they belong to | A moved comment is a lie about a different member |
| `.inl` files stay excluded from the formatter | `MatrixCommonImpl.inl` and `VectorCommonImpl.inl` are `#include`d inside a class body; formatting them standalone de-indents the whole file. Their `#include` directive is placed where the class's function layer begins |
| `Engine/CodingStandards.{h,cpp}` GOOD EXAMPLE is rewritten to the new layout; its BAD EXAMPLE blocks stay bad on purpose | The standard must demonstrate itself |

## 4. The comment ban, stated precisely

Forbidden in `.h` and `.cpp`: `///` doc comments, block comments, prose `//` comments,
`TODO`, commented-out code.

Exempt, and this list is exhaustive:

| Exemption | Reason |
|---|---|
| `// Copyright (c) … Hansol Park` on line 1 | Legal notice, not documentation |
| Structural label on a closing line: `#endif // PROFILE_ENABLED`, `#else // !__DEBUG__`, `} // namespace hbe` | Names only the construct that line closes. Already sanctioned by `AGENTS.md` |
| `// hb-standards:ignore` on a line | Tool directive, not prose. Same category as a formatter switch. Alternative considered and rejected: a sidecar `file:line` ignore list, which rots the moment a line is inserted |
| `Engine/CodingStandards.cpp` and the BAD EXAMPLE blocks of `Engine/CodingStandards.h` | The rule's own teaching exemplar; `check.sh` already exempts it from behavioural checks |

Consequence for the standard itself: the existing rule "user-facing contract goes in the paired
`.h`" is **replaced**. User-facing contract now lives in `docs/<Module>/<Class>/…`; implementation
notes live in `docs/design/<Name>_Design.html`.

## 5. Stage 0 — revise the standard and the skill (me, inline, before any sweep)

A rule nothing enforces decays. Measured today, the mechanisms exist:

| Check | Mechanism | Measured cost |
|---|---|---|
| Comment ban | `clang -Xclang -dump-tokens` emits `comment` tokens from the real lexer, so `"http://x"` in a string is never mistaken for a comment. Falls back to a lexical scanner for files with no compile command | ~0.3 s per file |
| Twelve-block layout | `clang -Xclang -ast-dump=json -ast-dump-filter=<Class>` yields `FieldDecl` / `CXXMethodDecl` in source order with access specifiers. `MemoryManager`: 14 fields, 52 methods | 0.3 s per class |
| Docs coverage | Structural: every entry in a module has `docs/<Module>/<Entry>/index.html`, linked from the module page, and `docs/index.html` links every module page and design document | Instant |
| `m_` prefix, explicit `inline` | Already greps, currently `[WARN]` | flip to `[FAIL]` |

Compile commands come from `cmake-build-debug/compile_commands.json` (134 translation units).
Files with no entry — `Linux*`, `Windows*`, `Win32*` on this host — report `[SKIP]`, never a false pass.

Stage 0 deliverables:

| File | Change |
|---|---|
| `docs/CodingStandards.md` | Rewrite the Comments section for the ban; replace the Member Ordering bullet with the twelve-block table |
| `AGENTS.md` | "It allows brief comments in header files only" is now false; rewrite the Self Documented Code section, and record the layout |
| `.pi/skills/hb-standards/scripts/check.sh` | Add the three checks above, flip the two advisories to failures |
| `.pi/skills/hb-standards/SKILL.md` | Document the new layers, the `[SKIP]` semantics, and the traps found while building them |
| `.Plans/DOCS_COVERAGE.md` | New ledger: every entry in every module, its docs status, its comment count. This is the object the coverage check reads |
| `JOURNAL.md` | Timestamped decision record |

Stage 0 is committed before any source file is touched, so the sweep is measured by a tool that
already embodies the rules.

## 6. Stage 1 — per-module cycle, one worker at a time

Measured 2026-09-29 after the checkers were fixed, so these are the real figures and not estimates:
comment lines are those the ban requires to move, layout findings come from the clang AST, docs
gaps are namespace-scope API entries with no page.

| Order | Module | Status | API pages missing | comment lines to move | layout findings |
|---|---|---|---|---|---|
| 1 | HSTL | **complete** `8854177` | 0 | 0 | 0 |
| 2 | Config | **complete** `914cad4` | 0 | 0 | 0 |
| 3 | Log | **complete** `8f1684a` | 0 | 0 | 0 |
| 4 | Engine | **complete** `5892e98` | 0 | 0 | 0 |
| 5 | Resource | pending | 2 | 8 | 4 |
| 6 | String | pending | 8 | 10 | 8 |
| 7 | Container | pending | 1 | 51 | 12 |
| 8 | OSAL | pending | 5 | 46 | 10 |
| 9 | Test | pending | 1 | 109 | 3 |
| 10 | Renderer | pending | 0 | 157 | 3 |
| 11 | Math | pending | 16 | 93 | 16 |
| 12 | Memory | pending | 17 | 98 | 19 |
| 13 | Core | pending | 22 | 1178 | 22 |
| 14 | Applications | pending | 0 | 26 | 0 |

**A fifth column the table above cannot show: method pages.** `docs_coverage.py` only asks whether a
class owns a page, so an entry can be "documented" with 18 pages that omit three methods. Measured with
`docs_methods.py` on 2026-09-30, **160 methods have no page**: Container 112, Test 21, Renderer 18, Log 3
(since written), Engine 2 (since written), Config 1, Resource 1. Math, Memory, String and OSAL report
zero — not because they are covered but because they own no class directory for the script to compare
against, so their method debt is unmeasured and at least equal to their method count. A module's comment
sweep cannot pass until this column is zero for it, because comments are the only place those contracts
currently live.

The modules are ordered by cost, cheapest complete cycle first, so the pipeline is proven on the
small ones and the documentation-heavy ones are the last thing standing if this stops.

Within a module, one subagent per batch of ≤ 16 files, batches run strictly one at a time.
`Engine/Core` is 46 files, so it is more than one batch; the module still gets four commits.

Subagent contract: `.Plans/STANDARD_WORKER_BRIEF.md`. Read the whole file, edit only the assigned
files, never run the formatter (the orchestrator owns the mechanical layer), never build, never
push, return a per-file verdict.

## 6a. Four commits per module, in this order

| Commit | Content | Verification before it is allowed |
|---|---|---|
| `<module> docs` | Write/update `docs/<Module>/…` from the **header**, per `.Plans/AUTHORING_method_and_class_pages.md`. One page per class, one page per method name | Docs coverage check passes for that module; HTML validator from the authoring contract passes |
| `<module> format` | `clang-format --apply` only. Includes the namespace de-indent | `check.sh --staged --no-build` clean; line-delta is whitespace and braces only |
| `<module> comment ban` | Delete comments. Pure deletions | The module's docs pages exist and were reviewed; diff is deletions only |
| `<module> layout and naming` | Twelve-block reorder, `m_` renames, `inline` removal, and the judgement rules: `[[nodiscard]]`, `explicit`, `= default`, `out`/`inOut`/`in`, log-before-return, `final`, `noexcept`, `static_assert`, composition | `check.sh --staged --no-build` clean **and** that module's library target compiles in Dev |

Module order, cheapest complete cycle first, so the pipeline is proven on the small ones. These are
measured numbers from after the mechanical sweep, not estimates: comment lines from `comments.py` per
module, layout findings from `layout.py` over the whole tree, page counts from `docs_coverage.py` and
`docs_methods.py` against the AST.

| Order | Module | Comment lines | Layout findings | Class pages missing | Method pages missing |
|---|---|---|---|---|---|
| — | Config, Engine, HSTL, Log, **Resource** | 0 | 0 | 0 | 0 |
| 1 | String | 19 | 11 | 8 | 0 |
| 2 | OSAL | 105 | 14 | 9 | 0 |
| 3 | Math | 132 | 26 | 16 | 0 |
| 4 | Memory | 149 | 23 | 19 | 0 |
| 5 | Test | 126 | 32 | 1 | 21 |
| 6 | Renderer | 265 | 44 | 0 | 18 |
| 7 | Core | 1,476 | 62 | 23 | 2 |
| 8 | Container | 60 | 59 | 2 | 112 |
| 9 | Applications + Examples | 297 | 0 | 0 | 0 |

Resource is the first module taken end to end through the full four-commit cycle after the mechanical
sweep, and it earned the "cheapest" label in a way the estimates did not predict: 11 comment lines and 20
layout findings, but documentation authoring found three behavioural defects that no test reports — the
reader's bounds guard ignoring its argument, a string overload that reads nothing, and a container cleared
before the read is verified. The guard and the container were fixed in `cc6c4ea`, each proven by a testlet
that fails against the previous code; the string overload remains an owner decision. Cheapest to migrate is not cheapest to understand.

Three readings this table forces:

- **Math, Memory, OSAL and String report 0 method pages because they own no class directory under
  `docs/`, not because their methods are covered.** `docs_methods.py` can only look for a page inside
  `docs/<Module>/<Class>/`; with no class directory there is nothing to search, so absence of findings
  is absence of measurement. Their 16, 19, 9 and 8 missing class pages are the real work, and the
  method tally stays unknown until those directories exist.
- **Container is 114 pages while holding only 60 comment lines.** It is by far the worst ratio in the
  tree, and the reason is that its contracts live in inline template code the class page cannot carry.
  It goes last not because it is small but because nothing else in the tree benefits from the authoring
  pattern that 112 pages would establish.
- **Core is 1,476 comment lines, 2.5x the next module.** One module holds more prose than the other
  eight combined, so the plan's original 14-step order was wrong in more than detail.

`Applications` and `Examples` are 297 comment lines with no API pages to write, and they are outside the
ledger, which walks `Engine/` only. Examples is additionally outside the root `CMakeLists`, so nothing
compiles it — a strip there cannot be verified by the build gate, only by token identity.

| Order | Module | Entries needing docs pages | Sources | Reorder violations |
|---|---|---|---|---|
| 1 | HSTL | ~0 | 5 | 0 |
| 2 | Log | 1 | 8 | 3 |
| 3 | Test | 1 | 7 | 3 |
| 4 | Config | 1 | 9 | 3 |
| 5 | Resource | 9 | 13 | 4 |
| 6 | Engine | 1 | 3 | 1 |
| 7 | Renderer | 1 | 13 | 3 |
| 8 | String | 17 | 15 | 8 |
| 9 | Container | 18 | 20 | 12 |
| 10 | OSAL | 17 | 53 | 10 |
| 11 | Math | 31 | 37+2 `.inl` | 15 |
| 12 | Memory | 39 | 39 | 19 |
| 13 | Core | 44 | 46 | 22 |
| 14 | Applications | 0 | 2 | 0 |

Within a module, one subagent per batch of ≤ 16 files, batches run strictly one at a time.
`Engine/Core` is 46 files, so it is three sequential batches; the module still gets four commits.

Subagent contract: read the whole file, report per-file findings, edit only the assigned files,
never run the formatter (the orchestrator owns the mechanical layer), never push,
return a structured verdict per file.

## 7. Verification gates

| Gate | When | Command |
|---|---|---|
| Mechanical lint | Every commit | `.pi/skills/hb-standards/scripts/check.sh --staged --no-build` |
| Module compile | Layout and naming commits | `cmake --build build --config Dev --target <Module>` |
| HTML validity | Docs commits | validator block from `.Plans/AUTHORING_method_and_class_pages.md` |
| Whole-tree lint | End of each module | `check.sh --all --no-build`, expecting the violation count to fall monotonically |
| Full build gate, once per 3 modules and at the end | Proves nothing was broken by the sweep | `check.sh --all --test` — Dev, Debug, Release, plus `EngineTest` |

Maximum 10 fix-and-reverify iterations per module, then stop and report.

### What Stage 0 and the first cycle proved, before a second module was started

| Finding | Where it landed |
|---|---|
| `comments.py` judged a structural label by its text, so it rejected the rule book's own example and would have accepted `// TODO`; separately, it parsed every C++ `if` as a preprocessor guard | fixed in `3d44cb1`, false findings 3035 → 2723 |
| `layout.py` reported "5 clean" for a module whose two class bodies it had never seen: no `ClassTemplateSpecializationDecl`, one filtered pass, retry only on a wholly empty dump | fixed in `3d44cb1`, `fcaa0b7` |
| A worker that built three configurations and diffed `-O2` assembly to justify collapsing four blank lines | brief now says where a worker's job ends |
| The HSTL worker's two disputes were both correct | the checkers were wrong, not the module. Fixed rather than the files being bent to fit them |

## 8. Honest scale, measured, not estimated

| Work item | Measured |
|---|---|
| Sources in scope | 272 |
| Files not clang-format-conformant today | 183, of which 109 are namespace-indent debt |
| Files violating the twelve-block layout | 104 of 133 files that contain a class body |
| Comment lines to migrate or delete | 1787 `///` + 727 `//` = 2514 |
| Entries needing a docs page | ~170 of ~217 |
| HTML pages today | 256, at one page per class and one per method name |
| HTML pages at full fidelity | order of 1100 to 1200 |
| Wall clock, sequential, at 2–5 minutes per subagent run | docs authoring alone is 8 to 15 hours; the whole job exceeds one session |

Therefore this plan is executed in checkpoints, module by module, and reported at every module
boundary. It is explicitly better to report "7 of 14 modules complete, the rest are pending and
here is the ledger" than to claim completion. Stage 0 and the first complete module (HSTL or Log)
are the pipeline proof: if that cycle does not produce a clean four-commit result, the plan is
revised before another module is spent on it.

## 9. Out of scope, stated so nobody drifts

- Renaming public API symbols beyond the `m_` prefix removal.
- Any behavioural change. A layout fix that changes behaviour is a defect, not a fix.
- `Examples/` edits, and any `.mm` / `.m` formatting (the config declares `Language: Cpp` only).
- Adding new engine features, or "while I am here" cleanups.
- Pushing. Nothing is pushed without explicit permission.

## 10. Risks

| Risk | Mitigation |
|---|---|
| Data member reorder changes initialisation order | Invariant table in §3, plus the Dev build per module; the layout check reads the AST, so a re-entry order change is visible as an ordering finding |
| Documentation loss | Docs commit precedes the comment-strip commit, per module, always. The coverage check is the gate |
| Reformat churn hides a real change | Format commits contain no hand edits at all |
| A worker breaks a file another worker owns | One worker at a time; batches are disjoint file lists |
| The gate fails on someone else's in-flight edit | `git status` before each commit; `Engine/Core/TaskSystem.h` already carries an uncommitted include-blank-line fix and `docs/Core/{Runnable,TaskID,Types}`, `docs/OSAL/Application` are untracked. Attributed before touched |
| The new AST checks disagree with the agents | The AST is authoritative for layout. If it reports a violation a human rejects, the check is wrong and gets fixed, not bypassed |
