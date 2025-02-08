// Created by mooming.go@gmail.com, 2017

#include "CommandLineArguments.h"

#include "System/Debug.h"
#include <iostream>

namespace HE
{
    CommandLineArguments::CommandLineArguments(int argc, const char* argv[])
    {
        Parse(argc, argv);
    }

    void CommandLineArguments::Print()
    {
        using namespace std;

        cout << "## Command Line Arguments" << endl;
        cout << "# Path: " << executablePath << endl;
        cout << "# Executable Filename: " << executableFilename << endl;
        cout << "# Number of Arguments = " << arguments.Size() << endl;

        int index = 0;
        for (auto& argument : arguments)
        {
            cout << ++index << " : " << argument << endl;
        }
    }

    void CommandLineArguments::Parse(int argc, const char* argv[])
    {
        using namespace std;

        Assert(
            argc > 0, "CommandLineArguments::Parse - Incorrect argc = ", argc);

        for (int i = 0; i < argc; ++i)
        {
            cout << i << " : " << argv[i] << endl;
        }
    }
} // namespace HE
