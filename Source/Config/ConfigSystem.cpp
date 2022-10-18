// Created by mooming.go@gmail.com 2022

#include "ConfigSystem.h"

#include "Log/Logger.h"
#include "String/StringUtil.h"


namespace HE
{
ConfigSystem::ConfigSystem()
	: configFile("Settings.config")
{
}

StaticString ConfigSystem::GetName() const
{
	using namespace StringUtil;
	static StaticString name(PrettyFunctionToCompactClassName(__PRETTY_FUNCTION__));

	return name;
}

void ConfigSystem::Initialize()
{
	Load();
}

bool ConfigSystem::IsEnabled(StaticString name) const
{
	return false;
}

int ConfigSystem::GetInt(StaticString name) const
{
	return -1;
}

float ConfigSystem::GetFloat(StaticString name) const
{
	return 0.0f;
}

void ConfigSystem::Load()
{
	auto log = Logger::Get(GetName());
	configFile.ForEach([&log](auto& item)
	{
		log.Out([&item](auto& ls)
		{
			ls << item.first.c_str() << " = " << item.second.c_str();
		});
	});
}

} // HE
