#pragma once

#include "base/common.h"

#include "data_source.h"
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
public:
  using ref = size_t;
  
private:
  mutable box::TableEntry _tableEntry;

  std::unique_ptr<data_source> _source;
  std::string _name;
  
public:
  Entry(const std::string& name, data_source* source) : _source(source), _name(name) { }
  
  const std::string& name() const { return _name; }
  
  box::count_t payloadLength() const { return 0 ; }
  
  box::TableEntry& tableEntry() const { return _tableEntry; }
};

class Stream
{
public:
  using ref = size_t;
  
private:
  mutable box::StreamEntry _streamEntry;
  
  std::vector<Entry::ref> entries;

public:
  Stream() { }
  
  box::count_t payloadLength() const { return 0 ; }
  
  box::StreamEntry& streamEntry() const { return _streamEntry; }
};

struct ArchiveOptions
{
  bool computeCRC32;
  bool computeMD5;
  bool computeSHA1;
  
  ArchiveOptions() : computeCRC32(true), computeMD5(true), computeSHA1(true) { }
};

class memory_buffer;
using W = memory_buffer;

class Archive
{
private:
  box::Header header;
  
  std::vector<Entry> entries;
  std::vector<Stream> streams;
  
  std::queue<box::Section> ordering;
  
public:
  Archive();
  template<typename WW> void write(W& w);
  
  Stream& streamByRef(Stream::ref ref) { return streams[ref]; }
};

