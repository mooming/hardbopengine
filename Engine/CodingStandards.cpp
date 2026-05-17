// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "CodingStandards.h"
#include <cstring>
#include <iostream>
#include <utility>

namespace hbe
{

CodingStandardsBase::CodingStandardsBase() {}

/**
 * @brief RVO/NRVO: Return by value
 *
 * Prefer returning objects by value to enable Return Value
 * Optimization (RVO) and Named RVO (NRVO).
 *
 * Why: RVO/NRVO allows the compiler to construct the
 * return value directly in the caller's stack frame, avoiding
 * copy/move operations entirely. Modern compilers apply this
 * optimization automatically for eligible return statements.
 */
CodingStandardsBase::InlinedData CodingStandardsBase::Compute()
{
    return InlinedData(42);
}

/**
 * @brief NRVO: Return local without std::move (efficient)
 *
 * The compiler applies Named Return Value Optimization when a
 * local variable is returned without std::move, constructing it
 * directly in the caller's stack frame and eliminating the 4 KB
 * copy entirely.
 *
 * Why: Without std::move the variable remains an lvalue, eligible
 * for NRVO. The compiler elides the copy/move and constructs the
 * object in place. With a 1024-int (4 KB) InlinedData, this elision
 * eliminates a non-trivial runtime copy.
 */
CodingStandardsBase::InlinedData CodingStandardsBase::Create()
{
    InlinedData result(42);
    return result;
}

/**
 * @brief BAD EXAMPLE: std::move on return prevents NRVO (less efficient)
 *
 * Using std::move forces the return value to be treated as an rvalue,
 * making it ineligible for NRVO. The compiler falls back to the move
 * constructor, performing a real 4 KB copy at runtime.
 *
 * Why: std::move prevents NRVO because NRVO requires the object to
 * be constructed directly in the target location. The explicit move
 * forces a move construction that could have been elided.
 */
CodingStandardsBase::InlinedData CodingStandardsBase::CreateWithMove()
{
    InlinedData result(42);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpessimizing-move"
    return std::move(result);
#pragma clang diagnostic pop
}

/**
 * @brief GOOD EXAMPLE: using std::move where it actually helps
 *
 * Use std::move when transferring ownership into a sink (a function
 * that intends to take ownership via an rvalue reference parameter).
 * This avoids an unnecessary copy when the caller no longer needs
 * the original object.
 *
 * Why: An rvalue reference parameter signals intent to take ownership.
 * Passing std::move(source) makes the caller's intent explicit and
 * enables move semantics, which can be significantly cheaper than a
 * deep copy — especially for heap-allocated types like TextBuffer where
 * a move is a pointer swap (O(1)) versus a deep copy (O(n)).
 *
 * @par Guidance:
 * - Do NOT use std::move on a return statement (breaks NRVO).
 * - Do NOT use std::move on a const object (falls back to copy).
 * - DO use std::move when passing to a sink that takes an rvalue
 *   reference (T&&) and the source will not be used again.
 * - DO use std::move in move constructors and move assignments
 *   to transfer owned resources.
 */
CodingStandardsBase::TextBuffer CodingStandardsBase::UseMoveCorrectly(TextBuffer&& source)
{
    TextBuffer result(std::move(source));
    return result;
}

void CodingStandardsBase::LogValidationError(const char* message)
{
    std::cerr << "Validation Error: " << message << std::endl;
}

bool CodingStandardsBase::ValidateLength(size_t length, size_t maxLength)
{
    return length <= maxLength;
}

CodingStandards::CodingStandards()
    : data{DefaultVersion, MaxNameLength, false}
{
}

CodingStandards::~CodingStandards() {}

void CodingStandards::SetData(const CodingStandardsData& newData)
{
    data = newData;
}

const CodingStandardsData& CodingStandards::GetData() const
{
    return data;
}

int CodingStandards::GetVersion() const
{
    return data.version;
}

void CodingStandards::Validate() const {}

void CodingStandards::Initialize()
{
    data.isInitialized = true;
}

bool CodingStandards::IsValid() const
{
    return data.isInitialized;
}

CodingStandardsBase::TextBuffer::TextBuffer()
    : data(nullptr)
    , length(0)
{
}

CodingStandardsBase::TextBuffer::TextBuffer(const char* text)
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

CodingStandardsBase::TextBuffer::TextBuffer(const TextBuffer& other)
    : data(nullptr)
    , length(other.length)
{
    if (other.data != nullptr)
    {
        data = new char[length + 1];
        std::memcpy(data, other.data, length + 1);
    }
}

CodingStandardsBase::TextBuffer::TextBuffer(TextBuffer&& other) noexcept
    : data(other.data)
    , length(other.length)
{
    other.data = nullptr;
    other.length = 0;
}

CodingStandardsBase::TextBuffer& CodingStandardsBase::TextBuffer::operator=(const TextBuffer& other)
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

CodingStandardsBase::TextBuffer& CodingStandardsBase::TextBuffer::operator=(TextBuffer&& other) noexcept
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

CodingStandardsBase::TextBuffer::~TextBuffer()
{
    delete[] data;
}

const char* CodingStandardsBase::TextBuffer::GetText() const
{
    return data != nullptr ? data : "";
}

} // namespace hbe
