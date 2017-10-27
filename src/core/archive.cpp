#include "archive.h"

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

  w.reserve(sizeof(rzip::Header));
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
        header.entryTableOffset = w.tell();
      }
    }
  }
}
