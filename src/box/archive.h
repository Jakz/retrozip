#pragma once

#include "base/common.h"

#include "core/data_source.h"

#include "filter_queue.h"
#include "header.h"

#include <queue>

class memory_buffer;
using W = memory_buffer;
using R = memory_buffer;

class ArchiveEntry
{
public:
  using ref = box::index_t;
  
private:
  mutable box::Entry _binary;
  
  /* TODO: manage data ownership */
  data_source* _source;
  std::string _name;
  filter_builder_queue _filters;
  mutable memory_buffer _payload;
    
  void serializePayload() const { _payload = _filters.payload(); }
  void unserializePayload() { _filters.unserialize(_payload); }
  
public:
  ArchiveEntry(const std::string& name, const box::Entry& binary, const std::vector<byte>& payload) :
    _name(name), _source(nullptr), _binary(binary), _payload(payload.size())
  {
    std::copy(payload.begin(), payload.end(), _payload.raw());
    unserializePayload();
  }
  
  ArchiveEntry(const std::string& name, data_source* source, const std::vector<filter_builder*>& filters) : _source(source), _name(name), _filters(filters) { }
  ArchiveEntry(const std::string& name, data_source* source) : _source(source), _name(name) { }
  
  void setName(const std::string& name) { this->_name = name; }
  const std::string& name() const { return _name; }
  
  const decltype(_source)& source() { return _source; }

  const memory_buffer& payload() const
  {
    serializePayload();
    return _payload;
  }
  
  box::count_t payloadLength() const
  {
    return static_cast<box::count_t>(payload().size());
  }
  
  void mapToStream(box::index_t streamIndex, box::index_t indexInStream)
  {
    _binary.stream = streamIndex;
    _binary.indexInStream = indexInStream;
  }

  void addFilter(filter_builder* builder) { _filters.add(builder); }
  const filter_builder_queue& filters() const { return _filters; }
  
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
  ArchiveStream(const std::vector<ArchiveEntry::ref>& indices, const std::vector<filter_builder*>& filters) : _entries(indices), _filters(filters) { }
  ArchiveStream(ArchiveEntry::ref entry) { assignEntry(entry); }
  ArchiveStream() { }
  ArchiveStream(const box::Stream& binary) : _binary(binary) { }
  
  void assignEntry(ArchiveEntry::ref entry) { _entries.push_back(entry); }
  void assignEntryAtIndex(size_t index, ArchiveEntry::ref entry) { _entries.resize(index+1, box::INVALID_INDEX); _entries[index] = entry; }
  
  const std::vector<ArchiveEntry::ref>& entries() const { return _entries; }
  
                
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
  const filter_builder_queue& filters() const { return _filters; }
  
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

class Archive;

class ArchiveReadHandle
{
private:
  R& r;
  const Archive& _archive;
  const ArchiveEntry& _entry;
  filter_cache _cache;
  
public:
  ArchiveReadHandle(R& r, const Archive& archive, const ArchiveEntry& entry) : r(r), _archive(archive), _entry(entry) { }
  data_source* source(bool total);
};

struct ArchiveFactory
{
  struct Entry
  {
    std::string name;
    data_source* source;
    std::vector<filter_builder*> filters;
  };
  
  struct Stream
  {
    std::vector<ArchiveEntry::ref> entries;
    std::vector<filter_builder*> filters;
  };
  
  struct Data
  {
    std::vector<Stream> streams;
    std::vector<Entry> entries;
  };
};

class Archive
{
private:
  Options _options;
  
  box::Header _header;
  
  std::vector<ArchiveEntry> _entries;
  std::vector<ArchiveStream> _streams;
  
  std::queue<box::Section> _ordering;
  
  void finalizeHeader(W& w);
  box::checksum_t calculateGlobalChecksum(W& w, size_t bufferSize) const;
    
  void writeEntry(W& w, ArchiveStream& stream, ArchiveEntry& entry);
  void writeEntryPayloads(W& w);
  void writeStreamPayloads(W& w);
  
  const ArchiveEntry& entryForRef(ArchiveEntry::ref ref) const { return _entries[ref]; }
  ArchiveEntry& entryForRef(ArchiveEntry::ref ref) { return _entries[ref]; }
  
public:
  Archive();
  
  void write(W& w);
  void read(R& r);
  
  const box::Header& header() const { return _header; }

  Options& options() { return _options; }
  const Options& options() const { return _options; }

  
  bool isValidMagicNumber() const;
  bool isValidGlobalChecksum(W& w) const;
  bool checkEntriesMappingToStreams() const;
  
  const decltype(_entries)& entries() const { return _entries; }
  const decltype(_streams)& streams() const { return _streams; }
  
  static Archive ofSingleEntry(const std::string& name, seekable_data_source* source, const std::initializer_list<filter_builder*>& builders);
  static Archive ofOneEntryPerStream(const std::vector<std::tuple<std::string, seekable_data_source*>>& entries, std::initializer_list<filter_builder*> builders);
  static Archive ofData(const ArchiveFactory::Data& data);

};

