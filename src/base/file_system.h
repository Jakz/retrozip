#pragma once

#include "path.h"

#include <vector>

class FileSystem
{
  
private:
  bool internalDeleteDirectory(const path& path) const;

public:
  const FileSystem* i();
  
  std::vector<path> contentsOfFolder(const path& folder) const;
  
  bool existsAsFolder(const path& path) const;
  bool existsAsFile(const path& path) const;
  
  bool createFolder(const path& folder, bool intermediate = true) const;
  
  bool deleteFile(const path& path) const;
};
