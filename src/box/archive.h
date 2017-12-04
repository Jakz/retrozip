#pragma once

#include "base/common.h"

#include "core/data_source.h"

#include "filter_queue.h"
#include "header.h"

#include <list>

class memory_buffer;
using W = memory_buffer;
using R = seekable_data_source;

template<typename ENV>
class FilteredEntry
{
private:
  filter_builder_queue _filters;
  mutable memory_buffer _payload;
  
public:
  FilteredEntry() { }
  FilteredEntry(const std::vector<filter_builder*>& filters) : _filters(filters) { }
  FilteredEntry(const std::vector<byte>& payload) : _payload(payload.size())
  {
    _payload.write(payload.data(), payload.size());
  }
  
  box::count_t payloadLength() const
  {
    return static_cast<box::count_t>(_filters.payloadLength());
  }
  
  const memory_buffer& payload() const
  {
    return _payload;
  }
  
  void serializePayload(const ENV& env) const
  {
    _payload = _filters.payload();
  }
  
  void unserializePayload(const ENV& env)
  {
    _filters.unserialize(env, _payload);
  }
  
  void addFilter(filter_builder* builder) { _filters.add(builder); }
  const filter_builder_queue& filters() const { return _filters; }
};

class ArchiveEntry : public FilteredEntry<archive_environment>
{
public:
  using ref = box::index_t;
  
private:
  mutable box::Entry _binary;
  
  /* TODO: manage data ownership */
  data_source* _source;
  std::string _name;

public:
  ArchiveEntry(const std::string& name, const box::Entry& binary, const std::vector<byte>& payload) : FilteredEntry<archive_environment>(payload),
    _name(name), _source(nullptr), _binary(binary)
  {

  }
  
  ArchiveEntry(const std::string& name, data_source* source, const std::vector<filter_builder*>& filters) : FilteredEntry<archive_environment>(filters), _source(source), _name(name) { }
  ArchiveEntry(const std::string& name, data_source* source) : _source(source), _name(name) { }
  
  void setName(const std::string& name) { this->_name = name; }
  const std::string& name() const { return _name; }
  
  const decltype(_source)& source() { return _source; }

  void mapToStream(box::index_t streamIndex, box::index_t indexInStream)
  {
    _binary.stream = streamIndex;
    _binary.indexInStream = indexInStream;
  }
  
  box::Entry& binary() const { return _binary; }
};

class ArchiveStream : public FilteredEntry<archive_environment>
{
public:
  using ref = box::index_t;
  
private:
  mutable box::Stream _binary;
  std::vector<ArchiveEntry::ref> _entries;

public:
  ArchiveStream(const std::vector<ArchiveEntry::ref>& indices, const std::vector<filter_builder*>& filters) : FilteredEntry<archive_environment>(filters), _entries(indices) { }
  ArchiveStream(ArchiveEntry::ref entry) { assignEntry(entry); }
  ArchiveStream() { }
  ArchiveStream(const box::Stream& binary, const std::vector<byte>& payload) : FilteredEntry<archive_environment>(payload), _binary(binary)
  {
  }
  
  void assignEntry(ArchiveEntry::ref entry) { _entries.push_back(entry); }
  void assignEntryAtIndex(size_t index, ArchiveEntry::ref entry) { _entries.resize(index+1, box::INVALID_INDEX); _entries[index] = entry; }
  
  const std::vector<ArchiveEntry::ref>& entries() const { return _entries; }
  
  box::Stream& binary() const { return _binary; }
};

class ArchiveGroup
{
private:
  std::vector<ArchiveEntry::ref> _entries;
  std::string _name;
  
public:
  ArchiveGroup(const std::string& name) : _name(name) { }
  ArchiveGroup(const std::string& name, const std::vector<ArchiveEntry::ref>& entries) : _name(name), _entries(entries) { }
  
  void addEntry(ArchiveEntry::ref index) { _entries.push_back(index); }
  
  using iterator = decltype(_entries)::const_iterator;
  iterator begin() const { return _entries.begin(); }
  iterator end() const { return _entries.end(); }
  
  const std::vector<ArchiveEntry::ref>& entries() const { return _entries; }
  box::count_t size() const { return static_cast<box::count_t>(_entries.size()); }

  const std::string& name() const { return _name; }
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
  
  bool isMultithreaded() const { return false; }
};

class Archive;

class ArchiveReadHandle : public data_source
{
private:
  R& r;
  archive_environment _env;
  const Archive& _archive;
  const ArchiveEntry& _entry;
  filter_cache _cache;
  data_source* _source;
  
public:
  ArchiveReadHandle(R& r, const Archive& archive, const ArchiveEntry& entry) : r(r), _archive(archive), _entry(entry), _source(nullptr) { }
  data_source* source(bool total);
  
  size_t read(byte* dest, size_t amount) { return _source->read(dest, amount); }
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

struct ArchiveSizeInfo
{
  size_t totalSize;
  size_t streamsPayload;
  size_t entriesPayload;
  size_t streamsData;
  
  size_t uncompressedEntriesData;
};

class Archive
{
private:
  Options _options;
  
  box::Header _header;
  
  archive_environment env;
  
  std::vector<ArchiveEntry> _entries;
  std::vector<ArchiveStream> _streams;
  std::vector<ArchiveGroup> _groups;
  
  std::unordered_map<box::Section, box::SectionHeader, enum_hash> _headers;
  
  std::list<box::Section> _ordering;
  
  void finalizeHeader(W& w);
  box::checksum_t calculateGlobalChecksum(W& w, size_t bufferSize) const;
  
  bool willSectionBeSerialized(box::Section section) const;
  
  void writeStream(W& w, ArchiveStream& stream);
  void writeEntryPayloads(W& w);
  void writeStreamPayloads(W& w);
  
  void readSection(R& r, const box::SectionHeader& header);
  
  const ArchiveEntry& entryForRef(ArchiveEntry::ref ref) const { return _entries[ref]; }
  ArchiveEntry& entryForRef(ArchiveEntry::ref ref) { return _entries[ref]; }
  
public:
  Archive();
  
  void write(W& w);
  void read(R& r);
  
  const box::Header& header() const { return _header; }
  const decltype(_headers)& sections() const { return _headers; }
  const box::SectionHeader* section(box::Section section) const
  {
    auto it = _headers.find(section);
    return it != _headers.end() ? &(it->second) : nullptr;
  }

  Options& options() { return _options; }
  const Options& options() const { return _options; }

  ArchiveSizeInfo sizeInfo() const;
  
  bool isValidMagicNumber() const;
  bool isValidGlobalChecksum(W& w) const;
  bool checkEntriesMappingToStreams() const;
  
  const decltype(_entries)& entries() const { return _entries; }
  const decltype(_streams)& streams() const { return _streams; }
  
  static Archive ofSingleEntry(const std::string& name, seekable_data_source* source, const std::initializer_list<filter_builder*>& builders);
  static Archive ofOneEntryPerStream(const std::vector<std::tuple<std::string, seekable_data_source*>>& entries, std::initializer_list<filter_builder*> builders);
  static Archive ofData(const ArchiveFactory::Data& data);
};

