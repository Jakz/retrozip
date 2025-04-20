#include "kernel.h"

#include "storage.h"
#include "cellar/fs/vfs.h"

using namespace cellar;

Kernel::Kernel()
{
  _storage = std::make_unique<Storage>(this);
  _fs = std::make_unique<FileSystem>(this);
}