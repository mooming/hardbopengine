## Code Standard

To maintain high code quality and consistency, please adhere to the following guidelines:

### Naming Conventions
- **Classes, Functions, and Types**: Use `PascalCase` (e.g., `MemoryManager`, `ConfigParam`). Prefer using `class` instead of `struct` unless it is a Plain Old Data (POD) type.

- **Variables**: Use `camelCase` (e.g., `defaultValue`, `isDone`). Do not use an `m_` prefix for member variables, and do not use Hungarian notation.
- **Constants**: Use `PascalCase` with a descriptive prefix (e.g., `MaxNameLength`).
- **Macros**: Use `SCREAMING_SNAKE_CASE` (e.g., `__DEBUG__`, `__UNIT_TEST__`).
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
- **noexcept**: Mark functions that do not throw with `noexcept`.
- **constexpr/consteval**: Use `constexpr` for compile-time constants and `consteval` for functions that must always evaluate at compile time.
- **static_assert**: Use `static_assert` for any condition that can be validated at compile time (struct sizes, constant ranges, template constraints, etc.). This catches violations at build time instead of at runtime. Prefer `static_assert` over a runtime `Assert()` when the expression is evaluable at compile time.
- **Virtual Functions**: Use `override` solely when you override a virtual function. Do not explicitly use the `virtual` keyword alongside `override`.
- **Interfaces**: Interfaces should be pure virtual. Define interfaces using the `I` prefix convention and ensure all virtual methods are `= 0` except the virtual destructor.
- **Composition Over Inheritance**: Prefer has-a (composition) over is-a (inheritance) unless inheritance is truly necessary for polymorphism or interface contract fulfillment.

### Code Formatting
- **Indentation**: Use tabs (not spaces) for indentation.
- **Column Limit**: 120 characters.
- **Brace Style**: K&R variant.
    - Always break line before '{': `type FunctionName(args) {`.
- **Readability**:
    - Place an empty line before `return` statements, unless the `return` is the only statement within its scope.
    - Place an empty line after close brackets `}`.
    - Place an empty line between member variables and methods for readability.
    - Prefer range-based for loops unless inevitable.
    - Include the file's own header first, followed by a blank line,
      then the remaining includes sorted alphabetically.
    - After all include and define directives at the top of source files,
      place two empty lines before the first code body.
- **System Compatibility**: Ensure every file ends with a newline character.
- **Namespaces**: Do not indent code blocks contained within namespaces.
- **Single-line Statements**: Avoid using braces for single-line `continue` or `return` statements.

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
- **Safety**: Always validate pointer validity before dereferencing or using them. Prefer passing by reference over raw pointers, and assume reference variables always point to valid addresses. Use local variables to take return values and validate them before using, except for builder design patterns. Avoid dynamic allocations as much as possible. Avoid using macros unless it's inevitable. Prefer using constexpr and consteval.
- **Scoping**: Use braces `{}` to clearly define the scope of local variables.

### Optimization & Move Semantics
- **RVO/NRVO**: Prefer returning objects by value to enable Return Value Optimization (RVO) and Named Return Value Optimization (NRVO).
  - Define functions to return values: `Object create()` instead of `Object* create() const`.
  - NRVO is automatically applied when returning a locally constructed object.
  - Avoid unnecessary `std::move` calls that prevent RVO/NRVO.