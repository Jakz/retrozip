#include "archive.h"

#include "tbx/streams/memory_buffer.h"
#include "tbx/streams/data_pipe.h"

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
  _ordering.push_back(box::Section::Header);
  _ordering.push_back(box::Section::SectionTable);
  _ordering.push_back(box::Section::EntryTable);
  _ordering.push_back(box::Section::StreamTable);
  _ordering.push_back(box::Section::MetadataTable);
  _ordering.push_back(box::Section::StreamData);
}

bool Archive::isValidMagicNumber() const { return _header.magic == std::array<u8, 4>({ 'b', 'o', 'x', '!' }); }

bool Archive::isValidGlobalChecksum(W& w) const
{
  return !_header.hasFlag(box::HeaderFlag::INTEGRITY_CHECKSUM_ENABLED) || _header.fileChecksum == calculateGlobalChecksum(w, _options.checksum.digesterBuffer);
}

ArchiveSizeInfo Archive::sizeInfo() const
{
  const size_t uncompressedEntriesSize = std::accumulate(_entries.begin(), _entries.end(), 0UL, [] (size_t count, const ArchiveEntry& entry) {
    return count + entry.binary().digest.size;
  });

  const size_t entriesPayload = std::accumulate(_entries.begin(), _entries.end(), 0UL, [](size_t count, const ArchiveEntry& entry) {
    return count + entry.payloadLength();
    });

  const size_t streamsPayload = std::accumulate(_streams.begin(), _streams.end(), 0UL, [] (size_t count, const ArchiveStream& stream) {
    return count + stream.payloadLength();
  });
  
  const size_t streamData = std::accumulate(_streams.begin(), _streams.end(), 0UL, [] (size_t count, const ArchiveStream& stream) {
    return count + stream.binary().length;
  });
  
  const size_t sizeOnDisk = sizeof(box::Header)
  + sizeof(box::Entry) * _entries.size()
  + sizeof(box::Stream) * _streams.size()
  + ((!_entries.empty() && !_streams.empty()) ? sizeof(box::SectionHeader)*4 : 0) /* entry table, stream table, stream data, entry names section headers */
  + (streamsPayload > 0 ? sizeof(box::SectionHeader) : 0)
  + std::accumulate(_streams.begin(), _streams.end(), 0UL, [] (size_t count, const ArchiveStream& entry) { return entry.binary().length + count; })
  + std::accumulate(_entries.begin(), _entries.end(), 0UL, [] (size_t count, const ArchiveEntry& entry) { return entry.name().length() + 1 + count; })
  + streamsPayload;
  
  return { sizeOnDisk, streamsPayload, entriesPayload, streamData, uncompressedEntriesSize };
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
      throw uexc(fmt::format("indexInStream not set for entry {}", index));
    else if (stream == box::INVALID_INDEX)
      throw uexc(fmt::format("stream index not set for entry {}", index));
    
    /* check that no other entry is mapped in the same position */
    auto existing = mapping.find({ stream, indexInStream });
    
    if (existing != mapping.end())
      throw uexc(fmt::format("entry {} and {} are both mapped to stream {}:{} ", index, existing->entryIndex, stream, indexInStream));
    
    /* check that mapping is consistent */
    if (stream >= _streams.size())
      throw uexc(fmt::format("stream index out of bounds for entry {}", index));
    else if (indexInStream >= _streams[binary.stream].entries().size())
      throw uexc(fmt::format("index in stream out of bounds ({}:{}) for entry {}", stream, indexInStream, index));
    
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
        throw uexc(fmt::format("entry not set stream {}", index));
      if (entry >= _entries.size())
        throw uexc(fmt::format("entry {} out of bounds for stream {}", entry, index));
    }
    
    ++index;
  }
  
  /* check that all groups have valid indices */
  for (const auto& group : _groups)
  {
    std::unordered_set<ArchiveEntry::ref> uniques(group.entries().begin(), group.entries().end());
    
    if (uniques.size() != group.size())
      throw uexc(fmt::format("group '{}' has non unique entries", group.name().c_str()));
    
    if (std::any_of(group.begin(), group.end(), [this](ArchiveEntry::ref index) { return index >= _entries.size() || index < 0; }))
      throw uexc(fmt::format("group '{}' has invalid indices", group.name()));
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
    archive._entries.emplace_back(entry.source, entry.filters, entry.metadata);
  
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
  
  archive.options().bufferSize = 16_kb;
  
  return archive;
}

bool Archive::willSectionBeSerialized(box::Section section) const
{
  switch (section)
  {
    case box::Section::Header: assert(false); return false;
    case box::Section::SectionTable: assert(false); return false;
    case box::Section::EntryTable: return !_entries.empty();
    case box::Section::StreamTable: return !_streams.empty();

    case box::Section::MetadataTable: return 
      std::any_of(_entries.begin(), _entries.end(), [](const ArchiveEntry& entry) { return entry.shouldSerializeMetadata(); }) ||
      std::any_of(_streams.begin(), _streams.end(), [](const ArchiveStream& stream) { return !stream.filters().empty(); });
    
    case box::Section::StreamData: return !_streams.empty();
            
    case box::Section::FirstFreeSectionIdent:
      //TODO: custom section serialization management
      assert(false);
  }
}

void Archive::write(W& w)
{
  assert(_ordering.front() == box::Section::Header);
  _ordering.pop_front();
  
  _headers.clear();

  env = { this, &w, filter_repository::instance() };

  refs refs;
  refs.header = w.reserve<box::Header>();

  TRACE_A("%p: archive::write() writing %lu entries in %lu streams", this, _entries.size(), _streams.size());

  while (!_ordering.empty())
  {
    box::Section section = _ordering.front();
    _ordering.pop_front();
    
    box::SectionHeader sectionHeader = box::SectionHeader(section);

    switch (section)
    {
      case box::Section::Header:
        /* already managed */
        
        /* section table must be first section after header */
        assert(_ordering.front() == box::Section::SectionTable);
      break;
        
      case box::Section::SectionTable:
      {
        size_t effectiveSections = std::count_if(_ordering.begin(), _ordering.end(), [this] (box::Section section) { return willSectionBeSerialized(section); });
        
        refs.sectionTable = w.reserveArray<box::SectionHeader>(effectiveSections);

        _header.index.offset = refs.sectionTable;
        _header.index.count = static_cast<box::count_t>(effectiveSections);
        _header.index.size = sizeof(box::SectionHeader) * _header.index.count;
        _header.index.type = box::Section::SectionTable;
        
        TRACE_A("%p: archive::write() reserved section table for %lu entries (%lu bytes) at %Xh (%lu)", this, _header.index.count, _header.index.size, _header.index.offset, _header.index.offset);
        break;
      }
        
      case box::Section::EntryTable:
      {
        /* save offset to the entry table and store it into header */
        refs.entryTable = w.reserveArray<box::Entry>(_entries.size());
        
        sectionHeader.offset = refs.entryTable;
        sectionHeader.count = static_cast<box::count_t>(_entries.size());
        sectionHeader.size = static_cast<box::length_t>(sizeof(box::Entry) * sectionHeader.count);
        
        TRACE_A("%p: archive::write() reserved entry table for %lu entries (%lu bytes) at %Xh (%lu)", this, sectionHeader.count, sectionHeader.size, sectionHeader.offset, sectionHeader.offset);
        break;
      }
        
      case box::Section::StreamTable:
      {
        /* save offset to the stream table and store it into header */
        refs.streamTable = w.reserveArray<box::Stream>(_streams.size());
        
        sectionHeader.offset = refs.streamTable;
        sectionHeader.count = static_cast<box::count_t>(_streams.size());
        sectionHeader.size = static_cast<box::length_t>(sizeof(box::Stream) * sectionHeader.count);
        
        TRACE_A("%p: archive::write() reserved stream table for %lu streams (%lu bytes) at %Xh (%lu)", this, sectionHeader.count, sectionHeader.size, sectionHeader.offset, sectionHeader.offset);
        break;
      }
 
      case box::Section::MetadataTable:
      {
        roff_t base = w.tell();
        roff_t offset = w.tell();
        
        sectionHeader.offset = w.tell();
        
        size_t idx = 0;
        /* store metadata for entries */
        for (const ArchiveEntry& entry : _entries)
        {
          /* entry has metadata or has a name or payload */
          if (entry.shouldSerializeMetadata())
          {
            /* set metadata info in entry header */
            entry.binary().metadataOffset = w.tell();

            enriched_data_sink::writeLEB128(&w, entry.metadata().size() + (entry.payloadLength() > 0 ? 1 : 0));
            
            for (const auto& mentry : entry.metadata())
            {
              TRACE_A2("%p: archive::write() writing entry metadata '%lu:%s' at %Xh (%lu)", this, idx, mentry.keyMnemonic().c_str(), offset, offset);
              mentry.serialize(&w);
            }

            offset = w.tell();
            ++idx;

            /* entry has payload, write it as a byte blob */
            if (!entry.filters().empty())
            {
              entry.serializePayload(env);
              const memory_buffer& payloadBuffer = entry.payload();
              std::vector<uint8_t> payload(payloadBuffer.raw(), payloadBuffer.raw() + payloadBuffer.size());
              box::MetadataEntry(box::KnownMetadata::FilterPayload, payload).serialize(&w);
              TRACE_A2("%p: archive::write() writing entry %lu:%lu payload of %lu bytes at %Xh (%lu)", this, entry.binary().stream, entry.binary().indexInStream, payload.size(), offset, offset);
              offset = w.tell();
            }
          }
          else
          {
            /* if no metadata we set offset to 0 and count to 0 */
            entry.binary().metadataOffset = 0;
          }
        }

        /* store metadata for streams, which is just payload */
        idx = 0;
        for (const ArchiveStream& stream : _streams)
        {
          if (!stream.filters().empty())
          {            
            stream.binary().payloadOffset = offset;
            stream.serializePayload(env);
            const memory_buffer& payloadBuffer = stream.payload();
            std::vector<uint8_t> payload(payloadBuffer.raw(), payloadBuffer.raw() + payloadBuffer.size());
            box::MetadataEntry(box::KnownMetadata::FilterPayload, payload).serialize(&w);
            TRACE_A2("%p: archive::write() writing stream %lu payload of %lu bytes at %Xh (%lu)", this, idx, payload.size(), offset, offset);
            offset = w.tell();
          }
          ++idx;
        }
        
        sectionHeader.size = static_cast<box::count_t>(offset - base);
        
        TRACE_A("%p: archive::write() written metadata table of %lu bytes at %Xh (%lu)", this, sectionHeader.size, sectionHeader.offset, sectionHeader.offset);
        break;
      }
        
      case box::Section::StreamData:
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
          
          TRACE_A("%p: archive::write() writing stream at offset %Xh (%lu)", this, stream.binary().offset, stream.binary().offset);
          
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

      case box::Section::FirstFreeSectionIdent:
      {
        //TODO: can this happen?
        break;
      }
    }
    
    /* if section is not the header or the section table and it has elements we prepare it to be serialized */
    if (section != box::Section::Header && section != box::Section::SectionTable && sectionHeader.size > 0)
      _headers.emplace(std::make_pair(section, sectionHeader));
  }
    
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
    case S::Header: /* should never happen */ assert(false); break;
    case S::SectionTable: /* should never happen */ assert(false); break;
    
    case S::EntryTable:
    {
      /* read entries */
      for (size_t i = 0; i < header.count; ++i)
      {
        r.seek(header.offset + i*sizeof(box::Entry));
        
        /* read entry */
        box::Entry entry;
        r.read(entry);

        /* load payload */
        std::vector<byte> payload;

        /* read metadata */
        metadata_list_t metadata;
        if (entry.metadataOffset) //TODO: using != 0 to guarantee presence of metadata but it's fragile (0 is still a valid offset)
        {
          
          r.seek(entry.metadataOffset);
          auto count = enriched_data_source::readLEB128(&r);
          TRACE_A2("%p: archive::readEntryTable() seeking to read %lu metadata at %Xh (%lu)", this, count, entry.metadataOffset, entry.metadataOffset);

          for (size_t j = 0; j < count; ++j)
          {
            metadata.push_back(box::MetadataEntry());
            metadata.back().unserialize(&r);

            /* if metadata is payload then get it and pop from metadata list */
            if (metadata.back().knownType() == box::KnownMetadata::FilterPayload)
            {
              payload = metadata.back().data();
              metadata.pop_back();
            }
          }
        }
        
        _entries.emplace_back(entry, payload, metadata);
      }

      break;
    }
      
    case S::StreamTable:
    {
      for (size_t i = 0; i < header.count; ++i)
      {
        r.seek(header.offset + i*sizeof(box::Stream));
        
        /* read stream header */
        box::Stream stream;
        r.read(stream);

        std::vector<byte> payload;

        /* load stream payload if present */
        if (stream.payloadOffset > 0) //TODO: 0 is not a nice value, we should need an optional offset (use s64 with negative as non present?)
        {
          r.seek(stream.payloadOffset);
          box::MetadataEntry payloadMetadata;
          payloadMetadata.unserialize(&r);
          assert(payloadMetadata.knownType() == box::KnownMetadata::FilterPayload && payloadMetadata.valueType() == box::MetadataValueType::Binary);
          payload = payloadMetadata.data();
        }

        _streams.emplace_back(stream, payload);
      }
      
      break;
    }
      
    case S::MetadataTable:
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
  
  env = { this, &r, filter_repository::instance() };
  
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
  
  /* unserialize payload for filters */
  for (auto& entry : _entries)
    entry.unserializePayload(env);
  for (auto& stream : _streams)
    stream.unserializePayload(env);

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
    
  for (ArchiveEntry::ref index : stream.entries())
  {
    ArchiveEntry& entry = _entries[index];
    data_source* source = entry.source();

    /* if source is nullptr we need to prepare it from the existing archive */
    if (!source)
    {

    }
    
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
  
  counter_t wholeCounter(streamCache.get());

  data_source* finalStream = &wholeCounter;

#if defined(DEBUG)
  source.setOnBegin([this, &sources](data_source* source) {
    auto it = std::find_if(sources.begin(), sources.end(), [source](const data_source_helper& helper) { return helper.source == source; });
    assert(it != sources.end());
    auto& entry = it->entry;
    TRACE_A("%p: archive::write() preparing to write entry %s", this, std::string(entry.name()).c_str());
  });
#endif

  source.setOnEnd([this, &sources](data_source* source) {
    /* TODO: this is linear, we can use a std::unordered_map if really many entries are stored in single stream but it's quite irrelevant */
    auto it = std::find_if(sources.begin(), sources.end(), [source](const data_source_helper& helper) { return helper.source == source; });
    assert(it != sources.end());
    TRACE_A("%p: archive::write() written %lu bytes, filtered into %lu", this, it->inputCounter->filter().count(), it->filteredCounter->filter().count());
  });
    
  assert(_options.bufferSize > 0);
  passthrough_pipe pipe(finalStream, &w, _options.bufferSize);
  pipe.process([this, &wholeCounter, &sources]() {
    //TODO: performance costly
    size_t inputSum = 0;
    for (const data_source_helper& helper : sources)
      inputSum += helper.inputCounter->filter().count();
 
    TRACE_A("%p: archive::write() processed %s into %s bytes", this, strings::humanReadableSize(inputSum, true).c_str(), strings::humanReadableSize(wholeCounter.filter().count(), true).c_str());
  });
  
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

void ArchiveReadHandle::prepareWorkflow(data_sink* sink)
{
  /* get final source for entry extraction */
  auto* source = this->source(true);

  /* generate last task with specified sink */
  _env.tasks.add(new simple_process_task("stream-process", source, sink, _entry.binary().digest.size));
}

ArchiveReadHandle::~ArchiveReadHandle()
{
  TRACE_A("% p: archive::destroy()");
}

data_source* ArchiveReadHandle::source(bool total)
{
  _cache.clear();
  
  TRACE_A("%p: archive::read() reading entry from stream %lu:%lu (size: %lu %lu)", this, _entry.binary().stream, _entry.binary().indexInStream, _entry.binary().digest.size, _entry.binary().filteredSize);

  /* first we need to know if stream is seekable, if it is we can seek to correct entry
     offset before reading from it, otherwise we need to skip */
  const ArchiveStream& stream = _archive.streams()[_entry.binary().stream];
  
  bool isSeekable = stream.binary().flags && box::StreamFlag::SEEKABLE;
  
  data_source* source = &r;
  
  size_t offset = stream.binary().offset;
  
  /* TODO: need to fix const-cast */
  _env = { const_cast<Archive*>(&_archive), &r, filter_repository::instance() };
  
  /*if (isSeekable)
  {
    // if stream is seekable we can find the offset to start reading from by adding all previous
    //   entries in the stream
    for (box::index_t i = 0; i < _entry.binary().indexInStream; ++i)
      offset += _archive.entries()[stream.entries()[i]].binary().compressedSize;
    
    source_filter<filters::skip_filter>* skipper = new source_filter<filters::skip_filter>(source, _archive.options().bufferSize, 0, _entry.binary().compressedSize, 0);
    _cache.cache(skipper);
    source = skipper;
  }*/
  
  /* move to the start of the stream */
  /*TODO: lambda_init_data_source is leaking */
  source = new lambda_init_data_source<data_source>(source, [this, offset]()
  {
    r.seek(offset);
  });
  
  //source = new source_filter<filters::skip_filter>(source, _archive.options().bufferSize, 0, _entry.binary().filteredSize, 0);
  
  /* this doesn't unapply entry filters, just stream filters */
  _cache.setSource(source);
  stream.filters().unsetup(_env);
  stream.filters().unapply(_cache);
  
  if (total)
  {
    _entry.filters().unsetup(_env);
    _entry.filters().unapply(_cache);
  }

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
    
    TRACE_A("%p: archive::read() stream not seekable, preparing to seek to %lu+%lu and produce %lu bytes", this, offset, skipAmount, amount);


    source_filter<filters::skip_filter>* skipper = new source_filter<filters::skip_filter>(source, _archive.options().bufferSize, skipAmount, amount, 0);
    _cache.cache(skipper);
    source = skipper;
  }

  return source;
}



void Metadata::serialize(data_sink* sink) const
{
  /* write count */
  enriched_data_sink::writeLEB128(sink, _entries.size());
  /* write each entry */
  for (const auto& entry : _entries)
    entry.serialize(sink);
}

void Metadata::unserialize(data_source* source)
{
  /* read count */
  uint64_t count = enriched_data_source::readLEB128(source);
  _entries.clear();
  _entries.resize(count);
  /* read each entry */
  for (uint64_t i = 0; i < count; ++i)
    _entries[i].unserialize(source);
}


std::string box::MetadataEntry::keyMnemonic() const
{
  if (isPredefinedKey())
  {
    switch (KnownMetadata(_type))
    {
      case KnownMetadata::Name: return "[k]name";
      case KnownMetadata::Comment : return "[k]comment";
      default: return "[k]";
    }
  }
  else if (isStringKey())
    return _key;
  else
    return std::to_string(_uid);
}

size_t box::MetadataEntry::sizeInBytes() const
{
  size_t size = sizeof(MetadataType);

  /* compute key size */
  if (isPredefinedKey())
    ; // predefined keys have no size, they are specified in the metadata type field
  else if (isStringKey())
    /* lbe of key length + key data */
    size += _key.size() + enriched_data_sink::sizeofLEB128(_key.size());
  else
    /* lbe of uid length */
    size += enriched_data_sink::sizeofLEB128(_uid);

  /* compute value size */
  switch (valueType())
  {
    case MetadataValueType::None: break;
    case MetadataValueType::Number: assert(false); break;
    case MetadataValueType::Binary:
    case MetadataValueType::String:
      size += enriched_data_sink::sizeofLEB128(_data.size()) + _data.size();
      break;
    default:
      break;
  }

  return size;
}


size_t box::MetadataEntry::serialize(data_sink* sink) const
{
  auto esink = enriched_data_sink(sink);
  esink.write<MetadataType>(_type);

  /* write key length (or uid) only if not predefined key */
  if (!isPredefinedKey())
    esink.writeLEB128(isStringKey() ? _key.length() : _uid);
  /* write value length */
  esink.writeLEB128(_data.size());

  /* is key is of string type then write the string */
  if (isStringKey())
    esink.write(_key.c_str(), _key.length());

  /* write data */
  esink.write(_data.data(), _data.size());  

  return sizeInBytes();
}

void box::MetadataEntry::unserialize(data_source* source)
{
  auto esource = enriched_data_source(source);
  /* read type*/
  _type = esource.read<MetadataType>();
  /* read key if present */
  uint64_t key = 0;
  if (!isPredefinedKey())
    key = esource.readLEB128();

  /* read data length */
  uint64_t dataLen = esource.readLEB128();

  /* now if key is of string type read key otherwise set uid */
  if (isStringKey())
  {
    _key.resize(key);
    esource.read(_key.data(), key);
  }
  else
    _uid = key;

  /* read data */
  _data.resize(dataLen);
  esource.read(_data.data(), dataLen);
}

bool box::MetadataEntry::operator==(const MetadataEntry& other) const
{
  return _type == other._type && _data == other._data && _uid == other._uid && _key == other._key;
}


