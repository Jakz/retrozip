#include "path.h"

#include "base/common.h"
#include "base/exceptions.h"

#include <sys/stat.h>
#include <dirent.h>

static constexpr const char SEPARATOR = '/';

bool path::exists() const
{
  struct stat buffer;
  return stat(data.c_str(), &buffer) == 0;
}

path path::relativizeToParent(const path& parent) const
{
  if (!strings::isPrefixOf(data, parent.data))
    throw exceptions::path_non_relative(parent, *this);
  else
    return path(data.substr(parent.data.length()+1));
}

path path::relativizeChildren(const path& children) const
{
  if (!strings::isPrefixOf(children.data, data))
    throw exceptions::path_non_relative(*this, children);
  else
    return path(children.data.substr(data.length()+1));
}

bool endsWith(const std::string& str, char c) { return str.back() == c; }
bool startsWith(const std::string& str, char c) { return str.front() == c; }
path path::append(const path& other) const
{
  if (data.empty())
    return other;
  else if (!endsWith(data,SEPARATOR) && !startsWith(other.data, SEPARATOR))
    return path(data + SEPARATOR + other.data);
  else if (endsWith(data, SEPARATOR) && startsWith(other.data, SEPARATOR))
    return path(data + other.data.substr(1));
  else
    return path(data + other.data);
}

bool path::hasExtension(const std::string& ext) const
{
  size_t index = data.find_last_of('.');
  return index != std::string::npos && data.substr(index+1) == ext;
}

std::unordered_set<path, path::hash> path::scanFolder(path base, bool recursive, predicate excludePredicate)
{
  std::unordered_set<path, path::hash> files;
  
  DIR *d;
  struct dirent *dir;
  d = opendir(base.c_str());
  
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      path name = path(dir->d_name);
      
      if (name == "." || name == ".." || name == ".DS_Store" || excludePredicate(path(name)))
        continue;
      else if (dir->d_type == DT_DIR && recursive)
      {
        auto rfiles = scanFolder(base.append(name), recursive, excludePredicate);
        files.reserve(files.size() + rfiles.size());
        files.insert(rfiles.begin(), rfiles.end());
      }
      else if (dir->d_type == DT_REG)
        files.insert(base.append(name));
    }
    
    closedir(d);
  }
  else
    throw exceptions::file_not_found(base);
  
  return files;
}
