#pragma once

#include "cellar/kernel.h"

#include "nana/gui/widgets/textbox.hpp"
#include "nana/gui/widgets/form.hpp"

namespace cellar
{
  class UserInterface : public KernelModule
  {
  protected:
    nana::form _form;
    nana::textbox _console;

  public:
    UserInterface(Kernel* kernel, const std::string& name);
    void init();

    void appendConsoleMessage(const std::string& message);
  };
}