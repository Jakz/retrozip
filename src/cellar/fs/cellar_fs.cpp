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

#define FUSE_DEBUG(__format__, ...) \
  do { \
    std::cout << fmt::format(__format__, __VA_ARGS__) << std::endl; \
  } while(false)

/*
    lock.lock(); \
    buffer.push_back(fmt::format(__format__, __VA_ARGS__)); \
    lock.unlock(); \ 
  } while(false)
  */

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

int CellarFS::access(const char* path, int) { return 0; }

int CellarFS::sgetattr(const char* path, FUSE_STAT* stbuf) { return instance->getattr(path, stbuf); }
int CellarFS::sreaddir(const char* path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, fuse_file_info* fi) { return instance->readdir(path, buf, filler, offset, fi); }
int CellarFS::sopendir(const char* path, fuse_file_info* fi) { return instance->opendir(path, fi); }

int CellarFS::create(const char* cpath, mode_t mode, struct fuse_file_info* fi)
{
  FUSE_DEBUG("create({})", cpath);

  return 0;
}

int CellarFS::mknod(const char* path, mode_t mode, dev_t device)
{
  FUSE_DEBUG("mknod({})", path);
  return 0;
}

int CellarFS::open(const char* cpath, struct fuse_file_info* fi)
{
  FUSE_DEBUG("open({})", cpath);

  path path = cpath;
  if (path.parent() == PATH_TO_SORT)
  {
    if (path.filename() != "desktop.ini")
    {
      fi->fh = 12345ULL;
      return 0;
    }
    
    
  }
  else
    return -ENOENT;
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
  FUSE_DEBUG("write({})", path);
  return length;
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
  //FUSE_DEBUG("getattr({})", path);
  
  int res = 0;
  memset(stbuf, 0, sizeof(FUSE_STAT));
  stbuf->st_nlink = 1;

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
    ATTR_AS_FILE(stbuf);

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

fs_ret CellarFS::readdir(const fs_path& path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, struct fuse_file_info* fi)
{
  FUSE_DEBUG("readdir({})", path);

  /* special case, all the DATs folders */
  if (fi->fh == FUSE_HANDLE_ROOT)
  {
    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);

    filler(buf, PATH_TO_SORT.substr(1).c_str(), nullptr, 0);

    //for (const auto& dat : data.dats())
    //  filler(buf, dat.second.folderName.c_str(), nullptr, 0);

    return 0;
  }
  else if (fi->fh == FUSE_HANDLE_TO_SORT)
  {
    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);
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
