// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Memory/MemoryManager.h"


namespace HE
{
    class Engine final
    {
    private:
        MemoryManager memoryManager;
        
    public:
        Engine() = default;
        ~Engine() = default;

        void Initialize(int argc, const char* argv[]);
        void Run();
        void Finalize();
    };
}
