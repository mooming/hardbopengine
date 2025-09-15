// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstddef>
#include <functional>

namespace hbe
{
	using Runnable = std::function<void(std::size_t, std::size_t)>;
} // namespace hbe
