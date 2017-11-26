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

std::vector<path> FileSystem::contentsOfFolder(const path& folder) const
{
  std::vector<path> files;
  
  //TODO: throw except if not exists
  
  DIR *d;
  struct dirent *dir;
  d = opendir(folder.c_str());
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      std::string fileName = dir->d_name;
      
      if (fileName != "." && fileName != "..")
        files.push_back(dir->d_name);
    }
    
    closedir(d);
  }
  
  return files;
}

bool FileSystem::createFolder(const path& path, bool intermediate) const
{
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
  struct stat buf;
  bool isDirectory = existsAsFolder(path);
  bool success = isDirectory ? internalDeleteDirectory(path) :  (remove(path.c_str()) == 0);
  return success;
}

#else
#error unimplemented FileSystem for platform
#endif
