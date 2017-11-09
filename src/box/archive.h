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

class ArchiveEntry
{
public:
  using ref = size_t;
  
private:
  mutable box::Entry _tableEntry;

  std::unique_ptr<data_source> _source;
  std::string _name;
  
public:
  ArchiveEntry(const std::string& name, data_source* source) : _source(source), _name(name) { }
  
  const std::string& name() const { return _name; }
  
  box::count_t payloadLength() const { return 0 ; }
  
  box::Entry& tableEntry() const { return _tableEntry; }
};

class ArchiveStream
{
public:
  using ref = size_t;
  
private:
  mutable box::Stream _streamEntry;
  
  std::vector<ArchiveEntry::ref> entries;

public:
  ArchiveStream() { }
  
  box::count_t payloadLength() const { return 0 ; }
  
  box::Stream& streamEntry() const { return _streamEntry; }
};

struct Options
{
  bool computeCRC32;
  bool computeMD5;
  bool computeSHA1;
  
  struct
  {
    bool calculateGlobalChecksum;
    size_t digesterBuffer;
  } checksum;
  
  Options() : computeCRC32(true), computeMD5(true), computeSHA1(true), checksum({true, MB1}) { }
};

class memory_buffer;
using W = memory_buffer;
using R = memory_buffer;

class Archive
{
private:
  Options _options;
  
  box::Header _header;
  
  std::vector<ArchiveEntry> entries;
  std::vector<ArchiveStream> streams;
  
  std::queue<box::Section> ordering;
  
  void finalizeHeader(W& w);
  box::checksum_t calculateGlobalChecksum(W& w, size_t bufferSize) const;
  
  void writeStream(W& w, ArchiveStream& stream);
  
public:
  Archive();
  
  void write(W& w);
  void read(R& r);
  
  const box::Header& header() const { return _header; }
  Options& options() { return _options; }
  
  bool isValidMagicNumber() const;
  bool isValidGlobalChecksum(W& w) const;
};

