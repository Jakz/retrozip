#pragma once

#include <memory>

#include "logger.h"

class path;

namespace tags
{
  class Tag;
  class TagPool;
}

namespace cellar
{
  class Kernel;
  
  class KernelModule
  {
  private:
    Kernel* _kernel;
    std::string _name;

  public:
    KernelModule(Kernel* kernel, const std::string& name) : _kernel(kernel), _name(name) { }
    Kernel* kernel() const { return _kernel; }

    Logger& log();
    void verify(bool condition, const std::string& message) { if (!condition) { log().error(_name, message); abort(); } }

    template<typename... Args> void trace(const std::string& format, Args&&... args) { log().trace(_name, format, std::forward<Args>(args)...); }
    template<typename... Args> void debug(const std::string& format, Args&&... args) { log().debug(_name, format, std::forward<Args>(args)...); }
    template<typename... Args> void info(const std::string& format, Args&&... args) { log().info(_name, format, std::forward<Args>(args)...); }
    template<typename... Args> void warning(const std::string& format, Args&&... args) { log().warning(_name, format, std::forward<Args>(args)...); }
    template<typename... Args> void error(const std::string& format, Args&&... args) { log().error(_name, format, std::forward<Args>(args)...); }
    template<typename... Args> void fatal(const std::string& format, Args&&... args) { log().fatal(_name, format, std::forward<Args>(args)...); }
  };

  class Storage;
  class Database;
  class Cataloguer;
  class UserInterface;
  
  namespace vfs
  {
    class VirtualFileSystem;
  }

  struct FileSystemBridge
  {
    void createFolder(const path& path, bool intermediate);
    std::vector<path> contentsOfFolder(const path& path);
  };

  class Kernel
  {
  protected:
    std::unique_ptr<Database> _db;
    std::unique_ptr<Storage> _storage;
    std::unique_ptr<vfs::VirtualFileSystem> _vfs;
    std::unique_ptr<FileSystemBridge> _fs;
    std::unique_ptr<tags::TagPool> _tags;
    std::unique_ptr<Cataloguer> _cataloguer;
    std::unique_ptr<UserInterface> _ui;
    Logger _logger;
  
  public:
    Kernel();
    Kernel(const Kernel&) = delete;
    ~Kernel();

    Storage* storage() const { return _storage.get(); }
    vfs::VirtualFileSystem* vfs() const { return _vfs.get(); }
    Database* db() const { return _db.get(); }
    FileSystemBridge* fs() const { return _fs.get(); }
    tags::TagPool* tags() const { return _tags.get(); }
    Cataloguer* cataloguer() const { return _cataloguer.get(); }
    UserInterface* ui() const { return _ui.get(); }

    Logger& log() { return _logger; }
  };

  inline Logger& KernelModule::log() { return _kernel->log(); }
}