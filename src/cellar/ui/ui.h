#pragma once

#include "cellar/kernel.h"

namespace cellar
{
  class UserInterface : public KernelModule
  {
  public:
    UserInterface(Kernel* kernel, const std::string& name) : KernelModule(kernel, name) { }
    void init();
  };
}