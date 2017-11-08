#pragma once

#include "base/common.h"

#include "core/data_source.h"
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

struct Options
{
  bool computeCRC32;
  bool computeMD5;
  bool computeSHA1;
  
  bool calculateSanityChecksums;
  
  Options() : computeCRC32(true), computeMD5(true), computeSHA1(true), calculateSanityChecksums(true) { }
};

class memory_buffer;
using W = memory_buffer;
using R = memory_buffer;

class Archive
{
private:
  Options options;
  
  box::Header header;
  
  std::vector<Entry> entries;
  std::vector<Stream> streams;
  
  std::queue<box::Section> ordering;
  
  void finalizeHeader(W& w);
  
  void writeStream(W& w, Stream& stream);
  
public:
  Archive();
  void write(W& w);
  void read(R& r);
  
  
  Stream& streamByRef(Stream::ref ref) { return streams[ref]; }
  
  
};

