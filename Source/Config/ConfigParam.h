// Created by mooming.go@gmail.com

#pragma once

#include <algorithm>
#include <thread>
#include "BuildConfig.h"
#include "ConfigSystem.h"
#include "Core/Debug.h"
#include "String/StaticString.h"

namespace hbe
{

	// ConfigParam represents a single config value with arbitrary file.
	// It could be thread-safe when IsAtomic is set to true.
	template<typename T, bool IsAtomic = false>
	class ConfigParam final
	{
		static constexpr size_t MaxNameLength = 127;
		static constexpr size_t MaxDescLength = 127;

		using TValue = typename std::conditional<IsAtomic, std::atomic<T>, T>::type;

	private:
#if ENGINE_PARAM_DESC_ENABLED
		StaticString name;
		StaticString desc;
#endif // ENGINE_PARAM_DESC_ENABLED

		TValue value;
		std::mutex lock;

#ifdef __DEBUG__
		std::thread::id threadID;
#endif // __DEBUG__

	public:
		ConfigParam(const char* inName, const char* inDesc, T defaultValue,
					std::thread::id id = std::this_thread::get_id())
#if ENGINE_PARAM_DESC_ENABLED
			:
			name(inName), desc(inDesc), value(defaultValue)
#else // ENGINE_PARAM_DESC_ENABLED
			: value(defaultValue)
#endif // ENGINE_PARAM_DESC_ENABLED
#ifdef __DEBUG__
			,
			threadID(id)
#endif // __DEBUG__
		{
			auto& settings = ConfigSystem::Get();
			settings.Register(*this);
		}

		StaticString GetName() const
		{
#if ENGINE_PARAM_DESC_ENABLED
			return name;
#else // ENGINE_PARAM_DESC_ENABLED
			return StaticString();
#endif // ENGINE_PARAM_DESC_ENABLED
		}

		StaticString GetDescription() const
		{
#if ENGINE_PARAM_DESC_ENABLED
			return desc;
#else // ENGINE_PARAM_DESC_ENABLED
			return StaticString();
#endif // ENGINE_PARAM_DESC_ENABLED
		}

		T Get()
		{
#ifdef __DEBUG__
			Assert(IsAtomic || std::this_thread::get_id() == threadID);
#endif // __DEBUG__

			return value;
		}

		void Set(const T& inValue)
		{
#ifdef __DEBUG__
			Assert(IsAtomic || std::this_thread::get_id() == threadID);
#endif // __DEBUG__

			value = inValue;
		}
	};

	template<typename T>
	using TConfigParam = ConfigParam<T, false>;

	template<typename T>
	using TAtomicConfigParam = ConfigParam<T, true>;

} // namespace hbe
