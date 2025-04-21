#include "kernel.h"

#include "storage.h"
#include "database.h"
#include "fs/cellar_fs.h"

using namespace cellar;

Kernel::Kernel()
{
  _storage = std::make_unique<Storage>(this, "storage");
  _vfs = std::make_unique<vfs::VirtualFileSystem>(this, "vfs");
  _db = std::make_unique<Database>(this, "database");
}

Kernel::~Kernel()
{

}

#include "tbx/base/file_system.h"

void FileSystemBridge::createFolder(const path& path, bool intermediate)
{
  FileSystem::i()->createFolder(path, intermediate);
}