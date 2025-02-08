// Created by mooming.go@gmail.com

#include "LogLine.h"

#include "Config/BuildConfig.h"
#include "Engine.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"
#include <chrono>

namespace HE
{

    LogLine::LogLine()
        : level(ELogLevel::Info),
          isLong(false),
          longText(nullptr)
    {
        text[0] = '\0';
        text[Config::LogLineLength - 1] = '\0';
    }

    LogLine::LogLine(LogLine&& rhs)
        : timeStamp(rhs.timeStamp),
          threadName(rhs.threadName),
          category(rhs.category),
          level(rhs.level),
          isLong(rhs.isLong)
    {
        if (unlikely(isLong))
        {
            longText = rhs.longText;
            longTextSize = rhs.longTextSize;

            rhs.isLong = false;
            rhs.longText = nullptr;
            rhs.longTextSize = 0;
            rhs.text[0] = '\0';

            return;
        }

        std::copy(std::begin(rhs.text), std::end(rhs.text), std::begin(text));
    }

    LogLine::LogLine(ELogLevel level, StaticString threadName,
        StaticString category, const char* inText, size_t size)
        : timeStamp(std::chrono::steady_clock::now()),
          threadName(threadName),
          category(category),
          level(level),
          isLong(size >= (Config::LogLineLength - 1))
    {
        if (unlikely(inText == nullptr))
        {
            text[0] = '\0';
            return;
        }

        if (likely(isLong))
        {
            auto& engine = Engine::Get();

#ifdef PROFILE_ENABLED
            auto& stat = engine.GetStatistics();
            stat.IncLongLogCount();
#endif // PROFILE_ENABLED

            auto& mmgr = engine.GetMemoryManager();
            longTextSize = size + 1;
            longText = (char*)mmgr.SysAllocate(longTextSize);
            std::copy(&inText[0], &inText[longTextSize], longText);

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

    LogLine::~LogLine()
    {
        if (likely(!isLong))
        {
            return;
        }

        Assert(longText != nullptr);
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.SysDeallocate(longText, longTextSize);
    }

    const char* LogLine::GetText() const
    {
        if (unlikely(isLong))
        {
            return longText;
        }

        return text;
    }

} // namespace HE
