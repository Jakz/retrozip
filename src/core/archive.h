#pragma once

#include "base/common.h"
#include "base/path.h"

#include "header.h"
#include <queue>

namespace header
{
  enum StorageMode
  {
    UNCOMPRESSED = 1,
    
  };
}

class Entry
{
private:
  rzip::TableEntry _tableEntry;

  path _path;
  std::string _name;
  
public:
  Entry(const class path& path, const std::string& name) : _path(path), _name(name) { }
  
  const path& path() const { return _path; }
  const std::string& name() const { return _name; }
  
  rzip::TableEntry& tableEntry() { return _tableEntry; }
};

class Stream
{
private:
  
public:
  Stream() { }
};

enum class Seek
{
  SET = SEEK_SET,
  END = SEEK_END,
  CUR = SEEK_CUR
};

class memory_buffer;
using W = memory_buffer;

class Archive
{
private:
  rzip::Header header;
  
  std::vector<Entry> entries;
  std::vector<Stream> streams;
  
  std::queue<rzip::Section> ordering;
  
public:
  Archive();
  template<typename WW> void write(W& w);
};

