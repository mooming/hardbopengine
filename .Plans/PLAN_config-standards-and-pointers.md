# PLAN — Apply hb-standards to Engine/Config and give it its API reference links

## Goal (confirmed before editing)

1. Run every hb-standards layer over `Engine/Config` and close what is gated.
2. Give the module its API reference documents *and* the link from the header to them —
   the `/// API reference:` pointer line, which `Engine/Config` does not carry on any entry.

## Measured baseline (every number below was produced by a checker, not assumed)

| Layer | Check | Baseline |
|---|---|---|
| 1 | `clang-format --dry-run --Werror` | 5 files flagged, all at the include-preamble seam (the A3 double blank the formatter deletes). Tolerated by the gate through `blank_lines.py --collapse-seam`; to be re-proven, not "fixed" |
| 2 | mechanical greps | 0 |
| 2b | `blank_lines.py` | 0 findings |
| 2b | `includes.py` | 0 findings |
| 3 | `comments.py` | 0 violations — the module is already comment-free |
| 4 | `layout.py` | 0 violations; 1 `ACCESS-REDUNDANT` **advice** at `ConfigFile.h:23` |
| 5 | `docs_methods.py Config` | 0 method pages missing |
| 5 | `htmlcheck.py` | 34 pages, 0 with problems |
| 5 | `docs_coverage.py check Config` | **exit 1** — 5 pages whose header carries no address |
| 5 | `docs_pass.py signatures` | **4 problems**: `ConfigFile/constructors`, `ConfigFile/get-value`, `ConfigSystem/constructors`, `ConfigSystem/register` |
| 6 | three-configuration build | not yet run |

Two exit codes were initially misread because the command was piped to `tail`, which puts `tail`'s
status in `$?`. `docs_coverage.py check Config` is exit **1**, not 0. The skill names this trap.

## Why the pointer is the last thing to add

The pointer is the claim that a page is complete. `ConfigSystem/register.html` and the two
`constructors` pages hold a Signature section that is **not** the header's text:

- the heading reads `Signatures` (plural). The contract and 457 of 472 method pages use `Signature`,
  which is why `docs_pass.py signatures` reports `NO SIGNATURE BLOCK` rather than a mismatch.
- the block carries six `///` comment lines (`/// the only constructible form`,
  `/// absence as data: an empty optional …`) presented inside a quotation of the header. The header is
  comment-free, so these are fabricated quotations — the citation rule forbids exactly this.
- `register.html` sits a prose `<p>` between the heading and the block, which is not the template shape.

Adding a pointer over those four pages would certify a signature the checker cannot verify. So the order
is: repair the pages, prove the signatures, then add the pointers, then prove the whole gate.

## Sequence

| # | Step | Proof |
|---|---|---|
| 1 | Snapshot the five headers (`git show HEAD:…`) | snapshots exist |
| 2 | `ConfigSystem/index.html`: give the two deleted `operator=` forms their own row. They are the only place the reference mentions them, and the tool's grouping puts `operator=` under its own name, not under `ConfigSystem` — so rewriting the constructors page **without** this row first deletes the fact from the reference. Fix the constructors row's `1 usable, 4 deleted` badge to match its own cell | `htmlcheck.py` |
| 3 | Four method pages: heading → `Signature`, sidebar label → `Signature`, lead-in prose moved into `Function description` (it must move *before* step 4, because the tool replaces everything between the heading and the first `</pre>`) | `htmlcheck.py` |
| 4 | `docs_pass.py signatures` per class — writes the header's text byte for byte, which removes the six fabricated comment lines | `0 problem(s)` per class |
| 5 | Add the five pointer lines. `ConfigFile`, `ConfigParam` (above the `template` line), `ConfigSystem` above their class declarations; `BuildConfig` above its first `#define` and `EngineConfig` above its first constant — both take the file-stem form, which `comments.py` admits only when the declaration below is not a type | `comments.py` still 0 violations, every pointer resolves |
| 6 | Re-run every layer, then the three-configuration gate detached | `GATE_EXIT=0` |
| 7 | Regenerate `.Plans/DOCS_COVERAGE.md`, log `JOURNAL.md`, commit | Config row reads `pointer = yes` |

## Left as reported debt, deliberately not edited

| Finding | Why not fixed here |
|---|---|
| 242 of 479 pages tree-wide (34 of them Config's) still carry the deprecated `ul class="methods"` sidebar list that the current chrome dropped | not gated, and Config would then disagree in one direction with 242 siblings; `docs_pass.py reskin` is the tool, and it is a tree-wide decision |
| `ConfigFile.h` names `std::function` without `#include <functional>` (compiles on the transitive path) | an include edit, not a formatting one — needs the build gate behind it |
| `ConfigFile::IsValid()` is a non-template body in the header, returning `auto` where `bool` is the contract | the header-minimality rule is explicitly review-and-report, not gated (145 such bodies sit in 39 headers) |
| `ConfigFile::Parse` reads `while (!ifs.eof())` and logs to `std::cout`, not the `Logger` | behaviour change, out of scope for a standards pass |
| `ConfigSystem::GetByte` returns `false` for a `uint8_t` | cosmetic, same value |
| `ConfigFile.h:23` `ACCESS-REDUNDANT` | the second `public:` marks the twelve-block boundary between the data layer and the function layer — advice the author may keep, and here it should stay |
