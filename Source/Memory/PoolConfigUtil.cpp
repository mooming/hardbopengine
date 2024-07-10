// Created by mooming.go@gmail.com, 2022

#include "PoolConfigUtil.h"

#include "System/Debug.h"


namespace HE
{

namespace PoolConfigUtil
{

void Normalize(TPoolConfigs& configs)
{
    std::sort(configs.begin(), configs.end());

    auto NumUniqueBlockSizes = [&configs]() -> size_t
    {
        size_t blockSize = 0;
        size_t uniqueItems = 0;

        for (auto& config : configs)
        {
            if (config.numberOfBlocks <= 0)
            {
                continue;
            }

            if (config.blockSize != blockSize)
            {
                ++uniqueItems;
                blockSize = config.blockSize;
            }
        }

        return uniqueItems;
    };

    const auto numUniqueBlocks = NumUniqueBlockSizes();

    TPoolConfigs temp;
    std::swap(configs, temp);
    configs.reserve(numUniqueBlocks);

    size_t blockSize = 0;
    size_t numBlocks = 0;

    for (auto& config : temp)
    {
        if (config.blockSize != blockSize)
        {
            if (blockSize > 0 && numBlocks > 0)
            {
                configs.emplace_back(blockSize, numBlocks);
            }

            blockSize = config.blockSize;
            numBlocks = 0;
        }

        numBlocks += config.numberOfBlocks;
    }

    if (blockSize > 0 && numBlocks > 0)
    {
        configs.emplace_back(blockSize, numBlocks);
    }

    Assert(configs.size() == numUniqueBlocks);
}

void MergeMax(TPoolConfigs& dst, TPoolConfigs& src)
{
    Normalize(dst);

    if (src.size() == 0)
    {
        return;
    }

    Normalize(src);

    size_t srcIndex = 0;
    const size_t srcLen = src.size();

    auto iterate = [&]()
    {
        if (srcIndex >= srcLen)
        {
            return;
        }

        size_t dstIndex = 0;
        const size_t dstLen = dst.size();

        while (dstIndex < dstLen)
        {
            auto dstBlockSize = dst[dstIndex].blockSize;

            Assert(srcIndex < src.size());
            while (src[srcIndex].blockSize < dstBlockSize)
            {
                dst.push_back(src[srcIndex]);
                ++srcIndex;

                if (unlikely(srcIndex >= srcLen))
                {
                    return;
                }
            }

            Assert(srcIndex < src.size());
            auto srcBlockSize = src[srcIndex].blockSize;

            if (srcBlockSize == dstBlockSize)
            {
                auto dstNumBlocks = dst[dstIndex].numberOfBlocks;
                auto srcNumBlocks = src[srcIndex].numberOfBlocks;

                dst[dstIndex].numberOfBlocks = std::max(dstNumBlocks, srcNumBlocks);
                ++srcIndex;

                if (unlikely(srcIndex >= srcLen))
                {
                    return;
                }
            }

            ++dstIndex;
        }
    };

    iterate();

    while (srcIndex < srcLen)
    {
        dst.push_back(src[srcIndex]);
        ++srcIndex;
    }

    src.clear();
    std::sort(dst.begin(), dst.end());
}

} // namespace PoolConfigUtil

} // namespace HE
