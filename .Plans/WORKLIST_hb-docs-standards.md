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
| 3 | Renderer | 0 (was 1 + 18 method pages) | 0 (was 239) | 0 (was 44) | 0 | M | **done** |
| 4 | OSAL | 0 (was 14 + methods) | 0 (was 96) | 0 (was 14) | 0 (was 11+11) | L | **done** |
| 5 | Math | 0 (was 16 + 263 method pages) | 0 (was 127) | 0 (was 42) | 0 | XL | **done** |
| 6 | String | 0 (was 8 + methods) | 0 (was 14) | 0 (was 14) | 0 | L | **done** |
| 7 | Test | 0 (was 1 + 10 method pages) | 0 (was 125) | 0 (was 35) | 0 | M | **done** |
| 8 | Core/Log/Config + Engine/HSTL/Resource pointers | 0 | 0 | 0 (was 10) | 0 | S | **done** |

Clean already: Config (docs), Core (docs), HSTL, Engine, Log (docs), Resource.

## Notes

- Renderer's `RenderCapabilities.h` alone holds 106 comment violations — worst file in the tree.
- Method-page debt the class-level ledger hides: Renderer 18, Test 21, Math and OSAL unknown until
  their class pages exist. Container's real total was 128, not 2 — measure with `docs_methods.py`
  before scheduling, not from the ledger's missing column.
- `quoted_blocks.py` caption regex fixed (2026-10-23): `operator` captions now capture the symbol
  past the word match. Wired into the gate 2026-10-25 (owner decision O14, triage-then-wire):
  the tree's 102-finding backlog was triaged to zero first, and the gate now fails a commit whose
  pages quote code that no longer matches the source.
- Pointers on a wrapped multi-line template header must sit directly above the `class` head —
  `comments.py`'s pointer reader cannot skip a `template <...>` continuation line that begins with
  `class` (HashMap.h, Map.h).
- Blank-line/include debt is `[DEBT]`-gated tree-wide but hard-fails scoped runs; absorbed by each
  module's format stage (mostly Memory + OSAL).
- Open owner decisions outside this worklist: O14 (`quoted_blocks.py` wiring), O27 (double-release
  detection), O22 (sharded option on `wip/option-c-sharded`). RESOLVED 2026-10-25: the
  `Letter::IsGenuineLetter` name/behaviour mismatch — renamed `IsNotWhiteSpace` (`5af48d1`).
