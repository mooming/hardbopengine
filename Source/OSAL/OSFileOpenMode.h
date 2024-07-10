
// Created by mooming.go@gmail.com 2022

#pragma once


namespace OS
{

struct FileOpenMode final
{
    int value = 0;

    void SetReadOnly();
    void SetWriteOnly();
    void SetReadWrite();
    void SetCreate();
    void SetTruncate();
    void SetAppend();
};

} // namespace OS
