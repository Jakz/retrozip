#include "file_system.h"

const FileSystem* FileSystem::i()
{
  static FileSystem instance;
  return &instance;
}

#if defined(__APPLE__)

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

void scanFolder(const path& root, const std::function<void(const path& path)>& lambda, bool recursive = true)
{
  TRACE_FS("%p: scanning folder %s", this, root.c_str());
  
  DIR *d;
  struct dirent *dir;
  d = opendir(root.c_str());
  
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      path name = path(dir->d_name);
      
      if (name == "." || name == ".." || name == ".DS_Store")
        continue;
      else if (dir->d_type == DT_DIR && recursive)
      {
        scanFolder(root, lambda, recursive);
      }
      else if (dir->d_type == DT_REG)
        lambda(root.append(name));
    }
    
    closedir(d);
  }
  else
    throw exceptions::file_not_found(root);
}

std::vector<path> FileSystem::contentsOfFolder(const path& base, bool recursive, predicate<path> excludePredicate) const
{
  std::vector<path> files;
  
  TRACE_FS("%p: scanning folder %s", this, base.c_str());
  
  DIR *d;
  struct dirent *dir;
  d = opendir(base.c_str());
  
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      path name = path(dir->d_name);
      
      if (name == "." || name == ".." || name == ".DS_Store" || excludePredicate(name))
        continue;
      else if (dir->d_type == DT_DIR && recursive)
      {
        auto rfiles = contentsOfFolder(base.append(name), recursive, excludePredicate);
        files.reserve(files.size() + rfiles.size());
        std::move(rfiles.begin(), rfiles.end(), std::back_inserter(files));
      }
      else if (dir->d_type == DT_REG)
        files.push_back(base.append(name));
    }
    
    closedir(d);
  }
  else
    throw exceptions::file_not_found(base);
  
  return files;
}

bool FileSystem::createFolder(const path& path, bool intermediate) const
{
  TRACE_FS("%p: creating folder %s (intermediate: %s)", this, path.c_str(), intermediate ? "true" : "false");

  
  if (intermediate)
    system((std::string("mkdir -p ")+path.c_str()).c_str());
  else
    mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  
  return true;
}

bool FileSystem::existsAsFolder(const path& path) const
{
  struct stat sb;
  return stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode);
}

bool FileSystem::existsAsFile(const path& path) const
{
  struct stat sb;
  return stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
}

bool FileSystem::internalDeleteDirectory(const path& path) const
{
  DIR* d = opendir(path.c_str());
  struct dirent *dir;
  bool success = true;
  
  if (d)
  {
    while ((dir = readdir(d)) != nullptr)
    {
      std::string fileName = dir->d_name;
      
      if (fileName != "." && fileName != "..")
        success &= deleteFile(path.append(fileName));
    }
    
    closedir(d);
  }
  
  success &= rmdir(path.c_str()) == 0;
  
  return success;
}

#include <fstream>

bool FileSystem::copy(const path &from, const path &to) const
{
  std::ifstream  src(from.str(), std::ios::binary);
  std::ofstream  dst(to.str(),   std::ios::binary);
  dst << src.rdbuf();
  return true;
}

bool FileSystem::deleteFile(const path& path) const
{
  TRACE_FS("%p: deleting file %s", this, path.c_str());

  bool isDirectory = existsAsFolder(path);
  bool success = isDirectory ? internalDeleteDirectory(path) :  (remove(path.c_str()) == 0);
  return success;
}

bool FileSystem::fallocate(const path &path, size_t aLength) const
{
  file_handle handle(path, file_mode::WRITING);
  int fd = handle.fd();
  
#if defined(HAVE_POSIX_FALLOCATE)
  return posix_fallocate(fd, 0, aLength) == 0;
#elif defined(XP_WIN)
  return PR_Seek64(aFD, aLength, PR_SEEK_SET) == aLength
  && 0 != SetEndOfFile(aFD);
#elif defined(__APPLE__)
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, (roff_t)aLength};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
  if(-1 == ret){
    // OK, perhaps we are too fragmented, allocate non-continuous
    store.fst_flags = F_ALLOCATEALL;
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (-1 == ret)
      return false;
  }
  return 0 == ftruncate(fd, aLength);
#elif defined(XP_UNIX)
  // The following is copied from fcntlSizeHint in sqlite
  /* If the OS does not have posix_fallocate(), fake it. First use
   ** ftruncate() to set the file size, then write a single byte to
   ** the last byte in each block within the extended region. This
   ** is the same technique used by glibc to implement posix_fallocate()
   ** on systems that do not have a real fallocate() system call.
   */
  struct stat buf;
  int fd = PR_FileDesc2NativeHandle(aFD);
  if (fstat(fd, &buf))
    return false;
  
  if (buf.st_size >= aLength)
    return false;
  
  const int nBlk = buf.st_blksize;
  
  if (!nBlk)
    return false;
  
  if (ftruncate(fd, aLength))
    return false;
  
  int nWrite; // Return value from write()
  PRInt64 iWrite = ((buf.st_size + 2 * nBlk - 1) / nBlk) * nBlk - 1; // Next offset to write to
  do {
    nWrite = 0;
    if (PR_Seek64(aFD, iWrite, PR_SEEK_SET) == iWrite)
      nWrite = PR_Write(aFD, "", 1);
    iWrite += nBlk;
  } while (nWrite == 1 && iWrite < aLength);
  return nWrite == 1;
#endif
  return false;
}

#elif defined(_WIN32)

#include <filesystem>

namespace fs = std::filesystem;

void scanFolder(const path& root, const std::function<void(bool, const path& path)>& lambda, bool recursive = true)
{
  std::vector<path> result;

  if (recursive)
  {
    for (const auto& entry : fs::recursive_directory_iterator(fs::path(root.data())))
    {
      lambda(fs::is_directory(entry), path(entry.path().string()));
    }
  }
  else
  {
    for (const auto& entry : fs::directory_iterator(fs::path(root.data())))
    {
      lambda(!fs::is_directory(entry), path(entry.path().string()));
    }
  }
}

std::vector<path> FileSystem::contentsOfFolder(const path& folder, bool recursive, predicate<path> exclude) const
{
  std::vector<path> result;

  if (recursive)
  {
    for (const auto& entry : fs::recursive_directory_iterator(fs::path(folder.data())))
    {
      auto path = entry.path().string();
      std::replace(path.begin(), path.end(), '\\', '/');
      
      if (!exclude(path))
        result.push_back(path);
    }
  }
  else
  {
    for (const auto& entry : fs::directory_iterator(fs::path(folder.data())))
    {
      auto path = entry.path().string();
      std::replace(path.begin(), path.end(), '\\', '/');

      if (!exclude(path))
        result.push_back(path);
    }
  }

  return result;
}

bool FileSystem::existsAsFolder(const path& path) const
{
  return fs::is_directory(fs::path(path.data()));
}
    
bool FileSystem::existsAsFile(const path& path) const
{
  return fs::is_regular_file(fs::path(path.data()));
}

bool FileSystem::copy(const path& from, const path& to) const
{
  fs::copy(fs::path(from.data()), fs::path(to.data()));
  return true;
}

bool FileSystem::createFolder(const path& folder, bool intermediate) const
{
  if (intermediate)
    fs::create_directories(fs::path(folder.data()));
  else
    fs::create_directory(fs::path(folder.data()));
  
  return true;
}

bool FileSystem::deleteFile(const path& path) const
{
  return fs::remove(fs::path(path.data()));
}

bool FileSystem::fallocate(const path& path, size_t size) const
{
  return false;
}

#else
#error unimplemented FileSystem for platform
#endif
