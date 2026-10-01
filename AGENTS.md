# AGENTS.md - HardBop Engine Developer Guide

**Mandatory for all AI agents.** All rules are binding; if you cannot follow a rule, explain why and ask for guidance.

## Project Overview
HardBop Engine: high‑performance C++23 engine with custom memory, task system, and OSAL for cross‑platform use.

## Self Documented Code and API Reference Documents
The engine codebase carries no comments at all — not in `.cpp`, and not in `.h` either. Names,
types and structure document the code; every sentence of prose belongs to the API reference.
The exemption list is exhaustive and recorded in `docs/CodingStandards.md`: the line-1 copyright
notice, a structural label on the line that closes its own construct (`#endif // PROFILE_ENABLED`,
`} // namespace hbe`), the `// hb-standards:ignore` tool directive, and the BAD EXAMPLE blocks of
`Engine/CodingStandards.{h,cpp}` that teach the rules by breaking them.

API reference documents are HTML files under "docs", one folder per module directory of "Engine/":
the Module Index page is "docs/index.html" and is the start page, and a module's reference is
"docs/<Module>/index.html", so the documentation tree matches the source tree. Create or revise
those files. The API documents provides these information.

Because source comments are banned, the reference is the only place a contract exists. It must
therefore cover **every** API and feature of the engine: one page per class (or macro set, or
free-function namespace) and one page per method name, per
`.Plans/AUTHORING_method_and_class_pages.md`. A page that does not document everything states
what it omits in its Coverage section — a partial page must never read as a complete one.

Module Index page links every module page and every design document of that module.
`.pi/skills/hb-standards/scripts/docs_coverage.py` proves the pairing, and it is the gate that
makes deleting a comment safe: no comment is removed until the entry it documents owns its page,
which the header addresses, with a page for every method name — checked per file by
`docs_coverage.py check-file <path>`, because one undocumented entry must not freeze a header
whose own reference is finished.

# Module
1. Module Description
2. Module Classes
2. Module Variables
3. Module Functions

# Class
1. Class Description
2. Template Parameters
3. Class Properties
4. Class Methods
5. Non-member methods(util or helper functions)

# Functions
1. Function Description
2. Parameters
3. Return Value
4. Examples

## Coding Standards

- **Member ordering: the data layer is one block.** All member variables are declared before all
  member functions, and within each of those two layers the sections run `public` → `protected` →
  `private`, with `static` first in every section. Types head the class, `friend` declarations end
  it. `docs/CodingStandards.md` carries the twelve-block table, and
  `.pi/skills/hb-standards/scripts/layout.py` checks it from the clang AST. When fixing a file,
  never change the order of the data members relative to each other: C++ initialises them in
  declaration order, so moving the block is safe and re-sequencing it is a silent behaviour change.

- **A type that needs a class constant follows that constant.** A nested type sized by `MaxProvidersPerLane`,
  or an alias bounded by `MaxQueueSize`, cannot be declared before the constant it names — the order the rule
  wants does not compile. Mark its declaration line `// hb-standards:ignore`, keep the constant directly above
  it, and put the reason in the module's design document; `layout.py` reports it as `MEMBER-WAIVED`, counted,
  never silent.

- **No comments in `.cpp` or `.h` files.** Both halves of the source pair are self-documented:
  names, types and structure carry the intent. Do not explain code in either file — put the
  explanation where a reader forms intent instead:
    - Material useful to **users of the engine** (contract, preconditions, ownership,
      lifetime, thread-safety, complexity a caller depends on) goes in the **HTML API
      reference**: `docs/<Module>/<Class>/…`.
    - Material useful for **implementation or system design** (invariants, algorithms,
      allocation strategy, locking protocol, platform quirks) goes in an HTML design
      document under `docs/`.
    - **An API reference pointer is an address, not documentation.** Every documented entry
      carries exactly one line, `/// API reference: docs/<Module>/<Entry>/index.html`,
      immediately above its declaration — and nothing more. The header then contains the way
      to the contract without keeping a copy of it, which is what a `@brief` sentence would
      have been: a second place the same claim lives. Both directions are enforced —
      `comments.py` refuses a pointer whose path does not resolve or which names a different
      entry, and `docs_coverage.py` refuses a page whose header carries no address. An entry
      without a page carries no pointer, because its prose has not moved yet.
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
