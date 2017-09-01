// Copyright, All rights reserved by Hansol Park (mooming.go@gmail.com)

#ifndef ComponentState_h
#define ComponentState_h

#include <array>
#include <ostream>

namespace HE
{

  enum class ComponentState : int
  {
    NONE,
    BORN,
    ALIVE,
    SLEEP,
    DEAD
  };

  inline std::ostream& operator<<(std::ostream& os, const ComponentState& state)
  {
    static const char* names[] = {
      "NONE",
      "BORN",
      "ALIVE",
      "SLEEP",
      "DEAD"
    };

    os << names[static_cast<int> (state)];
    return os;
  }
}

#endif /* ComponentState_h */
