// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ConfigParam.h"

template class hbe::ConfigParam<bool, true>;
template class hbe::ConfigParam<bool, false>;
template class hbe::ConfigParam<int, true>;
template class hbe::ConfigParam<int, false>;
template class hbe::ConfigParam<size_t, true>;
template class hbe::ConfigParam<size_t, false>;
template class hbe::ConfigParam<float, true>;
template class hbe::ConfigParam<float, false>;
