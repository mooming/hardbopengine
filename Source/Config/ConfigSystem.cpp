// Created by mooming.go@gmail.com

#include "ConfigSystem.h"

#include "ConfigParam.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"

namespace HE
{

    namespace
    {
        template <typename T>
        using TItem = ConfigSystem::ParamItem<T>;

        template <typename T>
        using TMap = ConfigSystem::TMap<T>;

        template <typename T>
        TItem<T>& FindGetSet(
            TMap<T>& map, const char* func, const StaticString& key)
        {
            const auto name = "EngineSettings";
            static TItem<T> nullPair;

            if (unlikely(key.IsNull()))
            {
                auto log = Logger::Get(name);
                log.OutWarning(
                    [func](auto& ls) { ls << func << " : key is null."; });

                return nullPair;
            }

            auto it = map.find(key.GetID());
            if (unlikely(it == map.end()))
            {
                auto log = Logger::Get(name);
                log.Out([func, key](auto& ls) {
                    ls << func << " : " << key << " is not found.";
                });

                return nullPair;
            }

            return it->second;
        }

        template <typename T>
        const TItem<T>& FindGetSet(
            const TMap<T>& map, const char* func, const StaticString& key)
        {
            const auto name = "EngineSettings";
            static TItem<T> nullPair;

            if (unlikely(key.IsNull()))
            {
                auto log = Logger::Get(name);
                log.OutWarning(
                    [func](auto& ls) { ls << func << " : key is null."; });

                return nullPair;
            }

            auto it = map.find(key.GetID());
            if (unlikely(it == map.end()))
            {
                auto log = Logger::Get(name);
                log.Out([func, key](auto& ls) {
                    ls << func << " : " << key << " is not found.";
                });

                return nullPair;
            }

            return it->second;
        }

    } // namespace

    ConfigSystem& ConfigSystem::Get()
    {
        static ConfigSystem instance;
        return instance;
    }

    const char* ConfigSystem::GetName() const
    {
        return "EngineSettings";
    }

    void ConfigSystem::Register(TConfigParam<bool>& param)
    {
        using TValue = uint8_t;
        auto name = param.GetName().GetID();

        if (unlikely(byteParams.find(name) != byteParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        byteParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TAtomicConfigParam<bool>& param)
    {
        using TValue = uint8_t;
        auto name = param.GetName().GetID();

        if (unlikely(byteParams.find(name) != byteParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        byteParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TConfigParam<uint8_t>& param)
    {
        using TValue = uint8_t;
        auto name = param.GetName().GetID();

        if (unlikely(byteParams.find(name) != byteParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        byteParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TAtomicConfigParam<uint8_t>& param)
    {
        using TValue = uint8_t;
        auto name = param.GetName().GetID();

        if (unlikely(byteParams.find(name) != byteParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        byteParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TConfigParam<int>& param)
    {
        using TValue = int;
        auto name = param.GetName().GetID();

        if (unlikely(intParams.find(name) != intParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        intParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TAtomicConfigParam<int>& param)
    {
        using TValue = int;
        auto name = param.GetName().GetID();

        if (unlikely(intParams.find(name) != intParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        intParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TConfigParam<size_t>& param)
    {
        using TValue = size_t;
        auto name = param.GetName().GetID();

        if (unlikely(sizeParams.find(name) != sizeParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        sizeParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TAtomicConfigParam<size_t>& param)
    {
        using TValue = size_t;
        auto name = param.GetName().GetID();

        if (unlikely(sizeParams.find(name) != sizeParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        sizeParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TConfigParam<float>& param)
    {
        using TValue = float;
        auto name = param.GetName().GetID();

        if (unlikely(floatParams.find(name) != floatParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        floatParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::Register(TAtomicConfigParam<float>& param)
    {
        using TValue = float;
        auto name = param.GetName().GetID();

        if (unlikely(floatParams.find(name) != floatParams.end()))
        {
            auto log = Logger::Get(GetName());
            log.OutFatalError([name](auto& ls) {
                ls << "The key " << name
                   << " is duplicated. It's not allowed strictly.";
            });

            return;
        }

        ParamItem<TValue> item;
        item.desc = param.GetDescription().GetID();
        item.getter = [&param]() -> TValue { return param.Get(); };

        item.setter = [&param](TValue value) { param.Set(value); };

        floatParams.emplace(std::make_pair(name, item));
    }

    void ConfigSystem::SetBool(const StaticString& key, bool value)
    {
        auto& item = FindGetSet(byteParams, __func__, key);
        auto& setter = item.setter;

        if (unlikely(setter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no setter.";
            });

            return;
        }

        setter(value);
    }

    void ConfigSystem::SetByte(const StaticString& key, uint8_t value)
    {
        auto& item = FindGetSet(byteParams, __func__, key);
        auto& setter = item.setter;

        if (unlikely(setter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no setter.";
            });

            return;
        }

        setter(value);
    }

    void ConfigSystem::SetInt(const StaticString& key, int value)
    {
        auto& item = FindGetSet(intParams, __func__, key);
        auto& setter = item.setter;

        if (unlikely(setter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no setter.";
            });

            return;
        }

        setter(value);
    }

    void ConfigSystem::SetSize(const StaticString& key, size_t value)
    {
        auto& item = FindGetSet(sizeParams, __func__, key);
        auto& setter = item.setter;

        if (unlikely(setter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no setter.";
            });

            return;
        }

        setter(value);
    }

    void ConfigSystem::SetFloat(const StaticString& key, float value)
    {
        auto& item = FindGetSet(floatParams, __func__, key);
        auto& setter = item.setter;

        if (unlikely(setter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no setter.";
            });

            return;
        }

        setter(value);
    }

    bool ConfigSystem::GetBool(const StaticString& key) const
    {
        auto& item = FindGetSet(byteParams, __func__, key);
        auto& getter = item.getter;

        if (unlikely(getter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no getter.";
            });

            return false;
        }

        return getter();
    }

    uint8_t ConfigSystem::GetByte(const StaticString& key) const
    {
        auto& item = FindGetSet(byteParams, __func__, key);
        auto& getter = item.getter;

        if (unlikely(getter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no getter.";
            });

            return false;
        }

        return getter();
    }

    int ConfigSystem::GetInt(const StaticString& key) const
    {
        auto& item = FindGetSet(intParams, __func__, key);
        auto& getter = item.getter;

        if (unlikely(getter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no getter.";
            });

            return -1;
        }

        return getter();
    }

    size_t ConfigSystem::GetSize(const StaticString& key) const
    {
        auto& item = FindGetSet(sizeParams, __func__, key);
        auto& getter = item.getter;

        if (unlikely(getter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no getter.";
            });

            return 0;
        }

        return getter();
    }

    float ConfigSystem::GetFloat(const StaticString& key) const
    {
        auto& item = FindGetSet(floatParams, __func__, key);
        auto& getter = item.getter;

        if (unlikely(getter == nullptr))
        {
            auto log = Logger::Get(GetName());
            log.Out([func = __func__, key](auto& ls) {
                ls << func << " : " << key << " has no getter.";
            });

            return 0.0f;
        }

        return getter();
    }

    void ConfigSystem::PrintAllParameters() const
    {
        auto log = Logger::Get(GetName(), ELogLevel::Verbose);
        log.Out("= Engine Parameters ====================================");

        auto PrintMap = [&log](auto& map) {
            for (auto& pair : map)
            {
                auto& name = pair.first;
                auto& getter = pair.second.getter;

                if (getter == nullptr)
                {
                    log.OutError(
                        [&name](auto& ls) { ls << name << " : null getter"; });

                    continue;
                }

                auto& desc = pair.second.desc;
                auto value = getter();

                log.Out([name, desc, value](auto& ls) {
                    ls << name << " = " << value << " (" << desc << ')';
                });
            }
        };

        PrintMap(byteParams);
        PrintMap(intParams);
        PrintMap(sizeParams);
        PrintMap(floatParams);

        log.Out("========================================================\n");
    }

} // namespace HE
