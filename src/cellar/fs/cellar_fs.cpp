#include "cellar_fs.h"

#include "tbx/extra/fmt/format.h"

#include <iostream>

//static const std::string PATH_ROOT = "/";
//static const std::string PATH_TO_SORT = "/ToSort";

static const std::string PATH_ROOT = "/";
static const std::string PATH_TO_SORT = "/ToSort";
#if !defined(O_ACCMODE)
#define O_ACCMODE     (O_RDONLY | O_WRONLY | O_RDWR)
#endif

#define FUSE_DEBUG_FLAG true

#define FUSE_DEBUG(__format__, ...) \
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
};


struct VirtualFile : public VirtualEntry
{
  std::vector<uint8_t> _content;
  FUSE_STAT stbuf;
  VirtualFileType _type;

  VirtualFile(const path& p) : VirtualEntry(p)
  {
    initStat(&stbuf);
  }
  
  VirtualFile() : VirtualFile("") { }

  auto type() const { return _type; }
  bool isFile() const override { return true; }

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

public:
  VirtualDirectory(const path& path) : VirtualEntry(path) { }

  void add(VirtualEntry* entry) { _content.push_back(entry); }
  VirtualEntry* get(size_t index) { return _content[index]; }

  bool isWritable() const { return true; }
  bool isFile() const override { return false; }
  virtual size_t count() const { return _content.size(); }
};




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

  VirtualDirectory* sortFolder() const;
};

VirtualFileSystem::VirtualFileSystem()
{
  _root.reset(new VirtualDirectory("/"));

  _toSortFolder = new VirtualDirectory("/ToSort");
  _root->add(_toSortFolder);

  _flatMapping["/"] = _root.get();
  _flatMapping["/ToSort"] = _toSortFolder;
  
  initStat(&_defaultDirectoryStat, true, true);
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

std::unordered_map<path, VirtualFile, path::hash> _files;


VirtualFileSystem vfs;


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

  //auto directory = vfs.findDirectory(path.parent());

  if (path.parent() == PATH_TO_SORT)
  {
    FUSE_DEBUG("create({})", cpath);
    auto it = _files.emplace(std::make_pair(cpath, VirtualFile(cpath)));
    it.first->second._type = VirtualFileType::ToBeOrganized;

    fi->fh = reinterpret_cast<uintptr_t>(&it.first->second);
    fi->keep_cache = 1;
  }
  else
  {
    FUSE_DEBUG("create({}, failed)", cpath);
    return -ENOENT;
  }

  return 0;
}

int CellarFS::mknod(const char* path, mode_t mode, dev_t device)
{
  FUSE_DEBUG("mknod({})", path);
  return 0;
}

int CellarFS::open(const char* cpath, struct fuse_file_info* fi)
{
  path path = cpath;
  if (path.parent() == PATH_TO_SORT)
  {
    if (path.filename() != "desktop.ini")
    {      
      std::string flags = "";
      if (fi->flags & O_RDONLY) flags += "O_RDONLY ";
      if (fi->flags & O_WRONLY) flags += "O_WRONLY ";
      if (fi->flags & O_RDWR) flags += "O_RDWR ";
      if (fi->flags & O_APPEND) flags += "O_APPEND ";
      if (fi->flags & O_CREAT) flags += "O_CREAT ";
      if (fi->flags & O_EXCL) flags += "O_EXCL ";
      if (fi->flags & O_TRUNC) flags += "O_TRUNC ";
      
      auto it = _files.find(path);
      if (it != _files.end())
      {
        fi->fh = reinterpret_cast<uintptr_t>(&it->second);



        FUSE_DEBUG("open({}, success, existing, {})", cpath, fi->fh, flags);
      }
      else
      {
        fi->fh = 0ULL;
        FUSE_DEBUG("open({}, success, non-existing, {})", cpath, fi->fh, flags);
      }

      return 0;
    }   
  }
  else
  {
    FUSE_DEBUG("open({}, failed)", cpath);
    return -ENOENT;
  }
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
  auto it = _files.find(path);

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
      //_files.erase(path);
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
  int res = 0;
  memset(stbuf, 0, sizeof(FUSE_STAT));
  stbuf->st_nlink = 1;

  stbuf->st_uid = 1000;
  stbuf->st_gid = 1000;

  auto now = std::time(nullptr);
  stbuf->st_mtim.tv_sec = now;
  stbuf->st_mtim.tv_nsec = 0;
  stbuf->st_atim = stbuf->st_mtim;
  stbuf->st_ctim = stbuf->st_mtim;

  if (path == PATH_ROOT)
  {
    ATTR_AS_DIR(stbuf);
  }
  else if (path == PATH_TO_SORT)
  {
    ATTR_AS_DIR(stbuf);
  }
  else if (path.parent() == PATH_TO_SORT)
  {
    auto filename = path.filename();
    auto it = _files.find(path);
    //FUSE_DEBUG("getattr({}, existing: {})", path, it != _files.end());

    if (it != _files.end())
    {
      *stbuf = it->second.stbuf;
      return 0;
    }
    else
      return -ENOENT;

    /*auto tpath = ::path(path.c_str() + 1);

    const DatFile* dat = data.datForName(tpath.str());

    if (dat)
      ATTR_AS_DIR(stbuf);
    else
    {
      auto ppath = tpath.parent();

      const DatFile* dat = data.datForName(ppath.str());

      if (dat)
      {
        auto fileName = tpath.filename();

        const DatGame* game = dat->gameByName(fileName);

        if (game)
        {
          ATTR_AS_FILE(stbuf);
          stbuf->st_size = data.hashes()[game->roms[0].ref].size();
        }
      }
    }*/

    /*stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(hello_str);*/
  }
  else
    res = -ENOENT;

  return res;
}

using fuse_opaque_handle = uintptr_t;

constexpr static fuse_opaque_handle FUSE_HANDLE_ROOT = 0xFFFFFFFFFFFFFFFFULL;
constexpr static fuse_opaque_handle FUSE_HANDLE_TO_SORT = 0xFFFFFFFFFFFFFFFEULL;

fs_ret CellarFS::opendir(const fs_path& path, fuse_file_info* fi)
{
  FUSE_DEBUG("opendir({})", path);

  /* opendir is called before readdir, we can store in fi->fh values used
     later by readdir */

  fs_ret ret = 0;
  fi->fh = 0ULL;

  /* if path is root use special value to signal it */
  if (path == PATH_ROOT)
    fi->fh = FUSE_HANDLE_ROOT;
  else if (path == PATH_TO_SORT)
    fi->fh = FUSE_HANDLE_TO_SORT;
  else if (path.isAbsolute())
  {
    //fi->fh = -ENOENT;
    
    /* path is made as "/some_nice_text", find a corresponding DAT */
    /*const DatFile* dat = data.datForName(path.makeRelative().str());

    if (dat)
      fi->fh = reinterpret_cast<uintptr_t>(dat);
    else
      fi->fh = -ENOENT;
    */
  }

  return ret;
}

FUSE_STAT sortStat;

fs_ret CellarFS::readdir(const fs_path& path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, struct fuse_file_info* fi)
{
  /* special case, all the DATs folders */
  if (fi->fh == FUSE_HANDLE_ROOT)
  {
    FUSE_DEBUG("readdir({})", path);

    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);

    initStat(&sortStat, true);

    filler(buf, PATH_TO_SORT.substr(1).c_str(), &sortStat, 0);

    //for (const auto& dat : data.dats())
    //  filler(buf, dat.second.folderName.c_str(), nullptr, 0);

    return 0;
  }
  else if (fi->fh == FUSE_HANDLE_TO_SORT)
  {
    FUSE_DEBUG("readdir({}, files: {})", path, _files.size());

    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);

    for (auto& file : _files)
    {
      FUSE_DEBUG("readdir[{}]  - filename: {}, path: {}, size: {}, mode: {:o}", path, file.first.filename(), file.first, file.second.stbuf.st_size, file.second.stbuf.st_mode);
      filler(buf, file.first.filename().c_str(), &file.second.stbuf, 0);
    }

    return 0;
  }
  else if (fi->fh)
  {
    /*const DatFile* dat = reinterpret_cast<const DatFile*>(fi->fh);

    if (dat)
    {
      filler(buf, ".", nullptr, 0);
      filler(buf, "..", nullptr, 0);
      */
      /* fill with all entry from DAT */
  /*    for (const auto& entry : dat->games)
        filler(buf, entry.name.c_str(), nullptr, 0);

      return 0;
    }*/
  }

  return -ENOENT;
}
