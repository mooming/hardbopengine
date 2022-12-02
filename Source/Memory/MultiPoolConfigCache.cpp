// Created by mooming.go@gmail.com, 2022

#include "MultiPoolConfigCache.h"

#include "HSTL/HString.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "Resource/Buffer.h"
#include "Resource/BufferInputStream.h"
#include "Resource/BufferOutputStream.h"
#include "String/StringUtil.h"
#include <exception>


namespace HE
{

StaticString MultiPoolConfigCache::GetClassName() const
{
    using namespace StringUtil;
    static StaticString className(ToCompactClassName(__PRETTY_FUNCTION__));

    return className;
}

void MultiPoolConfigCache::Swap(TCacheContainer& inOutData)
{
    std::swap(data, inOutData);
}

size_t MultiPoolConfigCache::Serialize(Buffer& outBuffer)
{
    using namespace StringUtil;
    auto log = Logger::Get(ToMethodName(__PRETTY_FUNCTION__));

    BufferOutputStream bos(outBuffer);

    StaticString className = GetClassName();
    const char* classNameStr = className.c_str();
    bos << classNameStr;
    bos << GetVersion();

    auto& data = GetData();
    std::sort(data.begin(), data.end());

    auto MergeBlocks = [](auto& a)
    {
        std::sort(a.begin(), a.end());

        TPoolConfigs temp;
        std::swap(a, temp);

        size_t blockSize = 0;
        size_t numBlocks = 0;

        for (auto& config : temp)
        {
            if (config.blockSize != blockSize)
            {
                if (blockSize > 0 && numBlocks > 0)
                {
                    a.emplace_back(blockSize, numBlocks);
                }

                blockSize = config.blockSize;
                numBlocks = 0;
            }

            numBlocks += config.numberOfBlocks;
        }

        if (blockSize > 0 && numBlocks > 0)
        {
            a.emplace_back(blockSize, numBlocks);
        }
    };

    auto MergeMax = [MergeBlocks](auto& a, auto& b)
    {
        MergeBlocks(a);

        if (b.size() == 0)
            return;

        MergeBlocks(b);

        size_t bIndex = 0;
        const size_t bLen = b.size();

        auto iterate = [&]()
        {
            if (bIndex >= bLen)
                return;

            size_t aIndex = 0;
            const size_t aLen = a.size();

            while (aIndex < aLen && bIndex < bLen)
            {
                auto aBlockSize = a[aIndex].blockSize;
                while (b[bIndex].blockSize < aBlockSize)
                {
                    a.push_back(b[bIndex]);
                    ++bIndex;

                    if (bIndex >= bLen)
                        return;
                }

                auto bBlockSize = b[bIndex].blockSize;
                if (bBlockSize == aBlockSize)
                {
                    auto aNumBlocks = a[aIndex].numberOfBlocks;
                    auto bNumBlocks = b[bIndex].numberOfBlocks;
                    a[aIndex].numberOfBlocks = std::max(aNumBlocks, bNumBlocks);
                    ++bIndex;

                    if (bIndex >= bLen)
                        return;
                }

                ++aIndex;
            }
        };

        iterate();

        while (bIndex < bLen)
        {
            a.push_back(b[bIndex]);
            ++bIndex;
        }
    };

    {
        TCacheContainer tempContainer;
        std::swap(data, tempContainer);

        StaticStringID itemName;
        TPoolConfigs tempConfigs;

        for (auto& item : tempContainer)
        {
            if (itemName != item.uniqueName)
            {
                if (tempConfigs.size() > 0)
                {
                    MergeBlocks(tempConfigs);
                    data.emplace_back(itemName, std::move(tempConfigs));
                }

                itemName = item.uniqueName;
                tempConfigs.clear();
            }

            auto& configs = item.configs;
            MergeMax(tempConfigs, configs);
        }
    }

    for (auto& item : data)
    {
        auto& configs = item.configs;
        std::sort(configs.begin(), configs.end());
    }

    size_t cacheSize = data.size();
    bos << cacheSize;

    if (unlikely(bos.HasError()))
    {
        log.OutError([](auto& ls)
        {
            ls << "An error occured while streaming out header data.";
        });

        return 0;
    }

    for (auto& item : data)
    {
        StaticString name(item.uniqueName);

        auto nameStr = name.c_str();
        bos << nameStr;

        auto& configs = item.configs;
        bos << configs.size();

        for (auto& config : configs)
        {
            bos << config.blockSize;
            bos << config.numberOfBlocks;

            log.Out([nameStr, &config](auto& ls)
            {
                ls << nameStr << " : block = " << config.blockSize
                    << ", n = " << config.numberOfBlocks;
            });
        }

        if (unlikely(bos.HasError()))
        {
            log.OutError([](auto& ls)
            {
                ls << "An error occured while streaming out pool config data";
            });

            return 0;
        }
    }

    return bos.GetCursor();
}

bool MultiPoolConfigCache::Deserialize(const Buffer& buffer)
{
    using namespace StringUtil;
    static StaticString logName(ToMethodName(__PRETTY_FUNCTION__));
    auto log = Logger::Get(logName);

    BufferInputStream bis(buffer);

    StaticString className;
    bis >> className;

    if (unlikely(className != GetClassName()))
    {
        log.OutError([this, className](auto& ls)
        {
            ls << "Invalid class name " << className << ", "
                << GetClassName() << " is expected.";
        });

        return false;
    }

    TVersion inVersion = 0;
    bis >> inVersion;

    if (unlikely(version != inVersion))
    {
        log.OutError([inVersion](auto& ls)
        {
            ls << "Version mismatched! Read version = " << inVersion
                << ", " << version << " is expected.";
        });

        return false;
    }

    size_t size = 0;
    bis >> size;

    if (unlikely(bis.HasError()))
    {
        log.OutError("Input stream failure.");
        return false;
    }

    data.reserve(size);

    for (size_t i = 0; i < size; ++i)
    {
        StaticString key;
        bis >> key;

        size_t configSize = 0;
        bis >> configSize;

        MemoryManager::TVector<PoolConfig> configs;
        configs.reserve(configSize);

        for (size_t j = 0; j < configSize; ++j)
        {
            size_t blockSize = 0;
            size_t numberOfBlocks = 0;

            bis >> blockSize;
            bis >> numberOfBlocks;

            configs.emplace_back(blockSize, numberOfBlocks);
        }

        if (unlikely(bis.HasError()))
        {
            log.OutError("Input stream failure.");
            return false;
        }

        data.emplace_back(key.GetID(), std::move(configs));
    }

#ifdef __DEBUG__
    for (auto& item : data)
    {
        auto& configs = item.configs;
        auto len = configs.size();
        for (size_t i = 1; i < len; ++i)
        {
            const auto& a = configs[i - 1];
            const auto& b = configs[i];
            if (unlikely(!(a < b)))
            {
                log.OutFatalError([&item, &a, &b](auto& ls)
                {
                    StaticString name(item.uniqueName);
                    ls << name << " : configs should be well-orddered. "
                        << a.blockSize << " < " << b.blockSize
                        << " should be true.";
                });
            }
        }
    }

    for (auto& item : data)
    {
        StaticString name(item.uniqueName);
        auto nameStr = name.c_str();
        auto& configs = item.configs;

        for (auto& config : configs)
        {
            log.Out([nameStr, &config](auto& ls)
            {
                ls << nameStr << " : block = " << config.blockSize
                    << ", n = " << config.numberOfBlocks;
            });
        }
    }
#endif // __DEBUG__

    return true;
}

} // HE
