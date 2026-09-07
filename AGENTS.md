# AGENTS.md - HardBop Engine Developer Guide

**Mandatory for all AI agents.** All rules are binding; if you cannot follow a rule, explain why and ask for guidance.

## Project Overview
HardBop Engine: high‑performance C++23 engine with custom memory, task system, and OSAL for cross‑platform use.

## Coding Standards

- **No comments in `.cpp` files.** Implementation files are self-documented: names, types
  and structure carry the intent. Do not explain code in the implementation — put the
  explanation where a reader forms intent instead:
    - Material useful to **users of the engine** (contract, preconditions, ownership,
      lifetime, thread-safety, complexity a caller depends on) goes in the paired `.h`.
    - Material useful for **implementation or system design** (invariants, algorithms,
      allocation strategy, locking protocol, platform quirks) goes in an HTML design
      document under `docs/`.
    - Exemptions: the line-1 `// Copyright (c) … Hansol Park` notice is a legal notice,
      not documentation, and `check.sh` requires it; `Engine/CodingStandards.cpp` is the
      rule's own teaching exemplar and carries deliberate BAD EXAMPLE commentary, so it
      is exempt for the same reason `check.sh` already exempts it from behavioural checks.
    - **Structural labels are not documentation.** A trailing comment that does nothing
      but name the construct its own line closes may stay: `#endif // PROFILE_ENABLED`,
      `#else // PROFILE_ENABLED`, `} // namespace hbe`. A bare `#endif` is not
      self-documenting — it cannot say which `#if` it closes — so the label serves the
      rule rather than evading it. The permission is strict: the comment must contain
      only the name of the closed construct. `} // namespace hbe  // TODO: rename` is
      prose wearing a label's clothes and must go. Labels on data tables (`// 'A' (65)`
      indexing a glyph array) are not structural labels — position already encodes them.

## Work Policy (must be strictly followed)

**All agents must adhere to the following policies without exception.**

1. Understand goals before acting.
2. Suggest improvements continuously.
3. Create a checklist after user approval.
4. Avoid scope creep; do only what is explicitly asked.
5. Self‑review before presenting.
6. Verify by building and testing; offer test methods.
7. Update checklist with brief summaries.
8. Commit after each task.
9. Never push without explicit permission.
10. Log progress with timestamps in `JOURNAL.md` (compact, top‑level summary).
11. Read `JOURNAL.md` summary first.
12. Follow coding standards in `docs/CodingStandards.md` and `Engine/CodingStandards.*`.
13. Use `batch_ai_prompt.sh` / `.bat` for batch source processing.

See [docs/WorkPolicy.md](docs/WorkPolicy.md) for the same content.

## Build System
See [docs/BuildSystem.md](docs/BuildSystem.md).

## Helper Script
`build.sh` builds specific targets with optional flags (default Dev):
```bash
./build.sh <target> [-dev] [-debug] [-release] [-clean] [-test]
# Example:
./build.sh Applications/VulkanExample -dev -debug -release -clean
```
There is no `-notest`: `build.sh` only builds and never runs tests. `-test` is the
opposite of what it sounds like — it reconfigures with `-D__TEST__ -D__UNIT_TEST__`
so the unit-test sources compile at all. Targets that are not an application
directory cannot be reached through this script (the basename becomes the target
name); build those with `cmake --build build --config <Config> --target <Name>`.

## Running Tests
Tests in `Engine/Test/UnitTestCollection.cpp`:
```bash
./build/Applications/EngineTest/<Config>/EngineTest
# <Config> = Debug|Dev|Release
```