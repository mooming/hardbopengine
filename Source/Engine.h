// Created by mooming.go@gmail.com, 2022

#pragma once

namespace HE
{
    class Engine final
    {
    public:
        Engine() = default;
        ~Engine() = default;

        void Initialize();
        void Run();
        void Finalize();
    };
}