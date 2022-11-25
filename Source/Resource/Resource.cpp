// Created by mooming.go@gmail.com, 2022

#include "Resource.h"


namespace HE
{

Resource::Resource()
    : version(0)
    , crc(0)
    , size(0)
{
}

Resource::Resource(StaticString path)
{
}

Resource::~Resource()
{

}

} // HE
