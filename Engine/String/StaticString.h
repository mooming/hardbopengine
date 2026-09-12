// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <ostream>
#include <string_view>
#include "Config/BuildConfig.h"
#include "StaticStringID.h"

namespace hbe
{

	template<typename T>
	concept CToZeroTerminateStr = requires(T t) { t.c_str(); };

	/// @brief An interned string stored in a global table for efficient comparison and storage.
	class StaticString final
	{
	public:
		StaticString() noexcept;
		StaticString(StaticStringID id) noexcept;
		StaticString(const char* string) noexcept;
		StaticString(const std::string_view& str) noexcept;

		template<CToZeroTerminateStr T>
		StaticString(const T& string) noexcept : StaticString(string.c_str())
		{}

		~StaticString() = default;

		[[nodiscard]] const char* c_str() const noexcept;

		[[nodiscard]] auto GetID() const noexcept { return id; }
		[[nodiscard]] bool IsNull() const noexcept { return id.ptr == nullptr; }
		[[nodiscard]] operator const char*() const noexcept { return c_str(); }
		bool operator<(const StaticString& rhs) const noexcept { return id.ptr < rhs.id.ptr; }
		bool operator==(const StaticString& rhs) const noexcept { return id.ptr == rhs.id.ptr; }

		friend std::ostream& operator<<(std::ostream& os, const StaticString& str) noexcept
		{
			os << str.c_str();
			return os;
		}

	private:
		StaticStringID id;
	};

} // namespace hbe

namespace std
{
	template<>
	struct hash<hbe::StaticString> final
	{
		std::size_t operator()(const hbe::StaticString& obj) const
		{
			return reinterpret_cast<std::size_t>(obj.GetID().ptr);
		}
	};
} // namespace std

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class StaticStringTest : public TestCollection
	{
	public:
		StaticStringTest() : TestCollection("StaticStringTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
