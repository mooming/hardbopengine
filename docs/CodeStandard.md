## Code Standard

To maintain code quality and consistency, please follow these guidelines:

### Naming Conventions
- **Classes/Functions/Types**: `PascalCase` (e.g., `MemoryManager`, `ConfigParam`)
- **Variables**: `camelCase` (e.g., `defaultValue`, `isDone`). Do not use 'm_' for member variables. Do not use hungarian notation.
- **Constants**: `PascalCase` with prefix (e.g., `MaxNameLength`)
- **Macros**: `SCREAMING_SNAKE_CASE` (e.g., `__DEBUG__`, `__UNIT_TEST__`)
- **Namespace**: `hbe` (HardBopEngine)
- **Internal types**: Prefix with `T` (e.g., `TLogOut`, `TAllocFunc`)

### Class Conventions
- **Order**: Properties first, high access permission first (public > protected > private). static first.
- Always use access specifiers regardless of class or struct.
- [[nodiscard]] for getters
- Use 'final' for all classes not designed for inheritance
- Do not use in-class initialisation(initializing member variable in header). Always use default constructor instead.
- Do not use 'inline' keyword
- Do not use 'virtual' keyword if it has 'override'

### Code Formatting
- **Indentation**: 4 spaces (UseTab: Always)
- **Column limit**: 120 characters
- **Brace wrapping**: K&R variant
- **Template declarations**: Always break before `>`
- **Readability**: Put an empty line before `return` unless 'return' is the only content within its scope. Place two empty lines after include declarations.
- **System Compatibility**: Pleace an empty line at the end of files
- Do not indent for namespaces
- Do not use brackets for online 'continue' and 'return'.


### Error Handling & Memory
- Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for debug checks.
- No exceptions (the project is exception-free).
- Prefer pass-by-reference over raw pointers.
- Prefer 'constexpr' and 'const'.
- Always check pointer validity before usage.
- Use brackets to limit scope of local variables clearly 
