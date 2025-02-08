// Created by mooming.go@gmail.com

#include "ConfigFile.h"

#include "HSTL/HString.h"
#include "String/StringUtil.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <unordered_map>

using namespace HSTL;

namespace HE
{

    ConfigFile::ConfigFile(const char* path)
        : isValid(false)
    {
        Parse(path);
    }

    ConfigFile::ConfigFile(const char* path, const char* fileName)
        : isValid(false)
    {
        TString filePath(path);

        auto lastChar = filePath.back();
        if (lastChar != '/' && lastChar != '\\')
        {
            filePath.append("/");
        }

        filePath.append(fileName);

        Parse(filePath.c_str());
    }

    ConfigFile::TValue ConfigFile::GetValue(const TString& key) const
    {
        auto found = keymap.find(key);
        if (found == keymap.end())
        {
            return TValue();
        }

        return found->second;
    }

    ConfigFile::TString ConfigFile::GetValue(
        const TString& key, const TString& defaultValue) const
    {
        auto found = keymap.find(key);
        if (found == keymap.end())
        {
            return defaultValue;
        }

        return found->second;
    }

    void ConfigFile::Parse(const char* filePath)
    {
        using namespace std;

        ifstream ifs(filePath);
        if (!ifs.is_open())
        {
            cout << "[ConfigFile] Not Found: " << filePath << endl;
            return;
        }

        cout << "[ConfigFile] Open " << filePath << endl;

        while (!ifs.eof())
        {
            TString line;
            getline(ifs, line);

            using TKeyValue = pair<TString, TString>;
            auto ParseLine = [&line]() -> TKeyValue {
                TKeyValue keyValue;
                if (line.empty())
                {
                    return keyValue;
                }

                auto separator = line.find('=');
                if (separator == TString::npos)
                {
                    return keyValue;
                }

                auto key = line.substr(0, separator);
                auto value = line.substr(separator + 1);
                keyValue.first = StringUtil::Trim(key);
                keyValue.second = StringUtil::Trim(value);

                return keyValue;
            };

            auto keyValue = ParseLine();
            if (keyValue.first.empty() || keyValue.second.empty())
            {
                continue;
            }

            keymap[keyValue.first] = keyValue.second;
        }

        isValid = true;
    }

    void ConfigFile::ForEach(
        std::function<void(const TMap::value_type&)> func) const
    {
        if (func == nullptr)
        {
            return;
        }

        for (auto& element : keymap)
        {
            func(element);
        }
    }
} // namespace HE
