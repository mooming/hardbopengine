// Created by mooming.go@gmail.com 2022

#include "LogLine.h"

#include "Engine.h"
#include "Config/BuildConfig.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"
#include <chrono>
#include <cstring>


namespace HE
{
LogLine::LogLine()
    : level(ELogLevel::Info)
    , isLong(false)
    , longText(nullptr)
{
    text[0] = '\0';
    text[Config::LogLineLength - 1] = '\0';
}

LogLine::LogLine(LogLine&& rhs)
    : timeStamp(rhs.timeStamp)
    , threadName(rhs.threadName)
    , category(rhs.category)
    , level(rhs.level)
    , isLong(rhs.isLong)
{
    if (unlikely(isLong))
    {
        longText = rhs.longText;
        rhs.isLong = false;
        rhs.text[0] = '\0';

        return;
    }

    std::copy(std::begin(rhs.text), std::end(rhs.text), std::begin(text));
}

LogLine::LogLine(ELogLevel level, StaticString threadName, StaticString category, const char* inText)
    : timeStamp(std::chrono::steady_clock::now())
    , threadName(threadName)
    , category(category)
    , level(level)
    , isLong(false)
{
    if (unlikely(inText == nullptr))
    {
        text[0] = '\0';
        return;
    }

#ifdef PROFILE_ENABLED
    {
        auto& engine = Engine::Get();
        auto& stat = engine.GetStatistics();
        stat.IncLogCount();
    }
#endif // PROFILE_ENABLED

    constexpr size_t LastIndex = Config::LogLineLength - 1;
    const auto length = StringUtil::StrLen(inText, LastIndex);

    std::copy(&inText[0], &inText[length], std::begin(text));
    text[length] = '\0';
}

LogLine::LogLine(ELogLevel level, StaticString threadName, StaticString category, bool isLong, const char* inText)
    : timeStamp(std::chrono::steady_clock::now())
    , threadName(threadName)
    , category(category)
    , level(level)
    , isLong(isLong)
{
    if (unlikely(inText == nullptr))
    {
        text[0] = '\0';
        return;
    }

    if (likely(isLong))
    {
#ifdef PROFILE_ENABLED
        auto& engine = Engine::Get();
        auto& stat = engine.GetStatistics();
        stat.IncLongLogCount();
#endif // PROFILE_ENABLED

        const auto length = StringUtil::StrLen(inText);
        longText = (char*)malloc(length + 1);
        std::copy(&inText[0], &inText[length], longText);
        longText[length] = '\0';

        return;
    }

#ifdef PROFILE_ENABLED
    {
        auto& engine = Engine::Get();
        auto& stat = engine.GetStatistics();
        stat.IncLogCount();
    }
#endif // PROFILE_ENABLED

    constexpr size_t LastIndex = Config::LogLineLength - 1;
    const auto length = StringUtil::StrLen(inText, LastIndex);
    std::copy(&inText[0], &inText[length], text);
    text[length] = '\0';
}

LogLine::~LogLine()
{
    if (likely(!isLong))
        return;

    Assert(longText != nullptr);
    free(longText);
}

const char* LogLine::GetText() const
{
    if (unlikely(isLong))
    {
        return longText;
    }

    return text;
}

} // HE
