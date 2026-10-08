// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <unordered_map>

#include "Memory/DefaultAllocator.h"


namespace hbe
{
template <class TKey, class T, class THash = std::hash<TKey>, class TPred = std::equal_to<TKey>>
using HUnorderedMap = std::unordered_map<TKey, T, THash, TPred, hbe::DefaultAllocator<std::pair<const TKey, T>>>;
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class HUnorderedMapTest final : public TestCollection
{
public:
	HUnorderedMapTest();
	~HUnorderedMapTest() override = default;

protected:
	void Prepare() override;
};
} // namespace hbe
#endif //TEST_ENABLED
