#include "archive.h"

#include "memory_buffer.h"

template<typename T> using ref = data_reference<T>;
template<typename T> using aref = array_reference<T>;

struct refs
{
  ref<box::Header> header;
  aref<box::TableEntry> entryTable;
  aref<box::StreamEntry> streamTable;
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

template<typename WW> void Archive::write(W& w)
{
  assert(ordering.front() == box::Section::HEADER);
  ordering.pop();
  
  refs refs;
  
  refs.header = w.reserve<box::Header>();

  header.version = box::CURRENT_VERSION;
  header.entryCount = static_cast<box::count_t>(entries.size());
  header.streamCount = static_cast<box::count_t>(streams.size());
  

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
        refs.entryTable = w.reserveArray<box::TableEntry>(header.entryCount);
        header.entryTableOffset = refs.entryTable;
        break;
      }
        
      case box::Section::STREAM_TABLE:
      {
        /* save offset to the stream table and store it into header */
        refs.streamTable = w.reserveArray<box::StreamEntry>(header.streamCount);
        header.streamTableOffset = refs.streamTable;
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
        for (const Entry& entry : entries)
        {
          box::TableEntry& tentry = entry.tableEntry();
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
        for (const Stream& stream : streams)
        {
          box::StreamEntry& sentry = stream.streamEntry();
          sentry.payload = base + length;
          sentry.payloadLength = stream.payloadLength();
          
          length += sentry.payloadLength;
        }
        
        w.reserve(length);
        break;
      }
        
      case box::Section::FILE_NAME_TABLE:
      {
        off_t base = w.tell();
        off_t offset = w.tell();
        
        header.nameTableOffset = base;
        
        for (const Entry& entry : entries)
        {
          entry.tableEntry().entryNameOffset = offset;
          w.write(entry.name().c_str(), 1, entry.name().length());
          w.write((char)'\0');
          
          offset = w.tell();
        }
        
        header.nameTableLength = static_cast<box::count_t>(offset - base);
        break;
      }
        
      case box::Section::STREAM_DATA:
      {
        /* main stream writing */
      }
    }
  }
}
