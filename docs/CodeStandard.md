## Code Standard

To maintain code quality and consistency, please follow these guidelines:

### Naming Conventions
- **Classes/Functions/Types**: `PascalCase` (e.g., `MemoryManager`, `ConfigParam`)
- **Variables**: `camelCase` (e.g., `defaultValue`, `isDone`)
- **Constants**: `PascalCase` with prefix (e.g., `MaxNameLength`)
- **Macros**: `SCREAMING_SNAKE_CASE` (e.g., `__DEBUG__`, `__UNIT_TEST__`)
- **Namespace**: `hbe` (HardBopEngine)
- **Internal types**: Prefix with `T` (e.g., `TLogOut`, `TAllocFunc`)

### Code Formatting
- **Indentation**: 4 spaces (UseTab: Always)
- **Column limit**: 120 characters
- **Brace wrapping**: K&R variant
- **Template declarations**: Always break before `>`
- **Readability**: Put an empty line before `return`. Place two empty lines after include declarations.

### Error Handling & Memory
- Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for debug checks.
- No exceptions (the project is exception-free).
- Prefer pass-by-reference over raw pointers.
- Always check pointer validity before usage.
