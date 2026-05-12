// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <string>

/**
 * @namespace hbe
 * @brief Contains global definitions and constants related to HardBop Engine coding standards.
 */
namespace hbe
{
    // --- Naming Conventions ---
    // Classes, Functions, Types: PascalCase
    // Variables: camelCase
    // Constants: PascalCase with a descriptive prefix (e.g., MaxNameLength)
    // Macros: SCRAMING_SNAKE_CASE

    /**
     * @brief Defines the maximum length allowed for names in the engine.
     */
    constexpr size_t MaxIdentifierLength = 128;

    /**
     * @brief Internal type representation for a logger output function.
     */
    using TLogOutFunc = void (*)();

    // --- Formatting Standards ---

    /**
     * @brief Defines the required indentation space count (4 spaces).
     */
    constexpr int IndentationSpaces = 4;
    
    /**
     * @brief Coding Standard Rule: Brace Style (K&R)
     * - Always break line before '{': `type FunctionName(args) {`
     */

    // --- Optimization & Move Semantics ---

    /**
     * @brief Coding Standard Rule: RVO/NRVO Guidelines
     * 
     * - Function return types should prefer non-pointer values when possible to enable 
     *   Return Value Optimization (RVO) and Named Return Value Optimization (NRVO).
     * - For functions that construct objects, define them by value in the declaration:
     *   `Object create()` instead of `Object* create() const`.
     * - NRVO is automatically applied when returning an object constructed locally
     *   within the function body. Avoid unnecessary copies.
     * - Use std::move explicitly only when moving is required, not as a workaround for 
     *   RVO/NRVO limitations.
     */

    // Empty struct to hold standard version info or constants if needed later
    struct CodeStandardInfo
    {
        static constexpr const char* Version = "1.0.0";
    };

    /**
     * @brief Example: Proper use of RVO/NRVO by returning objects by value.
     */
    static void Example_ProperUsage() 
    {
        // NRVO is automatically applied here
        auto result = CreateObject(); // Efficient, no explicit move needed

        // RVO applies when returned from the function
        return Result{42}; // Efficient, compiler can optimize this
    }

    /**
     * @brief Example: Bad practice - returns by pointer prevents RVO/NRVO.
     */
    static void Example_BadUsage() 
    {
        auto result = CreateObject(); // OK if NRVO applies to the temporary

        return Result{*result}; // Explicit move, not ideal for NRVO
    }

} // namespace hbe

#endif // CODING_STANDARDS_H
