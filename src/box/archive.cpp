#include "archive.h"

#include "core/memory_buffer.h"
#include "core/data_pipe.h"

using uexc = exceptions::unserialization_exception;

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
      throw uexc(fmt::sprintf("indexInStream not set for entry %lu", index));
    else if (stream == box::INVALID_INDEX)
      throw uexc(fmt::sprintf("stream index not set for entry %lu", index));
    
    /* check that no other entry is mapped in the same position */
    auto existing = mapping.find({ stream, indexInStream });
    
    if (existing != mapping.end())
      throw uexc(fmt::sprintf("entry %lu and %lu are both mapped to stream %lu:%lu ", index, existing->entryIndex, stream, indexInStream));
    
    /* check that mapping is consistent */
    if (stream >= _streams.size())
      throw uexc(fmt::sprintf("stream index out of bounds for entry %lu", index));
    else if (indexInStream >= _streams[binary.stream].entries().size())
      throw uexc(fmt::sprintf("index in stream out of bounds (%lu:%lu) for entry %lu", stream, indexInStream, index));
    
    mapping.insert({ stream, indexInStream, index });

    ++index;
  }
  
  /* check that all ref to entries are correct */
  index = 0;
  for (const auto& stream : _streams)
  {
    for (const auto entry : stream.entries())
    {
      if (entry == box::INVALID_INDEX)
        throw uexc(fmt::sprintf("entry not set stream %lu", index));
      if (entry >= _entries.size())
        throw uexc(fmt::sprintf("entry %lu out of bounds for stream %lu", entry, index));
    }
    
    ++index;
  }

  return true;
}

Archive Archive::ofSingleEntry(const std::string& name, seekable_data_source* source, const std::initializer_list<filter_builder*>& builders)
{
  return Archive::ofOneEntryPerStream({ { name, source } }, builders);
  
  Archive archive;
  
  archive._entries.emplace_back(name, source);
  archive._streams.emplace_back(0UL);
  
  for (auto* builder : builders) archive._entries.front().addFilter(builder);
  
  archive._entries.front().binary().stream = 0;
  archive._entries.front().binary().indexInStream = 0;
  
  return archive;
}

Archive Archive::ofOneEntryPerStream(const std::vector<std::tuple<std::string, seekable_data_source*>>& entries, std::initializer_list<filter_builder*> builders)
{
  Archive archive;
  
  box::index_t index = 0UL;
  for (const auto& entry : entries)
  {
    archive._entries.emplace_back(std::get<0>(entry), std::get<1>(entry));
    archive._streams.emplace_back(index);
    
    for (auto* builder : builders) archive._entries.back().addFilter(builder);

    auto& binary = archive._entries.back().binary();
    binary.indexInStream = 0;
    binary.stream = index;
    
    ++index;
  }
  
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
  
  TRACE_A("%p: archive::write() writing %lu entries in %lu streams", this, _header.entryCount, _header.streamCount);

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
        TRACE_A("%p: archive::write() reserved entry table for %lu entries (%lu bytes) at %Xh (%lu)", this, _header.entryCount, _header.entryCount*sizeof(box::Entry), _header.entryTableOffset,  _header.entryTableOffset);
        break;
      }
        
      case box::Section::STREAM_TABLE:
      {
        /* save offset to the stream table and store it into header */
        refs.streamTable = w.reserveArray<box::Stream>(_header.streamCount);
        _header.streamTableOffset = refs.streamTable;
        TRACE_A("%p: archive::write() reserved stream table for %lu streams (%lu bytes) at %Xh (%lu)", this, _header.streamCount, _header.streamCount*sizeof(box::Stream), _header.streamTableOffset,  _header.streamTableOffset);
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
          box::count_t payloadLength = entry.payloadLength();
          box::Entry& tentry = entry.binary();
          tentry.payload = payloadLength > 0 ? (base + length) : 0;
          tentry.payloadLength = payloadLength;
          
          length += tentry.payloadLength;
        }
        
        TRACE_A("%p: archive::write() reserved entries payload of %lu bytes at %Xh (%lu)", this, length, w.tell(), w.tell());
        
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
          box::count_t payloadLength = stream.payloadLength();
          box::Stream& sentry = stream.binary();
          sentry.payload = payloadLength > 0 ? (base + length) : 0;
          sentry.payloadLength = payloadLength;
          
          length += sentry.payloadLength;
        }
        
        TRACE_A("%p: archive::write() reserved stream payload of %lu bytes at %Xh (%lu)", this, length, w.tell(), w.tell());
        
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
          TRACE_A2("%p: archive::write() writing entry name '%s' at %Xh (%lu)", this, entry.name().c_str(), offset, offset);
          
          entry.binary().entryNameOffset = offset;
          w.write(entry.name().c_str(), 1, entry.name().length());
          w.write((char)'\0');
          
          offset = w.tell();
        }
        
        _header.nameTableLength = static_cast<box::count_t>(offset - base);
        
        TRACE_A("%p: archive::write() written name table of %lu bytes at %Xh (%lu)", this, _header.nameTableLength, _header.nameTableOffset, _header.nameTableOffset);
        break;
      }
        
      case box::Section::STREAM_DATA:
      {
        /* main stream writing */
        box::index_t streamIndex = 0, indexInStream = 0;
        for (ArchiveStream& stream : _streams)
        {
          indexInStream = 0;
          
          stream.binary().offset = w.tell();
          stream.binary().length = 0;
          
          
          for (ArchiveEntry::ref ref : stream.entries())
          {
            TRACE_A("%p: archive::write() writing entry %lu (stream %lu:%lu) at %Xh (%lu)", this, ref, streamIndex, indexInStream, stream.binary().offset, stream.binary().offset);
            
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
  
  if (!isValidMagicNumber())
    throw uexc("invalid magic number, expecting 'box!'");
  //TODO: check validity checksum etc
  
  /* read entries */
  for (size_t i = 0; i < _header.entryCount; ++i)
  {
    r.seek(_header.entryTableOffset + i*sizeof(box::Entry));

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
    
    /* load payload */
    std::vector<byte> payload;
    if (entry.payloadLength > 0)
    {
      r.seek(entry.payload);
      r.read(payload.data(), 1, entry.payloadLength);
    }
    
    _entries.emplace_back(name, entry, payload);
  }
  
  /* read stream headers */
  for (size_t i = 0; i < _header.streamCount; ++i)
  {
    r.seek(_header.streamTableOffset + i*sizeof(box::Stream));

    /* read stream header */
    box::Stream stream;
    r.read(stream);
    
    _streams.emplace_back(stream);
  }
  
  /* for each entry map it to the correct stream at correct index */
  ArchiveEntry::ref index = 0;
  for (const auto& entry : _entries)
  {
    box::index_t stream = entry.binary().stream;
    box::index_t indexInStream = entry.binary().indexInStream;
    
    if (stream != box::INVALID_INDEX && indexInStream != box::INVALID_INDEX && stream < _streams.size())
      _streams[stream].assignEntryAtIndex(indexInStream, index);
    
    ++index;
  }
  
  /* verify integrity of the whole mapping */
  checkEntriesMappingToStreams();
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
  filter_cache streamCache = stream.filters().apply(&filteredInputCounter);
  source = streamCache.get();

  /* effective written input */
  unbuffered_source_filter<filters::data_counter> outputCounter(source);
  
  source = &outputCounter;
  
  assert(_options.bufferSize > 0);
  passthrough_pipe pipe(source, &w, _options.bufferSize);
  pipe.process();
  
  entry.binary().originalSize = inputCounter.filter().count();
  entry.binary().filteredSize = filteredInputCounter.filter().count();
  entry.binary().compressedSize = outputCounter.filter().count();
  
  stream.binary().length += entry.binary().compressedSize;
  
  if (_options.digest.crc32)
    entry.binary().digest.crc32 = digester.filter().crc32();

  if (_options.digest.md5)
    entry.binary().digest.md5 = digester.filter().md5();

  if (_options.digest.sha1)
    entry.binary().digest.sha1 = digester.filter().sha1();
  
  TRACE_A("%p: archive::write() written %lu bytes, filtered into %lu and compressed into %lu bytes", this, entry.binary().originalSize, entry.binary().filteredSize, entry.binary().compressedSize);
  
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

data_source* ArchiveReadHandle::source(bool total)
{
  _cache.clear();
  
  /* first we need to know if stream is seekable, if it is we can seek to correct entry
     offset before reading from it, otherwise we need to skip */
  const ArchiveStream& stream = _archive.streams()[_entry.binary().stream];
  
  bool isSeekable = stream.binary().flags && box::StreamFlag::SEEKABLE;
  
  seekable_data_source* base = &r;
  
  data_source* source = base;
  size_t offset = stream.binary().offset;
  
  if (isSeekable)
  {
    /* if stream is seekable we can find the offset to start reading from by adding all previous
       entries in the stream */
    for (box::index_t i = 0; i < _entry.binary().indexInStream; ++i)
      offset += _archive.entries()[stream.entries()[i]].binary().compressedSize;
    
    source_filter<filters::skip_filter>* skipper = new source_filter<filters::skip_filter>(source, _archive.options().bufferSize, 0, _entry.binary().compressedSize, 0);
    _cache.cache(skipper);
    source = skipper;
  }
  
  /* move to the start of the stream */
  base->seek(offset);
  
  /* this doesn't unapply entry filters, just stream filters */
  _cache.setSource(source);
  stream.filters().unapply(_cache);
  
  if (total)
    _entry.filters().unapply(_cache);

  source = _cache.get();
  
  /* if stream is not seekable then we need to skip up to uncompressed size of all previous entries */
  if (!isSeekable)
  {
    size_t skipAmount = 0;
    size_t amount = total ? _entry.binary().originalSize : _entry.binary().filteredSize;
    
    for (box::index_t i = 0; i < _entry.binary().indexInStream; ++i)
    {
      const auto& b = _archive.entries()[stream.entries()[i]].binary();
      /* the amount to skip depends if we're extracting from stream filters or from both stream and entry */
      skipAmount += total ? b.originalSize : b.filteredSize;
    }

    source_filter<filters::skip_filter>* skipper = new source_filter<filters::skip_filter>(source, _archive.options().bufferSize, skipAmount, amount, 0);
    _cache.cache(skipper);
    source = skipper;
  }

  return source;
}




