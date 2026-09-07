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
- **Member Ordering**: Place `static` members first, followed by other members ordered by visibility (`public` > `protected` > `private`).
- **Access Specifiers**: Always explicitly define access specifiers for all classes and structs.
- **Getters**: Use the `[[nodiscard]]` attribute for getter functions and functions that return values.
- **Inheritance**: Use the `final` specifier for classes that are not intended to be inherited from.
- **Initialization**: Avoid in-class initialization except when using `constexpr`. Prefer initializing members via constructors.
- **Inline Functions**: Do not use the `inline` keyword explicitly. Do not define multi-line functions in headers unless it's absolutely necessary (e.g., for templates or `constexpr`).
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
    - After all include and define directives at the top of source files,
      place **one** empty line before the first code body.
      clang-format enforces this, and the rule is position-dependent, which is easy to
      get wrong: before a `using namespace` both one and two blank lines survive (three
      or more collapse to two), while before a namespace declaration, a function or a
      comment only one survives (two or more collapse to one). Two is therefore legal
      in exactly one position and wrong everywhere else, so always write one. This
      clang-format offers no option with which to opt out of the collapse.
- **System Compatibility**: Ensure every file ends with a newline character.
- **Namespaces**: Do not indent code blocks contained within namespaces
  (`NamespaceIndentation: None`). This is a deliberate owner decision on record,
  not a default — the engine body predates it and is largely indented, so expect
  legacy files to disagree until they are reformatted.
- **Single-line Statements**: Avoid using braces for single-line `continue` or `return` statements.

### Comments
- **No comments in `.cpp` files.** Implementation files are self-documented — names, types
  and structure carry the intent, so a comment is either redundant or a sign that a name
  should have carried it. Do not explain code inside the implementation.
- **Where prose goes instead.** A header is where a reader forms intent, so that is where
  explanation belongs:
    - Useful to **users of the engine** — contract, preconditions, ownership, lifetime,
      thread-safety, complexity a caller depends on → the paired **`.h`** declaration.
    - Useful for **implementation or system design** — invariants, algorithms, allocation
      strategy, locking protocol, platform quirks → an **HTML design document under `docs/`**
      (see `docs/RendererDesign.html` for the house style and `docs/design/*_Design.html`
      for the naming convention).
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
- **Exemptions.**
    - The line-1 `// Copyright (c) … Hansol Park` notice: a legal notice, not documentation,
      and required by the standards lint. Where an IDE banner wraps it (`//`, then the
      copyright, then `// Created by …`), keep the copyright line and drop the banner.
    - `Engine/CodingStandards.cpp`: the rule's own teaching exemplar, which carries
      deliberate BAD EXAMPLE commentary. `check.sh` already exempts
      `Engine/CodingStandards.*` from behavioural checks for the same reason.

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