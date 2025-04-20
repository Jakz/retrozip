#include "kernel.h"

#include "storage.h"

using namespace cellar;

Kernel::Kernel()
{
  _storage = std::make_unique<Storage>(this);
}