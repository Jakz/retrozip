#include "cellar_fs.h"

#include "tbx/extra/fmt/format.h"

#include <iostream>

static const char* hello_str = "Hello World!\n";
static const char* hello_path = "/hello";

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


struct VirtualFile
{
  path _path;
  std::vector<uint8_t> _content;
  FUSE_STAT stbuf;

  VirtualFile(const path& p) : _path(p) { }
  VirtualFile() : VirtualFile("")
  { 
    initStat(&stbuf);
  }
};

std::unordered_map<path, VirtualFile, path::hash> _files;



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
  FUSE_DEBUG("create({})", cpath);
  _files[cpath] = VirtualFile(cpath);
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
      if (FUSE_DEBUG_FLAG)
      {
        std::string flags = "";
        if (fi->flags & O_RDONLY) flags += "O_RDONLY ";
        if (fi->flags & O_WRONLY) flags += "O_WRONLY ";
        if (fi->flags & O_RDWR) flags += "O_RDWR "; 
        if (fi->flags & O_APPEND) flags += "O_APPEND ";
        if (fi->flags & O_CREAT) flags += "O_CREAT ";
        if (fi->flags & O_EXCL) flags += "O_EXCL ";
        if (fi->flags & O_TRUNC) flags += "O_TRUNC ";

        FUSE_DEBUG("open({}, success, {}, {})", cpath, fi->fh, flags);
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
  FUSE_DEBUG("getattr({})", path);

  
  //std::cout << "read" << std::endl;

  size_t len;
  (void)fi;
  if (strcmp(path, hello_path) != 0)
    return -ENOENT;
  len = strlen(hello_str);
  if (offset < len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buf, hello_str + offset, size);
  }
  else
    size = 0;
  return (int)size;
}

int CellarFS::write(const char* path, const char* buf, size_t length, FUSE_OFF_T offset, struct fuse_file_info* fi)
{
  auto it = _files.find(path);

  if (it == _files.end())
  {
    FUSE_DEBUG("write({}, failed)", path);
    return -ENOENT;
  }
  else
  {
    FUSE_DEBUG("write({}, length: {}, offset: {})", path, length, offset);

    auto& file = it->second;
    file._content.resize(length);
    memcpy(file._content.data(), buf, length);

    file.stbuf.st_size = length;
    file.stbuf.st_mtim.tv_sec = std::time(nullptr);
    file.stbuf.st_mtim.tv_nsec = 0;
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

  ops.opendir = &CellarFS::sopendir;
  ops.readdir = &CellarFS::sreaddir;

  ops.create = &CellarFS::create;
  ops.mknod = &CellarFS::mknod;

  ops.open = &CellarFS::open;
  ops.read = &CellarFS::read;
  ops.write = &CellarFS::write;
}

void CellarFS::createHandle()
{
  char* argv[] = { (char*)"fuse", (char*)"-f", /*(char*)"-d",*/ (char*)R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\mount)"};
  int i = fuse_main(sizeof(argv)/sizeof(argv[0]), (char**)argv, &ops, nullptr);
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
    FUSE_DEBUG("getattr({}, existing: {})", path, it != _files.end());

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

    for (const auto& file : _files)
    {
      FUSE_DEBUG("readdir[{}]  - filename: {} path: {}", path, file.first.filename(), file.first);
      filler(buf, file.first.filename().c_str(), &file.second.stbuf, 0);
    }
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
