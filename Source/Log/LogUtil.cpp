// Created by mooming.go@gmail.com 2022

#include "LogUtil.h"

#include "HSTL/HStringStream.h"
#include <sstream>


namespace HE
{
namespace LogUtil
{
HSTL::HInlineString<64> GetTimeStampString(const LogUtil::TTimePoint& startTime
    , const LogUtil::TTimePoint& currentTime)
{
    using namespace std;
    
    auto diff = currentTime - startTime;
    
    auto hours = chrono::duration_cast<chrono::hours>(diff);
    auto minutes = chrono::duration_cast<chrono::minutes>(diff);
    auto seconds = chrono::duration_cast<chrono::seconds>(diff);
    auto milliSeconds = chrono::duration_cast<chrono::milliseconds>(diff);
    
    int intHours = hours.count();
    int intMins = minutes.count() % 60;
    int intSecs = seconds.count() % 60;
    int intMSecs = milliSeconds.count() % 1000;
    
    HSTL::HInlineString<64> str;

    str += std::to_string(intHours);
    str += ':';
    str += std::to_string(intMins);
    str += ':';
    str += std::to_string(intSecs);
    str += '.';
    str += std::to_string(intMSecs);

    return str.c_str();
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
