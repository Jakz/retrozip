#pragma once

#include "base/common.h"

#include "core/data_source.h"

#include "filter_queue.h"
#include "header.h"

#include <queue>

class ArchiveEntry
{
public:
  using ref = size_t;
  
private:
  mutable box::Entry _binary;
  
  /* TODO: manage data ownership */
  /*std::unique_ptr<*/seekable_data_source*/*>*/ _source;
  std::string _name;
  filter_builder_queue _filters;
  mutable memory_buffer _payload;
  
public:
  ArchiveEntry(const std::string& name, seekable_data_source* source) : _source(source), _name(name) { }
  
  void setName(const std::string& name) { this->_name = name; }
  const std::string& name() const { return _name; }
  
  void setSource(seekable_data_source* source) { this->_source = source; /*std::unique_ptr<seekable_data_source>(source);*/ }
  const decltype(_source)& source() { return _source; }
  
  const memory_buffer& payload() const
  {
    _payload = _filters.payload();
    return _payload;
  }
  
  box::count_t payloadLength() const
  {
    return static_cast<box::count_t>(payload().size());
  }
  
  void addFilter(filter_builder* builder) { _filters.add(builder); }
  const filter_builder_queue& filters() { return _filters; }
  
  box::Entry& binary() const { return _binary; }
};

class ArchiveStream
{
public:
  using ref = size_t;
  
private:
  mutable box::Stream _binary;
  
  std::vector<ArchiveEntry::ref> _entries;
  filter_builder_queue _filters;
  mutable memory_buffer _payload;

public:
  ArchiveStream() { }
  
  void assignEntry(ArchiveEntry::ref entry) { _entries.push_back(entry); }
  const std::vector<ArchiveEntry::ref> entries() { return _entries; }
  
  const memory_buffer& payload() const
  {
    _payload = _filters.payload();
    return _payload;
  }
  
  box::count_t payloadLength() const
  {
    return static_cast<box::count_t>(payload().size());
  }
  
  void addFilter(filter_builder* builder) { _filters.add(builder); }
  const filter_builder_queue& filters() { return _filters; }
  
  box::Stream& binary() const { return _binary; }
};

struct Options
{
  size_t bufferSize;
  
  struct
  {
    bool crc32;
    bool md5;
    bool sha1;
  } digest;
  
  struct
  {
    bool calculateGlobalChecksum;
    size_t digesterBuffer;
  } checksum;
  
  Options() : bufferSize(16), digest({true, true, true}), checksum({true, MB1}) { }
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
  
  void writeEntry(W& w, ArchiveStream& stream, ArchiveEntry& entry);
  void writeEntryPayloads(W& w);
  void writeStreamPayloads(W& w);
  
  const ArchiveEntry& entryForRef(ArchiveEntry::ref ref) const { return entries[ref]; }
  ArchiveEntry& entryForRef(ArchiveEntry::ref ref) { return entries[ref]; }
  
public:
  Archive();
  
  void write(W& w);
  void read(R& r);

  const box::Header& header() const { return _header; }
  Options& options() { return _options; }
  
  bool isValidMagicNumber() const;
  bool isValidGlobalChecksum(W& w) const;
  
  static Archive ofSingleEntry(const std::string& name, seekable_data_source* source, std::initializer_list<filter_builder*> builders);
};

