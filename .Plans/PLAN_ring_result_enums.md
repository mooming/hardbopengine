# PLAN — the two ring result enums move into their own files

**Status:** executed 2026-10-19, committed as `f30c403`. Outcome per plan; the one deviation is recorded in JOURNAL
("a bug my own tooling contained" — the bulk `Queue::` rewrite mangled `TQueue::` first; the compiler caught it).

## 1. Goal

`EPushResult` and `EPopResult` exist **nine times**: nested in each of the four fixed-ring backends,
nested in `RingBuffer`, and nested in the benchmark-only `RingMPSCFetchAdd` (`Engine/Test/RingBufferPerf.cpp`).
Because the enums were nested, the concept had to reach through `typename U::EPushResult` and then
*re-check the enumerators* in a separate `requires` block (four clauses) — one per enumerator a backend
could forget. Hoisting the two enums to namespace scope in their own headers makes them **one type each**,
so the concept compares return types against a named type and the enumerator-checking block disappears.

```
concept CFixedRing (today)                          after
──────────────────────────────────────────────      ──────────────────────────────────────────
{ ring.Push(item) } -> same_as<typename U::EPushResult>;     { ring.Push(item) } -> same_as<EPushResult>;
{ ring.Push(move) } -> same_as<typename U::EPushResult>;     { ring.Push(move) } -> same_as<EPushResult>;
{ ring.Pop(out)   } -> same_as<typename U::EPopResult>;      { ring.Pop(out)   } -> same_as<EPopResult>;
&& requires { U::EPushResult::Pushed; U::EPushResult::Full;  (gone — the enumerators are the type,
              U::EPopResult::Element; U::EPopResult::Empty;   and the type is shared, not requested)
            }
```

## 2. Decisions (recorded because they are owner-visible)

| Decision | Call | Why |
|---|---|---|
| One shared enum per direction, **superset** spellings | `EPushResult{Pushed, Full, TooManyProducers}`, `EPopResult{Element, Empty, Blocked}` | The user pasted the chain's enums; per-direction duplication would keep the concept verbose. A fixed ring never answers `TooManyProducers`/`Blocked` — that is a documented restriction on its page, not a second type. |
| Home | `Engine/Container/EPushResult.h`, `EPopResult.h`, namespace `hbe` | Their users are the Container queue family. Model: `Engine/Core/ComponentState.h` — own file, own page folder, `/// API reference:` pointer directly above the enum. |
| No nested aliases | classes do **not** keep `using EPushResult = hbe::EPushResult;` | A second name for one fact; every call site is in-tree (the chain has no engine users outside tests, `grep`-proven), so they are updated instead of adapted. |
| Benchmark baseline `RingMPSCFetchAdd` | also uses the shared enums, its nested pair deleted | Same deduplication argument; it lives in `namespace hbe` already. |

## 3. Steps

| # | Edit | Proof |
|---|---|---|
| 1 | New `EPushResult.h` / `EPopResult.h` with pointer lines; add both to `Engine/Container/CMakeLists.txt` (headers are listed, not globbed) | `comments.py` accepts the pointers |
| 2 | Delete the four backends' nested enums; include the two headers; `.cpp` qualifications `X::EPushResult` → `EPushResult` | three-config build |
| 3 | `RingBuffer.h`: delete nested enums, include headers, compact the concept (table §1), simplify `Push`/`Pop` bodies that spelled `U::EPushResult` | build + existing testlets unchanged semantically |
| 4 | `RingBuffer.cpp` testlets (`Queue::EPushResult::…` spellings) and `RingBufferPerf.cpp` (`TQueue::EPushResult::…` ×2, nested pair) | build |
| 5 | Docs: `docs/Container/EPushResult/index.html` + `EPopResult/index.html` via `docs_page.py`; module page nav + entry rows; five class pages re-emit their enum tables as links; `RingBuffer/index.html` `U` row + Coverage re-read against the compact concept; `Signature` sections re-verified verbatim | `docs_coverage.py check Container`, `docs_methods.py`, `docs_pass.py signatures`, `htmlcheck.py` at 0 *for the touched pages* |
| 6 | Gates: `check.sh` on changed files (three-config build + `EngineTest`), JOURNAL entry, commit | `GATE_EXIT`/`check.sh` exit 0 |

## 4. Risks

| Risk | Handling |
|---|---|
| A backend silently stops answering a required enumerator | impossible by construction once the type is shared — the enumerators come with the type; that *is* the compaction |
| `RingBuffer::Push` maps backend `Full` onto chain `Full` — a mapping that used to cross two enum types | now same-type comparisons; the `Assert` ("block answered Full although its claim had room") keeps its meaning and is re-read |
| Signature pages drift | `docs_pass.py signatures` compares each page against the header verbatim — unchanged strings expected, gate proves it |
| Scope creep | nothing else in the concept changes (`ValueType`, `GetCapacity`, `ClaimedSlotCount`, power-of-two clause stay) |

## 5. Non-goals

- The `ClaimedSlotCount()` naming item from the journal stays owed — different decision.
  *(Closed later the same day: renamed `GetSlotVersion` — see JOURNAL 17:42.)*
- No behaviour change: same enumerators, same returns, same hot paths.
