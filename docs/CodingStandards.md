## Code Standard

To maintain high code quality and consistency, please adhere to the following guidelines:

### Naming Conventions
- **Classes, Functions, and Types**: Use `PascalCase` (e.g., `MemoryManager`, `ConfigParam`). Prefer using `class` instead of `struct` unless it is a Plain Old Data (POD) type.

- **Variables**: Use `camelCase` (e.g., `defaultValue`, `isDone`). Do not use an `m_` prefix for member variables, and do not use Hungarian notation.
- **Return-by-Reference Parameters**: Prefix out-parameters (write-only references) with `out` (e.g., `bool TryParse(const char* text, int& outResult);`). Prefix in-out parameters (read-write references) with `inOut` (e.g., `void Normalize(Vector3& inOutVector);`).
- **Function Parameter Naming**: When a function parameter would collide with a member variable name, prefix it with `in` (e.g., `TestCollection(const char* inTitle) : title(inTitle) {}`).
- **Constants**: Use `PascalCase` with a descriptive prefix (e.g., `MaxNameLength`).
- **Macros**: Use `SCREAMING_SNAKE_CASE` (e.g., `DEBUG_ENABLED`, `UNIT_TEST_ENABLED`). Do not use a leading double underscore or underscore+capital prefix — those names are reserved for the C++ implementation.
- **Namespaces**: Use the `hbe` namespace.
- **Template Type Parameters & Type Aliases**: Prefix with `T` (e.g., `template <typename TEntry>`, `using TIndex = uint32_t;`).

### Class Conventions
- **Member Ordering**: The data layer of a class is one visible block, separated from the function
  layer. All member variables are declared before all member functions, and inside each of those
  two layers the sections run `public` → `protected` → `private` with `static` first in every
  section. Types are neither data nor functions, so they head the class. A class body therefore
  has at most twelve populated blocks, in this order:

    | Block | Contents | Example |
    |---|---|---|
    | 0 | nested types, aliases, enumerators — `public` → `protected` → `private` | `using TId = TAllocatorID;` |
    | 1 | public static variables | `static constexpr TId SystemAllocatorID = 0;` |
    | 2 | public variables | `uint32_t id;` |
    | 3 | protected static variables | `static constexpr int MaxRetry;` |
    | 4 | protected variables | `MemoryManager* owner;` |
    | 5 | private static variables | `static const char* Tag;` |
    | 6 | private variables | `AllocatorProxy allocators[MaxNumAllocators];` |
    | 7 | public static functions | `static MemoryManager& GetInstance();` |
    | 8 | public functions — constructors, destructors and operators included | `void PostEngineInit() noexcept;` |
    | 9 | protected static functions | `static int Clamp(int value);` |
    | 10 | protected functions | `void LogContext() const;` |
    | 11 | private static functions | `static bool IsValid(TId id);` |
    | 12 | private functions | `void RegisterSystemAllocator();` |

    `friend` declarations are neither, and sit at the end of the class.
    - Why: a reader who wants to know what a class *holds* should find it in one place, at one
      depth, without scrolling past the API that happens to use it. Interleaving the two layers
      makes the state of a 200-line class a scavenger hunt.
    - Enforcement: `.pi/skills/hb-standards/scripts/layout.py`, which reads the clang AST. It is
      not a grep, because `std::function<void(int)> cb;` is data that contains parentheses and
      `explicit operator bool() const` is a function with no name.
    - **Safety invariant when fixing a file**: the relative order of the data members with respect
      to each other must not change. C++ initialises non-static data members in declaration order,
      so moving the whole block above the functions is provably semantics-preserving, while
      re-sequencing two variables against each other is a silent behaviour change that no
      compiler, lint or test necessarily reports.
    - Machine-readable form: the block table above is the specification `layout.py` implements.
    - **Exception: a type whose definition needs a class constant follows that constant.** Types head
      the class because they are neither state nor behaviour, but a nested type that sizes itself with
      `MaxProvidersPerLane`, or an alias that bounds itself with `MaxQueueSize`, cannot name a name the
      compiler has not seen yet — the order this rule wants does not compile. Put `// hb-standards:ignore`
      on the type's own declaration line and keep the constant immediately above it, which is the only
      place it can go.
    - That directive carries no reason, because engine sources carry no prose; the reason belongs in the
      module's design document. `layout.py` reports such a member as `MEMBER-WAIVED` and prints the count
      in its summary, so a waived violation can never read as a clean file.
- **Access Specifiers**: Always explicitly define access specifiers for all classes and structs.
- **Getters**: Use the `[[nodiscard]]` attribute for getter functions and functions that return values.
- **Inheritance**: Use the `final` specifier for classes that are not intended to be inherited from.
- **Initialization**: Avoid in-class initialization except when using `constexpr`. Prefer initializing members via constructors.
- **Inline Functions**: Do not write `inline` where it does nothing. A member function defined inside its
  class is already inline, and a template does not need the keyword, so writing it there is noise.
  It is **not** a general ban: a function or operator *defined at namespace scope in a header* needs
  `inline`, because without it every translation unit that includes the header emits its own
  definition and the link fails with a duplicate symbol. `Assert`, `FatalAssert`, `IdentityMatrix` and
  `operator<<` for `ComponentState` are in that category, so removing the keyword from them is a build
  break, not a cleanup. Where a namespace-scope definition in a header is genuinely wanted, `inline` is
  what makes it legal — see *Header minimality* for when a body belongs in a header at all.
- **Header minimality**: declare in the header, define in the source. A function body in a `.h` is the
  reader's least necessary information about an API, and every line of it is paid for by everyone who
  includes the file. A non-template, non-`constexpr` body therefore goes in the `.cpp`; the header keeps
  the declaration, the types, the constants and the contract.
    - Why: the header *is* the API surface. A header where half the lines are bodies hides the shape of the
      class behind its implementation, and the twelve-block layout — state above the API — stops being
      skimmable once members are interleaved with their own code.
    - The optimisation argument, stated honestly rather than assumed. A body in a header is free to inline
      for every includer; once it lives in a `.cpp`, only its own translation unit can inline it unless
      link-time optimization runs. This tree builds Release at `-O3` with **no `-flto` and no
      `INTERPROCEDURAL_OPTIMIZATION`**, measured from `CMakeLists.txt`, so cross-translation-unit inlining is
      not happening today and a hot trivial accessor does cost a call. The rule accepts that cost: the way to
      buy the inlining back is IPO on the Release configuration, not bodies back into headers. Where a profile
      names a specific accessor, that is an argument for `-flto`, not for an exemption.
    - Exceptions are technical, not stylistic: templates and members of class templates (the definition must
      reach the instantiation site — measured: 960 of the engine's ~1,104 header bodies are this case);
      `constexpr`/`consteval` functions a caller evaluates at compile time, which cannot hide behind a call;
      a `friend` operator defined inside the class where argument-dependent lookup is the reason; and the
      teaching pair `Engine/CodingStandards.{h,cpp}`, which carries deliberate BAD EXAMPLEs.
    - Not machine-checked yet. A body in a header is decidable from the clang AST — a method record carrying a
      `body` — so a checker is cheap, but the rule enters as a review item first: 145 bodies sit in the 39
      engine headers that declare no templates (`Core/Task.h` 12, `Math/CoordinateOrientation.h` 12,
      `Memory/PoolAllocator.h` 12, `Core/SystemStatistics.h` 10, `Core/ResultPacket.h` 9), and a gate switched
      on over a backlog that size is red on the day it ships, which trains reviewers to ignore it.
- **Single-argument Constructors**: Mark single-argument constructors with `explicit` to prevent implicit conversions.
- **Defaulted Members**: Use `= default` instead of empty `{}` for trivial special member function implementations.
- **Member Initializer Lists**: Prefer member initializer lists over assignment in constructors.
- **noexcept**: Mark functions that do not throw with `noexcept`. **Warning**: Do not add `noexcept` to complex functions whose implementation cannot be guaranteed exception-free — a `noexcept` violation at runtime terminates the process.
- **constexpr/consteval**: Use `constexpr` for compile-time constants and `consteval` for functions that must always evaluate at compile time.
- **static_assert**: Use `static_assert` for any condition that can be validated at compile time (struct sizes, constant ranges, template constraints, etc.). This catches violations at build time instead of at runtime. Prefer `static_assert` over a runtime `Assert()` when the expression is evaluable at compile time.
- **Virtual Functions**: Use `override` solely when you override a virtual function. Do not explicitly use the `virtual` keyword alongside `override`.
- **Interfaces**: Interfaces should be pure virtual. Define interfaces using the `I` prefix convention and ensure all virtual methods are `= 0` except the virtual destructor.
- **Composition Over Inheritance**: Prefer has-a (composition) over is-a (inheritance) unless inheritance is truly necessary for polymorphism or interface contract fulfillment.

### Code Formatting
- **Indentation**: Use tabs (not spaces) for indentation.
- **Column Limit**: 120 characters.
- **Brace Style**: Allman — the line **always breaks before** an opening brace, for
  functions, classes, structs, namespaces, enums and control statements alike.
    - Example:
      ```cpp
      void FunctionName(args)
      {
          if (condition)
          {
              DoWork();
          }
      }
      ```
    - This is **not** K&R, which attaches the brace (`void FunctionName(args) {`),
      and **not** BSD/KNF, which breaks only for functions while keeping
      `if (condition) {` and `namespace x {` attached. This codebase breaks for
      control statements and namespaces too, which makes it Allman.
    - **There are no exemptions.** An empty body still puts each brace on its own
      line — `void FunctionName() noexcept` followed by `{` and `}` on separate
      lines — and the same holds for an empty `struct`/`class`/`enum`.
    - **One exception is forced by the formatter, not chosen.** A `requires` body of a `concept` is
      written by clang-format as `concept C = requires(T t) {`, brace attached, and clang-format 22 has
      no `BraceWrapping` key for a concept — passing `AfterConcept` is rejected with `unknown key`. Since
      the formatter is the arbiter of this file's shape and a gate that can never pass is worse than one
      irregular brace, `CLockable` in `Engine/Core/ScopedLock.h` keeps the formatter's shape. Do not
      "fix" it back to Allman: that reintroduces a permanent format failure.
    - Machine-readable form: `.clang-format`. It spells the rule as
      `BreakBeforeBraces: Custom` with an explicit `BraceWrapping` table. Note that
      `AllowShortFunctionsOnASingleLine` must stay `None` alongside
      `SplitEmptyFunction: true`, or the empty-body rule silently disappears.
- **Readability**:
    - Place an empty line before `return` statements, unless the `return` is the only statement within its scope.
    - Place an empty line after close brackets `}`.
    - Place an empty line between member variables and methods for readability.
    - Prefer range-based for loops unless inevitable.
    - Include blocks are grouped and sorted, own header first:
      ```cpp
      #include "OwnHeader.h"            // the file's own header, when one exists

      #include <atomic>                 // standard headers, alphabetical
      #include <cstring>

      #include "Config/ConfigParam.h"   // project headers, alphabetical
      #include "Log/Logger.h"
      ```
    - After all include and define directives at the top of source files, place **one** empty line
      before the first code body, and **one** after `#pragma once`.
      The formatter is a **ceiling, and in one case a floor**. This tree sets `MaxEmptyLinesToKeep: 2`,
      and measured against it: three or more blank lines collapse to two in every position tried —
      between data members, before a function, before a comment — while one or two both survive in all
      of them. It also *inserts* one place: `SeparateDefinitionBlocks: Always` puts a blank line between
      definition blocks, so two inline member bodies can never be made adjacent, and a grouping that
      wants them so is fighting the formatter. Everywhere else the formatter only caps, which is why a
      file can be format-clean and grouped badly — grouping is a reader's edit, and these numbers are a
      convention the formatter tolerates rather than one it enforces.
    - **Blank lines mark groups.** Members of one concern sit adjacent with no blank between them;
      exactly one blank line separates two concerns; a block never opens or closes on a blank line. Two
      blank lines is what the ceiling permits, not what the style wants — it reads as a paragraph break
      of the wrong size, and prevailing practice agrees (45 of 50 Core and Memory headers put exactly
      one after `#pragma once`). The order within a block is a reader's judgement of what the class is
      for, so no tool decides it; `.pi/skills/hb-standards/scripts/prove_regroup.py` is what proves the
      edit stayed whitespace and order, which is the part a tool can decide.
    - Two claims formerly made here were wrong and were measured out: that only one blank line survives
      before a namespace declaration, function or comment (two survives at each), and that the formatter
      "enforces" the one-line rule. Do not re-derive blank-line behaviour from prose — run the formatter
      on a probe and read its output. **Probe through stdin, not a file outside the repository**:
      clang-format takes its style from the directory of the file it is given, so a probe written to
      `/tmp` silently runs LLVM defaults and "verifies" a config this tree does not use. That is how the
      claim above came to be written down as measured.
- **System Compatibility**: Ensure every file ends with a newline character.
- **Namespaces**: Do not indent code blocks contained within namespaces
  (`NamespaceIndentation: None`). This is a deliberate owner decision on record,
  not a default — the engine body predates it and is largely indented, so expect
  legacy files to disagree until they are reformatted.
- **Single-line Statements**: Avoid using braces for single-line `continue` or `return` statements.

- **Unit-test blocks go at the end of the file.** A `#ifdef __UNIT_TEST__` region — a test
  class declaration, or test-only API — is always the last thing in the file, after the
  production declarations it relates to. One such region per file; a file that seems to need
  two belongs merged.
    - Why: the reader opens a header for its API. Test-only surface parked above the class
      pushes that API down and makes the top of the file about something they did not come
      for. Test-only code also depends on everything in the file, so it reads correctly at
      the bottom and nowhere else.
    - Scope: applies to `.h` and `.cpp` alike.
    - Exemption: an in-function conditional is not a trailing block. `main()` in
      `Applications/EngineTest/TestMain.cpp` wraps its own body with an `#else` branch, so
      its guard already terminates the file's logic and cannot be relocated.

### Comments
- **No comments in source files — `.h` and `.cpp` alike.** Implementation and declaration files
  carry code only. A comment in either half of the source pair is a defect, not a style choice:
  the engine's prose lives in the HTML reference under `docs/`, where it is written once, is
  searchable, and cannot drift from a signature by one edited line.
- **Where prose goes instead.**
    - Useful to **users of the engine** — contract, preconditions, ownership, lifetime,
      thread-safety, complexity a caller depends on → `docs/<Module>/<Class>/…`, the class and
      method pages of the API reference. `.Plans/AUTHORING_method_and_class_pages.md` is the page
      contract.
    - Useful for **implementation or system design** — invariants, algorithms, allocation
      strategy, locking protocol, platform quirks → an **HTML design document under `docs/`**
      (see `docs/RendererDesign.html` for the house style and `docs/design/*_Design.html`
      for the naming convention).
- **The ban is only safe because the replacement is proved first.** Deleting a comment whose page
  does not exist yet destroys the only copy of that contract. Per module the order is therefore
  always: write the pages → prove coverage → delete the comments. `docs_coverage.py` is that
  proof, and `docs/index.html` must reach every module page for the site to count as a reference.
- **Exemptions, and this list is exhaustive.**
    - **An API reference pointer is an address, not prose.** Every documented entry carries exactly
      one `///` line naming its own page, immediately above the declaration:
        ```cpp
        /// API reference: docs/Core/ScopedTime/index.html
        class ScopedTime final
        ```
      The reader of a header gets the way to the contract without the header owning a copy of it, and
      unlike a `@brief` sentence the line can be checked in both directions: `comments.py` refuses a
      pointer whose path does not resolve, whose module is not the file's own, whose entry the file
      does not declare, or which sits on anything other than its own declaration; `docs_coverage.py`
      refuses a page whose header carries no address. The form is exact — one line, that prefix, that
      path shape — because a line free to grow a sentence is a `@brief` comment that escaped the ban.
      An entry without a page has no pointer: the address would lead nowhere, and its prose has not
      moved yet.
    - The line-1 `// Copyright (c) … Hansol Park` notice: a legal notice, not documentation,
      and required by the standards lint. Where an IDE banner wraps it (`//`, then the
      copyright, then `// Created by …`), keep the copyright line and drop the banner.
    - **Structural labels are not documentation.** A trailing comment whose entire content is
      the name of the construct its own line closes may remain:
        ```cpp
        #endif // MEMORY_VERIFICATION_ENABLED
        #else  // !__DEBUG__
        }      // namespace hbe
        }}     // namespace hbe::StringUtil
        ```
      A bare `#endif` is not self-documenting: it cannot state which `#if` it closes, so the
      label advances the rule instead of evading it. Naming the guard in negated form
      (`!__DEBUG__`) and qualifying a namespace (`hbe::StringUtil`) still count as naming the
      construct. The permission is deliberately narrow:
        - the comment must name **only** the closed construct — no sentence, no TODO, no
          reasoning. `} // namespace hbe  // TODO: rename` is prose in a label's clothes.
        - it must sit on the closing line itself. A label on the line *above* is a comment.
        - data-table indices are **not** structural labels. `// 'A' (65)` above a glyph row
          restates the array index, which position already encodes; write the index rule once
          in the design document instead.
    - **`// hb-standards:ignore`** on a line: a tool directive in the class of a compiler warning
      suppression, not prose. It is what a deliberate exception costs, and it stays visible in
      review precisely because every other comment is forbidden.
    - `Engine/CodingStandards.cpp` and the BAD EXAMPLE blocks of `Engine/CodingStandards.h`:
      the rule's own teaching exemplar, which carries deliberate BAD EXAMPLE commentary.
      `check.sh` already exempts `Engine/CodingStandards.*` from behavioural checks.
- **Enforcement**: `.pi/skills/hb-standards/scripts/comments.py`, a lexer rather than a grep, so
  `http://example.com` inside a string literal is never reported as a comment.

### Error Handling, Logging & Memory Management
- **Error and Warning Logging**: Always output a descriptive log
  message (via `std::cerr` or a dedicated logger) in error or
  warning conditions before returning, asserting, or taking
  corrective action. Logging provides essential diagnostic
  information for debugging and post-mortem analysis — a bare
  `Assert()` or silent early-return makes failures much harder
  to diagnose.
  - Include the specific reason and relevant state values in the
    log message so it is actionable.
  - Log *before* any `Assert()` or `return` so the information
    survives even if the assertion terminates the process.
  - Use distinct prefixes/severities (e.g. `"Error:"` vs
    `"Warning:"`) so messages can be filtered.
  - Do not log in hot paths where the condition is expected and
    handled silently; reserve logging for exceptional or
    unexpected conditions.
- **Assertions**: Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for runtime checks and debugging.
- **Exception-free**: The engine is exception-free; do not use C++ exceptions.
- **Pointers**: Prefer passing by reference over raw pointers whenever possible, and assume reference variables always point to valid addresses.
- **Constants**: Favor the use of `constexpr` and `const`.
- **Safety**: Always validate pointer validity before dereferencing or using them. Use local variables to take return values and validate them before using, except for builder design patterns. Avoid dynamic allocations as much as possible. Avoid using macros unless it's inevitable.
- **Scoping**: Use braces `{}` to clearly define the scope of local variables.

### Optimization & Move Semantics
- **RVO/NRVO**: Prefer returning objects by value to enable Return Value Optimization (RVO) and Named Return Value Optimization (NRVO).
  - Define functions to return values: `Object create()` instead of `Object* create() const`.
  - NRVO is automatically applied when returning a locally constructed object.
  - Avoid unnecessary `std::move` calls that prevent RVO/NRVO.