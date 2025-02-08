// Created by mooming.go@gmail.com, 2022

#include "ScopedLock.h"

#include <mutex>

namespace HE
{
    template class ScopedLock<std::mutex>;
} // namespace HE
