// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OBB.h"


namespace hbe
{
template class OBB<float>;
} // namespace hbe

#ifdef TEST_ENABLED

namespace hbe
{
void OBBTest::Prepare() noexcept
{
}
} // namespace hbe

#endif // TEST_ENABLED
