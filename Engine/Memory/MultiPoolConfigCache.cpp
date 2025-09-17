// Created by mooming.go@gmail.com

#include "MultiPoolConfigCache.h"

#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "PoolConfigUtil.h"
#include "Resource/BufferInputStream.h"
#include "Resource/BufferOutputStream.h"
#include "String/StringUtil.h"

namespace hbe
{
	StaticString MultiPoolConfigCache::GetClassName() const
	{
		using namespace StringUtil;
		static StaticString className(ToCompactClassName(__PRETTY_FUNCTION__));

		return className;
	}

	size_t MultiPoolConfigCache::Serialize(Buffer& outBuffer)
	{
		using namespace StringUtil;
		auto log = Logger::Get(ToCompactMethodName(__PRETTY_FUNCTION__));

		BufferOutputStream bos(outBuffer);

		StaticString className = GetClassName();
		const char* classNameStr = className.c_str();
		bos << classNameStr;
		bos << GetVersion();

		Normalize();

		size_t cacheSize = data.size();
		bos << cacheSize;

		if (unlikely(bos.HasError()))
		{
			log.OutError([](auto& ls) { ls << "An error occured while streaming out header data."; });

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

			if (unlikely(bos.HasError()))
			{
				log.OutError([](auto& ls)
				{
					ls << "An error occured while streaming out pool config "
						  "data";
				});

				return 0;
			}
		}

		return bos.GetCursor();
	}

	bool MultiPoolConfigCache::Deserialize(const Buffer& buffer)
	{
		using namespace StringUtil;
		static StaticString logName(ToCompactMethodName(__PRETTY_FUNCTION__));
		auto log = Logger::Get(logName);

		BufferInputStream bis(buffer);

		StaticString className;
		bis >> className;

		if (unlikely(className != GetClassName()))
		{
			log.OutError([this, className](auto& ls)
			{ ls << "Invalid class name " << className << ", " << GetClassName() << " is expected."; });

			return false;
		}

		TVersion inVersion = 0;
		bis >> inVersion;

		if (unlikely(version != inVersion))
		{
			log.OutError([inVersion](auto& ls)
			{ ls << "Version mismatched! Read version = " << inVersion << ", " << version << " is expected."; });

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
						ls << name << " : configs should be well-orddered. " << a.blockSize << " < " << b.blockSize
						   << " should be true.";
					});
				}
			}
		}
#endif // __DEBUG__

		return true;
	}

	void MultiPoolConfigCache::Normalize()
	{
		std::sort(data.begin(), data.end());

		for (auto& item : data)
		{
			PoolConfigUtil::Normalize(item.configs);
		}

		auto CountUniqueAllocators = [this]() -> size_t
		{
			size_t count = 0;

			StaticStringID itemName;
			for (auto& item : data)
			{
				if (item.configs.size() <= 0)
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
			if (item.configs.size() <= 0)
			{
				continue;
			}

			if (itemName != item.uniqueName)
			{
				if (tempConfigs.size() > 0)
				{
					data.emplace_back(itemName, std::move(tempConfigs));
				}

				itemName = item.uniqueName;
				tempConfigs.clear();
			}

			PoolConfigUtil::MergeMax(tempConfigs, item.configs);
		}

		if (tempConfigs.size() > 0)
		{
			data.emplace_back(itemName, std::move(tempConfigs));
		}

		std::sort(data.begin(), data.end());
	}

} // namespace hbe
