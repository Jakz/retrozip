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
  using timestamp_t = s64;
  using index_t = s32;
  using checksum_t = hash::crc32_t;
  using digester_t = hash::crc32_digester;
  using payload_uid = u32;

  static constexpr index_t INVALID_INDEX = -1;

  static constexpr version_t CURRENT_VERSION = 0x00000001;

  enum class Section : u32
  {
    Header = 1,
    SectionTable,
    EntryTable,
    MetadataTable,
    EntryPayload,
    StreamTable,
    StreamPayload,
    StreamData,

    FirstFreeSectionIdent = 1U << 31
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

    SectionHeader() : SectionHeader(Section::Header) { }
    SectionHeader(Section type) : offset(0), size(0), type(type), count(0) { }
    
  } PACKED_ATTRIBUTE;
  
  struct Header
  {
    std::array<u8, 4> magic; // must be "box!"
    u32 version;
    
    bit_mask<HeaderFlag> flags;
    
    SectionHeader index;
    
    length_t fileLength;
    u32 custom;
    checksum_t fileChecksum;

    Header() : magic({{'b','o','x','!'}}), custom(0) { }
    
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
    length_t filteredSize;
    
    DigestInfo digest;
    
    index_t stream;
    index_t indexInStream;
    
    timestamp_t timestamp;

    offset_t metadataOffset;

    offset_t payloadOffset;
    count_t payloadLength;

    Entry() :
      filteredSize(0), digest(), timestamp(0),
      stream(INVALID_INDEX), indexInStream(INVALID_INDEX) { }
  } PACKED_ATTRIBUTE;
  
  struct Stream
  {
    bit_mask<StreamFlag> flags;
    
    offset_t offset;
    length_t length;
    
    offset_t payload;
    count_t payloadLength;
    
    checksum_t checksum;
  } PACKED_ATTRIBUTE;
  
  struct Payload
  {
    length_t length;
    payload_uid identifier;
    u32 hasNext; /* to pad to 16 bytes */
    
    Payload() : length(0), identifier(0), hasNext(0) { }
    Payload(payload_uid identifier, length_t length, bool hasNext = false) :
      length(length), identifier(identifier), hasNext(hasNext ? 1 : 0) { }
  } PACKED_ATTRIBUTE;
  
  struct Group
  {
    count_t size;
  } PACKED_ATTRIBUTE;

  enum MetadataType : uint8_t
  {
    ValueTypeMask  = 0b00000011,
    
    ValueNone      = 0b00000000,
    ValueString    = 0b00000001,
    ValueBinary    = 0b00000010,
    ValueNumber    = 0b00000011,

    KeyTypeMask    = 0b00000100,
    KeyString      = 0b00000100,
    KeyUid         = 0b00000000,

    StringString = KeyString | ValueString,
    StringBinary = KeyString | ValueBinary,
    UidString    = KeyUid | ValueString,
    UidBinary    = KeyUid | ValueBinary,

    PredefinedMask = 0b10000000,
    PredefinedName = PredefinedMask | (0b00001 << 2) | ValueString,
    PredefinedFilterPayload = PredefinedMask | (0b00010 << 2) | ValueBinary,
    PredefinedComment = PredefinedMask | (0b00011 << 2) | ValueString,
    PredefinedGroup = PredefinedMask | (0b00100 << 2) | ValueString,
  };

  enum class MetadataValueType
  {
    None = ValueNone,
    String = ValueString,
    Binary = ValueBinary,
    Number = ValueNumber,
    
  };

  enum class KnownMetadata : uint8_t
  {
    None = 0,
    Name = PredefinedName,
    Comment = PredefinedComment,
    FilterPayload = PredefinedFilterPayload,
  };

  struct MetadataEntry
  {

  protected:
    std::string _key;
    uint64_t _uid;

    std::vector<uint8_t> _data;
    MetadataType _type;
    
  public:
    MetadataEntry() : _uid(0), _type(MetadataType::StringString) { }
    
    /* string key constructors */
    MetadataEntry(std::string_view key, std::string_view data) : _key(key), _uid(0), _data(data.begin(), data.end()), _type(MetadataType::StringString) { }
    MetadataEntry(std::string_view key, const std::vector<uint8_t>& data) : _key(key), _uid(0), _data(data), _type(MetadataType::StringBinary) { }
    /* numerical key constructors */
    MetadataEntry(uint64_t uid, std::string_view data) : _uid(uid), _data(data.begin(), data.end()), _type(MetadataType::UidString) { }
    MetadataEntry(uint64_t uid, const std::vector<uint8_t>& data) : _uid(uid), _data(data), _type(MetadataType::UidBinary) { }
    
    MetadataEntry(KnownMetadata key, std::string_view data) : _key(), _uid(0), _data(data.begin(), data.end()), _type(static_cast<MetadataType>(int(key) | int(ValueString))) { }

    std::string_view literal() const
    { 
      assert(valueType() == MetadataValueType::String); 
      return std::string_view(reinterpret_cast<const char*>(_data.data()), _data.size()); 
    }

    const std::string& key() const { assert(isStringKey());  return _key; }
    uint64_t uid() const { assert(!isStringKey());  return _uid; }

    std::string keyMnemonic() const;

    const std::vector<uint8_t>& data() const { return _data; }
    MetadataType type() const { return _type; }
    MetadataValueType valueType() const { return static_cast<MetadataValueType>(_type & ValueTypeMask); }
    KnownMetadata knownType() const { return static_cast<KnownMetadata>(_type); }
    
    void setValue(std::string_view value) 
    { 
      assert(valueType() == MetadataValueType::String); 
      _data.assign(value.begin(), value.end()); 
    }
    
    size_t sizeInBytes() const;

    bool isPredefinedKey() const { return _type & MetadataType::PredefinedMask; }
    bool isStringKey() const { return !isPredefinedKey() && (_type & MetadataType::KeyTypeMask) == MetadataType::KeyString; }
    
    size_t serialize(data_sink* sink) const;
    void unserialize(data_source* source);

    bool operator==(const MetadataEntry& other) const;
    bool operator!=(const MetadataEntry& other) const { return !(*this == other); }
  };

  STRUCT_PACKING_POP
}
