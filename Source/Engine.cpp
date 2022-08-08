// Created by mooming.go@gmail.com, 2022

#include "Engine.h"

#include <iostream>


namespace HE
{
    void Engine::Initialize(int argc, const char* argv[])
    {
        using namespace std;

        cout << "[HE][Initialize] Command Line Arguments" << endl;
        for (int i = 0; i < argc; ++i)
        {
            cout << i << " : " << argv[i] << endl;
        }

        memoryManager.Initialize();
    }

    void Engine::Run()
    {
    }

    void Engine::Finalize()
    {
    }
} // HE
