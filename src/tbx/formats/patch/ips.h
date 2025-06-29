#pragma once

#include <vector>
#include "tbx/base/common.h"

namespace patch::ips
{
  enum class ParseResult
  {
    InvalidHeader,
    InvalidRecordOffset,
    InvalidRecordHeader,
    InvalidRleData,
    Ok,
  };
  
  struct Entry
  {
    size_t offset; // 24 bits, big endian
    size_t size; // 16 bits, big endian

    std::vector<uint8_t> data;
    size_t rleSize;
    uint8_t rleByte;

    void apply(uint8_t* target) const
    {
      if (rleSize > 0)
      {
        std::fill_n(target + offset, rleSize, rleByte);
      }
      else if (!data.empty())
      {
        std::memcpy(target + offset, data.data(), size);
      }
    }
  };

  struct PatchData
  {
    std::vector<Entry> entries;

    ParseResult parse(uint8_t* data, size_t length)
    {
      /* verify first 5 bytes */
      if (length < 5 || memcmp(data, "PATCH", 5) != 0)
        return ParseResult::InvalidHeader;

      data += 5;
      length -= 5;
      
      bool finished = false;

      while (!finished)
      {
        if (length < 3)
            return ParseResult::InvalidRecordOffset;

        /* if length is "EOF" then we've finished */
        if (memcmp(data, "EOF", 3) == 0) 
        {
          finished = true;
          break;
        }

        if (length < 5)
          return ParseResult::InvalidRecordOffset;

        /* read offset as 3 bytes big endian number */
        size_t offset = (size_t)data[0] << 16 | (size_t)data[1] << 8 | data[2];
        /* read byte amount to write */
        size_t size = ((size_t)data[3] << 8) | data[4];

        data += 5;
        length -= 5;

        /* rle mode */
        if (size == 0)
        {
          if (length < 3)
            return ParseResult::InvalidRleData;

          size_t rleSize = (size_t)data[0] << 8 | data[1];
          uint8_t rleByte = data[2];

          entries.push_back({ offset, 0, {}, rleSize, rleByte });

          data += 3;
          length -= 3;
        }
        /* direct mode */
        else
        {
          if (size > length)
            return ParseResult::InvalidRecordHeader;

          Entry entry = { offset, size, std::vector<uint8_t>(data, data + size), 0, 0 };
          entries.push_back(entry);

          data += size;
          length -= size;
        }
      }

      return ParseResult::Ok;
    }
  };
}

struct data_source;
struct seekable_data_source;
struct data_sink;

namespace patch::ups
{
  struct Header
  {
    char magic[4];
    uint64_t inputSize;
    uint64_t outputSize;
    uint64_t headerSizeInBytes;
  };

  struct Checksum
  {
    uint32_t inputChecksum;
    uint32_t outputChecksum;
    uint32_t patchChecksum;
  };

  enum class Status
  {
    InvalidPatchChecksum,
    InvalidSourceSize,
    InvalidSourceChecksum,
    Ok,
  };
 
  struct Patch
  {
  protected:
    Header _header;
    Checksum _checksum;
    std::vector<uint8_t> _data;

  public:

    Status load(seekable_data_source* source);
    Status write(data_sink* sink);

    Status apply(seekable_data_source* source, data_sink* sink) const;
    Status generate(seekable_data_source* source, seekable_data_source* patched);


  protectedExposed:
    /* read a variable int and shift pointer by given amount*/
    static uint64_t readVariableInt(data_source* src);
    static void writeVariableInt(data_sink* sink, uint64_t value);

    static void test();
  };
}