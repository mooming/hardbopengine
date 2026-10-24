# PLAN — String module standards cycle

Sixth module on `.Plans/WORKLIST_hb-docs-standards.md`. The skill flags String as too large to
review at once; the owner chose the single sequential worker model, so the docs stage is one worker
working per class, smallest first.

## Findings (measured 2026-10-23)

### Layout — 14 findings, 6 files

StaticStringTable.h 5, InlineStringBuilder.h 3, StringBuilder.h 3, String.h 2, StaticString.h 1.
Read each; data above functions, statics above instances.

### Format

No clang-format failures; re-measure touched files.

### Docs — 8 class pages + method pages

InlineStringBuilder, Letter, StaticString, StaticStringID, StaticStringTable, String,
StringBuilder, StringUtil (utility header). EndLine is documented but its pointer is missing.

### Comments — 14 in 9 files

Mostly one class-level @brief per header; String.cpp carries 6 implementation notes.

## Order

1. layout → 2. format/seams → 3. docs (worker) → 4. strip → 5. gate, ledger, journal
