#pragma once

#include "data/entry.h"
#include "kernel.h"

namespace cellar
{
  class Cataloguer : public KernelModule
  {
  public:
    Cataloguer(Kernel* kernel, const std::string& name) : KernelModule(kernel, name) { }

    void catalogue(Game* game);
  };
}
