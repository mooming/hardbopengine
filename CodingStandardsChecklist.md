# CodingStandards Examples Checklist

## Task Summary
Ensure CodingStandards.h provides good and bad examples for each item in CodingStandards.md with C++ language-specific explanations.

## Checklist

### 1. Naming Conventions ✅
- [x] Classes, Functions, Types: PascalCase (with bad example: lowercase)
- [x] Variables: camelCase (with bad example: m_ prefix, Hungarian notation)
- [x] Constants: PascalCase with descriptive prefix
- [x] Macros: SCREAMING_SNAKE_CASE
- [x] Internal Types: T prefix

### 2. Class Conventions ✅
- [x] Member Ordering: static > public > protected (with bad example: wrong order)
- [x] Access Specifiers: explicit (with bad example: missing explicit)
- [x] Getters: [[nodiscard]]
- [x] Inheritance: final specifier
- [x] Initialization: constructors over in-class (with bad example: non-constexpr init)
- [x] Inline: no explicit inline keyword
- [x] Virtual: override without virtual

### 3. Code Formatting ✅
- [x] Brace Style: K&R (break line before '{')
- [x] Single-line Statements: omit braces
- [x] Namespaces: no indent (with bad example: indented)
- [x] Include Order: alphabetical, project headers first

### 4. Error Handling & Memory Management ✅
- [x] Assertions: Assert() usage (with bad example: missing Assert)
- [x] Exception-free: no throw (with bad example: using exceptions)
- [x] Pointers: references over raw pointers (with bad example: raw pointers)
- [x] Constants: constexpr/const (with bad example: non-const)
- [x] Scoping: explicit braces

### 5. Optimization & Move Semantics ✅
- [x] RVO/NRVO: return by value (with bad example: return by pointer)
- [x] NRVO: no unnecessary std::move
- [x] constexpr and consteval
- [x] Avoid dynamic allocations

### 6. Additional C++ Examples ✅
- [x] Range-based for loops
- [x] enum class (instead of plain enum)
- [x] noexcept
- [x] Uniform/brace initialization

## Verification
- [x] Syntax check passed
- [x] All examples have C++ language-specific explanations
- [x] Bad examples removed where trivial (opposite of good)