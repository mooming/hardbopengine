// Created by mooming.go@gmail.com 2022

#pragma once


namespace OS
{

struct ProtectionMode final
{
    int value = 0;

    void SetForbidden();
    void SetReadable();
    void SetWritable();
    void SetExecutable();
};

} // namespace OS
