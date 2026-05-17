// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "CodingStandards.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>


namespace hbe
{
namespace examples
{

CodingStandardsBase::CodingStandardsBase() noexcept {}

bool CodingStandardsBase::TryParse(const char* text, int& outResult) noexcept
{
    /*
     * out-prefix: `outResult` is written to but never read — the
     * caller receives the parsed value through this reference.
     */
    if (text == nullptr)
        return false;

    char* end = nullptr;
    long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0')
        return false;

    outResult = static_cast<int>(parsed);
    return true;
}

void CodingStandardsBase::ClampToRange(int& inOutValue, int min, int max) noexcept
{
    /*
     * inOut-prefix: `inOutValue` is both read and written, making
     * it an in-out parameter.
     */
    if (inOutValue < min)
        inOutValue = min;
    else if (inOutValue > max)
        inOutValue = max;
}

InlinedData::InlinedData(int value) noexcept
{
    for (auto& v : buffer)
    {
        v = value;
    }
}

DataProcessor::DataProcessor(int initialValue) noexcept
    : value(initialValue)
{
}

void DataProcessor::Process() noexcept
{
    for (auto& v : workBuffer)
    {
        v = value;
    }
}

int DataProcessor::GetValue() const noexcept
{
    return value;
}

void DataProcessor::SetValue(int inValue) noexcept
{
    /*
     * in-prefix avoids name collision with the member `value`,
     * making the assignment clear and unambiguous.
     */
    value = inValue;
}

/*
 * RVO/NRVO: Return by value. Prefer returning objects by value
 * to enable Return Value Optimization (RVO) and Named Return
 * Value Optimization (NRVO).
 */
InlinedData CodingStandardsBase::Compute() noexcept
{
    return InlinedData(42);
}

/*
 * NRVO: Return local without std::move (efficient). The compiler
 * applies NRVO when a local variable is returned as an lvalue,
 * constructing the 4 KB InlinedData directly in the caller's
 * stack frame — no copy or move occurs.
 */
InlinedData CodingStandardsBase::Create() noexcept
{
    InlinedData result(42);

    return result;
}

/*
 * BAD EXAMPLE: std::move on return prevents NRVO (less efficient).
 * std::move converts the local into an rvalue, making it ineligible
 * for NRVO. The compiler falls back to a real 4 KB move.
 */
InlinedData CodingStandardsBase::CreateWithMove() noexcept
{
    InlinedData result(42);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpessimizing-move"
    return std::move(result);
#pragma clang diagnostic pop
}

/*
 * GOOD EXAMPLE: using std::move where it actually helps. Pass
 * std::move(source) to sink parameters (T&&) to enable O(1)
 * pointer-swap moves instead of O(n) deep copies.
 *
 * Guidance:
 * - Do NOT use std::move on a return statement (breaks NRVO).
 * - Do NOT use std::move on a const object (falls back to copy).
 * - DO use std::move when passing to a T&& sink parameter.
 * - DO use std::move in move constructors and move assignments.
 */
TextBuffer CodingStandardsBase::UseMoveCorrectly(TextBuffer&& source) noexcept
{
    TextBuffer result(std::move(source));

    return result;
}

/*
 * Logging in error/warning conditions. Always output a descriptive
 * log message before returning, asserting, or taking corrective
 * action when an error or warning is detected. A bare Assert() or
 * silent early-return makes it difficult to diagnose what went wrong
 * and where.
 *
 * Guidance:
 * - Include the specific reason and relevant state values in the
 *   log message so it is actionable.
 * - Log *before* any Assert() or return so the information
 *   survives even if the assertion terminates the process.
 * - Use distinct prefixes ("Error:" vs "Warning:") so messages
 *   can be filtered by severity.
 * - Do not log in hot paths where the condition is expected and
 *   handled silently; reserve logging for truly exceptional or
 *   unexpected conditions.
 *
 * In this example, an input value exceeding the expected range
 * triggers a warning log before corrective action (clamping).
 * The critical invariant is still asserted at the end.
 */
void CodingStandards::ProcessWithErrorLogging() noexcept
{
    int inputSize = 200;

    if (inputSize > static_cast<int>(MaxNameLength))
    {
        std::cerr << "Warning: inputSize (" << inputSize
                  << ") exceeds MaxNameLength (" << MaxNameLength
                  << "). Clamping to MaxNameLength." << std::endl;
        inputSize = MaxNameLength;
    }

    Assert(inputSize >= 0);
}

/*
 * Error logging helper. Logs a validation error message via std::cerr
 * before the caller handles the failure. Demonstrates the rule:
 * always log in error conditions.
 *
 * Distinct prefix ("Validation Error:") makes the message
 * identifiable and filterable by severity.
 */
void CodingStandardsBase::LogValidationError(const char* message) noexcept
{
    std::cerr << "Validation Error: " << message << std::endl;
}

bool CodingStandardsBase::ValidateLength(size_t length, size_t maxLength) noexcept
{
    return length <= maxLength;
}

CodingStandards::CodingStandards() noexcept
    : data{DefaultVersion, MaxNameLength, false}
{
}

CodingStandards::~CodingStandards() = default;

void CodingStandards::ProcessBraced() noexcept {}

void CodingStandards::SetData(const CodingStandardsData& newData) noexcept
{
    data = newData;
}

const CodingStandardsData& CodingStandards::GetData() const noexcept
{
    return data;
}

int CodingStandards::GetVersion() const noexcept
{
    return data.version;
}

void CodingStandards::Validate() const noexcept {}

void CodingStandards::Initialize() noexcept
{
    data.isInitialized = true;
}

bool CodingStandards::IsValid() const noexcept
{
    return data.isInitialized;
}

/*
 * Assertions with pointer validation: Uses Assert() and
 * validates pointers before dereference — demonstrating
 * assertion-based defensive programming.
 */
void CodingStandards::ProcessWithAssertion(const int* ptr) noexcept
{
    Assert(ptr != nullptr);
    Assert(*ptr > 0);
}

/*
 * Range-based for with validation: Prefer range-based for
 * when iterating over containers, combined with Assert()
 * for invariant checking.
 */
void CodingStandards::ProcessWithRangeFor(std::vector<int>& values) noexcept
{
    for (const auto& v : values)
    {
        Assert(v >= 0);
    }
}

/*
 * Scoped variable with safety validation: Uses explicit
 * braces to narrow variable lifetime and validates return
 * values before using them.
 */
int CodingStandards::ComputeWithValidation() noexcept
{
    {
        int temp = DefaultVersion;
        if (temp < 0)
        {
            Assert(false && "Invalid default version");
            return -1;
        }
    }

    return MaxNameLength;
}

/*
 * Stack allocation: Prefer fixed-size stack buffers over
 * dynamic allocation.
 */
void CodingStandards::ProcessWithStackBuffer() noexcept
{
    int buffer[256];
    for (int i = 0; i < 256; ++i)
    {
        buffer[i] = i * i;
    }
}

TextBuffer::TextBuffer() noexcept
    : data(nullptr)
    , length(0)
{
}

TextBuffer::TextBuffer(const char* text)
    : data(nullptr)
    , length(0)
{
    if (text != nullptr)
    {
        length = std::strlen(text);
        data = new char[length + 1];
        std::memcpy(data, text, length + 1);
    }
}

TextBuffer::TextBuffer(const TextBuffer& other)
    : data(nullptr)
    , length(other.length)
{
    if (other.data != nullptr)
    {
        data = new char[length + 1];
        std::memcpy(data, other.data, length + 1);
    }
}

TextBuffer::TextBuffer(TextBuffer&& other) noexcept
    : data(other.data)
    , length(other.length)
{
    other.data = nullptr;
    other.length = 0;
}

TextBuffer& TextBuffer::operator=(const TextBuffer& other)
{
    if (this != &other)
    {
        delete[] data;
        length = other.length;
        if (other.data != nullptr)
        {
            data = new char[length + 1];
            std::memcpy(data, other.data, length + 1);
        }
        else
        {
            data = nullptr;
        }
    }

    return *this;
}

TextBuffer& TextBuffer::operator=(TextBuffer&& other) noexcept
{
    if (this != &other)
    {
        delete[] data;
        data = other.data;
        length = other.length;
        other.data = nullptr;
        other.length = 0;
    }

    return *this;
}

TextBuffer::~TextBuffer()
{
    delete[] data;
}

const char* TextBuffer::GetText() const noexcept
{
    return data != nullptr ? data : "";
}

} // namespace examples
} // namespace hbe
