// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Memory/BaseAllocator.h"
#include <unordered_map>


namespace HSTL
{

template < class Key, class T,
           class Hash = std::hash<Key>,
           class Pred = std::equal_to<Key>>
using HUnorderedMap = std::unordered_map<Key, T, Hash>;

} // HSTL
