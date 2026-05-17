// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <string>
#include <vector>

#include "Core/Debug.h"

namespace hbe
{

// ========================================================================
// POD STRUCT: CodingStandardsData
// Demonstrates POD conventions: trivial, standard layout,
// no virtual or user-defined special members, brace-init ready.
// ========================================================================
struct CodingStandardsData
{
    int version;
    size_t maxNameLength;
    bool isInitialized;
};

// ========================================================================
// INTERFACE: ICodingStandards
// Demonstrates interface conventions: I-prefix, pure virtual (= 0),
// virtual destructor, abstract contract.
// ========================================================================
/**
 * @brief Interfaces should be pure virtual — every method except the
 * virtual destructor must be declared with `= 0`.
 *
 * A pure virtual interface enforces a strict contract: it defines what
 * a type can do without prescribing how or what state it holds. This
 * decouples abstraction from implementation, allowing full substitution
 * of any conforming type.
 *
 * Why: A non-pure (blank) virtual in an interface is ambiguous —
 * it blurs the line between a base with optional defaults and a contract.
 * Pure `= 0` forces every derived class to explicitly provide an
 * implementation, eliminating the risk of accidentally relying on a
 * base default that may not be semantically correct for all types.
 * The `I` prefix makes the interface nature visible at the declaration
 * site without requiring the reader to scan the member list.
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
// BASE CLASS: CodingStandardsBase
// Demonstrates base class conventions: protected helpers, static members,
// constexpr, consteval, noexcept, RVO/NRVO, single-line statements,
// K&R brace style.
// ========================================================================
class CodingStandardsBase
{
public:
    struct InlinedData
    {
        int buffer[1024];

        explicit InlinedData(int value = 0)
        {
            for (auto& v : buffer)
            {
                v = value;
            }
        }
    };

    class TextBuffer final
    {
    public:
        TextBuffer();
        explicit TextBuffer(const char* text);
        TextBuffer(const TextBuffer& other);
        TextBuffer(TextBuffer&& other) noexcept;
        TextBuffer& operator=(const TextBuffer& other);
        TextBuffer& operator=(TextBuffer&& other) noexcept;
        ~TextBuffer();

        [[nodiscard]] const char* GetText() const;

    private:
        char* data;
        size_t length;
    };

    static constexpr int MaxValue = 100;
    static constexpr int ArraySize = 64;

    CodingStandardsBase();
    virtual ~CodingStandardsBase() = default;

    InlinedData Compute();
    InlinedData Create();
    InlinedData CreateWithMove();
    static TextBuffer UseMoveCorrectly(TextBuffer&& source);

    /**
     * @brief constexpr and consteval
     *
     * Use constexpr for compile-time computable values. Use
     * consteval (C++20) for functions that must compute at
     * compile time.
     *
     * Why: constexpr enables compile-time evaluation,
     * reducing runtime overhead. consteval guarantees compile-time
     * evaluation and provides better error messages for
     * invalid constant expressions.
     */
    static consteval int Square(int x)
    {
        return x * x;
    }

    /**
     * @brief noexcept for non-throwing functions
     *
     * Mark functions that cannot throw with noexcept. This
     * enables compiler optimizations and documents intent.
     *
     * Why: noexcept (C++11) allows the compiler to
     * perform optimizations like moving instead of copying
     * when throwing is impossible. It also enables compile-time
     * checks for exception specifications.
     */
    static void ProcessNoExcept() noexcept
    {
        Assert(true);
    }

    /**
     * @brief Single-line Statements: Avoid unnecessary braces
     *
     * For single-line continue or return statements, omit braces
     * to improve readability.
     *
     * Why: Unnecessary braces on single-line statements add
     * visual noise without providing benefit. However, when adding
     * code to a single-line block, braces help prevent bugs.
     */
    int EvaluateFlag(bool flag)
    {
        if (flag)
            return 42;
        return 0;
    }

    /**
     * @brief Brace Style: K&R variant
     *
     * Always break line before opening brace:
     * `type FunctionName(args) {`
     *
     * Why: K&R style places the opening brace on a new line for
     * functions, creating visual separation and following established
     * C++ conventions for better readability.
     */
    void ProcessBraced()
    {
        int value = 0;
        if (value > 0)
        {
            value++;
        }
    }

protected:
    static void LogValidationError(const char* message);
    [[nodiscard]] static bool ValidateLength(size_t length, size_t maxLength);
};

// ========================================================================
// CONCRETE CLASS: CodingStandards
// Extends CodingStandardsBase and implements ICodingStandards.
// Demonstrates: final, override, has-a, access specifiers,
// constructor init, getters, naming, error handling, optimization.
// ========================================================================
class CodingStandards final : public CodingStandardsBase, public ICodingStandards
{
public:
    // ====================================================================
    // Naming conventions
    // ====================================================================

    struct GoodClass
    {
        void ProcessData() {}
    };

    /**
     * @brief BAD EXAMPLE: lowercase class names
     *
     * Using lowercase violates naming conventions and makes types harder to
     * distinguish from variables in C++.
     *
     * Why: C++ has no built-in type name resolution, so lowercase
     * class names blend with variable names, reducing code clarity.
     */
    struct BadClass
    {
        void processdata() {}
    };

    struct GoodVariables
    {
        int defaultValue;
        bool isDone;
    };

    struct BadVariables
    {
        int m_value;
        bool bIsDone;
    };

    using TData = CodingStandardsData;
    using TLogOutFunc = void (*)();
    using TAllocFunc = void (*)(void*, size_t);

    static constexpr int DefaultVersion = 1;
    static constexpr size_t MaxNameLength = 128;

    // ====================================================================
    // Class conventions
    // ====================================================================

    struct GoodMemberOrdering
    {
        static constexpr int ClassVersion = 42;
        public: int publicData;
        protected: int protectedData;
    };

    /**
     * @brief Access Specifiers
     *
     * Always explicitly define access specifiers for all classes and structs.
     * Even if members default to public in structs, explicit specifiers
     * improve clarity and prevent accidental exposure.
     *
     * Why: In structs, members default to public. Explicit specifiers
     * prevent accidental member exposure during maintenance and clearly
     * document the intended interface boundary.
     */
    struct GoodAccessSpecifiers
    {
        public: void PublicMethod() {}
        protected: void ProtectedMethod() {}
        private: void PrivateMethod() {}
    };

    /**
     * @brief BAD EXAMPLE: Missing explicit access specifiers
     *
     * Without explicit access specifiers, it's unclear which members are
     * intended to be public interface vs implementation details.
     *
     * Why: C++ structs default to public, but this implicit behavior
     * makes the design intent unclear and can lead to accidental exposure
     * of internal implementation.
     */
    struct BadAccessSpecifiers
    {
        void HelperMethod() {}
        int internalValue;
    };

    /**
     * @brief Getters: [[nodiscard]] attribute
     *
     * Mark getter functions with [[nodiscard]] to prevent accidental
     * discarding of return values, which often indicates a logic error.
     *
     * Why: [[nodiscard]] was added in C++17 specifically to catch
     * cases where return values are ignored. For pure getters, discarding
     * the result typically indicates a bug.
     */
    struct GoodGetter
    {
        [[nodiscard]] int GetValue() const { return 0; }
    };

    /**
     * @brief Initialization: Prefer constructors
     *
     * Avoid in-class initialization except for constexpr values.
     * Use constructors for member initialization to ensure proper
     * setup and validation.
     *
     * Why: In-class initialization was added in C++11 but creates
     * ambiguity when both in-class init and constructor init are present.
     * Constructor initialization is more explicit and allows validation.
     */
    struct GoodInitialization
    {
        int value;
        GoodInitialization(int v) : value(v) {}
    };

    struct BadInitialization
    {
        int value;
        std::string name;
    };

    /**
     * @brief Inline Functions
     *
     * Do not use the 'inline' keyword explicitly. Modern compilers
     * automatically optimize based on complexity and call patterns.
     *
     * Why: The 'inline' keyword in C++ serves only to allow
     * multiple definitions across translation units, not to force
     * inlining. Compiler heuristics are better at deciding when to
     * inline than manual placement.
     */
    struct GoodInline
    {
        void Process() {}
    };

    /**
     * @brief Virtual Functions: Do not use 'virtual' with 'override'
     *
     * When a function is marked 'override', the 'virtual' keyword is
     * redundant and can be omitted for clarity.
     *
     * Why: 'override' implies virtual behavior. Including both
     * keywords is redundant since 'override' already indicates the
     * function is virtual and overrides a base class method.
     */
    struct GoodVirtualBase
    {
        virtual void BaseMethod() {}
    };

    struct GoodVirtualOverride final : GoodVirtualBase
    {
        void BaseMethod() override {}
    };

    // ====================================================================
    // Error handling & memory management
    // ====================================================================

    /**
     * @brief Assertions: Use Assert() and FatalAssert()
     *
     * Use Assert() from Core/Debug.h for runtime checks that should
     * only trigger in debug builds. FatalAssert() for critical errors
     * that always terminate.
     *
     * Why: Assertions catch programmer errors at runtime rather
     * than allowing undefined behavior to occur. They document
     * assumptions and fail fast in debug builds.
     */
    struct GoodAssertions
    {
        static int DummyCompute(int v)
        {
            return v;
        }

        void Process(int* ptr)
        {
            Assert(ptr != nullptr);
            int value = DummyCompute(*ptr);
            Assert(value > 0);
        }
    };

    /**
     * @brief Exception-free Design
     *
     * The engine is exception-free. Do not use try/catch or throw.
     * Use return codes, optional types, or assertion-based error
     * handling instead.
     *
     * Why: Exceptions add overhead even when not thrown, can
     * complicate code flow, and make resource management more complex.
     * Error codes and assertions provide clearer control flow.
     */
    struct GoodExceptionFree
    {
        enum class ErrorCode { Success, NotFound, Invalid };

        ErrorCode Process(bool condition)
        {
            if (condition)
                return ErrorCode::Success;
            return ErrorCode::NotFound;
        }
    };

    /**
     * @brief BAD EXAMPLE: Using C++ exceptions
     *
     * Throwing exceptions violates the engine's exception-free policy.
     * Exceptions cause stack unwinding, require try/catch blocks,
     * and add overhead.
     *
     * Why: C++ exceptions require compiler support for stack
     * unwinding, preventing certain optimizations and adding runtime
     * overhead even when exceptions aren't thrown.
     */
    struct BadExceptionUsage
    {
        void Process(bool condition)
        {
            if (!condition)
                throw std::runtime_error("Operation failed");
        }
    };

    /**
     * @brief Pointers: Prefer references over raw pointers
     *
     * Pass objects by reference rather than raw pointers whenever
     * possible. References always point to valid objects (by convention).
     *
     * Why: References cannot be null (by design), cannot be
     * reseated, and have clear lifetime semantics. Raw pointers
     * require null checks and don't convey ownership semantics.
     */
    struct GoodPointers
    {
        void ProcessData(int& value) { value++; }
    };

    /**
     * @brief BAD EXAMPLE: Raw pointers for input parameters
     *
     * Raw pointers don't convey nullability semantics and require
     * defensive null checks at every use site.
     *
     * Why: A pointer parameter could be null, expired, or
     * aliased. Without documentation, callers don't know the
     * nullability contract. References eliminate this ambiguity.
     */
    struct BadPointers
    {
        void ProcessData(int* value) { (*value)++; }
    };

    struct GoodConstants
    {
        static constexpr int MaxValue2 = 100;
        const int DefaultValue;
    };

    struct BadConstants
    {
        int maxValue;
        int defaultValue;
    };

    /**
     * @brief Scoping: Use braces to define variable scope
     *
     * Use explicit braces {} to limit the lifetime of temporary
     * variables, making code intent clearer and preventing accidents.
     *
     * Why: C++ has block scope. Explicit braces limit variable
     * lifetimes, reduce cognitive load, and prevent accidental use
     * of variables beyond their intended scope.
     */
    struct GoodScoping
    {
        static int ComputeValue() { return 42; }

        void Process()
        {
            {
                int temp = ComputeValue();
                Assert(temp > 0);
            }
        }
    };

    /**
     * @brief Safety: Validate inputs and use local variables
     *
     * Validate pointer validity before use. Use local variables to
     * capture and check return values. Put error logs in error
     * conditions.
     *
     * Why: Defensive programming prevents undefined behavior.
     * Local variables capture values before use, preventing issues
     * with async modifications or dangling references.
     */
    struct GoodSafety
    {
        static int ComputeValue() { return -1; }

        void Process(int& result)
        {
            int value = ComputeValue();
            if (value < 0)
            {
                Assert(false && "Invalid value computed");
                return;
            }
            result = value;
        }
    };

    // ====================================================================
    // Optimization & move semantics
    // ====================================================================

    /**
     * @brief BAD EXAMPLE: Returning by pointer
     *
     * Returning raw pointers prevents RVO/NRVO and introduces
     * potential lifetime issues. Callers must manage memory.
     *
     * Why: Pointers don't support RVO/NRVO since they point
     * to existing objects. The caller must manage allocation and
     * lifetime, and null checks are required at every call site.
     */
    struct BadReturnByPointer
    {
        struct ReturnByPointerResult { int value; };

        ReturnByPointerResult* Compute()
        {
            return new ReturnByPointerResult{42};
        }
    };

    /**
     * @brief BAD EXAMPLE: Unnecessary std::move
     *
     * Do not use std::move on return statements where NRVO applies.
     * The compiler applies NRVO before std::move would be considered.
     *
     * Why: std::move on a local variable prevents NRVO because
     * it makes the variable an rvalue, but NRVO requires the object to
     * be constructed directly in the target location. Explicit move
     * can actually prevent this optimization.
     */
    struct BadUnnecessaryMove
    {
        struct UnnecessaryMoveResult { int value; };

        static UnnecessaryMoveResult Create()
        {
            UnnecessaryMoveResult result{42};
            return result;
        }
    };

    /**
     * @brief Avoid dynamic allocations
     *
     * Prefer stack allocation over heap allocation. Use fixed-size
     * containers and avoid new/delete where possible.
     *
     * Why: Dynamic allocation has overhead (allocation time,
     * fragmentation, cache unfriendliness) and requires careful
     * lifetime management. Stack allocation is faster and
     * automatically managed.
     */
    struct GoodAllocationAvoidance
    {
        void Process()
        {
            int buffer[256];
            for (int i = 0; i < 256; ++i)
            {
                buffer[i] = i;
            }
        }
    };

    /**
     * @brief Range-based for loops
     *
     * Prefer range-based for loops when iterating over containers
     * to improve readability and prevent off-by-one errors.
     *
     * Why: Range-based for (C++11) automatically handles
     * begin/end iteration, reducing boilerplate and eliminating
     * common errors like using <= instead of < in loop conditions.
     */
    struct GoodRangeFor
    {
        void Process(std::vector<int>& values)
        {
            for (const auto& v : values)
            {
                Assert(v >= 0);
            }
        }
    };

    /**
     * @brief enum class instead of plain enum
     *
     * Use enum class for type-safe enumerations with explicit
     * scope, preventing implicit conversions to int.
     *
     * Why: Plain enums implicitly convert to int, causing
     * accidental mixing with other integer types. enum class
     * (C++11) provides strong typing and scoped names.
     */
    struct GoodEnumClass
    {
        enum class ErrorCode { None, NotFound, Invalid, Internal };

        ErrorCode Process()
        {
            return ErrorCode::None;
        }
    };

    struct GoodBraceInit
    {
        int value;
        std::string name;
    };

    CodingStandards();
    ~CodingStandards() override;

    // ICodingStandards implementation (override without 'virtual')
    void Validate() const override;
    void Initialize() override;
    [[nodiscard]] bool IsValid() const override;

    // Public methods: getters marked [[nodiscard]]
    void SetData(const CodingStandardsData& newData);
    [[nodiscard]] const CodingStandardsData& GetData() const;
    [[nodiscard]] int GetVersion() const;

private:
    // Has-a relationship: CodingStandards owns CodingStandardsData
    CodingStandardsData data;
};

} // namespace hbe
