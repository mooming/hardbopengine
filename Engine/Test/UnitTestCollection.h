// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#ifdef __TEST__

#include <cstddef>

namespace hbe
{
namespace Test
{
/// @brief Register every test collection and flatten them into the sequence of testlets the harness schedules.
/// @details Call this while the engine is initialised and before Run is entered. Registering here rather than from
///          inside the suite's own work item is what gives the harness an expected total to hold the run against: a
///          suite that counted its own collections from inside the run would report zero for a run that never began,
///          agree with the zero that executed, and certify a dead engine as balanced.
void RegisterSuite();

/// @brief Post the suite's first testlet onto the base stream; each testlet posts the next, and the last one reports
///        and requests shutdown, so `Engine::Run` is what drives the whole suite and returns only once it is done.
/// @details One testlet per work item, so the run needs many passes of the engine loop rather than one. The base
///          stream has to be throttled for that to mean anything - see the allowance set in the harness - because an
///          unbounded pass would take all of them at once and make the count vacuous again.
void ScheduleSuiteOnBaseStream();
} // namespace Test
} // namespace hbe

#endif // __TEST__
