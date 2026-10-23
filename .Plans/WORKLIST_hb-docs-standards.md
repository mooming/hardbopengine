# Worklist — hb-docs & hb-standards application

Measured 2026-10-23 against `master` (`dffb5c0`) with `check.sh --all --no-build`,
`docs_coverage.py ledger` and per-file `comments.py`.

## Tree-wide totals

| Debt | Total |
| ---- | ----- |
| Missing doc pages | 42 |
| Comment violations | 645 in 7 modules |
| Member-layout violations | 208 in 37 files |
| clang-format non-conformant files | 18 |
| Blank-line findings (A rules) | 23 in 8 files |
| Pages missing `/// API reference:` pointer | 38 |

## Module order

| # | Module | Missing pages | Comments | Layout | Format | Weight | Status |
| - | ------ | ------------- | -------- | ------ | ------ | ------ | ------ |
| 1 | Memory | 0 | 0 (was 5) | 0 (was 26) | 0 (was 7) | S | **done** |
| 2 | Container | 0 (was 2 + 128 method pages) | 0 (was 55) | 0 (was 72) | 0 | M | **done** |
| 3 | Renderer | 1 (VulkanCapabilities) | 239 in 9 files | 44 in 1 file | — | M | pending |
| 4 | OSAL | 14 | 96 in 16 files | 14 in 5 files | 11 files + 11 B2 | L | pending |
| 5 | Math | 16 (all classes) | 127 in 20 files | 42 in 13 files | — | XL | pending |
| 6 | String | 8 | 14 in 9 files | 14 in 1 file | — | L | pending (per-header) |
| 7 | Test | 1 (TestletDispatch) | 125 in 7 files | 33 in 4 files | — | M | owner decision |
| 8 | Core/Log/Config | 0 | 0 | 10 in 8 files | — | S | pending |

Clean already: Config (docs), Core (docs), HSTL, Engine, Log (docs), Resource.

## Notes

- Renderer's `RenderCapabilities.h` alone holds 106 comment violations — worst file in the tree.
- Method-page debt the class-level ledger hides: Renderer 18, Test 21, Math and OSAL unknown until
  their class pages exist. Container's real total was 128, not 2 — measure with `docs_methods.py`
  before scheduling, not from the ledger's missing column.
- `quoted_blocks.py` caption regex fixed (2026-10-23): `operator` captions now capture the symbol
  past the word match. Still unwired from the gate per O14; 3 structurally-unresolvable `()`-overload
  captions remain in docs/Container (Deque/constructors, Queue/constructors, Queue/front).
- Pointers on a wrapped multi-line template header must sit directly above the `class` head —
  `comments.py`'s pointer reader cannot skip a `template <...>` continuation line that begins with
  `class` (HashMap.h, Map.h).
- Blank-line/include debt is `[DEBT]`-gated tree-wide but hard-fails scoped runs; absorbed by each
  module's format stage (mostly Memory + OSAL).
- Open owner decisions outside this worklist: O14 (`quoted_blocks.py` wiring), O27 (double-release
  detection), O22 (sharded option on `wip/option-c-sharded`).
