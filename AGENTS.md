# AGENTS.md - HardBop Engine Developer Guide

**Mandatory for all AI agents.** All rules are binding; if you cannot follow a rule, explain why and ask for guidance.

## Project Overview
HardBop Engine: high‑performance C++23 engine with custom memory, task system, and OSAL for cross‑platform use.

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
./build.sh <target> [-dev] [-debug] [-release] [-clean] [-notest]
# Example:
./build.sh Applications/TriangleExample -dev -debug -release -clean -notest
```

## Running Tests
Tests in `Engine/Test/UnitTestCollection.cpp`:
```bash
./build/Applications/EngineTest/<Config>/EngineTest
# <Config> = Debug|Dev|Release
```