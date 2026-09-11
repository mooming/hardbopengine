// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ConfigSystem.h"

#include "ConfigParam.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"


namespace hbe
{

	namespace
	{
		template<typename T>
		using TItem = ConfigSystem::ParamItem<T>;

		template<typename T>
		using TMap = ConfigSystem::TMap<T>;

		template<typename T>
		TItem<T>& findGetSet(TMap<T>& map, const char* func, const StaticString& key)
		{
			const auto name = "EngineSettings";
			static TItem<T> nullPair;

			if (unlikely(key.isNull()))
			{
				auto log = Logger::get(name);
				log.outWarning([func](auto& ls) { ls << func << " : key is null."; });

				return nullPair;
			}

			auto it = map.find(key.getID());
			if (unlikely(it == map.end()))
			{
				auto log = Logger::get(name);
				log.out([func, key](auto& ls) { ls << func << " : " << key << " is not found."; });

				return nullPair;
			}

			return it->second;
		}

		template<typename T>
		const TItem<T>& findGetSet(const TMap<T>& map, const char* func, const StaticString& key)
		{
			const auto name = "EngineSettings";
			static TItem<T> nullPair;

			if (unlikely(key.isNull()))
			{
				auto log = Logger::get(name);
				log.outWarning([func](auto& ls) { ls << func << " : key is null."; });

				return nullPair;
			}

			auto it = map.find(key.getID());
			if (unlikely(it == map.end()))
			{
				auto log = Logger::get(name);
				log.out([func, key](auto& ls) { ls << func << " : " << key << " is not found."; });

				return nullPair;
			}

			return it->second;
		}

	} // namespace

	ConfigSystem& ConfigSystem::get() noexcept
	{
		static ConfigSystem instance;
		return instance;
	}

	const char* ConfigSystem::getName() const noexcept { return "EngineSettings"; }

	void ConfigSystem::Register(TConfigParam<bool>& param)
	{
		using TValue = uint8_t;
		auto name = param.getName().getID();

		if (unlikely(byteParams.find(name) != byteParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		byteParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TAtomicConfigParam<bool>& param)
	{
		using TValue = uint8_t;
		auto name = param.getName().getID();

		if (unlikely(byteParams.find(name) != byteParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		byteParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TConfigParam<uint8_t>& param)
	{
		using TValue = uint8_t;
		auto name = param.getName().getID();

		if (unlikely(byteParams.find(name) != byteParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		byteParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TAtomicConfigParam<uint8_t>& param)
	{
		using TValue = uint8_t;
		auto name = param.getName().getID();

		if (unlikely(byteParams.find(name) != byteParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		byteParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TConfigParam<int>& param)
	{
		using TValue = int;
		auto name = param.getName().getID();

		if (unlikely(intParams.find(name) != intParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		intParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TAtomicConfigParam<int>& param)
	{
		using TValue = int;
		auto name = param.getName().getID();

		if (unlikely(intParams.find(name) != intParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		intParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TConfigParam<size_t>& param)
	{
		using TValue = size_t;
		auto name = param.getName().getID();

		if (unlikely(sizeParams.find(name) != sizeParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		sizeParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TAtomicConfigParam<size_t>& param)
	{
		using TValue = size_t;
		auto name = param.getName().getID();

		if (unlikely(sizeParams.find(name) != sizeParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		sizeParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TConfigParam<float>& param)
	{
		using TValue = float;
		auto name = param.getName().getID();

		if (unlikely(floatParams.find(name) != floatParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		floatParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::Register(TAtomicConfigParam<float>& param)
	{
		using TValue = float;
		auto name = param.getName().getID();

		if (unlikely(floatParams.find(name) != floatParams.end()))
		{
			auto log = Logger::get(getName());
			log.outFatalError([name](auto& ls)
			{ ls << "The key " << name << " is duplicated. It's not allowed strictly."; });

			return;
		}

		ParamItem<TValue> item;
		item.desc = param.getDescription().getID();
		item.getter = [&param]() -> TValue { return param.get(); };

		item.setter = [&param](TValue value) { param.set(value); };

		floatParams.emplace(std::make_pair(name, item));
	}

	void ConfigSystem::setBool(const StaticString& key, bool value)
	{
		auto& item = findGetSet(byteParams, __func__, key);
		auto& setter = item.setter;

		if (unlikely(setter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no setter."; });

			return;
		}

		setter(value);
	}

	void ConfigSystem::setByte(const StaticString& key, uint8_t value)
	{
		auto& item = findGetSet(byteParams, __func__, key);
		auto& setter = item.setter;

		if (unlikely(setter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no setter."; });

			return;
		}

		setter(value);
	}

	void ConfigSystem::setInt(const StaticString& key, int value)
	{
		auto& item = findGetSet(intParams, __func__, key);
		auto& setter = item.setter;

		if (unlikely(setter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no setter."; });

			return;
		}

		setter(value);
	}

	void ConfigSystem::setSize(const StaticString& key, size_t value)
	{
		auto& item = findGetSet(sizeParams, __func__, key);
		auto& setter = item.setter;

		if (unlikely(setter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no setter."; });

			return;
		}

		setter(value);
	}

	void ConfigSystem::setFloat(const StaticString& key, float value)
	{
		auto& item = findGetSet(floatParams, __func__, key);
		auto& setter = item.setter;

		if (unlikely(setter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no setter."; });

			return;
		}

		setter(value);
	}

	bool ConfigSystem::getBool(const StaticString& key) const noexcept
	{
		auto& item = findGetSet(byteParams, __func__, key);
		auto& getter = item.getter;

		if (unlikely(getter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no getter."; });

			return false;
		}

		return getter();
	}

	uint8_t ConfigSystem::getByte(const StaticString& key) const noexcept
	{
		auto& item = findGetSet(byteParams, __func__, key);
		auto& getter = item.getter;

		if (unlikely(getter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no getter."; });

			return false;
		}

		return getter();
	}

	int ConfigSystem::getInt(const StaticString& key) const noexcept
	{
		auto& item = findGetSet(intParams, __func__, key);
		auto& getter = item.getter;

		if (unlikely(getter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no getter."; });

			return -1;
		}

		return getter();
	}

	size_t ConfigSystem::getSize(const StaticString& key) const noexcept
	{
		auto& item = findGetSet(sizeParams, __func__, key);
		auto& getter = item.getter;

		if (unlikely(getter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no getter."; });

			return 0;
		}

		return getter();
	}

	float ConfigSystem::getFloat(const StaticString& key) const noexcept
	{
		auto& item = findGetSet(floatParams, __func__, key);
		auto& getter = item.getter;

		if (unlikely(getter == nullptr))
		{
			auto log = Logger::get(getName());
			log.out([func = __func__, key](auto& ls) { ls << func << " : " << key << " has no getter."; });

			return 0.0f;
		}

		return getter();
	}

	void ConfigSystem::printAllParameters() const
	{
		auto log = Logger::get(getName(), ELogLevel::Verbose);
		log.out("= Engine Parameters ====================================");

		auto PrintMap = [&log](auto& map)
		{
			for (auto& pair : map)
			{
				auto& name = pair.first;
				auto& getter = pair.second.getter;

				if (getter == nullptr)
				{
					log.outError([&name](auto& ls) { ls << name << " : null getter"; });

					continue;
				}

				auto& desc = pair.second.desc;
				auto value = getter();

				log.out([name, desc, value](auto& ls) { ls << name << " = " << value << " (" << desc << ')'; });
			}
		};

		PrintMap(byteParams);
		PrintMap(intParams);
		PrintMap(sizeParams);
		PrintMap(floatParams);

		log.out("========================================================\n");
	}

} // namespace hbe
