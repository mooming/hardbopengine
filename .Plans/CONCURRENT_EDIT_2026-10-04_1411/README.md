# Concurrent edit captured at 2026-10-04 14:11

`Engine/Core/TaskStream.h` and `Engine/Core/TaskStream.cpp` were rewritten by a writer outside the F5
dead-code session while that session was running its verification gates (`TaskStream.h` mtime
`2026-10-04 14:11:15`, `TaskStream.cpp` mtime `2026-10-04 13:57:04`). `git stash list` still holds the
parked redesign, so this is a third line of work, not that one reapplied.

The F5 session's own edits to those two files are exactly four hunks: delete `TaskStream::TaskQueueItem`
from the header and its two out-of-line definitions from the source, and delete
`TaskStream::ProcessPostedTasks` from both. Everything else in `foreign_and_f5_mixed.diff` came from the
other writer.

| Change | File | Measured effect |
|---|---|---|
| ~30 inline member bodies converted to out-of-line declarations (`void SetMaxAge(std::chrono::nanoseconds) noexcept;`), 288 lines → 168 | `TaskStream.h` | Names `SetMaxAge`, `GetMaxAge`, `GetFifoWeight`, `GetPriorityWeight`, `GetLoopCount`, `Join`, `RequestClose`, `IsCloseRequested`, `IsClosed`, `AttachProvider`, `Update`, `DrainForShutdown`, `AbandonHeldWork`, `CountPendingItems` and others |
| Definitions for those declarations | `TaskStream.cpp` | **Not written.** Measured by pairing every declaration in the captured header against `TaskStream::<name>` in the captured source: **22 methods are declared and never defined** — `GetAbandonedWorkNoticeCount`, `GetAgedOutWorkCount`, `GetDrivenPassCount`, `GetFifoWeight`, `GetGeneralQueueRefusalCount`, `GetLaneWorkRefusalCount`, `GetLoopCount`, `GetMaxAge`, `GetName`, `GetPriorityWeight`, `GetProviderAskWhileSpentCount`, `GetStreamIndex`, `GetThread`, `GetThreadID`, `IsClosed`, `IsCloseRequested`, `IsDrivenByShutdownPump`, `Join`, `RequestClose`, `RequestWindowAdvance`, `SetDrivenByShutdownPump`, `SetMaxAge`. Each was an inline body at `HEAD`, so the captured pair compiles and cannot link |
| `[[nodiscard]]` added to declarations that did not have it (`AttachProvider`, `DetachProvider`, `IsProviderAttached`, `Update`, `DrainForShutdown`, `AbandonHeldWork`, `CountPendingItems`) | `TaskStream.h` | Attribute change on live APIs, not an F5 change |
| `FireAbandonedNotice` declared `static`, its `const` dropped | both files | Header and source agree with each other, so this one is self-consistent — it is simply not F5 |
| `#include "Container/Array.h"` removed | `TaskStream.h` | Not an F5 change |
| `#include "Engine/Engine.h"` removed | `TaskStream.cpp` | Not an F5 change |

Nothing here is a judgement that the other work is wrong: converting header bodies to source bodies is
exactly what `docs/CodingStandards.md` "Declare in the header, define in the source" asks for. It is only
that the work is not F5, it arrived mid-task, and the captured form is mid-migration: the bodies left the
header before any of them arrived in the source, which is 22 unresolved symbols. The F5 session restored
both files to `HEAD` plus its own four hunks at `2026-10-04 14:20`.

Recover the captured state with:

```bash
cp .Plans/CONCURRENT_EDIT_2026-10-04_1411/TaskStream.h.as-found Engine/Core/TaskStream.h
cp .Plans/CONCURRENT_EDIT_2026-10-04_1411/TaskStream.cpp.as-found Engine/Core/TaskStream.cpp
```

## Measured again at 2026-10-04 18:05

Everything above describes the capture moment. Two claims have since stopped being true, and both are
kept rather than rewritten because the dates are the record.

| Claim at capture | Measured at 18:05 |
|---|---|
| "22 methods are declared and never defined … the captured pair compiles and cannot link" | All 22 names in the table above now have a `TaskStream::<name>` definition in `Engine/Core/TaskStream.cpp` (scripted check: 22 checked, 0 undefined). The pair links: `build/Applications/EngineTest/Dev/EngineTest` built at 17:05 and ran 375 testlets. The bodies arrived in the source after this file was written |
| The capture was "restored to `HEAD` plus its own four hunks at `2026-10-04 14:20`" | The captured state came back into the working tree afterwards and is committed as `2363525`, together with the four F5 hunks. The two lines cannot be split by commit because both edit `TaskStream.h` and `TaskStream.cpp`; they are one commit that names both |

The dead-code session reported its own commit as `933125e` on `master`. No such object exists in this
repository - `git cat-file -t 933125e` answers "Not a valid object name", `git log --all --grep=isDone`
finds nothing, and `git worktree list` shows this checkout only - so the pile this repo held staged was
the only surviving copy of that work, and `2363525` is where it landed.
