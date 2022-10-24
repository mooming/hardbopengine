// Created by mooming.go@gmail.com, 2022

#pragma once
#include "String/StaticStringID.h"
#include <unordered_map>


namespace HE
{

class StaticString;

template <typename T, bool IsAtomic>
class ConfigParam;

class ConfigSystem final
{
    template <typename T>
    using TConfigParam = ConfigParam<T, false>;

    template <typename T>
    using TAtomicConfigParam = ConfigParam<T, true>;

public:
    template <typename T>
    using TGetter = std::function<T()>;

    template <typename T>
    using TSetter = std::function<void(T)>;

    template <typename T>
    struct ParamItem final
    {
        StaticStringID desc;
        TGetter<T> getter;
        TSetter<T> setter;
    };

    template <typename T>
    using TMap = std::unordered_map<StaticStringID, ParamItem<T>>;

private:
    TMap<uint8_t> byteParams;
    TMap<int> intParams;
    TMap<size_t> sizeParams;
    TMap<float> floatParams;

private:
    ConfigSystem(const ConfigSystem&) = delete;
    ConfigSystem(ConfigSystem&&) = delete;
    ConfigSystem& operator= (const ConfigSystem&) = delete;
    ConfigSystem& operator= (ConfigSystem&&) = delete;

public:
    static ConfigSystem& Get();
    ConfigSystem() = default;
    ~ConfigSystem() = default;

    const char* GetName() const;

    void Register(TConfigParam<bool>& engineParam);
    void Register(TAtomicConfigParam<bool>& engineParam);
    void Register(TConfigParam<uint8_t>& engineParam);
    void Register(TAtomicConfigParam<uint8_t>& engineParam);
    void Register(TConfigParam<int>& engineParam);
    void Register(TAtomicConfigParam<int>& engineParam);
    void Register(TConfigParam<size_t>& engineParam);
    void Register(TAtomicConfigParam<size_t>& engineParam);
    void Register(TConfigParam<float>& engineParam);
    void Register(TAtomicConfigParam<float>& engineParam);

    void SetBool(const StaticString& key, bool value);
    void SetByte(const StaticString& key, uint8_t value);
    void SetInt(const StaticString& key, int value);
    void SetSize(const StaticString& key, size_t value);
    void SetFloat(const StaticString& key, float value);

    bool GetBool(const StaticString& key) const;
    uint8_t GetByte(const StaticString& key) const;
    int GetInt(const StaticString& key) const;
    size_t GetSize(const StaticString& key) const;
    float GetFloat(const StaticString& key) const;

    void PrintAllParameters() const;
};
} // HE
