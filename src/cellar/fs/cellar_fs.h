#pragma once

#include "cellar/kernel.h"
#include "tbx/base/path.h"

#include "fuse.h"

namespace cellar::vfs
{
  using fs_path = path;
  enum class VirtualFileType { ToBeOrganized };

  struct VirtualEntry
  {
  protected:
    path _path;

  public:
    VirtualEntry(const path& path) : _path(path) { }

    virtual bool isFile() const = 0;
    const path& path() const { return _path; }
    auto filename() const { return _path.filename(); }
  };


  struct VirtualFile : public VirtualEntry
  {
    std::vector<uint8_t> _content;
    FUSE_STAT stbuf;
    VirtualFileType _type;

    VirtualFile(const ::path& p) : VirtualEntry(p) { }
    VirtualFile() : VirtualFile("") { }

    auto type() const { return _type; }
    bool isFile() const override { return true; }

    void setSize(size_t length) { stbuf.st_size = length; }

    size_t write(const char* buf, size_t length, FUSE_OFF_T offset);
    size_t read(char* buf, size_t length, FUSE_OFF_T offset);
  };


  struct VirtualDirectory : public VirtualEntry
  {
  protected:
    std::vector<VirtualEntry*> _content;
    std::unordered_map<fs_path, VirtualEntry*, path::hash> _flatMapping;

  public:
    VirtualDirectory(const fs_path& path) : VirtualEntry(path) { }

    void add(VirtualEntry* entry);
    /* remove virtual file from directory, return true if file was present */
    bool remove(VirtualEntry* entry);

    VirtualEntry* get(size_t index) { return _content[index]; }
    VirtualFile* get(const fs_path& filename);

    bool isWritable() const { return true; }
    bool isFile() const override { return false; }
    virtual size_t count() const { return _content.size(); }

    void print(size_t indent = 0);
  };

  struct VirtualFileSystem : public KernelModule
  {
  protected:
    std::unique_ptr<VirtualDirectory> _root;
    VirtualDirectory* _toSortFolder;

    std::unordered_map<path, VirtualDirectory*, path::hash> _flatMapping;

    FUSE_STAT _defaultDirectoryStat;

    void initStat(FUSE_STAT* stat, bool dir, bool readonly);

  public:
    VirtualFileSystem(Kernel* kernel, const std::string& name);

    /* return a default directory stat which is used for all directories */
    const FUSE_STAT* defaultDirectoryStat() const { return &_defaultDirectoryStat; }
    void initStat(VirtualFile* file, bool readonly) { initStat(&file->stbuf, false, readonly); }

    VirtualDirectory* findDirectory(const path& path);
    VirtualDirectory* sortFolder() const { return _toSortFolder; }
    VirtualDirectory* root() const { return _root.get(); }

    void generateFoldersForDATs();

    bool filesReadyToBeSorted(VirtualFile* file);

    void mount();
  };
}