// Created by mooming.go@gmail.com, 2022

#pragma once

#include "ConfigFile.h"
#include "String/StaticString.h"


namespace HE
{
class ConfigSystem final
{
private:
	ConfigFile configFile;

public:
	ConfigSystem();
	~ConfigSystem() = default;

	StaticString GetName() const;
	void Initialize();

	bool IsEnabled(StaticString name) const;
	int GetInt(StaticString name) const;
	float GetFloat(StaticString name) const;

private:
	void Load();
};
} // HE
