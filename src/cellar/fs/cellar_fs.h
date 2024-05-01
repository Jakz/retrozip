#pragma once

#define FUSE_USE_VERSION 26
#include <cstring>
#include <cerrno>
#include "fuse/fuse.h"

#include "tbx/base/path.h"

using fs_ret = int;
using fs_path = path;

using fsblkcnt_t = uint64_t;
using fsfilcnt_t = uint64_t;

using fuse_offset = long long;

class CellarFS
{
private:
  static CellarFS* instance;
  struct fuse_operations ops;
  fuse* fs;

  static int statsfs(const char* foo, struct statvfs* stats);

  static int access(const char* path, int);

  static int sgetattr(const char* path, FUSE_STAT* stbuf);
  static int sreaddir(const char* path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, fuse_file_info* fi);
  static int sopendir(const char* path, fuse_file_info* fi);

  static int create(const char* path, mode_t mode, struct fuse_file_info* fi);
  static int mknod(const char* path, mode_t mode, dev_t device);

  
  static int open(const char* path, struct fuse_file_info* fi);
  static int read(const char* path, char* buf, size_t size, fuse_offset offset, struct fuse_file_info* fi);
  static int write(const char* path, const char* buf, size_t size, fuse_offset offset, struct fuse_file_info* fi);

  fs_ret getattr(const fs_path& path, FUSE_STAT* stbuf);

  fs_ret opendir(const fs_path& path, fuse_file_info* fi);
  fs_ret readdir(const fs_path& path, void* buf, fuse_fill_dir_t filler, fuse_offset offset, fuse_file_info* fi);

public:
  CellarFS();
  void createHandle();
};

