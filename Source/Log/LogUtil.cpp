// Created by mooming.go@gmail.com 2022

#include "LogUtil.h"

#include "String/StringBuilder.h"


namespace HE
{
namespace LogUtil
{

const TTimePoint& GetStartTime()
{
    static TTimePoint startTime;
    return startTime;
}

void GetTimeStampString(InlineStringBuilder<64>& outStr
    , const TTimePoint& currentTime)
{
    using namespace std;

    auto diff = currentTime - GetStartTime();
    
    auto hours = chrono::duration_cast<chrono::hours>(diff);
    auto minutes = chrono::duration_cast<chrono::minutes>(diff);
    auto seconds = chrono::duration_cast<chrono::seconds>(diff);
    auto milliSeconds = chrono::duration_cast<chrono::milliseconds>(diff);
    
    auto intHours = hours.count();
    auto intMins = minutes.count() % 60;
    auto intSecs = seconds.count() % 60;
    auto intMSecs = milliSeconds.count() % 1000;

    outStr << std::to_string(intHours);
    outStr << ':';
    outStr << std::to_string(intMins);
    outStr << ':';
    outStr << std::to_string(intSecs);
    outStr << '.';
    outStr << std::to_string(intMSecs);
}

StaticString GetLogLevelString(ELogLevel level)
{
    static StaticString none("None");
    static StaticString verbose("Verbose");
    static StaticString info("Info");
    static StaticString significant("Significant");
    static StaticString warning("Warning");
    static StaticString error("Error");
    static StaticString fatalError("FatalError");
    
    switch (level)
    {
        case ELogLevel::Verbose:
            return verbose;
            
        case ELogLevel::Info:
            return info;
            
        case ELogLevel::Significant:
            return significant;
            
        case ELogLevel::Warning:
            return warning;
            
        case ELogLevel::Error:
            return error;
            
        case ELogLevel::FatalError:
            return fatalError;
            
        default:
            return StaticString();
    }
}

} // LogUtil
} // HE
