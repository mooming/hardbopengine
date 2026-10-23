# PLAN — Container module standards cycle

Second module on `.Plans/WORKLIST_hb-docs-standards.md`.

## Findings (measured 2026-10-23)

### Layout — 69 findings, 8 files

| File | Findings |
| ---- | -------- |
| AtomicStackView.h | 2 types after data + ACCESS-EMPTY public section |
| BoundedPriorityQueue.h | 15 — private helpers declared above the public API; type TPredicate misplaced |
| Deque.h | 9 — 3 types after constants; 6 data after functions |
| FixedRingBuffer.h | 5 — data after functions |
| HashMap.h | 11 — 3 types after constants; 8 data after functions |
| Map.h | 9 — 4 types after constants; 5 data after functions |
| Queue.h | 1 — data after functions |
| Vector.h | 7 — 3 types after constants; 4 data after functions |

RingBuffer.h: 2 MEMBER-WAIVED (constants-above-type waivers, recorded).
ACCESS-REDUNDANT advice in Array.h, LinkedList.h — group boundaries, read and decide.

### Format

No clang-format failures. Blank-line/include debt re-measured per touched file at stage 2.

### Docs — 2 missing pages

- `EHashEntryState` (enum, HashMap.h)
- `LinkedListNode` (struct, LinkedList.h)

### Comments — 55 in 6 files

AtomicStackView.h 1, BoundedPriorityQueue.h 33, BoundedPriorityQueue.cpp 5,
FixedRingBuffer.h 1, FixedRingBuffer.cpp 1, LinkedList.h 14.

## Order

1. layout (docs describe the code but pages for HashMap/LinkedList quote signatures — layout
   before docs so pages quote the final header)
2. format + rule sets A/B on touched files
3. docs: two pages via hb-docs, pointers, renav
4. comment strip, 6 files, token proofs
5. ledger, journal, three-configuration gate
