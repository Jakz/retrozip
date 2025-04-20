#pragma once

#include <memory>

namespace cellar
{
  class Kernel;
  
  class KernelComponent
  {
  private:
    Kernel* _kernel;

  public:
    KernelComponent(Kernel* kernel) : _kernel(kernel) { }
    Kernel* kernel() const { return _kernel; }
  };

  class Storage;


  class Kernel
  {
  protected:
    std::unique_ptr<Storage> _storage;

  public:
    Kernel();
    Kernel(const Kernel&) = delete;

    Storage* storage() const { return _storage.get(); }
  };


}