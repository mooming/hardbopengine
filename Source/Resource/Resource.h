// Created by mooming.go@gmail.com, 2022

#pragma once
#include "String/StaticString.h"
#include <cstddef>
#include <cstdint>


namespace HE
{

class Resource
{
private:
    StaticString path;
    uint32_t version;
    uint32_t crc;
    size_t size;

public:
    Resource();
    Resource(StaticString path);
    virtual ~Resource();

    float GetProgress() const;

    bool IsLoading() const;
    bool IsLoaded() const;
    bool IsSaving() const;

    void RequestSave();
    void RequestLoad();
    void WaitForLoadingCompleted();
    void Release();

    inline size_t GetSize() const { return size; }
    inline StaticString GetPath() const { return path; }
    inline StaticString GetKey() const { return path; }

protected:
    virtual void PreSave();
    virtual void PostSave();

    virtual void PreLoad();
    virtual void PostLoad();

    virtual void PreRelease();
    virtual void PostRelease();
};

} // HE
