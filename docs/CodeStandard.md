## Code Standard

To maintain high code quality and consistency, please adhere to the following guidelines:

### Naming Conventions
- **Classes, Functions, and Types**: Use `PascalCase` (e.g., `MemoryManager`, `ConfigParam`).
- **Variables**: Use `camelCase` (e.g., `defaultValue`, `isDone`). Do not use an `m_` prefix for member variables, and do not use Hungarian notation.
- **Constants**: Use `PascalCase` with a descriptive prefix (e.g., `MaxNameLength`).
- **Macros**: Use `SCRAMING_SNAKE_CASE` (e.g., `__DEBUG__`, `__UNIT_TEST__`).
- **Namespaces**: Use the `hbe` namespace.
- **Internal Types**: Prefix with `T` (e.g., `TLogOut`, `TAllocFunc`).

### Class Conventions
- **Member Ordering**: Place `static` members first, followed by other members ordered by visibility (`public` > `protected` > `private`).
- **Access Specifiers**: Always explicitly define access specifiers for all classes and structs.
- **Getters**: Use the `[[nodiscard]]` attribute for getter functions.
- **Inheritance**: Use the `final` specifier for classes that are not intended to be inherited from.
- **Initialization**: Avoid in-class initialization except when using `constexpr`. Prefer initializing members via constructors.
- **Inline Functions**: Do not use the `inline` keyword unless necessary (e.g., to prevent ODR violations).
- **Virtual Functions**: Do not explicitly use the `virtual` keyword when the function is marked with `override`.

### Code Formatting
- **Indentation**: 4 spaces (Always use tabs for indentation).
- **Column Limit**: 120 characters.
- **Brace Style**: K&R variant.
- **Readability**:
    - Place an empty line before `return` statements, unless the `return` is the only statement within its scope.
    - Place two empty lines after include declarations.
- **System Compatibility**: Ensure every file ends with a newline character.
- **Namespaces**: Do not indent code blocks contained within namespaces.
- **Single-line Statements**: Avoid using braces for single-line `continue` or `return` statements.

### Error Handling & Memory Management
- **Assertions**: Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for runtime checks and debugging.
- **Exception-free**: The engine is exception-free; do not use C++ exceptions.
- **Pointers**: Prefer passing by reference over raw pointers whenever possible.
- **Constants**: Favor the use of `constexpr` and `const`.
- **Safety**: Always validate pointer validity before dereferencing or using them.
- **Scoping**: Use braces `{}` to clearly define the scope of local variables.