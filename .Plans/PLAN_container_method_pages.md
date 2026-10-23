# Plan: author the 128 owed Container method pages

Task: one page per declared method NAME for nine classes in docs/Container/, smallest first,
each class gate-clean before the next. Authoring only — no commit, no push, nothing outside
docs/Container/ and /tmp/hbe-frag/.

## Order (owed counts from docs_methods.py Container)

| # | Class | Owed |
|---|-------|------|
| 1 | AtomicStackView | 3 (constructors, destructor, isempty) |
| 2 | Queue | 8 |
| 3 | BoundedPriorityQueue | 10 |
| 4 | Map | 16 |
| 5 | FixedRingBuffer | 16 |
| 6 | Vector | 17 |
| 7 | Deque | 18 |
| 8 | HashMap | 19 |
| 9 | LinkedList | 21 |

## Per-class procedure

1. Read the header in full (and the .cpp / test .cpp where a behavioural claim needs it).
2. Write one body fragment per method page to /tmp/hbe-frag/Container/<Class>/<stem>.body.html
   with the five fixed sections (signature, description, parameters, return, example).
3. Generate chrome: `docs_page.py method Container <Class> <stem> --source Engine/Container/<Class>.h
   --label "<declared name>" --summary "<one line>" --body <fragment>`.
4. Normalize signatures: `docs_pass.py signatures docs/Container/<Class> Engine/Container/<Class>.h <Class>`
   (this pass rewrites each Signature section to the header's declaration byte-for-byte, so the
   fragment's placeholder text is not load-bearing).
5. Gates: `docs_pass.py signatures` → 0 problems; `docs_methods.py Container --only <Class>` →
   class gone; `docs_page.py check Container` → 0 with problems.
6. Update the class page's methods table (link the new pages) and drop the corresponding
   Coverage row, so the class page stops calling documented entries undocumented.

## Rules held

- Signature section = the header's declaration, body never; the pass enforces it.
- Cite file + symbol, never line numbers; quote exact code only where it is the contract.
- No unprovable claims (thread-safety, complexity, ownership) — omit, do not guess.
- Operators: stems from the OPERATOR_PAGES table; constructors → constructors, destructor → destructor.
- Escape `&`, `<`, `>` inside code/pre everywhere.

## Status log

- (2026-02) started; AtomicStackView first.
- AtomicStackView: 3 pages written, gates clean, class page linked.
- Queue: 8 pages written, gates clean, class page linked. Note: quoted_blocks reports 3 CAPTION
  UNRESOLVED on my Queue pages that quote Deque's overloads/ctor — the caption parser cannot carry
  parameters for `operator` symbols nor distinguish same-parameter overloads; the corpus carries the
  same verdicts elsewhere (e.g. docs/Memory/PoolAllocator/operator-assign.html). The quoted text
  itself is verbatim.
- BoundedPriorityQueue: 10 pages written (incl. 3 private helpers), gates clean, class page linked
  and its method table extended with the private rows.
- Map: 16 pages written (incl. 5 private helpers), gates clean, class page linked and extended.
- Deque: 18 pages written (incl. WrapIndex, Grow, DestroyAll), gates clean, class page linked and
  extended. Real call sites from Engine/Core/TaskStream.cpp (TaskStream::Update,
  TaskStream::AbandonHeldWork).
- HashMap: 19 pages written (incl. 7 private helpers), gates clean, class page linked and extended.
- LinkedList: 21 pages written (incl. 6 private helpers), gates clean, class page linked and extended.
- Fixed one non-verbatim quote the quoted_blocks gate caught: Map/end.html had `auto it = m.Find(99);`
  where the source reuses `it` — corrected to the source's spelling.
- Remaining quoted_blocks findings in docs/Container: 1 stale + 1 unresolved pre-existing in
  docs/Container/Array (not mine); 4 CAPTION UNRESOLVED on my pages that quote Deque's constructor
  (4 same-parameter ctors) and overloads — the caption parser cannot carry operator symbols' parameter
  lists nor distinguish identical empty parameter lists; all quoted text verified verbatim by hand
  against the header.
- Final: 128 method pages written across 9 classes; docs_methods.py Container ends "method pages
  missing: 0"; docs_page.py check Container: 252 pages, 0 with problems; every class's docs_pass
  signatures: 0 problem(s). Nothing outside docs/Container/ and .Plans/ touched; nothing committed.
