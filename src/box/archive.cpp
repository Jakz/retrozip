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
  ordering.push(box::Section::HEADER);
  ordering.push(box::Section::ENTRY_TABLE);
  ordering.push(box::Section::ENTRY_PAYLOAD);
  ordering.push(box::Section::STREAM_TABLE);
  ordering.push(box::Section::STREAM_PAYLOAD);
  ordering.push(box::Section::STREAM_DATA);
  ordering.push(box::Section::FILE_NAME_TABLE);
}

bool Archive::isValidMagicNumber() const { return _header.magic == std::array<u8, 4>({ 'b', 'o', 'x', '!' }); }

bool Archive::isValidGlobalChecksum(W& w) const
{
  return !_header.hasFlag(box::HeaderFlag::INTEGRITY_CHECKSUM_ENABLED) || _header.fileChecksum == calculateGlobalChecksum(w, _options.checksum.digesterBuffer);
}

void Archive::write(W& w)
{
  assert(ordering.front() == box::Section::HEADER);
  ordering.pop();
  
  refs refs;
  
  refs.header = w.reserve<box::Header>();

  _header.entryCount = static_cast<box::count_t>(entries.size());
  _header.streamCount = static_cast<box::count_t>(streams.size());
  

  while (!ordering.empty())
  {
    box::Section section = ordering.front();
    ordering.pop();
    
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
        for (const ArchiveEntry& entry : entries)
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
        for (const ArchiveStream& stream : streams)
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
        
        for (const ArchiveEntry& entry : entries)
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
      }
    }
  }
  
  /* when we arrive here we suppose all streams have been written and all data
     in Stream and Entry has been prepared and filled */
  
  
  /* fill the array of file entries */
  for (size_t i = 0; i < entries.size(); ++i)
    refs.entryTable.write(entries[i].binary(), i);
  
  /* fill the array of stream entries */
  for (size_t i = 0; i < streams.size(); ++i)
    refs.streamTable.write(streams[i].binary(), i);
  
  /* this should be the last thing we do since it optionally computes hash for the whole file */
  finalizeHeader(w);
  refs.header.write(_header);
}

void Archive::read(R& r)
{
  r.seek(0);
  r.read(_header);
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
  data_source* source = entry.source().get();
  
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
  
  passthrough_pipe pipe(source, &w, _options.bufferSize);
  pipe.process();
}

void Archive::writeStream(W& w, ArchiveStream& stream)
{
  
}
