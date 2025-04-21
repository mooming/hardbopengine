// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstddef>
#include <functional>

namespace HE
{
// @summary A function that can run on work streams.
// A runnable can be distributed on multiple work streams. The workIndex and
// numWorks will be used to split the work.
// @param workIndex An index of the work
// @param numWokrs The number of total works
using Runnable = std::function<void(std::size_t /* workIndex */, std::size_t /*numWorks*/)>;
} // namespace HE
