#pragma once

#include <memory>
#include <vector>
#include <string_view>

#include "common.h"
#include "logger.h"

class path;
class Logger;

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

    void doLog(LogLevel level, std::string_view section, std::string_view message) const;

  public:
    KernelModule(Kernel* kernel, const std::string& name) : _kernel(kernel), _name(name) { }
    Kernel* kernel() const { return _kernel; }

    Logger& log();
    void verify(bool condition, const std::string_view message) { if (!condition) { doLog(LogLevel::Error, _name, message); abort(); } }

    template<typename... Args> void trace(std::string_view format, Args&&... args) { doLog(LogLevel::Trace, _name, fmt::vformat(format, fmt::make_format_args(args...))); }
    template<typename... Args> void debug(std::string_view format, Args&&... args) { doLog(LogLevel::Debug, _name, fmt::vformat(format, fmt::make_format_args(args...))); }
    template<typename... Args> void info(std::string_view format, Args&&... args) { doLog(LogLevel::Info, _name, fmt::vformat(format, fmt::make_format_args(args...))); }
    template<typename... Args> void warning(std::string_view format, Args&&... args) { doLog(LogLevel::Warning, _name, fmt::vformat(format, fmt::make_format_args(args...))); }
    template<typename... Args> void error(std::string_view format, Args&&... args) { doLog(LogLevel::Error, _name, fmt::vformat(format, fmt::make_format_args(args...))); }
    template<typename... Args> void fatal(std::string_view format, Args&&... args) { doLog(LogLevel::Fatal, _name, fmt::vformat(format, fmt::make_format_args(args...))); }
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