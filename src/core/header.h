#pragma once

#include "base/common.h"
#include "hash/hash.h"

namespace rzip
{
  using version_t = u32;
  using offset_t = u64;
  using count_t = u32;
  using length_t = u64;
  using checksum_t = hash::crc32_t;
  
  static constexpr version_t CURRENT_VERSION = 0x00000001;
  
  enum class Section
  {
    HEADER = 1,
    ENTRY_TABLE,
    ENTRY_PAYLOAD,
    STREAM_TABLE,
    STREAM_PAYLOAD,
    STREAM_DATA,
    FILE_NAME_TABLE
  };
  
  enum class HeaderFlags : u32;
  
  struct Header
  {
    std::array<u8, 4> magic; // must be rzip
    u32 version;
    
    bit_mask<HeaderFlags> flags;
    
    count_t entryCount;
    count_t streamCount;
    
    offset_t entryTableOffset;
    offset_t streamTableOffset;
    offset_t entryFileTableOffset;
    
    length_t fileLength;
    checksum_t fileChecksum;
    
    Header() : magic({{'r','z','i','p'}}) { }
    
  } __attribute__((packed));
  
  struct DigestInfo
  {
    hash::crc32_t crc32;
    hash::md5_t md5;
    hash::sha1_t sha1;
  };
  
  enum class StorageMode : u32;
  enum class StorageSubmode : u32;
  
  struct TableEntry
  {
    StorageMode mode;
    StorageSubmode submode;
    
    length_t realSize;
    length_t uncompressedSize;
    length_t compressedSize;
    
    DigestInfo digest;
    
    count_t stream;
    count_t indexInStream;
    
    offset_t payload;
    count_t payloadLength;
  } __attribute__((packed));
  
  enum class StreamType : u32;
  
  struct StreamEntry
  {
    StreamType type;
    
    offset_t offset;
    length_t length;
    checksum_t checksum;
    
    offset_t payload;
    count_t payloadLength;
    
  } __attribute__((packed));
}
