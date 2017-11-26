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

bool FileSystem::deleteFile(const path& path) const
{
  TRACE_FS("%p: deleting file %s", this, path.c_str());

  bool isDirectory = existsAsFolder(path);
  bool success = isDirectory ? internalDeleteDirectory(path) :  (remove(path.c_str()) == 0);
  return success;
}

#else
#error unimplemented FileSystem for platform
#endif
