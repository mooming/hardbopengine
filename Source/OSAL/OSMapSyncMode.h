
// Created by mooming.go@gmail.com 2022

#pragma once

namespace OS
{

struct MapSyncMode final
{
    int value = 0;

    void SetAsync();
    void SetSync();
    void Invalidate();
};

} // namespace OS
