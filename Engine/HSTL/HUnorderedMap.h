// Created by mooming.go@gmail.com, 2022

#pragma once

#include <unordered_map>
#include "Memory/DefaultAllocator.h"

namespace hbe
{

	template<class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
	using HUnorderedMap = std::unordered_map<Key, T, Hash, Pred, hbe::DefaultAllocator<std::pair<const Key, T>>>;

} // namespace HSTL

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class HUnorderedMapTest : public TestCollection
	{
	public:
		HUnorderedMapTest();
		virtual ~HUnorderedMapTest() = default;

	protected:
		virtual void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
