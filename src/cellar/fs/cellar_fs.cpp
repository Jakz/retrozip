#include "cellar_fs.h"

#include "tbx/extra/fmt/format.h"

#include <iostream>

//static const std::string PATH_ROOT = "/";
//static const std::string PATH_TO_SORT = "/ToSort";

#if !defined(O_ACCMODE)
#define O_ACCMODE     (O_RDONLY | O_WRONLY | O_RDWR)
#endif

#define SUCCESS 0

#define FUSE_DEBUG_FLAG false

#if FUSE_DEBUG_FLAG
#define FUSE_DEBUG(__format__, ...) \
  do { \
    std::cout << fmt::format(__format__, __VA_ARGS__) << std::endl << std::flush; \
  } while(false)
#else
#define FUSE_DEBUG(...) do { } while(false)
#endif

#define LOG_DEBUG(__format__, ...) \
  do { \
    std::cout << fmt::format(__format__, __VA_ARGS__) << std::endl << std::flush; \
  } while(false)
/*
    lock.lock(); \
    buffer.push_back(fmt::format(__format__, __VA_ARGS__)); \
    lock.unlock(); \ 
  } while(false)
  */

static void initStat(FUSE_STAT * stbuf, bool dir = false)
{
  memset(stbuf, 0, sizeof(FUSE_STAT));
  stbuf->st_nlink = 1;

  stbuf->st_uid = 1000;
  stbuf->st_gid = 1000;

  auto now = std::time(nullptr);
  stbuf->st_mtim.tv_sec = now;
  stbuf->st_mtim.tv_nsec = 0;
  stbuf->st_atim = stbuf->st_mtim;
  stbuf->st_ctim = stbuf->st_mtim;

  if (dir)
    stbuf->st_mode = S_IFDIR | 0777;
  else
    stbuf->st_mode = S_IFREG | 0666;
}

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

  size_t write(const char* buf, size_t length, FUSE_OFF_T offset)
  {
    size_t requiredSize = offset + length;
    if (_content.size() < requiredSize)
      _content.resize(requiredSize);

    memcpy(_content.data() + offset, buf, length);

    stbuf.st_size = std::max(_content.size(), (size_t)stbuf.st_size);
    return length;
  }

  size_t read(char* buf, size_t length, FUSE_OFF_T offset)
  {
    if (offset < _content.size())
    {
      if (offset + length > _content.size())
        length = _content.size() - offset;
      memcpy(buf, _content.data() + offset, length);
    }
    else
      length = 0;

    return length;
  }
};


struct VirtualDirectory : public VirtualEntry
{
protected:
  std::vector<VirtualEntry*> _content;
  std::unordered_map<fs_path, VirtualEntry*, fs_path::hash> _flatMapping;

public:
  VirtualDirectory(const fs_path& path) : VirtualEntry(path) { }

  void add(VirtualEntry* entry)
  { 
    _content.push_back(entry);
    _flatMapping[entry->filename()] = entry;
  }

  VirtualEntry* get(size_t index) { return _content[index]; }
  VirtualFile* get(const fs_path& filename);

  bool isWritable() const { return true; }
  bool isFile() const override { return false; }
  virtual size_t count() const { return _content.size(); }

  void print(size_t indent = 0);
};


VirtualFile* VirtualDirectory::get(const fs_path& filename)
{
  auto it = _flatMapping.find(filename);
  VirtualEntry* entry = nullptr;

  if (it != _flatMapping.end())
    entry = it->second;

  if (entry && entry->isFile())
    return static_cast<VirtualFile*>(entry);
  else
    return nullptr;
}


void VirtualDirectory::print(size_t indent)
{
  std::string indentation(indent, ' ');
  std::cout << indentation << _path << std::endl;

  for (size_t i = 0; i < count(); ++i)
  {
    auto* entry = get(i);

    if (entry->isFile())
      std::cout << std::string(indent + 2, ' ') << entry->filename() << std::endl;
    else
      ((VirtualDirectory*)entry)->print(indent + 2);
  }
}


struct VirtualFileSystem
{
protected:
  std::unique_ptr<VirtualDirectory> _root;
  VirtualDirectory* _toSortFolder;

  std::unordered_map<path, VirtualDirectory*, path::hash> _flatMapping;

  FUSE_STAT _defaultDirectoryStat;

  void initStat(FUSE_STAT* stat, bool dir, bool readonly);

public:
  VirtualFileSystem();

  /* return a default directory stat which is used for all directories */
  const FUSE_STAT* defaultDirectoryStat() const { return &_defaultDirectoryStat; }

  VirtualDirectory* findDirectory(const path& path);
  VirtualDirectory* sortFolder() const { return _toSortFolder; }
  VirtualDirectory* root() const { return _root.get(); }

  void initDats();

  void initStat(VirtualFile* file, bool readonly) { initStat(&file->stbuf, false, readonly); }
};

#include "data/database.h"

extern DatabaseData data;

VirtualFileSystem::VirtualFileSystem()
{
  _root.reset(new VirtualDirectory("/"));

  _toSortFolder = new VirtualDirectory("/ToSort");
  _root->add(_toSortFolder);

  _flatMapping["/"] = _root.get();
  _flatMapping["/ToSort"] = _toSortFolder;

  initStat(&_defaultDirectoryStat, true, true);
}

void VirtualFileSystem::initDats()
{
  VirtualDirectory* dats = new VirtualDirectory("/Dats");
  _root->add(dats);
  _flatMapping["/Dats"] = dats;
  
  for (const auto& dat : data.dats())
  {
    path name = dat.second.name;
    path folderName = name.filenameWithoutExtension();
    path folderPath = path("/Dats/") + folderName;
    auto* folder = new VirtualDirectory(folderPath);
    _flatMapping[folderPath] = folder;
    dats->add(folder);

    /*for (const DatGame& game : dat.second.games)
    {
      if (!game.roms.empty())
      {
        const DatRom& rom = game[0];

        VirtualFile* file = new VirtualFile(folderPath + rom.name);
        initStat(file, true);
        file->setSize(data.hashes()[rom.ref].size());

        folder->add(file);
      }
    }*/
  }
}

VirtualDirectory* VirtualFileSystem::findDirectory(const path& path)
{
  auto it = _flatMapping.find(path);
  return it != _flatMapping.end() ? it->second : nullptr;
}

void VirtualFileSystem::initStat(FUSE_STAT* stbuf, bool dir = false, bool readonly = false)
{
  memset(stbuf, 0, sizeof(FUSE_STAT));
  stbuf->st_nlink = 1;

  stbuf->st_uid = 1000;
  stbuf->st_gid = 1000;

  auto now = std::time(nullptr);
  stbuf->st_mtim.tv_sec = now;
  stbuf->st_mtim.tv_nsec = 0;
  stbuf->st_atim = stbuf->st_mtim;
  stbuf->st_ctim = stbuf->st_mtim;

  if (dir)
  {
    stbuf->st_mode = S_IFDIR;
    stbuf->st_mode |= 0111;
  }
  else
    stbuf->st_mode = S_IFREG;

  if (readonly)
    stbuf->st_mode |= 0444;
  else
    stbuf->st_mode |= 0666;
}

VirtualFileSystem vfs;

void initVFS()
{
  vfs.initDats();
}

CellarFS* CellarFS::instance = nullptr;

int CellarFS::statsfs(const char* foo, struct statvfs* stats)
{
  stats->f_bsize = 2048;
  stats->f_blocks = 2;
  stats->f_bfree = std::numeric_limits<fsblkcnt_t>::max();
  stats->f_bavail = std::numeric_limits<fsblkcnt_t>::max();
  stats->f_files = 3;
  stats->f_ffree = std::numeric_limits<fsfilcnt_t>::max();
  stats->f_namemax = 256;
  return 0;
}

int CellarFS::access(const char* cpath, int)
{
  FUSE_DEBUG("access({})", cpath);
  return 0;
}

int CellarFS::sgetattr(const char* path, FUSE_STAT* stbuf)
{ 
  return instance->getattr(path, stbuf);
}

int CellarFS::sreaddir(const char* path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, fuse_file_info* fi) { return instance->readdir(path, buf, filler, offset, fi); }
int CellarFS::sopendir(const char* path, fuse_file_info* fi) { return instance->opendir(path, fi); }

int CellarFS::create(const char* cpath, mode_t mode, struct fuse_file_info* fi)
{
  path path = cpath;

  auto directory = vfs.findDirectory(path.parent());

  /* directory exists */
  if (directory)
  {
    if (directory->isWritable())
    {
      FUSE_DEBUG("create({})", cpath);

      /* create new file, init and return */
      VirtualFile* file = new VirtualFile(path);
      directory->add(file);
      vfs.initStat(file, false);

      /* if file is created in to sort folder it needs to be organized */
      if (directory == vfs.sortFolder())
        file->_type = VirtualFileType::ToBeOrganized;

      fi->fh = reinterpret_cast<uintptr_t>(file);
      return SUCCESS;
    }
    else
    {
      FUSE_DEBUG("create({}, failed: not writable)", cpath);
      return -EACCES;
    }
  }
  else
  {
    FUSE_DEBUG("create({}, failed: not exists)", cpath);
    return -ENOENT;
  }
}

int CellarFS::mknod(const char* path, mode_t mode, dev_t device)
{
  FUSE_DEBUG("mknod({})", path);
  return 0;
}

int CellarFS::open(const char* cpath, struct fuse_file_info* fi)
{
  path path = cpath;

  auto directory = vfs.findDirectory(path.parent());

  if (directory)
  {
    if (path.filename() == "desktop.ini")
      return 0;
    else
    {      
      auto* file = directory->get(path.filename());

      if (file)
      {
        fi->fh = reinterpret_cast<uintptr_t>(file);
        return SUCCESS;
      }
      else
      {
        FUSE_DEBUG("open({}, failed: file not existing)", cpath);
        return -ENOENT;
      }
    }
  }
  else
  {
    FUSE_DEBUG("open({}, failed: directory not existing)", cpath);
    return -ENOENT;
  }

  /*
  std::string flags = "";
  if (fi->flags & O_RDONLY) flags += "O_RDONLY ";
  if (fi->flags & O_WRONLY) flags += "O_WRONLY ";
  if (fi->flags & O_RDWR) flags += "O_RDWR ";
  if (fi->flags & O_APPEND) flags += "O_APPEND ";
  if (fi->flags & O_CREAT) flags += "O_CREAT ";
  if (fi->flags & O_EXCL) flags += "O_EXCL ";
  if (fi->flags & O_TRUNC) flags += "O_TRUNC ";
  */
}

int CellarFS::read(const char* path, char* buf, size_t size, fuse_offset offset, struct fuse_file_info* fi)
{
  FUSE_DEBUG("read({})", path);

  VirtualFile* file = (VirtualFile*)fi->fh;

  if (!fi->fh)
    return -ENOENT;
  else
  {
    FUSE_DEBUG("read({}, size: {}, offset: {})", path, size, offset);

    auto* file = reinterpret_cast<VirtualFile*>(fi->fh);

    size_t readSize = file->read(buf, size, offset);
    file->stbuf.st_atim.tv_sec = std::time(nullptr);
    file->stbuf.st_atim.tv_nsec = 0;

    return readSize;
  }
}

int CellarFS::write(const char* path, const char* buf, size_t length, FUSE_OFF_T offset, struct fuse_file_info* fi)
{
  if (!fi->fh)
  {
    FUSE_DEBUG("write({}, failed)", path);
    return -ENOENT;
  }
  else
  {
    FUSE_DEBUG("write({}, length: {}, offset: {})", path, length, offset);

    auto* file = reinterpret_cast<VirtualFile*>(fi->fh);

    file->write(buf, length, offset);
    file->stbuf.st_mtim.tv_sec = std::time(nullptr);
    file->stbuf.st_mtim.tv_nsec = 0;

    return length;
  }
}

CellarFS::CellarFS() : fs(nullptr)
{
  instance = this;

  memset(&ops, 0, sizeof(fuse_operations));

  ops.statfs = &CellarFS::statsfs;
  ops.access = &CellarFS::access;
  ops.getattr = &CellarFS::sgetattr;
  ops.getxattr = &CellarFS::sgetxattr;

  ops.opendir = &CellarFS::sopendir;
  ops.readdir = &CellarFS::sreaddir;

  ops.create = &CellarFS::create;
  ops.mknod = &CellarFS::mknod;
 
  ops.open = &CellarFS::open;
  ops.read = &CellarFS::read;
  ops.write = &CellarFS::write;

  ops.flush = &CellarFS::flush;
  ops.release = &CellarFS::release;
  ops.releasedir = &CellarFS::releasedir;

  ops.utimens = &CellarFS::utimens;
}

void CellarFS::createHandle()
{
  char* argv[] = { (char*)"fuse", (char*)"-f", /*(char*)"-d",*/ (char*)"-s", (char*)R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\mount)"};
  int i = fuse_main(sizeof(argv)/sizeof(argv[0]), (char**)argv, &ops, nullptr);
}

fs_ret CellarFS::flush(const char* path, struct fuse_file_info* fi)
{
  FUSE_DEBUG("flush({})", path);
  return 0;
}

fs_ret CellarFS::release(const char* path, struct fuse_file_info* fi)
{
  FUSE_DEBUG("release({})", path);

  if (fi->fh)
  {
    auto* file = reinterpret_cast<VirtualFile*>(fi->fh);

    if (file->type() == VirtualFileType::ToBeOrganized)
    {
      LOG_DEBUG("  file must be organized");

      Hasher hasher;
      auto hash = hasher.compute(file->_content.data(), file->_content.size());

      auto result = data.hashes().find(hash);

      if (result)
      {
        LOG_DEBUG("  found valid entry: {}", result->roms[0].game->name);
      }
      else
        LOG_DEBUG("  not valid entry");
    }
  }
  else
  {
    FUSE_DEBUG("release({}, failed)", path);
    return -ENOENT;
  }

  return 0;
}

fs_ret CellarFS::releasedir(const char* path, struct fuse_file_info* fi)
{
  FUSE_DEBUG("releasedir({})", path);
  return 0;
}

fs_ret CellarFS::utimens(const char* path, const struct timespec tv[2])
{
  FUSE_DEBUG("utimens({})", path);
  return 0;
}

fs_ret CellarFS::sgetxattr(const char* path, const char* name, char* value, size_t size)
{
  FUSE_DEBUG("sgetxattr({}, {})", path, name);
  return -ENOSYS;
}

#define ATTR_AS_FILE(x) x->st_mode = S_IFREG | 0666
#define ATTR_AS_DIR(x) x->st_mode = S_IFDIR | 0777

fs_ret CellarFS::getattr(const fs_path& path, FUSE_STAT* stbuf)
{  
  auto directory = vfs.findDirectory(path);

  if (directory)
  {
    //FUSE_DEBUG("getaddr({}, success)", path);
    *stbuf = *vfs.defaultDirectoryStat();
    return SUCCESS;
  }

  directory = vfs.findDirectory(path.parent());

  if (directory)
  {
    auto* file = directory->get(path.filename());

    if (file)
    {
      //FUSE_DEBUG("getaddr({}, success)", path);
      *stbuf = file->stbuf;
      return SUCCESS;
    }
  }

  FUSE_DEBUG("getaddr({}, failed: path not found)", path);
  return -ENOENT;
}

using fuse_opaque_handle = uintptr_t;

fs_ret CellarFS::opendir(const fs_path& path, fuse_file_info* fi)
{
  FUSE_DEBUG("opendir({})", path);

  /* opendir is called before readdir, we can store in fi->fh values used
     later by readdir */

  fs_ret ret = 0;
  fi->fh = 0ULL;

  auto directory = vfs.findDirectory(path);

  if (directory)
  {
    fi->fh = reinterpret_cast<uintptr_t>(directory);
    return 0;
  }
  else
    return -ENOENT;
}

fs_ret CellarFS::readdir(const fs_path& path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, struct fuse_file_info* fi)
{
  if (!fi->fh)
  {
    FUSE_DEBUG("readdir({}, failed: not existing)", path);
    return -ENOENT;
  }
  else
  {
    VirtualDirectory* directory = reinterpret_cast<VirtualDirectory*>(fi->fh);

    FUSE_DEBUG("readdir({}, files: {})", path, directory->count());

    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);

    for (size_t i = 0; i < directory->count(); ++i)
    {
      auto* entry = directory->get(i);

      if (entry->isFile())
      {
        VirtualFile* file = (VirtualFile*)entry;
        FUSE_DEBUG("  - file: {}, size: {}, mode: {:o}", file->path(), file->stbuf.st_size, file->stbuf.st_mode);
        filler(buf, entry->filename().c_str(), &((VirtualFile*)entry)->stbuf, 0);
      }
      else
      {
        FUSE_DEBUG("  - dir: {}", entry->filename());
        filler(buf, entry->filename().c_str(), vfs.defaultDirectoryStat(), 0);
      }
    }

    return SUCCESS;
  }
}
