#include "archive.h"

#include "core/memory_buffer.h"
#include "core/data_pipe.h"

template<typename T> using ref = data_reference<T>;
template<typename T> using aref = array_reference<T>;

struct refs
{
  ref<box::Header> header;
  aref<box::Entry> entryTable;
  aref<box::Stream> streamTable;
};

Archive::Archive()
{
  _ordering.push(box::Section::HEADER);
  _ordering.push(box::Section::ENTRY_TABLE);
  _ordering.push(box::Section::ENTRY_PAYLOAD);
  _ordering.push(box::Section::STREAM_TABLE);
  _ordering.push(box::Section::STREAM_PAYLOAD);
  _ordering.push(box::Section::STREAM_DATA);
  _ordering.push(box::Section::FILE_NAME_TABLE);
}

bool Archive::isValidMagicNumber() const { return _header.magic == std::array<u8, 4>({ 'b', 'o', 'x', '!' }); }

bool Archive::isValidGlobalChecksum(W& w) const
{
  return !_header.hasFlag(box::HeaderFlag::INTEGRITY_CHECKSUM_ENABLED) || _header.fileChecksum == calculateGlobalChecksum(w, _options.checksum.digesterBuffer);
}

bool Archive::checkEntriesMappingToStreams() const
{
  struct index_pair
  {
    box::index_t stream;
    box::index_t indexInStream;
    size_t entryIndex;
    
    bool operator==(const index_pair& other) const { return stream == other.stream && indexInStream == other.indexInStream; }
  };
  
  struct mapping_hash
  {
    size_t operator()(const index_pair& pair) const
    {
      static_assert(sizeof(size_t) == sizeof(box::index_t)*2, "");
      return ((size_t)pair.stream << 32ULL) | pair.indexInStream;
    }
  };
  
  std::unordered_set<index_pair, mapping_hash> mapping;
  
  size_t index = 0;
  for (const auto& entry : _entries)
  {
    const auto& binary = entry.binary();
    const box::index_t stream = binary.stream;
    const box::index_t indexInStream = binary.indexInStream;
    
    /* check that stream index and index in stream are set */
    if (indexInStream == box::INVALID_INDEX)
      throw exceptions::unserialization_exception(fmt::sprintf("indexInStream not set for entry %lu", index));
    else if (stream == box::INVALID_INDEX)
      throw exceptions::unserialization_exception(fmt::sprintf("stream index not set for entry %lu", index));
    
    /* check that no other entry is mapped in the same position */
    auto existing = mapping.find({ stream, indexInStream });
    
    if (existing != mapping.end())
      throw exceptions::unserialization_exception(fmt::sprintf("entry %lu and %lu are both mapped to stream %lu:%lu ", index, existing->entryIndex, stream, indexInStream));
    
    /* check that mapping is consistent */
    if (stream >= _streams.size())
      throw exceptions::unserialization_exception(fmt::sprintf("stream index out of bounds for entry %lu", index));
    else if (indexInStream >= _streams[binary.stream].entries().size())
      throw exceptions::unserialization_exception(fmt::sprintf("index in stream out of bounds (%lu:%lu) for entry %lu", stream, indexInStream, index));
    
    mapping.insert({ stream, indexInStream, index });

    ++index;
  }
  
  /* check that all ref to entries are correct */
  index = 0;
  for (const auto& stream : _streams)
  {
    for (const auto entry : stream.entries())
    {
      if (entry >= _entries.size())
        throw exceptions::unserialization_exception(fmt::sprintf("entry %lu out of bounds for stream %lu", entry, index));
    }
    
    ++index;
  }

  
  return true;
}

Archive Archive::ofSingleEntry(const std::string& name, seekable_data_source* source, std::initializer_list<filter_builder*> builders)
{
  Archive archive;
  
  
  archive._entries.emplace_back(name, source);
  archive._streams.push_back(ArchiveStream());
  
  for (auto* builder : builders) archive._entries.front().addFilter(builder);
  
  archive._streams.front().assignEntry(0UL);
  
  archive._entries.front().binary().stream = 0;
  archive._entries.front().binary().indexInStream = 0;
  
  return archive;
}

void Archive::write(W& w)
{
  assert(_ordering.front() == box::Section::HEADER);
  _ordering.pop();
  
  refs refs;
  
  refs.header = w.reserve<box::Header>();

  _header.entryCount = static_cast<box::count_t>(_entries.size());
  _header.streamCount = static_cast<box::count_t>(_streams.size());
  

  while (!_ordering.empty())
  {
    box::Section section = _ordering.front();
    _ordering.pop();
    
    switch (section)
    {
      case box::Section::HEADER: /* already managed */ break;
        
      case box::Section::ENTRY_TABLE:
      {
        /* save offset to the entry table and store it into header */
        refs.entryTable = w.reserveArray<box::Entry>(_header.entryCount);
        _header.entryTableOffset = refs.entryTable;
        break;
      }
        
      case box::Section::STREAM_TABLE:
      {
        /* save offset to the stream table and store it into header */
        refs.streamTable = w.reserveArray<box::Stream>(_header.streamCount);
        _header.streamTableOffset = refs.streamTable;
        break;
      }
        
      case box::Section::ENTRY_PAYLOAD:
      {
        off_t base = w.tell();
        off_t length = 0;
        
        /* for each entry we get the payload length to compute each payload 
           offset inside the file, we also compute the total entry payload 
           length to reserve it
         */
        for (const ArchiveEntry& entry : _entries)
        {
          box::Entry& tentry = entry.binary();
          tentry.payload = base + length;
          tentry.payloadLength = entry.payloadLength();
          
          length += tentry.payloadLength;
        }
        
        w.reserve(length);
        break;
      }
        
      case box::Section::STREAM_PAYLOAD:
      {
        off_t base = w.tell();
        off_t length = 0;
        
        /* for each entry we get the payload length to compute each payload
         offset inside the file, we also compute the total entry payload
         length to reserve it
         */
        for (const ArchiveStream& stream : _streams)
        {
          box::Stream& sentry = stream.binary();
          sentry.payload = base + length;
          sentry.payloadLength = stream.payload().size();
          
          length += sentry.payloadLength;
        }
        
        w.reserve(length);
        break;
      }
        
      case box::Section::FILE_NAME_TABLE:
      {
        off_t base = w.tell();
        off_t offset = w.tell();
        
        _header.nameTableOffset = base;
        
        /* write NUL terminated name */
        for (const ArchiveEntry& entry : _entries)
        {
          entry.binary().entryNameOffset = offset;
          w.write(entry.name().c_str(), 1, entry.name().length());
          w.write((char)'\0');
          
          offset = w.tell();
        }
        
        _header.nameTableLength = static_cast<box::count_t>(offset - base);
        break;
      }
        
      case box::Section::STREAM_DATA:
      {
        /* main stream writing */

        box::index_t streamIndex = 0, indexInStream = 0;
        for (ArchiveStream& stream : _streams)
        {
          for (ArchiveEntry::ref ref : stream.entries())
          {
            ArchiveEntry& entry = entryForRef(ref);
            entry.mapToStream(streamIndex, indexInStream);
            writeEntry(w, stream, entryForRef(ref));
                              
            ++indexInStream;
          }
          ++streamIndex;
        }
        
        break;
      }
    }
  }
  
  writeEntryPayloads(w);
  writeStreamPayloads(w);
  
  /* when we arrive here we suppose all streams have been written and all data
     in Stream and Entry has been prepared and filled */
  
  
  /* fill the array of file entries */
  for (size_t i = 0; i < _entries.size(); ++i)
    refs.entryTable.write(_entries[i].binary(), i);
  
  /* fill the array of stream entries */
  for (size_t i = 0; i < _streams.size(); ++i)
    refs.streamTable.write(_streams[i].binary(), i);
  
  /* this should be the last thing we do since it optionally computes hash for the whole file */
  finalizeHeader(w);
  refs.header.write(_header);
}

void Archive::read(R& r)
{
  r.seek(0);
  r.read(_header);
  
  //TODO: check validity
  
  /* read entries */
  r.seek(_header.entryTableOffset);
  for (size_t i = 0; i < _header.entryCount; ++i)
  {
    /* read entry */
    box::Entry entry;
    r.read(entry);
    
    /* read entry name */
    r.seek(entry.entryNameOffset);
    //TODO: ugly
    std::string name;
    char c;
    r.read(&c, sizeof(char), 1);
    while (c) {
      name += c;
      r.read(&c, sizeof(char), 1);
    }
    
    _entries.emplace_back(name, entry);
  }
  
}

void Archive::finalizeHeader(W& w)
{
  
  w.seek(0, Seek::END);
  _header.fileLength = w.tell();
  
  _header.version = box::CURRENT_VERSION;
  

  /* this must be done last */
  if (_options.checksum.calculateGlobalChecksum)
  {
    _header.flags.set(box::HeaderFlag::INTEGRITY_CHECKSUM_ENABLED);
    _header.fileChecksum = calculateGlobalChecksum(w, _options.checksum.digesterBuffer);
  }
}

box::checksum_t Archive::calculateGlobalChecksum(W& w, size_t bufferSize) const
{
  /* we need to calculate checksum of file but we need to skip the checksum itself */
  offset_t checksumOffset = offsetof(box::Header, fileChecksum);

  box::digester_t digester;
  w.seek(0);
  byte* buffer = new byte[bufferSize];
  
  digester.update(&_header, checksumOffset);
  digester.update(&_header + sizeof(box::checksum_t), sizeof(box::Header) - checksumOffset - sizeof(box::checksum_t));
  w.seek(sizeof(box::Header), Seek::SET);
  
  size_t read = 0;
  while ((read = w.read(buffer, 1, bufferSize)) > 0)
    digester.update(buffer, read);
  
  delete [] buffer;
  
  return digester.get();
}

void Archive::writeEntry(W& w, ArchiveStream& stream, ArchiveEntry& entry)
{
  data_source* source = entry.source();
  
  /* first we wrap with a counter filter to calculate the original input size */
  unbuffered_source_filter<filters::data_counter> inputCounter(source);
  /* then we apply digest calculator filter */
  unbuffered_source_filter<filters::multiple_digest_filter> digester(&inputCounter, _options.digest.crc32, _options.digest.md5, _options.digest.sha1);
  
  /* then we apply all filters from entry */
  filter_cache entryCache = entry.filters().apply(&digester);
  source = entryCache.get();
  
  /* size of input transformed by entry filters before being sent to stream */
  unbuffered_source_filter<filters::data_counter> filteredInputCounter(source);

  
  /* then we apply all filters from stream */
  filter_cache streamCache = entry.filters().apply(&filteredInputCounter);
  source = entryCache.get();

  /* effectve written input */
  unbuffered_source_filter<filters::data_counter> outputCounter(source);
  
  source = &outputCounter;
  
  assert(_options.bufferSize > 0);
  passthrough_pipe pipe(source, &w, _options.bufferSize);
  pipe.process();
  
  entry.binary().originalSize = inputCounter.filter().count();
  entry.binary().entrySize = filteredInputCounter.filter().count();
  entry.binary().compressedSize = outputCounter.filter().count();
  
  if (_options.digest.crc32)
    entry.binary().digest.crc32 = digester.filter().crc32();

  if (_options.digest.md5)
    entry.binary().digest.md5 = digester.filter().md5();

  if (_options.digest.sha1)
    entry.binary().digest.sha1 = digester.filter().sha1();
  
  //entry.binary().indexInStream
  //entry.binary().stream
}

/* precondition: payload offset has been set for entries */
void Archive::writeEntryPayloads(W& w)
{
  for (const ArchiveEntry& entry : _entries)
  {
    const memory_buffer& payload = entry.payload();
    
    w.seek(entry.binary().payload);
    w.write(payload.raw(), 1, payload.size());
  }
}

/* precondition: payload offset has been set for entries */
void Archive::writeStreamPayloads(W& w)
{
  for (const ArchiveStream& stream : _streams)
  {
    const memory_buffer& payload = stream.payload();
    
    w.seek(stream.binary().payload);
    w.write(payload.raw(), 1, payload.size());
  }
}

void Archive::writeStream(W& w, ArchiveStream& stream)
{
  
}
