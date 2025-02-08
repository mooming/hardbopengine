// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Container/Array.h"
#include "String/String.h"

namespace HE
{
    class CommandLineArguments final
    {
    private:
        String executablePath;
        String executableFilename;
        Array<String> arguments;

    public:
        CommandLineArguments() = delete;
        CommandLineArguments(int argc, const char *argv[]);
        ~CommandLineArguments() = default;

        auto &GetArguments() const { return arguments; }

        void Print();

    private:
        void Parse(int argc, const char *argv[]);
    };
} // namespace HE
