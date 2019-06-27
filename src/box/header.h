#pragma once

#include "tbx/base/common.h"
#include "tbx/hash/hash.h"

namespace box
{
  using version_t = u32;
  using offset_t = u64;
  using count_t = u32;
  using length_t = u64;
  using slength_t = u32;
  using tlength_t = u16;
  using index_t = s32;
  using checksum_t = hash::crc32_t;
  using digester_t = hash::crc32_digester;
  using payload_uid = u32;

  static constexpr index_t INVALID_INDEX = -1;

  static constexpr version_t CURRENT_VERSION = 0x00000001;

  enum class Section : u32
  {
    HEADER = 1,
    SECTION_TABLE,
    ENTRY_TABLE,
    COMMENTS_TABLE,
    ENTRY_PAYLOAD,
    STREAM_TABLE,
    STREAM_PAYLOAD,
    STREAM_DATA,
    FILE_NAME_TABLE,
    GROUP_TABLE,

    FIRST_FREE_SECTION_IDENT = 1U << 31
  };

  enum class HeaderFlag : u64
  {
    INTEGRITY_CHECKSUM_ENABLED = 0x01LLU
  };

  enum class StreamFlag : u64
  {
    SEEKABLE,
    HAS_CHECKSUM
  };

  STRUCT_PACKING_PUSH

  struct SectionHeader
  {
    offset_t offset;
    length_t size;
    Section type;
    count_t count;
    
  } PACKED_ATTRIBUTE;
  
  struct Header
  {
    std::array<u8, 4> magic; // must be "box!"
    u32 version;
    
    bit_mask<HeaderFlag> flags;
    
    SectionHeader index;
    
    length_t fileLength;
    checksum_t fileChecksum;

    Header() : magic({{'b','o','x','!'}}) { }
    
    bool hasFlag(HeaderFlag flag) const { return flags && flag; }
    
  } PACKED_ATTRIBUTE;
  

  struct DigestInfo
  {
    length_t size;
    hash::crc32_t crc32;
    hash::md5_t md5;
    hash::sha1_t sha1;
    
    DigestInfo() : size(0), crc32(0), md5(), sha1() { }
    DigestInfo(length_t size, hash::crc32_t crc32, const hash::md5_t& md5, const hash::sha1_t& sha1) : size(size), crc32(crc32), md5(md5), sha1(sha1) { }
    
    bool operator==(const DigestInfo& other) const { return size == other.size && crc32 == other.crc32 && md5 == other.md5 && sha1 == other.sha1; }
    
    struct hash
    {
      size_t operator()(const DigestInfo& digest) const { return std::hash<size_t>()(digest.crc32); }
    };
    
  } PACKED_ATTRIBUTE;
  
  enum class StorageMode : u32;
  enum class StorageSubmode : u32;
  
  struct Entry
  {
    //length_t originalSize; //TODO moved to DigestInfo
    length_t filteredSize;
    
    DigestInfo digest;
    
    index_t stream;
    index_t indexInStream;
    
    offset_t payload;
    count_t payloadLength;
    
    offset_t entryNameOffset;
    
    Entry() :
      filteredSize(0), digest(),
      stream(INVALID_INDEX), indexInStream(INVALID_INDEX) { }
  } PACKED_ATTRIBUTE;
  
  struct Stream
  {
    bit_mask<StreamFlag> flags;
    
    offset_t offset;
    length_t length;
    checksum_t checksum;
    
    offset_t payload;
    count_t payloadLength;
    
  } PACKED_ATTRIBUTE;
  
  struct Payload
  {
    payload_uid identifier;
    length_t length;
    u32 hasNext; /* to pad to 16 bytes */
  } PACKED_ATTRIBUTE;
  
  struct Group
  {
    count_t size;
  } PACKED_ATTRIBUTE;

  STRUCT_PACKING_POP
}
