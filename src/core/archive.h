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

class W
{
private:
  off_t position;
  size_t size;
  byte* buffer;
  
public:
  W() : position(0), size(0) { }
  
  virtual void seek(off_t offset, Seek origin)
  {
    switch (origin) {
      case Seek::CUR: position += offset; position = std::min(0LL, position); break;
      case Seek::SET: position = offset; break;
      case Seek::END: position = size - offset; position = std::min(0LL, position); break;
    }
  }
  
  virtual off_t reserve(size_t v)
  {
    assert(position == size);
    off_t p = tell();
    position += v;
    size += v;
    return p;
  }
  
  virtual off_t tell() const { return position; }
  
  virtual void read(void* data, size_t size, size_t count) { position += size*count; }
  virtual void write(const void* data, size_t size, size_t count) { position += size*count; }
};

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

