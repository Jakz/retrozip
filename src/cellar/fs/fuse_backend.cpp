#include "cellar_fs.h"

#include "tbx/extra/fmt/format.h"
#include "tbx/base/file_system.h"

#include "fuse_backend.h"
using namespace cellar::vfs;

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
    vfs->trace(fmt::format(__format__, __VA_ARGS__)); \
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

FuseBackend* FuseBackend::instance = nullptr;

int FuseBackend::statsfs(const char* foo, struct statvfs* stats)
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

int FuseBackend::access(const char* cpath, int)
{
  FUSE_DEBUG("access({})", cpath);
  return 0;
}

int FuseBackend::sgetattr(const char* path, FUSE_STAT* stbuf)
{ 
  return instance->getattr(path, stbuf);
}

int FuseBackend::sreaddir(const char* path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, fuse_file_info* fi) { return instance->readdir(path, buf, filler, offset, fi); }
int FuseBackend::sopendir(const char* path, fuse_file_info* fi) { return instance->opendir(path, fi); }

int FuseBackend::create(const char* cpath, mode_t mode, struct fuse_file_info* fi)
{
  path path = cpath;

  auto directory = vfs->findDirectory(path.parent());

  /* directory exists */
  if (directory)
  {
    if (directory->isWritable())
    {
      FUSE_DEBUG("create({})", cpath);

      /* create new file, init and return */
      VirtualFile* file = new VirtualFile(path);
      directory->add(file);
      vfs->initStat(file, false);

      /* if file is created in to sort folder it needs to be organized */
      if (directory == vfs->sortFolder())
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

int FuseBackend::mknod(const char* path, mode_t mode, dev_t device)
{
  FUSE_DEBUG("mknod({})", path);
  return 0;
}

int FuseBackend::open(const char* cpath, struct fuse_file_info* fi)
{
  path path = cpath;

  auto directory = vfs->findDirectory(path.parent());

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

int FuseBackend::read(const char* path, char* buf, size_t size, fuse_offset offset, struct fuse_file_info* fi)
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

int FuseBackend::write(const char* path, const char* buf, size_t length, FUSE_OFF_T offset, struct fuse_file_info* fi)
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

VirtualFileSystem* FuseBackend::vfs = nullptr;


FuseBackend::FuseBackend() : fs(nullptr), started(false)
{
  instance = this;

  memset(&ops, 0, sizeof(fuse_operations));

  ops.statfs = &FuseBackend::statsfs;
  ops.access = &FuseBackend::access;
  ops.getattr = &FuseBackend::sgetattr;
  ops.getxattr = &FuseBackend::sgetxattr;

  ops.opendir = &FuseBackend::sopendir;
  ops.readdir = &FuseBackend::sreaddir;

  ops.create = &FuseBackend::create;
  ops.mknod = &FuseBackend::mknod;
 
  ops.open = &FuseBackend::open;
  ops.read = &FuseBackend::read;
  ops.write = &FuseBackend::write;

  ops.flush = &FuseBackend::flush;
  ops.release = &FuseBackend::release;
  ops.releasedir = &FuseBackend::releasedir;

  ops.utimens = &FuseBackend::utimens;
}

static const char* mountPoint = R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\mount)";

void FuseBackend::mount(VirtualFileSystem* vfs)
{
  FuseBackend::vfs = vfs;
  started = true;

  vfs->info("mounting fuse backend");
  
  char* argv[] = { (char*)"fuse", (char*)"-f", /*(char*)"-d",*/ (char*)"-s", (char*)mountPoint};
  int i = fuse_main(sizeof(argv)/sizeof(argv[0]), (char**)argv, &ops, nullptr);

  vfs->info("unmounting fuse backend");
  started = false;
}

void FuseBackend::unmount()
{
  fuse_unmount(mountPoint, nullptr);
}

fs_ret FuseBackend::flush(const char* path, struct fuse_file_info* fi)
{
  FUSE_DEBUG("flush({})", path);
  return 0;
}

fs_ret FuseBackend::release(const char* path, struct fuse_file_info* fi)
{
  FUSE_DEBUG("release({})", path);

  if (fi->fh)
  {
    auto* file = reinterpret_cast<VirtualFile*>(fi->fh);

    if (file->type() == VirtualFileType::ToBeOrganized)
    {
      LOG_DEBUG("  file must be organized");

      bool organized = vfs->filesReadyToBeSorted(file);

      vfs->sortFolder()->remove(file);
      delete file;
    }
  }
  else
  {
    FUSE_DEBUG("release({}, failed)", path);
    return -ENOENT;
  }

  return 0;
}

fs_ret FuseBackend::releasedir(const char* path, struct fuse_file_info* fi)
{
  FUSE_DEBUG("releasedir({})", path);
  return 0;
}

fs_ret FuseBackend::utimens(const char* path, const struct timespec tv[2])
{
  FUSE_DEBUG("utimens({})", path);
  return 0;
}

fs_ret FuseBackend::sgetxattr(const char* path, const char* name, char* value, size_t size)
{
  FUSE_DEBUG("sgetxattr({}, {})", path, name);
  return -ENOSYS;
}

#define ATTR_AS_FILE(x) x->st_mode = S_IFREG | 0666
#define ATTR_AS_DIR(x) x->st_mode = S_IFDIR | 0777

fs_ret FuseBackend::getattr(const fs_path& path, FUSE_STAT* stbuf)
{  
  auto directory = vfs->findDirectory(path);
  FUSE_DEBUG("getattr({}): searching", path);

  if (directory)
  {
    FUSE_DEBUG("getattr({}): found", path);
    *stbuf = *vfs->defaultDirectoryStat();
    return SUCCESS;
  }

  FUSE_DEBUG("getattr({}): searching", path.parent());
  directory = vfs->findDirectory(path.parent());

  if (directory)
  {
    auto* file = directory->get(path.filename());
    FUSE_DEBUG("getattr({}): searching file", path.filename());


    if (file)
    {
      //FUSE_DEBUG("getaddr({}, success)", path);
      FUSE_DEBUG("getattr({}): found file", path.filename());
      *stbuf = file->stbuf;
      return SUCCESS;
    }
  }

  FUSE_DEBUG("getattr({}): failed", path);
  return -ENOENT;
}

using fuse_opaque_handle = uintptr_t;

fs_ret FuseBackend::opendir(const fs_path& path, fuse_file_info* fi)
{
  FUSE_DEBUG("opendir({})", path);

  /* opendir is called before readdir, we can store in fi->fh values used
     later by readdir */

  fs_ret ret = 0;
  fi->fh = 0ULL;

  auto directory = vfs->findDirectory(path);

  if (directory)
  {
    fi->fh = reinterpret_cast<uintptr_t>(directory);
    return 0;
  }
  else
    return -ENOENT;
}

fs_ret FuseBackend::readdir(const fs_path& path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, struct fuse_file_info* fi)
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
        filler(buf, entry->filename().c_str(), vfs->defaultDirectoryStat(), 0);
      }
    }

    return SUCCESS;
  }
}
