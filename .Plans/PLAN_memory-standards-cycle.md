# PLAN — Memory module standards cycle

Owner decision: start the worklist with Memory (docs already complete, so the cycle
is layout → format → comment strip → ledger).

## Scope

`Engine/Memory/**` only. Docs are already complete (23/23 entries, pointers in place),
so no `hb-docs` authoring is expected — only verification that reordering did not
drift signatures.

## Findings to fix (measured, from `check.sh --all --no-build`)

### Layout (26 findings, 6 files)

| File | Findings |
| ---- | -------- |
| DefaultAllocator.h | 1 — static `GetFallbackCount` sits after instance functions |
| InlinePoolAllocator.h | 5 — static `GetName`/`GetBlockSize`/`GetNumBlocks`/`WritePointerToNext`/`GetPointerToNext` misplaced |
| MemoryManager.h | 10 — type `UsageRecord`, static `GetName`, 8 data members after functions |
| MultiPoolAllocator.h | 2 — type `TInitializerList`, static `CalculateNumberOfBlocks` |
| MultiPoolConfigCache.h | 1 — static `GetVersion` |
| ThreadSafeMultiPoolAllocator.h | 2 — type `TInitializerList`, static `CalculateNumberOfBlocks` |

ACCESS-REDUNDANT advice (not gated, decided by reading): AllocatorScope.h, Optional.h,
ScopedAllocator.h, Shareable.h.

### Format (7 files)

AllocatorScope.h, InlineMonotonicAllocator.cpp, MemoryManager.cpp, MonotonicAllocator.cpp,
MultiPoolAllocator.cpp, PoolAllocator.cpp, ThreadSafeMultiPoolAllocator.cpp.

### Comments (5 findings, 1 file)

AllocatorProxy.h lines 19–20: parameter-name comments `/*userData*/` `/*allocSize*/`
`/*pointer*/`. Delete via `comments.py --strip` only after `docs_coverage.py check-file`
passes for that file; prove with `code_tokens`.

## Commits (one per stage, no push)

1. `refactor(memory): the twelve-block layout` — 6 files, `layout.py` 0 and
   `--init-order` 0, module compiles.
2. `style(memory): the format pass` — `check.sh --apply` scope, `prove_format.py`
   with manifest, rule set A + B, `prove_regroup.py --whitespace-only`.
3. `refactor(memory): the parameter comments deleted` — strip AllocatorProxy.h,
   `code_tokens` proof, format, token-equal.
4. `docs(journal)`: ledger + JOURNAL update.

## Verification

- `layout.py` and `layout.py --init-order` → 0 on all 6 files
- `blank_lines.py`, `includes.py` → 0 on touched files
- `docs_coverage.py check Memory` → 0 missing; `docs_methods.py Memory` → 0
- `comments.py` → 0 on Engine/Memory
- Three-configuration build gate (`gate.sh spawn --all --test`, `GATE_EXIT=0`)

## Risks

- Moving data members changes initialisation order — mitigated by the data-block-moves-
  as-unit rule and `--init-order` gate.
- `MemoryManager.h` moves 8 data members after `GetUsage`; its constructor initializer
  list must be re-sequenced (compiler `-Wreorder` will catch any miss).
