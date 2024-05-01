#include "path.h"

#include "tbx/base/common.h"
#include "tbx/base/exceptions.h"

#include <sys/stat.h>
//#include <dirent.h>

#include "file_system.h"

static constexpr const char SEPARATOR = '/';

path::path(const char* data) : _data(data)
{
  if (_data.back() == SEPARATOR && _data.length() > 1)
    _data.pop_back();
}

path::path(const std::string& data) : _data(data)
{
  if (!_data.empty() && _data.back() == SEPARATOR && _data.length() > 1)
    _data.pop_back();
}

bool path::isAbsolute() const
{
  return !_data.empty() && _data[0] == SEPARATOR;
}

bool path::isFolder() const { return FileSystem::i()->existsAsFolder(*this); }

bool path::exists() const
{
  return FileSystem::i()->existsAsFile(*this) || FileSystem::i()->existsAsFolder(*this);
}

path path::relativizeToParent(const path& parent) const
{
  if (!strings::isPrefixOf(_data, parent._data))
    throw exceptions::path_non_relative(parent, *this);
  else
    return path(_data.substr(parent._data.length()+1));
}

path path::relativizeChildren(const path& children) const
{
  if (!strings::isPrefixOf(children._data, _data))
    throw exceptions::path_non_relative(*this, children);
  else
    return path(children._data.substr(_data.length()+1));
}

std::string path::filename() const
{
  size_t index = _data.find_last_of(SEPARATOR);

  return index != std::string::npos ? _data.substr(index+1) : _data;
}

bool endsWith(const std::string& str, char c) { return str.back() == c; }
bool startsWith(const std::string& str, char c) { return str.front() == c; }
path path::append(const path& other) const
{
  if (other.isAbsolute())
    throw exceptions::path_exception(fmt::sprintf("path::append: children %s can't be absolute", other.c_str()));
  
  if (_data.empty())
    return other;
  else if (!endsWith(_data,SEPARATOR) && !startsWith(other._data, SEPARATOR))
    return path(_data + SEPARATOR + other._data);
  else if (endsWith(_data, SEPARATOR) && startsWith(other._data, SEPARATOR))
    return path(_data + other._data.substr(1));
  else
    return path(_data + other._data);
}

bool path::hasExtension(const std::string& ext) const
{
  size_t index = _data.find_last_of('.');
  return index != std::string::npos && _data.substr(index+1) == ext;
}

path path::removeLast() const
{
  size_t index = _data.rfind(SEPARATOR);
  
  if (index != std::string::npos)
    return path(_data.substr(0, index));
  else
    return path();
}

std::pair<path, path> path::splitParentAndFilename() const
{
  size_t index = _data.rfind(SEPARATOR);
  
  if (index == std::string::npos)
    return std::make_pair(path(), *this);
  else
    return std::make_pair(_data.substr(0, index), _data.substr(index+1));
}

path path::removeAllButFirst() const
{
  size_t index = _data.find(SEPARATOR);
  return index != std::string::npos ? _data.substr(index+1) : _data;
}

path path::makeRelative() const
{
  return _data[0] == SEPARATOR ? path(&_data[1]) : *this;
}

size_t path::length() const
{
  struct stat sb;
  stat(_data.c_str(), &sb);
  return sb.st_size;
}
