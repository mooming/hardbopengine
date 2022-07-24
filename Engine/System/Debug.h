// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Platform.h"
#include "PrintArgs.h"

#ifdef __DEBUG__

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace
{

  inline void Assert(bool shouldBeTrue)
  {
    if (shouldBeTrue)
      return;

    PrintArgs("[Assert] Please check it.");
    std::abort();
  }

  template <typename ... Types>
  inline void AssertMessage(bool shouldBeTrue, Types&& ... args)
  {
    if (shouldBeTrue)
      return;

    PrintArgs("[Assert] ", std::forward<Types>(args) ...);
    std::abort();
  }
}

#else // __DEBUG__

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace
{

  inline void Assert(bool)
  {
  }

  template <typename ... Types>
  inline void AssertMessage(bool, const char*, Types&& ...)
  {
  }
}
#endif // __DEBUG__

namespace
{

  inline void FatalAssert(bool shouldBeTrue)
  {
    if (shouldBeTrue)
      return;

    PrintArgs("[FatalAssert] Please check it.");
    std::abort();
  }

  template <typename ... Types>
  inline void FatalAssertMessage(bool shouldBeTrue, Types&& ... args)
  {
    if (shouldBeTrue)
      return;

    PrintArgs("[FatalAssert] ", std::forward<Types>(args) ...);
    std::abort();
  }
}
