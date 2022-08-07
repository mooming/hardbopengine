// Created by mooming.go@gmail.com 2016

#pragma once

#include <string>


namespace StringUtil
{
	std::string Trim(const std::string& str);
	std::string TrimPath(std::string path);
	std::string ToLowerCase(std::string src);

	bool EqualsIgnoreCase(std::string a, std::string b);
	bool StartsWith(std::string src, std::string startTerm);
	bool StartsWithIgnoreCase(std::string src, std::string startTerm);
	bool EndsWith(std::string src, std::string endTerm);

	std::string PathToName(std::string path);
}
