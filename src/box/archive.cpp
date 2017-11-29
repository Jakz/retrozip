#include "archive.h"

#include "core/memory_buffer.h"
#include "core/data_pipe.h"

using uexc = exceptions::unserialization_exception;

template<typename T> using ref = data_reference<T>;
template<typename T> using aref = array_reference<T>;

struct refs
{
  ref<box::Header> header;
  aref<box::SectionHeader> sectionTable;
  
  aref<box::Entry> entryTable;
  aref<box::Stream> streamTable;
  aref<box::Group> groupTable;
};

Archive::Archive()
{
  _ordering.push_back(box::Section::HEADER);
  _ordering.push_back(box::Section::SECTION_TABLE);
  _ordering.push_back(box::Section::ENTRY_TABLE);
  _ordering.push_back(box::Section::ENTRY_PAYLOAD);
  _ordering.push_back(box::Section::STREAM_TABLE);
  _ordering.push_back(box::Section::STREAM_PAYLOAD);
  _ordering.push_back(box::Section::STREAM_DATA);
  _ordering.push_back(box::Section::FILE_NAME_TABLE);
  _ordering.push_back(box::Section::GROUP_TABLE);
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
  
  /* check that all groups have valid indices */
  for (const auto& group : _groups)
  {
    std::unordered_set<ArchiveEntry::ref> uniques(group.entries().begin(), group.entries().end());
    
    if (uniques.size() != group.size())
      throw uexc(fmt::sprintf("group %s has non unique entries", group.name().c_str()));
    
    if (std::any_of(group.begin(), group.end(), [this](ArchiveEntry::ref index) { return index >= _entries.size() || index < 0; }))
      throw uexc(fmt::sprintf("group '%s' has invalid indices", group.name()));
  }

  return true;
}

Archive Archive::ofSingleEntry(const std::string& name, seekable_data_source* source, const std::initializer_list<filter_builder*>& builders)
{
  ArchiveFactory::Data data;
  data.entries.push_back({ name, source, builders});
  return Archive::ofData(data);
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

Archive Archive::ofData(const ArchiveFactory::Data& data)
{
  Archive archive;
  
  //TODO: check validity (eg multiple ArchiveEntry::ref)b
  
  archive._entries.reserve(data.entries.size());
  
  for (const auto& entry : data.entries)
    archive._entries.emplace_back(entry.name, entry.source, entry.filters);
  
  for (const auto& stream : data.streams)
    archive._streams.emplace_back(stream.entries, stream.filters);
  
  box::index_t streamIndex = 0, indexInStream = 0;
  for (const auto& stream : archive._streams)
  {
    indexInStream = 0;
    
    for (const auto index : stream.entries())
    {
      archive._entries[index].binary().stream = streamIndex;
      archive._entries[index].binary().indexInStream = indexInStream;
      ++indexInStream;
    }
    
    ++streamIndex;
  }
  
  archive.options().bufferSize = KB16;
  
  return archive;
}

bool Archive::willSectionBeSerialized(box::Section section) const
{
  switch (section)
  {
    case box::Section::HEADER: assert(false); return false;
    case box::Section::SECTION_TABLE: assert(false); return false;
    case box::Section::ENTRY_TABLE: return !_entries.empty();
    case box::Section::ENTRY_PAYLOAD: return std::any_of(_entries.begin(), _entries.end(), [] (const ArchiveEntry& entry) { return entry.payloadLength() > 0; });
    
    case box::Section::STREAM_TABLE: return !_streams.empty();
    case box::Section::STREAM_PAYLOAD: return std::any_of(_streams.begin(), _streams.end(), [] (const ArchiveStream& stream) { return stream.payloadLength() > 0; });
      
    case box::Section::FILE_NAME_TABLE: return !_entries.empty();
    case box::Section::STREAM_DATA: return !_streams.empty();
      
    case box::Section::GROUP_TABLE: return !_groups.empty();
  }
}

void Archive::write(W& w)
{
  assert(_ordering.front() == box::Section::HEADER);
  _ordering.pop_front();
  
  _headers.clear();

  refs refs;
  refs.header = w.reserve<box::Header>();

  TRACE_A("%p: archive::write() writing %lu entries in %lu streams", this, _entries.size(), _streams.size());

  while (!_ordering.empty())
  {
    box::Section section = _ordering.front();
    _ordering.pop_front();
    
    box::SectionHeader sectionHeader { 0, 0, section, 0 };

    switch (section)
    {
      case box::Section::HEADER:
        /* already managed */
        
        /* section table must be first section after header */
        assert(_ordering.front() == box::Section::SECTION_TABLE);
      break;
        
      case box::Section::SECTION_TABLE:
      {
        size_t effectiveSections = std::count_if(_ordering.begin(), _ordering.end(), [this] (box::Section section) { return willSectionBeSerialized(section); });
        
        refs.sectionTable = w.reserveArray<box::SectionHeader>(effectiveSections);

        _header.index.offset = refs.sectionTable;
        _header.index.count = static_cast<box::count_t>(effectiveSections);
        _header.index.size = sizeof(box::SectionHeader)* _header.index.count;
        _header.index.type = box::Section::SECTION_TABLE;
        
        TRACE_A("%p: archive::write() reserved section table for %lu entries (%lu bytes) at %Xh (%lu)", this, _header.index.count, _header.index.size, _header.index.offset, _header.index.offset);
        break;
      }
        
      case box::Section::ENTRY_TABLE:
      {
        /* save offset to the entry table and store it into header */
        refs.entryTable = w.reserveArray<box::Entry>(_entries.size());
        
        sectionHeader.offset = refs.entryTable;
        sectionHeader.count = static_cast<box::count_t>(_entries.size());
        sectionHeader.size = static_cast<box::length_t>(sizeof(box::Entry) * sectionHeader.count);
        
        TRACE_A("%p: archive::write() reserved entry table for %lu entries (%lu bytes) at %Xh (%lu)", this, sectionHeader.count, sectionHeader.size, sectionHeader.offset, sectionHeader.offset);
        break;
      }
        
      case box::Section::STREAM_TABLE:
      {
        /* save offset to the stream table and store it into header */
        refs.streamTable = w.reserveArray<box::Stream>(_streams.size());
        
        sectionHeader.offset = refs.streamTable;
        sectionHeader.count = static_cast<box::count_t>(_streams.size());
        sectionHeader.size = static_cast<box::length_t>(sizeof(box::Stream) * sectionHeader.count);
        
        TRACE_A("%p: archive::write() reserved stream table for %lu streams (%lu bytes) at %Xh (%lu)", this, sectionHeader.count, sectionHeader.size, sectionHeader.offset, sectionHeader.offset);
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
        
        if (length > 0)
          TRACE_A("%p: archive::write() reserved entries payload of %lu bytes at %Xh (%lu)", this, length, w.tell(), w.tell());
        
        sectionHeader.offset = w.tell();
        sectionHeader.size = length;
        
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
        
        if (length > 0)
          TRACE_A("%p: archive::write() reserved stream payload of %lu bytes at %Xh (%lu)", this, length, w.tell(), w.tell());
        
        sectionHeader.offset = w.tell();
        sectionHeader.size = length;
        
        w.reserve(length);
        break;
      }
        
      case box::Section::FILE_NAME_TABLE:
      {
        off_t base = w.tell();
        off_t offset = w.tell();
        
        sectionHeader.offset = w.tell();
        
        /* write NUL terminated name */
        for (const ArchiveEntry& entry : _entries)
        {
          TRACE_A2("%p: archive::write() writing entry name '%s' at %Xh (%lu)", this, entry.name().c_str(), offset, offset);
          
          entry.binary().entryNameOffset = offset;
          w.write(entry.name().c_str(), 1, entry.name().length());
          w.write((char)'\0');
          
          offset = w.tell();
        }
        
        sectionHeader.size = static_cast<box::count_t>(offset - base);
        
        TRACE_A("%p: archive::write() written name table of %lu bytes at %Xh (%lu)", this, sectionHeader.size, sectionHeader.offset, sectionHeader.offset);
        break;
      }
        
      case box::Section::GROUP_TABLE:
      {
        off_t base = w.tell();
        off_t offset = w.tell();
        
        sectionHeader.offset = base;
        sectionHeader.count = static_cast<box::count_t>(_groups.size());
        
        for (const auto& group : _groups)
        {
          /* write group size, then indices, then name */
          w.write(static_cast<box::count_t>(group.size()));
          w.write(group.entries().data(), sizeof(ArchiveEntry::ref), group.size());
          w.write(group.name().c_str(), 1, group.name().length());
          w.write((char)'\0');
        }
        
        sectionHeader.size = static_cast<box::count_t>(offset - base);
        
        if (sectionHeader.size > 0)
          TRACE_A("%p: archive::write() written group table of %lu bytes at %Xh (%lu)", this, sectionHeader.count, sectionHeader.offset, sectionHeader.offset);
        break;
      }
        
      case box::Section::STREAM_DATA:
      {
        sectionHeader.offset = w.tell();
        sectionHeader.count = 1;
        
        /* main stream writing */
        box::index_t streamIndex = 0, indexInStream = 0;
        for (ArchiveStream& stream : _streams)
        {
          indexInStream = 0;
          
          stream.binary().offset = w.tell();
          stream.binary().length = 0;
          
          writeStream(w, stream);
          
          /*for (ArchiveEntry::ref ref : stream.entries())
          {
            TRACE_A("%p: archive::write() writing entry %lu (stream %lu:%lu) at %Xh (%lu)", this, ref, streamIndex, indexInStream, stream.binary().offset, stream.binary().offset);
            
            ArchiveEntry& entry = entryForRef(ref);
            entry.mapToStream(streamIndex, indexInStream);
            writeEntry(w, stream, entryForRef(ref));
                              
            ++indexInStream;
          }
          ++streamIndex;*/
        }
        
        sectionHeader.size = w.tell() - sectionHeader.offset;
        
        break;
      }
    }
    
    if (section != box::Section::HEADER && section != box::Section::SECTION_TABLE && sectionHeader.size > 0)
      _headers.emplace(std::make_pair(section, sectionHeader));
  }
  
  writeEntryPayloads(w);
  writeStreamPayloads(w);
  
  /* when we arrive here we suppose all streams have been written and all data
     in Stream and Entry has been prepared and filled */
  
  /* fill section headers */
  assert(_headers.size() == refs.sectionTable.count());
  size_t i = 0;
  for (const auto& section : _headers)
    refs.sectionTable.write(section.second, i++);
  
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

void Archive::readSection(R& r, const box::SectionHeader& header)
{
  const box::Section section = header.type;
  using S = box::Section;
  
  switch (section)
  {
    case S::HEADER: /* should never happen */ assert(false); break;
    case S::SECTION_TABLE: /* should never happen */ assert(false); break;
    
    case S::ENTRY_TABLE:
    {
      /* read entries */
      for (size_t i = 0; i < header.count; ++i)
      {
        r.seek(header.offset + i*sizeof(box::Entry));
        
        /* read entry */
        box::Entry entry;
        r.read(entry);
        
        /* read entry name */
        r.seek(entry.entryNameOffset);
        //TODO: ugly
        std::string name;
        char c;
        r.read(c);
        while (c) {
          name += c;
          r.read(c);
        }
        
        /* load payload */
        std::vector<byte> payload;
        if (entry.payloadLength > 0)
        {
          payload.resize(entry.payloadLength);
          r.seek(entry.payload);
          r.read(payload.data(), entry.payloadLength);
        }
        
        _entries.emplace_back(name, entry, payload);
      }

      break;
    }
      
    case S::STREAM_TABLE:
    {
      for (size_t i = 0; i < header.count; ++i)
      {
        r.seek(header.offset + i*sizeof(box::Stream));
        
        /* read stream header */
        box::Stream stream;
        r.read(stream);
        
        /* load payload */
        std::vector<byte> payload;
        if (stream.payloadLength > 0)
        {
          payload.resize(stream.payloadLength);
          r.seek(stream.payload);
          r.read(payload.data(), stream.payloadLength);
        }
        
        _streams.emplace_back(stream, payload);
      }
      
      break;
    }
      
    case S::GROUP_TABLE:
    {
      r.seek(header.offset);
      for (size_t i = 0; i < header.count; ++i)
      {
        box::count_t size;
        r.read(size);
        
        std::vector<ArchiveEntry::ref> indices(size);
        r.read((byte*)indices.data(), sizeof(ArchiveEntry::ref) * size);
        
        //TODO: ugly
        std::string name;
        char c;
        r.read(c);
        while (c) {
          name += c;
          r.read(c);
        }
        
        _groups.emplace_back(name, indices);
      }
      
      break;
    }
      
    case S::ENTRY_PAYLOAD:
    case S::STREAM_PAYLOAD:
    case S::FILE_NAME_TABLE:
      /* do nothing, these are managed when reading respective parents */
      break;
  }
}

void Archive::read(R& r)
{
  /* clear everything */
  _headers.clear();
  _entries.clear();
  _streams.clear();
  _groups.clear();
  
  /* read header */
  r.seek(0);
  r.read(_header);
  
  /* read sections */
  r.seek(_header.index.offset);
  for (size_t i = 0; i < _header.index.count; ++i)
  {
    box::SectionHeader header;
    r.read(header);
    _headers.emplace(std::make_pair(header.type, header));
  }
  
  if (!isValidMagicNumber())
    throw uexc("invalid magic number, expecting 'box!'");
  //TODO: check validity checksum etc
  
  /* read each section if needed */
  for (const auto& section : _headers)
    readSection(r, section.second);
  
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

void Archive::writeStream(W& w, ArchiveStream& stream)
{
  using digester_t = unbuffered_source_filter<filters::multiple_digest_filter>;
  using counter_t = unbuffered_source_filter<filters::data_counter>;
  
  struct data_source_helper
  {
    ArchiveEntry& entry;
    data_source* source;
    digester_t* digester;
    counter_t* inputCounter;
    counter_t* filteredCounter;
    filter_cache cache;
  };
  
  std::vector<data_source_helper> sources;
  
  sources.reserve(stream.entries().size());
  
  archive_environment env = { this, filter_repository::instance() };
  
  for (ArchiveEntry::ref index : stream.entries())
  {
    ArchiveEntry& entry = _entries[index];
    data_source* source = entry.source();
    
    /* first we wrap with a counter filter to calculate the original input size */
    auto* inputCounter = new counter_t(source);
    /* then we apply digest calculator filter */
    auto* digester = new digester_t(inputCounter, _options.digest.crc32, _options.digest.md5, _options.digest.sha1);
    
    /* then we apply all filters from entry */
    entry.filters().setup(env);
    filter_cache cache = entry.filters().apply(digester);

    /* size of input transformed by entry filters before being sent to stream */
    auto* filteredCounter = new counter_t(cache.get());
    
    source = filteredCounter;

    /* add counters to cache to allow releasing them after we've done with the stream */
    cache.cache(inputCounter);
    cache.cache(digester);
    cache.cache(filteredCounter);
  
    /* we move because cache contains unique_ptr */
    sources.push_back({ entry, source, digester, inputCounter, filteredCounter, std::move(cache) });
  };
  
  std::vector<data_source*> sourcesOnly;
  
  sourcesOnly.reserve(sources.size());
  std::transform(sources.begin(), sources.end(), std::back_inserter(sourcesOnly), [] (const data_source_helper& helper) { return helper.source; });
  multiple_data_source source(sourcesOnly);
  
  
  /* then we apply all filters from stream */
  stream.filters().setup(env);
  filter_cache streamCache = stream.filters().apply(&source);
  
  counter_t compressedCounter(streamCache.get());
  counter_t wholeCounter(&compressedCounter);

  data_source* finalStream = &wholeCounter;

#if defined(DEBUG)
  source.setOnBegin([this, &sources](data_source* source) {
    auto it = std::find_if(sources.begin(), sources.end(), [source](const data_source_helper& helper) { return helper.source == source; });
    assert(it != sources.end());
    auto& entry = it->entry;
    TRACE_A("%p: archive::write() preparing to write entry %s", this, entry.name().c_str());
  });
#endif

  source.setOnEnd([this, &sources, &compressedCounter](data_source* source) {
    /* TODO: this is linear, we can use a std::unordered_map if really many entries are stored in single stream but it's quite irrelevant */
    auto it = std::find_if(sources.begin(), sources.end(), [source](const data_source_helper& helper) { return helper.source == source; });
    assert(it != sources.end());
    auto& entry = it->entry;
    entry.binary().compressedSize = compressedCounter.filter().count();
    compressedCounter.filter().reset();
    TRACE_A("%p: archive::write() written %lu bytes, filtered into %lu and compressed into %lu bytes", this, it->inputCounter->filter().count(), it->filteredCounter->filter().count(), entry.binary().compressedSize);
  });
    
  assert(_options.bufferSize > 0);
  passthrough_pipe pipe(finalStream, &w, _options.bufferSize);
  pipe.process();
  
  for (const data_source_helper& helper : sources)
  {
    auto& entry = helper.entry;
    
    entry.binary().digest.size = helper.inputCounter->filter().count();
    entry.binary().filteredSize = helper.filteredCounter->filter().count();
    
    if (_options.digest.crc32)
      entry.binary().digest.crc32 = helper.digester->filter().crc32();
    
    if (_options.digest.md5)
      entry.binary().digest.md5 = helper.digester->filter().md5();
    
    if (_options.digest.sha1)
      entry.binary().digest.sha1 = helper.digester->filter().sha1();
  }
  
  stream.binary().length = wholeCounter.filter().count();
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
  
  entry.binary().digest.size = inputCounter.filter().count();
  entry.binary().filteredSize = filteredInputCounter.filter().count();
  entry.binary().compressedSize = outputCounter.filter().count();
  
  stream.binary().length += entry.binary().compressedSize;
  
  if (_options.digest.crc32)
    entry.binary().digest.crc32 = digester.filter().crc32();

  if (_options.digest.md5)
    entry.binary().digest.md5 = digester.filter().md5();

  if (_options.digest.sha1)
    entry.binary().digest.sha1 = digester.filter().sha1();
  
  TRACE_A("%p: archive::write() written %lu bytes, filtered into %lu and compressed into %lu bytes", this, entry.binary().digest.size, entry.binary().filteredSize, entry.binary().compressedSize);
  
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
  
  TRACE_A("%p: archive::read() reading entry from stream %lu:%lu (size: %lu %lu %lu)", this, _entry.binary().stream, _entry.binary().indexInStream, _entry.binary().digest.size, _entry.binary().filteredSize, _entry.binary().compressedSize);

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
    size_t amount = total ? _entry.binary().digest.size : _entry.binary().filteredSize;
    
    for (box::index_t i = 0; i < _entry.binary().indexInStream; ++i)
    {
      const auto& b = _archive.entries()[stream.entries()[i]].binary();
      /* the amount to skip depends if we're extracting from stream filters or from both stream and entry */
      skipAmount += total ? b.digest.size : b.filteredSize;
    }
    
    TRACE_A("%p: archive::read() stream not seekable, preparing to seek to %lu+%lu and read %lu bytes", this, offset, skipAmount, amount);


    source_filter<filters::skip_filter>* skipper = new source_filter<filters::skip_filter>(source, _archive.options().bufferSize, skipAmount, amount, 0);
    _cache.cache(skipper);
    source = skipper;
  }

  return source;
}




