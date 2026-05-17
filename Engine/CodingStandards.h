// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <string>
#include <vector>

#include "Core/Debug.h"

/*
 * Include and define directives are grouped at the top of the file.
 * After the last include / define directive, place two empty lines
 * before the first code body.
 *
 * Include ordering: own header first, then standard headers in
 * alphabetical order, then project headers in alphabetical order.
 *
 * Formatting conventions used throughout this file:
 * - Tabs for indentation (not spaces).
 * - 120-character column limit.
 * - Files end with a trailing newline.
 * - Namespace bodies are NOT indented.
 */


namespace hbe
{
/* Top-level namespace for all HardBop Engine code */

// ========================================================================
// NAMESPACE: examples
// Everything lives here: the POD, the interface, the base, the concrete
// class, and all convention-demonstration types.  Consumers reference
// the relevant type directly via hbe::examples::.
// ========================================================================
namespace examples
{

// ========================================================================
// POD STRUCT: CodingStandardsData
// Demonstrates naming and POD conventions:
//   - Classes, functions, and types use PascalCase
//     (e.g. CodingStandardsData, CodingStandardsBase).
//   - Use class by default; use struct only for POD types.
//   - POD structs: trivial, standard layout, no virtual methods,
//     no user-defined special members, brace-initialization ready.
//   - Member variables use camelCase (e.g. maxNameLength, version),
//     no m_ prefix, no Hungarian notation.
// ========================================================================
struct CodingStandardsData
{
    int version;
    size_t maxNameLength;
    bool isInitialized;
};

/* Validate struct layout / size assumptions at compile time */
static_assert(sizeof(CodingStandardsData) >= sizeof(int) + sizeof(size_t) + sizeof(bool),
    "CodingStandardsData is unexpectedly small");

// ========================================================================
// INTERFACE: ICodingStandards
// Demonstrates interface conventions: I-prefix, pure virtual (= 0),
// virtual destructor, and abstract contract.
// ========================================================================
/**
 * @brief Interfaces should be pure virtual — every method except the
 * virtual destructor must be declared with `= 0`.
 *
 * A pure virtual interface enforces a strict contract: it defines what
 * a type can do without prescribing how or what state it holds. This
 * decouples abstraction from implementation, allowing any conforming
 * type to be substituted freely.
 *
 * Why: A virtual method with a default implementation in an interface is
 * ambiguous — it blurs the line between a base class that provides
 * optional defaults and a pure contract.  Using `= 0` forces every
 * derived class to provide its own implementation, eliminating the risk
 * of accidentally inheriting a default that may be semantically wrong.
 * The `I` prefix makes the interface nature visible at the declaration
 * site so the reader does not need to scan the member list.
 */
class ICodingStandards
{
public:
    virtual ~ICodingStandards() = default;

    virtual void Validate() const = 0;
    virtual void Initialize() = 0;
    [[nodiscard]] virtual bool IsValid() const = 0;
};

// ========================================================================
// RVO/NRVO: InlinedData (large-ish stack data type)
// A 4 KiB data block initialized from a single int value.
// Exists so the CodingStandardsBase methods below can demonstrate
// RVO vs. NRVO vs. std::move-on-return behaviour.
// ========================================================================
class InlinedData final
{
public:
    explicit InlinedData(int value = 0) noexcept;

    int buffer[1024];
};

/*
 * A reusable processing component.
 * Encapsulates a value and a work buffer. Designed to be
 * used as a building block via composition, not as a
 * base class.
 */
class DataProcessor
{
public:
    explicit DataProcessor(int initialValue) noexcept;
    void Process() noexcept;
    [[nodiscard]] int GetValue() const noexcept;

    /*
     * in-prefix parameter: When a function parameter would collide
     * with a member variable name, prefix it with `in`.
     * Here `inValue` avoids colliding with the member `value`.
     */
    void SetValue(int inValue) noexcept;

private:
    int value;
    int workBuffer[256];
};

class TextBuffer final
{
public:
    TextBuffer() noexcept;
    explicit TextBuffer(const char* text); // Deliberately noexcept-free: uses new (can throw std::bad_alloc)
    TextBuffer(const TextBuffer& other);   // Deliberately noexcept-free: copy allocates
    TextBuffer(TextBuffer&& other) noexcept;
    TextBuffer& operator=(const TextBuffer& other);
    TextBuffer& operator=(TextBuffer&& other) noexcept;
    ~TextBuffer();

    [[nodiscard]] const char* GetText() const noexcept;

private:
    char* data;
    size_t length;
};

/* Type aliases and template type parameters use T prefix */
using TData = CodingStandardsData;
using TLogOutFunc = void (*)();
using TAllocFunc = void (*)(void*, size_t);

/* Template type parameters also use T prefix */
template <typename TEntry>
class TemplateExample final
{
    /* Validate template parameter constraints at compile time */
    static_assert(std::is_integral_v<TEntry>,
        "TemplateExample requires an integral type");

public:
    explicit TemplateExample(const TEntry& initial) noexcept
        : value(initial)
    {
    }

    [[nodiscard]] const TEntry& GetValue() const
    {
        return value;
    }

private:
    TEntry value;
};



/*
 * BAD EXAMPLE: Inheritance for component reuse.
 * Inheriting from DataProcessor just to use its Process()
 * and work buffer creates an is-a relationship that is
 * semantically incorrect. The base's full interface leaks
 * including GetValue() which may not make sense for this type.
 *
 * Why (cache): Base and derived members interleave in a single
 * allocation. The work buffer is always present regardless of
 * whether this type needs its own processing strategy.
 *
 * Why (flexibility): BadProcessor exposes GetValue() and
 * Process() as public methods even though only Process() is
 * relevant. There is no way to restrict the inherited
 * interface or swap the processing strategy.
 */
class BadProcessor final : private DataProcessor
{
public:
    explicit BadProcessor(int initialValue) noexcept
        : DataProcessor(initialValue)
    {
    }

    void ProcessMore() noexcept
    {
        Process();
    }
};

/*
 * Composition with a component. Embedding DataProcessor
 * as a member makes the dependency explicit. The component's
 * interface is accessed through a named member, and the outer
 * class controls exactly what it exposes.
 *
 * Why (cache): The component is still contiguous when stored
 * inline, but you are free to choose indirection (e.g.
 * DataProcessor* or unique_ptr<DataProcessor>) to keep
 * the outer class small when processing is not always needed.
 *
 * Why (flexibility): Only the methods you explicitly define
 * or delegate are visible. You can swap DataProcessor for
 * a different engine, add pre/post processing, or guard
 * access — all without affecting callers.
 */
class Processor final
{
public:
    explicit Processor(int initialValue) noexcept
        : processor(initialValue)
    {
    }

    void ProcessMore() noexcept
    {
        processor.Process();
    }

private:
    DataProcessor processor;
    int extraData;
};


// ========================================================================
// BASE CLASS: CodingStandardsBase
// Demonstrates base class conventions:
//   - Member ordering: static members first, then by visibility
//     (public > protected > private).
//   - explicit constructors for single-argument conversions.
//   - = default for trivial special member functions.
//   - Member initializer lists preferred over assignment.
//   - noexcept on non-throwing functions.
//   - constexpr for compile-time constants, consteval for CT-only
//     functions.
//   - static_assert for compile-time validation (struct sizes,
//     constant ranges, template constraints).
//   - [[nodiscard]] on getters and value-returning functions.
//   - override (without virtual) when overriding a virtual.
//   - final specifier on classes not intended for inheritance.
//   - No explicit inline keyword; multi-line functions in headers
//     only for templates and constexpr/consteval.
//   - In-class initialization only for static constexpr members;
//     regular members initialized via constructors.
// ========================================================================
class CodingStandardsBase
{
public:

    /*
     * Use static constexpr for compile-time constant values.
     * Constants use PascalCase with a descriptive prefix
     * (e.g. MaxValue, ArraySize, DefaultVersion, MaxNameLength).
     */
    static constexpr int MaxValue = 100;
    static constexpr int ArraySize = 64;

    /* Validate constant ranges at compile time */
    static_assert(MaxValue > 0, "MaxValue must be positive");

    CodingStandardsBase() noexcept;
    virtual ~CodingStandardsBase() = default;

    InlinedData Compute() noexcept;
    InlinedData Create() noexcept;
    InlinedData CreateWithMove() noexcept;
    static TextBuffer UseMoveCorrectly(TextBuffer&& source) noexcept;

    /* Use consteval (C++20) for functions that must evaluate at compile time */
    static consteval int Square(int x)
    {
        return x * x;
    }

    /*
     * noexcept for non-throwing functions: Mark functions that
     * cannot throw with noexcept to enable compiler optimizations
     * and document intent.
     *
     * WARNING: Do NOT add noexcept to complex functions whose
     * implementation cannot be guaranteed exception-free. A
     * noexcept contract broken at runtime terminates the process,
     * making it worse than no annotation at all. When in doubt
     * about a non-trivial function, leave noexcept off.
     *
     * In this exception-free engine, use of `new`, dynamic
     * allocation, or external calls are signals to omit noexcept.
     * See TextBuffer for examples: copy construction (uses `new`)
     * intentionally omits noexcept, while move construction (no
     * allocation) correctly marks noexcept.
     *
     * Macros (e.g. Assert) use SCREAMING_SNAKE_CASE and are used
     * only where a function call cannot be substituted.
     *
     * Assert() requires pointer validation before dereference
     * (see ProcessWithAssertion in .cpp). The engine is exception-
     * free — no try/catch/throw is used anywhere.
     */
    static void ProcessNoExcept() noexcept
    {
        Assert(true);
    }

    /*
     * out-prefix parameter: Prefix write-only reference
     * parameters with `out` to distinguish them from inputs.
     */
    static bool TryParse(const char* text, int& outResult) noexcept;

    /*
     * inOut-prefix parameter: Prefix read-write reference
     * parameters with `inOut` to signal modification intent.
     */
    static void ClampToRange(int& inOutValue, int min, int max) noexcept;

    /*
     * Single-line Statements: Avoid unnecessary braces for
     * single-line continue or return statements to improve
     * readability.
     */
    int EvaluateFlag(bool flag) noexcept
    {
        if (flag)
            return 42;
        return 0;
    }

    /*
     * Brace Style: K&R variant. Always break the line before
     * the opening brace: type FunctionName(args) {
     */
    virtual void ProcessBraced() noexcept
    {
        int value = 0;
        if (value > 0)
        {
            value++;
        }
    }

    /*
     * Readability whitespace rules (applied throughout both .h and .cpp):
     *   - Place an empty line before return statements, unless the
     *     return is the only statement within its scope.
     *   - Place an empty line after close brackets }.
     *   - Place an empty line between member variables and methods.
     */

protected:
    static void LogValidationError(const char* message) noexcept;
    [[nodiscard]] static bool ValidateLength(size_t length, size_t maxLength) noexcept;
};

// ========================================================================
// CONCRETE CLASS: CodingStandards
// Extends CodingStandardsBase and implements ICodingStandards.
// Demonstrates concrete-class conventions:
//   - final specifier on classes not intended for inheritance.
//   - public / private access specifiers explicitly declared.
//   - static constexpr members initialized in-class; regular
//     members initialized via constructor member initializer list.
//   - explicit constructor avoided for default/empty constructors
//     (use explicit only for single-argument constructors).
//   - [[nodiscard]] on all getter functions.
//   - override (without virtual) when overriding a virtual function.
//   - Has-a relationship via private member (composition).
//   - Pointer parameters validated before dereference.
// ========================================================================
class CodingStandards final : public CodingStandardsBase, public ICodingStandards
{
public:
    static constexpr int DefaultVersion = 1;
    static constexpr size_t MaxNameLength = 128;

    /* Validate constant ranges at compile time */
    static_assert(DefaultVersion > 0, "DefaultVersion must be positive");
    static_assert(MaxNameLength > 0, "MaxNameLength must be positive");

    CodingStandards() noexcept;
    ~CodingStandards() override;

    // ICodingStandards implementation — override without virtual
    void Validate() const noexcept override;
    void Initialize() noexcept override;
    [[nodiscard]] bool IsValid() const noexcept override;

    // Public methods
    void ProcessBraced() noexcept override;
    void SetData(const CodingStandardsData& newData) noexcept;
    [[nodiscard]] const CodingStandardsData& GetData() const noexcept;
    [[nodiscard]] int GetVersion() const noexcept;

    // Convention demonstration methods (bodies in .cpp)
    /*
     * Prefer passing by reference over raw pointers. When a pointer
     * is required, always validate it before dereference.
     */
    void ProcessWithAssertion(const int* ptr) noexcept;
    void ProcessWithRangeFor(std::vector<int>& values) noexcept;
    [[nodiscard]] int ComputeWithValidation() noexcept;
    void ProcessWithStackBuffer() noexcept;

    /*
     * Error and warning logging: Always output a descriptive log
     * message in error or warning conditions before returning,
     * asserting, or taking corrective action. This provides essential
     * diagnostic information for debugging and post-mortem analysis.
     * See .cpp for the comprehensive rule explanation.
     */
    void ProcessWithErrorLogging() noexcept;

private:
    // Has-a relationship: CodingStandards owns CodingStandardsData
    CodingStandardsData data;
};

} // namespace examples

} // namespace hbe
