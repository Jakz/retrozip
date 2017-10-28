#include "archive.h"

#include "memory_buffer.h"

template<typename T> using ref = data_reference<T>;
template<typename T> using aref = array_reference<T>;

struct refs
{
  ref<rzip::Header> header;
  aref<rzip::TableEntry> entryTable;
  aref<rzip::StreamEntry> streamTable;
};

Archive::Archive()
{
  ordering.push(rzip::Section::HEADER);
  ordering.push(rzip::Section::ENTRY_TABLE);
  ordering.push(rzip::Section::ENTRY_PAYLOAD);
  ordering.push(rzip::Section::STREAM_TABLE);
  ordering.push(rzip::Section::STREAM_PAYLOAD);
  ordering.push(rzip::Section::STREAM_DATA);
  ordering.push(rzip::Section::FILE_NAME_TABLE);
}

template<typename WW> void Archive::write(W& w)
{
  assert(ordering.front() == rzip::Section::HEADER);
  ordering.pop();
  
  refs refs;
  
  refs.header = w.reserve<rzip::Header>();

  header.version = rzip::CURRENT_VERSION;
  header.entryCount = static_cast<rzip::count_t>(entries.size());
  header.streamCount = static_cast<rzip::count_t>(streams.size());
  

  while (!ordering.empty())
  {
    rzip::Section section = ordering.front();
    ordering.pop();
    
    switch (section)
    {
      case rzip::Section::HEADER: /* already managed */ break;
        
      case rzip::Section::ENTRY_TABLE:
      {
        refs.entryTable = w.reserveArray<rzip::TableEntry>(header.entryCount);
        header.entryTableOffset = refs.entryTable;
        break;
      }
        
      case rzip::Section::STREAM_TABLE:
      {
        refs.streamTable = w.reserveArray<rzip::StreamEntry>(header.streamCount);
        header.streamTableOffset = refs.streamTable;
        break;
      }
        
      case rzip::Section::ENTRY_PAYLOAD:
      {
        off_t base = w.tell();
        off_t length = 0;
        
        /* for each entry we get the payload length to compute each payload 
           offset inside the file, we also compute the total entry payload 
           length to reserve it
         */
        for (const Entry& entry : entries)
        {
          rzip::TableEntry& tentry = entry.tableEntry();
          tentry.payload = base + length;
          tentry.payloadLength = entry.payloadLength();
          
          length += tentry.payloadLength;
        }
        
        w.reserve(length);
        break;
      }
    }
  }
}
