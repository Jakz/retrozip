#pragma once

#include "base/common.h"
#include "hash/hash.h"

namespace box
{
  using version_t = u32;
  using offset_t = u64;
  using count_t = u32;
  using length_t = u64;
  using slength_t = u32;
  using index_t = s32;
  using checksum_t = hash::crc32_t;
  using digester_t = hash::crc32_digester;
  using payload_uid = u32;
  
  static constexpr index_t INVALID_INDEX = -1;
  
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
  
  enum class HeaderFlag : u64
  {
    INTEGRITY_CHECKSUM_ENABLED = 0x01LLU
  };
  
  struct Header
  {
    std::array<u8, 4> magic; // must be "box!"
    u32 version;
    
    bit_mask<HeaderFlag> flags;
    
    count_t entryCount;
    count_t streamCount;
    
    offset_t entryTableOffset;
    offset_t streamTableOffset;
    offset_t nameTableOffset;
    count_t nameTableLength;
    
    length_t fileLength;
    checksum_t fileChecksum;
    
    Header() : magic({{'b','o','x','!'}}) { }
    
    bool hasFlag(HeaderFlag flag) const { return flags && flag; }
    
  } __attribute__((packed));
  
  struct DigestInfo
  {
    hash::crc32_t crc32;
    hash::md5_t md5;
    hash::sha1_t sha1;
    
    DigestInfo() : crc32(0), md5(), sha1() { }
    DigestInfo(hash::crc32_t crc32, const hash::md5_t& md5, const hash::sha1_t& sha1) : crc32(crc32), md5(md5), sha1(sha1) { }
  };
  
  enum class StorageMode : u32;
  enum class StorageSubmode : u32;
  
  struct Entry
  {
    StorageMode mode;
    StorageSubmode submode;
    
    length_t originalSize;
    length_t filteredSize;
    length_t compressedSize;
    
    DigestInfo digest;
    
    index_t stream = INVALID_INDEX;
    index_t indexInStream = INVALID_INDEX;
    
    offset_t payload;
    count_t payloadLength;
    
    offset_t entryNameOffset;
    
    Entry() : stream(INVALID_INDEX), indexInStream(INVALID_INDEX) { }
  } __attribute__((packed));
  
  enum class StreamType : u32;
  
  struct Stream
  {
    StreamType type;
    
    offset_t offset;
    length_t length;
    checksum_t checksum;
    
    offset_t payload;
    count_t payloadLength;
    
  } __attribute__((packed));
  
  struct Payload
  {
    payload_uid identifier;
    length_t length;
    u32 hasNext; /* to pad to 16 bytes */
  } __attribute__((packed));
}
