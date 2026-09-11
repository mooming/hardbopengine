// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MultiPoolConfigCache.h"

#include <algorithm>
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "PoolConfigUtil.h"
#include "Resource/BufferInputStream.h"
#include "Resource/BufferOutputStream.h"
#include "String/StringUtil.h"

namespace hbe
{
	StaticString MultiPoolConfigCache::getClassName()
	{
		using namespace StringUtil;
		static StaticString className(toCompactClassName(__PRETTY_FUNCTION__));

		return className;
	}

	size_t MultiPoolConfigCache::serialize(Buffer& outBuffer)
	{
		using namespace StringUtil;
		auto log = Logger::get(toCompactMethodName(__PRETTY_FUNCTION__));

		BufferOutputStream bos(outBuffer);

		StaticString className = getClassName();
		const char* classNameStr = className.c_str();
		bos << classNameStr;
		bos << getVersion();

		normalize();

		size_t cacheSize = data.size();
		bos << cacheSize;

		if (unlikely(bos.hasError()))
		{
			log.outError([](auto& ls) { ls << "An error occured while streaming out header data."; });

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
			}

			if (unlikely(bos.hasError()))
			{
				log.outError([](auto& ls)
				{
					ls << "An error occured while streaming out pool config "
						  "data";
				});

				return 0;
			}
		}

		return bos.getCursor();
	}

	bool MultiPoolConfigCache::deserialize(const Buffer& buffer)
	{
		using namespace StringUtil;
		static StaticString logName(toCompactMethodName(__PRETTY_FUNCTION__));
		auto log = Logger::get(logName);

		BufferInputStream bis(buffer);

		StaticString className;
		bis >> className;

		if (unlikely(className != getClassName()))
		{
			log.outError([className](auto& ls)
			{ ls << "Invalid class name " << className << ", " << getClassName() << " is expected."; });

			return false;
		}

		TVersion inVersion = 0;
		bis >> inVersion;

		if (unlikely(version != inVersion))
		{
			log.outError([inVersion](auto& ls)
			{ ls << "Version mismatched! Read version = " << inVersion << ", " << version << " is expected."; });

			return false;
		}

		size_t size = 0;
		bis >> size;

		if (unlikely(bis.hasError()))
		{
			log.outError("Input stream failure.");
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

			if (unlikely(bis.hasError()))
			{
				log.outError("Input stream failure.");
				return false;
			}

			data.emplace_back(key.getID(), std::move(configs));
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
					log.outFatalError([&item, &a, &b](auto& ls)
					{
						StaticString name(item.uniqueName);
						ls << name << " : configs should be well-orddered. " << a.blockSize << " < " << b.blockSize
						   << " should be true.";
					});
				}
			}
		}
#endif // __DEBUG__

		return true;
	}

	void MultiPoolConfigCache::normalize()
	{
		std::sort(data.begin(), data.end());

		for (auto& item : data)
		{
			PoolConfigUtil::normalize(item.configs);
		}

		auto CountUniqueAllocators = [this]() -> size_t
		{
			size_t count = 0;

			StaticStringID itemName;
			for (auto& item : data)
			{
				if (item.configs.empty())
				{
					continue;
				}

				if (item.uniqueName != itemName)
				{
					++count;
					itemName = item.uniqueName;
				}
			}

			return count;
		};

		auto uniqueAllocators = CountUniqueAllocators();

		TMultiPoolConfigs tempData;
		std::swap(data, tempData);
		data.reserve(uniqueAllocators);

		StaticStringID itemName;
		TVector<PoolConfig> tempConfigs;

		for (auto& item : tempData)
		{
			if (item.configs.empty())
			{
				continue;
			}

			if (itemName != item.uniqueName)
			{
				if (!tempConfigs.empty())
				{
					data.emplace_back(itemName, std::move(tempConfigs));
				}

				itemName = item.uniqueName;
				tempConfigs.clear();
			}

			PoolConfigUtil::mergeMax(tempConfigs, item.configs);
		}

		if (!tempConfigs.empty())
		{
			data.emplace_back(itemName, std::move(tempConfigs));
		}

		std::sort(data.begin(), data.end());
	}

} // namespace hbe
