# PLAN — OSAL module standards cycle

Fourth module on `.Plans/WORKLIST_hb-docs-standards.md`. The largest OS-layer debt on the worklist.

## Findings (measured 2026-10-23)

### Layout — 14 findings, 5 headers

Application.h 1, Directory.h 2, File.h 1, SourceLocation.h 4, Window.h 6.

### Format + include layout — 11 platform cpp files

`Linux{FileHandle,FileOpenMode,InputOutput,MapSyncMode,ProtectionMode,Thread}.cpp` and
`OSX{FileHandle,FileOpenMode,InputOutput,MapSyncMode,ProtectionMode}.cpp`: clang-format
(2 lines each) + include block not alphabetical (B2). Seams re-decided after the formatter.

### Docs — 14 class pages + method pages (count measured when classes exist)

Application, Directory, File, FileHandle, FileOpenMode, MapSyncMode, OSAbstractLayer,
OSDebug, OSInputOutput, OSMemory, OSThread, ProtectionMode, SourceLocation, Window.
Authored by one sequential subagent (owner's chosen model), verified by me.

### Comments — 96 in 16 files

Headers (Application 7, OSThread 17, Window 7, OSMemory.h 3, SourceLocation 3, OSDebug 2,
OSAbstractLayer 1, OSInputOutput 1, File 1, Directory 1), platform cpps (OSMemory.cpp 27,
WindowsMemory.cpp 3, OSXThread.cpp 2, OSXMemory.cpp 2, LinuxMemory.cpp 2, LinuxWindow.cpp 1).
The OSMemory.cpp block is the biggest single implementation-note set — likely a design document.

## Order

1. layout → 2. format + include layout + seams → 3. docs (subagent) → 4. strip → 5. gate, ledger, journal
