# Journal

## Top-Level Summary

- **Project**: HardBop Engine — high-performance C++23 engine
- **Current focus**: Build system, test output compaction, AGENTS.md revision
- **Build status**: Debug — 51/51 PASS, 4 expected performance warnings
- **Recent**: Removed redundant per-thread `Num Pop` logging from AtomicStackViewTest (2500 lines → 0), reducing test log from 5300→2796 lines (47% drop)

---

## 2026-05-17

### AGENTS.md revision
- Revised "CMake Configuration" section: never edit generated CMakeLists.txt directly; modify `.module.config`/specifier files and regenerate with `makebuild`
- Added "Improving MakeBuild" instruction: extend MakeBuild itself when new build features cannot be expressed through existing config files
- Committed as 6e19f9c

### Build & fixes
- Fixed incomplete `cap`→`capacity` rename in Vector.h (line 249: `std::swap(capacity, rhs.cap)`)
- Debug build passes 51/51 tests (0 fail, 4 known performance warnings)

### Log compaction
- Made `MultiPoolAllocator::PrintUsage()` safe to call regardless of `PROFILE_ENABLED` by gating its body; removed guard from call site — compiler elides the empty function
- Replaced per-iteration affinity logs in TaskStreamAffinity.cpp with first-3/last-3 summary (saved ~1034 lines)
- Removed per-worker range logs from Bagel Problem tests
- Reduced `numGrowth` 10→3 in ImportanceResampling TC2/TC3 (growth tests)
- Reduced `numRepeat` 10→5 in StratifiedSampling test
- Removed per-iteration result logs in MonteCarlo/Stratified/ImportanceResampling tests
- **Removed per-thread `Num Pop` logging from AtomicStackViewTest** — 2500 redundant lines eliminated (47% of log); aggregate summary already present
- Final test log: **2796 lines** (down from original ~7570, **63% total reduction**)
