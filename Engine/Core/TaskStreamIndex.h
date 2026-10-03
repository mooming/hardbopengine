// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace hbe
{
/// @brief Identifies one task stream, to the scheduler, to providers, and to outcome delivery.
/// @details The same value names a stream from every direction: a provider attaches to a list of them, a
///          produce context says which one is currently producing, and a successor task names the stream
///          its result has to return to. Those three uses disagreeing would be a silent mismatch, which is
///          why there is one alias rather than three.
/// @details TaskStream re-exports this rather than defining its own, so the identity of a stream cannot
///          become two different widths in one build. It used to be the index type of the task array
///          itself, which tied a stream's identity to the container holding its tasks.
/// @note Named stream constants arrive with B4. Until then TaskSystem::BaseStreamIndex and IOStreamIndex
///       are the only values in circulation.
using TStreamIndex = int;
} // namespace hbe
