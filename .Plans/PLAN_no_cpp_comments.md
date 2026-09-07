# PLAN: No comments in .cpp files

**Status:** awaiting final approval
**Created:** 2026-09-08
**Decided by user:** Q1(a) copyright exempt · Q2 CodingStandards.cpp exempt · Q3(b) declare + full sweep · Q4(b) migrate, split by audience

---

## 1. Rule being added

> **No comments in `.cpp` files.** Implementation files are self-documented: names,
> types and structure carry the intent. Put explanatory prose in the paired header
> (`.h`) where a reader forms intent, or in a `docs/` design document for material
> that explains implementation or system design.

**Exemptions (both user-confirmed):**

| Exemption | Basis |
|---|---|
| Line-1 `// Copyright (c) 2026 Hansol Park ...` | Legal notice, not documentation. Mechanically required by `check.sh:246` — forbidding it would fail every file in the tree |
| `Engine/CodingStandards.cpp` | Teaching exemplar. Mirrors existing precedent at `check.sh:205`, which already exempts `Engine/CodingStandards.*` from behavioural checks because it carries deliberate BAD EXAMPLE blocks |

**Destination split (user-specified):**

| Content type | Destination |
|---|---|
| Useful to **users of the engine** (contract, preconditions, ownership, thread-safety, lifetime, complexity callers depend on) | Paired `.h` declaration |
| Useful for **implementation or system design** (invariants, algorithms, allocation strategy, locking protocol, platform quirks) | `docs/` HTML design document |

---

## 2. Measured scope

| Measure | Value |
|---|---|
| Tracked `.cpp` files | 132 |
| Already conform (copyright-only) | 9 |
| Exempt exemplar | 1 (`Engine/CodingStandards.cpp`) |
| **Files to sweep** | **122** |
| **Comment lines to remove** | **753** |
| …of which trailing-on-code (trim, do not delete line) | 462 (61%) |
| Standalone comment lines | 291 |
| TODO/FIXME/NOTE markers | 1 |
| Commented-out dead code | 6 lines |
| Files with line-leading `/* */` blocks | 1 (`Engine/OSAL/OSXThread.cpp`) |
| String literals containing `//` | 0 |
| Raw strings containing comment markers | 0. **Correction:** an earlier revision of this plan claimed a raw string existed at `TestMain.cpp:36`. Re-verified with `grep -n 'R"('` on a clean tree: no raw string exists anywhere in that file. Its `-test` banner is a chain of adjacent ordinary literals at 40-53, with a comment above explaining why they are not a raw string. The stripper still handles raw strings generically for the rest of the tree, and the banner is string content either way. |

Densest targets: `VulkanRenderer.cpp` (51) · `MemoryManager.cpp` (42) · `PoolAllocator.cpp` (35) ·
`OSMemory.cpp` (30) · `VulkanCapabilities.cpp` (26) · `PerlinNoise.cpp` (20) ·
`ThreadSafeMultiPoolAllocator.cpp` (20) · `MultiPoolAllocator.cpp` (20) ·
`Examples/MacOSApp/Source/Framebuffer.cpp` (104).

---

## 3. Method — script for precision, subagents for judgment

| Phase | Tool | Rationale |
|---|---|---|
| Capture + classify content | **subagent fan-out per module** | Audience judgment (`.h` vs `docs/`) is not mechanical |
| Header migrations | me, applied by hand | Small, reviewable edits |
| HTML design documents | me, hand-written | No generator exists; house style is hand-authored |
| Comment removal | **one deterministic Python state machine** | 122 LLM file-edits = 122 chances to silently alter code. A tokenizer is exact and reviewable in one read |
| Adversarial diff review | **subagent fan-out per module** | Independent skeptics catch what one tired reviewer misses |
| Build + test gate | `check.sh` + 3-config build | Mechanical |

**Ordering constraint — capture before strip.** Content must be extracted and migrated
*before* removal, or the material is gone from the working tree.

### 3.1 Stripper specification

Character state machine, not regex. Must handle: normal · in `"string"` · in `'c'` ·
in `R"delim(... )delim"` raw string · line continuation `\`+newline · `//` to EOL ·
`/* */` across lines. Never touches string contents.

Per-line outcomes:
- Whole-line comment → delete line.
- Trailing comment after code → truncate at comment start, then `rstrip()` whitespace.
  (Mandatory: `check.sh:248` fails on trailing whitespace.)
- Block comment spanning code → excise span, keep code on both sides.
- Line 1 copyright → preserved verbatim.
- Resulting consecutive blank lines collapsed per existing surrounding layout; formatting
  is otherwise **not** touched (a blanket `clang-format` would rewrite thousands of lines
  in the ~218 files that predate current rules, burying this diff).

### 3.2 Documentation targets

Reuse what exists rather than creating parallels:

| Source area | Target |
|---|---|
| Renderer / Vulkan | append to existing `docs/RendererDesign.html` |
| Memory / allocators | new `docs/design/MemoryDesign_Design.html`; cross-check against existing `AllocatorGuide.md` + `MemoryManagement_Guide.md` to avoid duplicating prose |
| Math (Perlin, Monte Carlo, sampling) | new `docs/design/NumericalMethods_Design.html` |
| OSAL / threading | new `docs/design/OSAL_Design.html` |
| String / Container / Core | `.h` unless a real design narrative exists |

Style: self-contained HTML, dark GitHub theme CSS variables, mermaid via CDN — matching
`docs/RendererDesign.html`. Naming follows the existing `docs/design/*_Design.html` convention.

---

## 4. Verification

### 4.1 Gate baseline must be taken FIRST
`check.sh` lints **the files a commit touched**. Touching 124 files drags pre-existing
violations from files that predate current rules into the failure set. So:
1. `check.sh --all --no-build` **before** → record baseline.
2. Re-run **after** → assert **zero new** violation categories vs baseline.

### 4.2 Code-neutrality proof (primary, strongest check)
Per file, compare whitespace-normalized preprocessed output of original vs rewritten:

```python
# flags taken verbatim from cmake-build-debug/compile_commands.json (129 entries, real -I paths)
clang++ <original flags, minus -c/-o> -E -P   # the preprocessor deletes comments
```

The compiler removes comments itself, so if `A` and `B` preprocess to the same token
stream, no code changed — comments cannot hide a code change from this test.
Validated: toy case identical after whitespace normalization; real case
(`MemoryManager.cpp`) `exit 0`, 3,825,700 bytes.
Raw `-E -P` diff is **not** usable — it differs by whitespace; normalization is required.

### 4.3 Mechanical assertions
- Copyright header intact in all 129 files (`check.sh:246` logic).
- `Engine/CodingStandards.cpp` byte-identical except the appended rule block.
- Zero comment markers remain in the 122 swept files.
- No file lost its trailing newline; no trailing whitespace introduced.

### 4.4 Build + test gate (mandatory, last)
`check.sh` across **Dev, Debug, Release** for `EngineTest`, `VulkanExample`, `WindowExample`;
then `EngineTest` suite (53 collections) — remembering `build.sh ... -test` is required for
tests to compile at all, and that `PoolAllocatorTest` TC2 / `BufferTest` TC3 have
documented nondeterminism, so a single red run must be re-checked before being blamed
on this change.

### 4.5 Adversarial review fan-out
Per module: (1) did any non-comment byte change? (2) was any deleted comment load-bearing
and not migrated? Each finding must name file:line.

---

## 5. Commit boundaries

1. Rule declaration: `AGENTS.md`, `docs/CodingStandards.md`, `Engine/CodingStandards.cpp`
2. Header migrations
3. HTML design documents
4. The sweep (122 files)
5. `JOURNAL.md` entry

No push. Commit locally only, per Work Policy #9.

---

## 6. Risk register

| Risk | Severity | Mitigation |
|---|---|---|
| Stripper eats code inside a string | High | State machine + §4.2 compiler oracle on all 122 files |
| Deleting line instead of trailing comment kills code | High | 462 lines at stake; 4-way outcome logic + §4.2 |
| Pre-existing violations falsely attributed to this change | Medium | §4.1 baseline-before/after |
| Blanket reformat buries the real diff | Medium | No global `clang-format`; hygiene-only edits |
| Knowledge lost from working tree | Medium | §3.2 capture-before-strip, §4.5 review |
| `hb-standards:ignore` suppression unavailable in `.cpp` after sweep | Low today | Currently **0** uses in any `.cpp`; revisit if a future rule needs silencing |
| Vulkan/Memory/Math prose loss if migration judged too thin | Medium | Subagent fan-out with explicit "is a maintainer able to work safely without this?" test |

---

## 7. Explicit non-goals

- No change to `.h`/`.inl`/`.mm`/`.m` comment policy — rule is `.cpp`-scoped as written.
- No `check.sh` enforcement rule added (declined in Q3; available as separate follow-up).
- No reformatting beyond trailing-whitespace hygiene.
- No push.
