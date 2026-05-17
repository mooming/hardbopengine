## Code Standard

To maintain high code quality and consistency, please adhere to the following guidelines:

### Naming Conventions
- **Classes, Functions, and Types**: Use `PascalCase` (e.g., `MemoryManager`, `ConfigParam`). Prefer using `class` instead of `struct` unless it is a Plain Old Data (POD) type.

- **Variables**: Use `camelCase` (e.g., `defaultValue`, `isDone`). Do not use an `m_` prefix for member variables, and do not use Hungarian notation.
- **Constants**: Use `PascalCase` with a descriptive prefix (e.g., `MaxNameLength`).
- **Macros**: Use `SCRAMING_SNAKE_CASE` (e.g., `__DEBUG__`, `__UNIT_TEST__`).
- **Namespaces**: Use the `hbe` namespace.
- **Internal Types**: Prefix with `T` (e.g., `TLogOut`, `TAllocFunc`).

### Class Conventions
- **Member Ordering**: Place `static` members first, followed by other members ordered by visibility (`public` > `protected` > `private`).
- **Access Specifiers**: Always explicitly define access specifiers for all classes and structs.
- **Getters**: Use the `[[nodiscard]]` attribute for getter functions and functions that return values.
- **Inheritance**: Use the `final` specifier for classes that are not intended to be inherited from.
- **Initialization**: Avoid in-class initialization except when using `constexpr`. Prefer initializing members via constructors.
- **Inline Functions**: Do not use the `inline` keyword explicitly. Do not define multi-line functions in headers unless it's absolutely necessary (e.g., for templates or `constexpr`).
- **Virtual Functions**: Do not explicitly use the `virtual` keyword when the function is marked with `override`.
- **Interfaces**: Interfaces should be pure virtual. Define interfaces using the `I` prefix convention and ensure all virtual methods are `= 0` except the virtual destructor.
- **Composition Over Inheritance**: Prefer has-a (composition) over is-a (inheritance) unless inheritance is truly necessary for polymorphism or interface contract fulfillment.

### Code Formatting
- **Indentation**: Use tabs (not spaces) for indentation.
- **Column Limit**: 120 characters.
- **Brace Style**: K&R variant.
    - Always break line before '{': `type FunctionName(args) {`.
- **Readability**:
    - Place an empty line before `return` statements, unless the `return` is the only statement within its scope.
    - Prefer range-based for loops unless inevitable.
    - Include declarations are sorted by alphabetical order, except for the file's own header.
    - Place two empty lines after include declarations.
- **System Compatibility**: Ensure every file ends with a newline character.
- **Namespaces**: Do not indent code blocks contained within namespaces.
- **Single-line Statements**: Avoid using braces for single-line `continue` or `return` statements.

### Error Handling & Memory Management
- **Assertions**: Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for runtime checks and debugging.
- **Exception-free**: The engine is exception-free; do not use C++ exceptions.
- **Pointers**: Prefer passing by reference over raw pointers whenever possible, and assume reference variables always point to valid addresses.
- **Constants**: Favor the use of `constexpr` and `const`.
- **Safety**: Always validate pointer validity before dereferencing or using them. Prefer passing by reference over raw pointers, and assume reference variables always point to valid addresses. Use local variables to take return values and validate them before using, except for builder design patterns. Avoid dynamic allocations as much as possible. Avoid using macros unless it's inevitable. Prefer using constexpr and consteval. Always put error logs in error conditions.
- **Scoping**: Use braces `{}` to clearly define the scope of local variables.

### Optimization & Move Semantics
- **RVO/NRVO**: Prefer returning objects by value to enable Return Value Optimization (RVO) and Named Return Value Optimization (NRVO).
  - Define functions to return values: `Object create()` instead of `Object* create() const`.
  - NRVO is automatically applied when returning a locally constructed object.
  - Avoid unnecessary `std::move` calls that prevent RVO/NRVO.