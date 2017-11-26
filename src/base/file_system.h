#pragma once

#include "path.h"
#include "common.h"

#include <vector>

class FileSystem
{
  
private:
  bool internalDeleteDirectory(const path& path) const;

public:
  static const FileSystem* i();
  
  std::vector<path> contentsOfFolder(const path& folder, bool recursive = true, predicate<path> exclude = [](const path&){ return false; }) const;
  
  bool existsAsFolder(const path& path) const;
  bool existsAsFile(const path& path) const;
  
  bool createFolder(const path& folder, bool intermediate = true) const;
  
  bool deleteFile(const path& path) const;
};
