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
private:
  mutable rzip::TableEntry _tableEntry;

  std::unique_ptr<data_source> _source;
  std::string _name;
  
public:
  Entry(const std::string& name, data_source* source) : _source(source), _name(name) { }
  
  const std::string& name() const { return _name; }
  
  rzip::count_t payloadLength() const { return 0 ; }
  
  rzip::TableEntry& tableEntry() const { return _tableEntry; }
};

class Stream
{
private:
  mutable rzip::StreamEntry _streamEntry;

public:
  Stream() { }
  
  rzip::count_t payloadLength() const { return 0 ; }
  
  rzip::StreamEntry& streamEntry() const { return _streamEntry; }
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

