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
  class FileSystem;


  class Kernel
  {
  protected:
    std::unique_ptr<Storage> _storage;
    std::unique_ptr<FileSystem> _fs;

  public:
    Kernel();
    Kernel(const Kernel&) = delete;

    Storage* storage() const { return _storage.get(); }
    FileSystem* storage() const { return _fs.get(); }
  };


}