#include "kernel.h"

#include "storage.h"
#include "database.h"
#include "cataloguer.h"
#include "logger.h"
#include "data/tags.h"
#include "fs/cellar_fs.h"
#include "ui/ui.h"

using namespace cellar;

Kernel::Kernel()
{
  _storage = std::make_unique<Storage>(this, "storage");
  _vfs = std::make_unique<vfs::VirtualFileSystem>(this, "vfs");
  _db = std::make_unique<Database>(this, "database");
  _cataloguer = std::make_unique<Cataloguer>(this, "cataloguer");
  _tags = std::make_unique<tags::TagPool>();
  _ui = std::make_unique<UserInterface>(this, "ui");
}

Kernel::~Kernel()
{

}

void KernelModule::doLog(LogLevel level, std::string_view section, std::string_view message) const
{
  kernel()->log().log(level, section, message);
  if (kernel()->ui())
  kernel()->ui()->appendConsoleMessage(fmt::format("[{}] {}", section, message));
}


#include "tbx/base/file_system.h"

void FileSystemBridge::createFolder(const path& path, bool intermediate)
{
  FileSystem::i()->createFolder(path, intermediate);
}

std::vector<path> FileSystemBridge::contentsOfFolder(const path& path)
{
  return FileSystem::i()->contentsOfFolder(path);
}
