// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "String/StaticStringTable.h"


namespace HE
{
    class Engine final
    {
    private:
        MemoryManager memoryManager;
        StaticStringTable staticStringTable;
        Logger logger;
        
    public:
        Engine();
        ~Engine();

        void Initialize(int argc, const char* argv[]);
        void Run();
        
        StaticString GetName() const;
        
        inline auto& GetMemoryManager() { return memoryManager; }
        inline auto& GetLogger() { return logger; }
        
    };
}
