// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"


namespace hbe
{

class RendererTest final : public TestCollection
{
public:
	RendererTest() noexcept;

protected:
	void Prepare() override;
};

} // namespace hbe

#endif // __UNIT_TEST__